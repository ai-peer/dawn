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

#include <sstream>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "dawn/common/Math.h"
#include "dawn/native/Buffer.h"
#include "dawn/native/Device.h"
#include "dawn/native/Queue.h"
#include "dawn/native/ResourceHeap.h"
#include "dawn/native/ResourceHeapAllocator.h"

namespace dawn::native {

namespace {

constexpr size_t kPowerOfTwoMaxSize = 4ull * 1024 * 1024;  // 4MB

// Assuming 60 submissions per second. We want to keep the unused allocations alive for at least 5
// seconds.
constexpr ExecutionSerial kKeepAliveDuration(60 * 5);

}  // namespace

TempGPUBufferManager::TempGPUBufferManager(DeviceBase* device, wgpu::BufferUsage bufferUsage)
    : mDevice(device), mBufferUsage(bufferUsage) {}

TempGPUBufferManager::~TempGPUBufferManager() = default;

ResultOrError<BufferBase*> TempGPUBufferManager::Allocate(uint64_t allocationSize,
                                                          ExecutionSerial useInSerial) {
    uint64_t actualSize = allocationSize;
    if (allocationSize <= kPowerOfTwoMaxSize) {
        // Try to allocate power of two buffer first. To reduce the number of different allocated
        // sizes we need to track.
        actualSize = uint64_t(1) << Log2(allocationSize);
        if (actualSize < allocationSize) {
            actualSize <<= 1;
        }
    }

    // Try to find in buffers free list.
    auto bucketIte = mBuffersFreeList.find(actualSize);
    if (bucketIte != mBuffersFreeList.end()) {
        auto buffer = bucketIte->second.TakeOneFromFirstSerial();
        if (bucketIte->second.Empty()) {
            mBuffersFreeList.erase(bucketIte);  // Remove empty bucket.
        }
        return TrackBuffer(std::move(buffer), useInSerial);
    }

    return DoAllocateBuffer(actualSize, useInSerial);
}

void TempGPUBufferManager::Deallocate(ExecutionSerial completedSerial) {
    // Transfer to free list if the buffer is already done being used.
    for (auto& buffer : mInflightAllocations.IterateUpTo(completedSerial)) {
        mBuffersFreeList[buffer->GetSize()].Enqueue(std::move(buffer), completedSerial);
    }

    mInflightAllocations.ClearUpTo(completedSerial);

    if (completedSerial < kKeepAliveDuration) {
        return;
    }

    // Release bufers that are unused for at least kKeepAliveDuration submissions.
    absl::InlinedVector<BufferFreeList::iterator, 1> emptyBuckets = {};
    for (auto ite = mBuffersFreeList.begin(); ite != mBuffersFreeList.end(); ++ite) {
        ite->second.ClearUpTo(completedSerial - kKeepAliveDuration);
        if (ite->second.Empty()) {
            emptyBuckets.emplace_back(ite);
        }
    }

    for (auto bucket : emptyBuckets) {
        mBuffersFreeList.erase(bucket);
    }
}

BufferBase* TempGPUBufferManager::TrackBuffer(Ref<BufferBase> buffer, ExecutionSerial useInSerial) {
    BufferBase* ptr = buffer.Get();
    mInflightAllocations.Enqueue(std::move(buffer), useInSerial);
    return ptr;
}

ResultOrError<BufferBase*> TempGPUBufferManager::DoAllocateBuffer(uint64_t size,
                                                                  ExecutionSerial useInSerial) {
    BufferDescriptor desc;
#if defined(DAWN_ENABLE_ASSERTS)
    std::ostringstream ss;
    ss << "Dawn_TempGPUBuffer" << mNextAllocationID++;
    desc.label = ss.str().c_str();
#else
    desc.label = "Dawn_TempGPUBuffer";
#endif
    desc.size = size;
    desc.usage = mBufferUsage;
    Ref<BufferBase> buffer;
    DAWN_TRY_ASSIGN(buffer, mDevice->CreateBuffer(&desc));

    return TrackBuffer(std::move(buffer), useInSerial);
}

}  // namespace dawn::native
