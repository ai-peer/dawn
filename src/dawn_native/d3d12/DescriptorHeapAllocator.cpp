// Copyright 2017 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn_native/d3d12/DescriptorHeapAllocator.h"

#include "common/Assert.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    DescriptorHeapHandle::DescriptorHeapHandle()
        : mDescriptorHeap(nullptr), mSizeIncrement(0), mOffset(0) {
    }

    DescriptorHeapHandle::DescriptorHeapHandle(ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                               uint32_t sizeIncrement,
                                               uint64_t offset)
        : mDescriptorHeap(descriptorHeap), mSizeIncrement(sizeIncrement), mOffset(offset) {
    }

    ID3D12DescriptorHeap* DescriptorHeapHandle::Get() const {
        return mDescriptorHeap.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapHandle::GetCPUHandle(uint32_t index) const {
        ASSERT(mDescriptorHeap);
        auto handle = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += mSizeIncrement * (index + mOffset);
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapHandle::GetGPUHandle(uint32_t index) const {
        ASSERT(mDescriptorHeap);
        auto handle = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += mSizeIncrement * (index + mOffset);
        return handle;
    }

    DescriptorHeapAllocator::DescriptorHeapAllocator(Device* device)
        : mDevice(device),
          mSizeIncrements{
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
              device->GetD3D12Device()->GetDescriptorHandleIncrementSize(
                  D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
          } {
    }

    MaybeError DescriptorHeapAllocator::Initialize() {
        return EnsureSpaceForFullPipelineLayout();
    }

    ResultOrError<DescriptorHeapHandle> DescriptorHeapAllocator::Allocate(
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t count,
        uint32_t allocationSize,
        D3D12_DESCRIPTOR_HEAP_FLAGS flags,
        bool forceAllocation) {
        DescriptorHeapInfo* heapInfo = &mHeapInfos[type];
        if (count == 0) {
            return DescriptorHeapHandle{heapInfo->heap, mSizeIncrements[type], 0};
        }
        const Serial pendingSerial = mDevice->GetPendingCommandSerial();
        uint64_t startOffset = heapInfo->allocator.Allocate(count, pendingSerial);
        if (startOffset != RingBufferAllocator::kInvalidOffset) {
            return DescriptorHeapHandle{heapInfo->heap, mSizeIncrements[type], startOffset};
        }

        // Allow the client to re-request a larger allocation size should the allocator exceed
        // capacity. Ensures a new heap isn't only created with a partial allocation (ie. dirty
        // bindgroups) where non-dirty groups remain on the old heap. The same bound heap must
        // contain all bindgroups.
        if (!forceAllocation) {
            return DescriptorHeapHandle{};
        }

        // If the heap has no more space, replace the heap with a new one of the specified size
        DAWN_TRY(ReallocateHeap(type, allocationSize, flags));

        startOffset = heapInfo->allocator.Allocate(count, pendingSerial);
        return DescriptorHeapHandle(heapInfo->heap, mSizeIncrements[type], startOffset);
    }

    MaybeError DescriptorHeapAllocator::ReallocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                       uint32_t allocationSize,
                                                       D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
        // Deallocate the previous heap when no longer used.
        ComPtr<ID3D12DescriptorHeap> previousHeap = mHeapInfos[type].heap;
        if (previousHeap != nullptr) {
            mDevice->ReferenceUntilUnused(previousHeap);
        }

        // Create the new descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC heapDescriptor;
        heapDescriptor.Type = type;
        heapDescriptor.NumDescriptors = allocationSize;
        heapDescriptor.Flags = flags;
        heapDescriptor.NodeMask = 0;
        ComPtr<ID3D12DescriptorHeap> heap;
        DAWN_TRY(CheckHRESULT(
            mDevice->GetD3D12Device()->CreateDescriptorHeap(&heapDescriptor, IID_PPV_ARGS(&heap)),
            "ID3D12Device::CreateDescriptorHeap"));

        // Store it internally as the current heap for this type.
        mHeapInfos[type].heap = heap;
        mHeapInfos[type].allocator = RingBufferAllocator(allocationSize);
        mHeapInfos[type].heapSerial++;

        return {};
    }

    MaybeError DescriptorHeapAllocator::EnsureSpaceForFullPipelineLayout() {
        // Just reallocate both heaps for now, but eventually do something better where we check if
        // we have enough space in the RingBufferAllocators
        DAWN_TRY(ReallocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1,
                                D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE));
        DAWN_TRY(ReallocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                                D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE,
                                D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE));
        return {};
    }

    ResultOrError<DescriptorHeapHandle> DescriptorHeapAllocator::AllocateCPUHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t count) {
        return Allocate(type, count, count, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, true);
    }

    ResultOrError<DescriptorHeapHandle> DescriptorHeapAllocator::AllocateGPUHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t count) {
        ASSERT(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
               type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        unsigned int heapSize = (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
                                     ? D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1
                                     : D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE);
        return Allocate(type, count, heapSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, false);
    }

    void DescriptorHeapAllocator::Deallocate(uint64_t lastCompletedSerial) {
        for (uint32_t i = 0; i < mHeapInfos.size(); i++) {
            if (mHeapInfos[i].heap != nullptr) {
                mHeapInfos[i].allocator.Deallocate(lastCompletedSerial);
            }
        }
    }

    ID3D12DescriptorHeap* DescriptorHeapAllocator::GetDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE type) const {
        return mHeapInfos[type].heap.Get();
    }

    Serial DescriptorHeapAllocator::GetGPUDescriptorHeapSerial(
        D3D12_DESCRIPTOR_HEAP_TYPE type) const {
        ASSERT(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
               type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        return mHeapInfos[type].heapSerial;
    }

}}  // namespace dawn_native::d3d12
