// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF AD

#include "dawn/native/TempGPUBufferManager.h"

#include <map>

#include "absl/container/inlined_vector.h"
#include "dawn/native/Buffer.h"
#include "dawn/native/Device.h"
#include "dawn/native/Queue.h"
#include "dawn/native/ResourceHeap.h"
#include "dawn/native/ResourceHeapAllocator.h"

namespace dawn::native {

namespace {

constexpr size_t kMaxBuddyMemory = 4ull * 1024 * 1024 * 1024;  // 4GB
constexpr size_t kBuddyBlockSize = 4ull * 1024 * 1024;         // 4MB

// Assuming 60 submissions per second. We want to keep the unused allocations alive for at least 5
// seconds.
constexpr ExecutionSerial kKeepAliveDuration(60 * 5);

// This is the base buffer which will be used for suballocations.
class TempGPUBufferBaseAllocation : public ResourceHeapBase {
  public:
    explicit TempGPUBufferBaseAllocation(Ref<BufferBase> buffer) : mBuffer(std::move(buffer)) {}
    ~TempGPUBufferBaseAllocation() override = default;

    BufferBase* GetBuffer() const { return mBuffer.Get(); }

    Ref<BufferBase> TakeBuffer() { return std::move(mBuffer); }

  private:
    Ref<BufferBase> mBuffer;
};

}  // namespace

// This is the allocator which will be used for allocating base buffers.
class TempGPUBufferManager::Allocator : public ResourceHeapAllocator {
  public:
    explicit Allocator(DeviceBase* device, wgpu::BufferUsage bufferUsage)
        : mDevice(device), mBufferUsage(bufferUsage) {}
    ~Allocator() override = default;

    ResultOrError<std::unique_ptr<ResourceHeapBase>> AllocateResourceHeap(uint64_t size) override {
        Ref<BufferBase> buffer;
        DAWN_TRY_ASSIGN(buffer, AllocateBuffer(size));

        return {std::make_unique<TempGPUBufferBaseAllocation>(std::move(buffer))};
    }

    void DeallocateResourceHeap(std::unique_ptr<ResourceHeapBase> allocation) override {
        auto bufferAllocation = static_cast<TempGPUBufferBaseAllocation*>(allocation.get());

        DeallocateBuffer(bufferAllocation->TakeBuffer());

        std::move(allocation).reset();
    }

    ResultOrError<Ref<BufferBase>> AllocateBuffer(uint64_t size) {
        // Try to find in existing resource in deletion queue.
        auto bucketIte = mBuffersToDelete.find(size);
        if (bucketIte != mBuffersToDelete.end()) {
            auto existingBuffer = bucketIte->second.TakeOneFromFirstSerial();
            if (bucketIte->second.Empty()) {
                mBuffersToDelete.erase(bucketIte);  // Remove empty bucket.
            }
            return std::move(existingBuffer);
        }

        Ref<BufferBase> buffer;
        BufferDescriptor desc;
        desc.label = "Dawn_TempGPUBuffer";
        desc.size = size;
        desc.usage = mBufferUsage;
        return mDevice->CreateBuffer(&desc);
    }

    void DeallocateBuffer(Ref<BufferBase> buffer) {
        mBuffersToDelete[buffer->GetSize()].Enqueue(std::move(buffer),
                                                    mDevice->GetQueue()->GetPendingCommandSerial());
    }

    void Tick(ExecutionSerial completedSerial) {
        if (completedSerial < kKeepAliveDuration) {
            return;
        }

        absl::InlinedVector<AllocationsQueuePerSize::iterator, 1> emptyBuckets = {};
        for (auto ite = mBuffersToDelete.begin(); ite != mBuffersToDelete.end(); ++ite) {
            ite->second.ClearUpTo(completedSerial - kKeepAliveDuration);
            if (ite->second.Empty()) {
                emptyBuckets.emplace_back(ite);
            }
        }

        for (auto bucket : emptyBuckets) {
            mBuffersToDelete.erase(bucket);
        }
    }

  private:
    raw_ptr<DeviceBase> mDevice;
    const wgpu::BufferUsage mBufferUsage;

    using AllocationsQueuePerSize =
        std::map<uint64_t, SerialQueue<ExecutionSerial, Ref<BufferBase>>>;

    AllocationsQueuePerSize mBuffersToDelete;
};

TempGPUBufferManager::TempGPUBufferManager(DeviceBase* device, wgpu::BufferUsage bufferUsage)
    : mDevice(device),
      mAllocator(std::make_unique<Allocator>(device, bufferUsage)),
      mBuddySubAllocator(kMaxBuddyMemory, kBuddyBlockSize, mAllocator.get()) {}

TempGPUBufferManager::~TempGPUBufferManager() = default;

ResultOrError<TempGPUBufferManager::Allocation> TempGPUBufferManager::Allocate(
    uint64_t allocationSize,
    ExecutionSerial useInSerial,
    uint64_t offsetAlignment) {
    if (allocationSize <= kBuddyBlockSize) {
        // Try to suballocate first
        ResourceMemoryAllocation allocation;
        if (!mDevice->ConsumedError(mBuddySubAllocator.Allocate(allocationSize, offsetAlignment),
                                    &allocation)) {
            mInflightSubAllocations.Enqueue(allocation, useInSerial);

            auto* bufferAllocation =
                static_cast<TempGPUBufferBaseAllocation*>(allocation.GetResourceHeap());

            Allocation result;
            result.buffer = bufferAllocation->GetBuffer();
            result.offset = allocation.GetOffset();
            result.size = allocationSize;

            return result;
        }
    }

    // If suballocation not possible, try to directly allocate
    Ref<BufferBase> buffer;
    DAWN_TRY_ASSIGN(buffer, mAllocator->AllocateBuffer(allocationSize));

    Allocation result;
    result.buffer = buffer.Get();
    result.offset = 0;
    result.size = allocationSize;

    mInflightAllocations.Enqueue(std::move(buffer), useInSerial);

    return result;
}

void TempGPUBufferManager::Deallocate(ExecutionSerial completedSerial) {
    for (const auto& subAllocation : mInflightSubAllocations.IterateUpTo(completedSerial)) {
        mBuddySubAllocator.Deallocate(subAllocation);
    }
    for (auto& allocation : mInflightAllocations.IterateUpTo(completedSerial)) {
        mAllocator->DeallocateBuffer(std::move(allocation));
    }

    mInflightSubAllocations.ClearUpTo(completedSerial);
    mInflightAllocations.ClearUpTo(completedSerial);

    mAllocator->Tick(completedSerial);
}

}  // namespace dawn::native
