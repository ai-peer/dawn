// Copyright 2020 The Dawn Authors
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

#include "common/Math.h"

#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/NonShaderVisibleDescriptorAllocatorD3D12.h"

namespace dawn_native { namespace d3d12 {

    NonShaderVisibleDescriptorAllocator::NonShaderVisibleDescriptorAllocator(
        Device* device,
        uint32_t descriptorCount,
        uint32_t heapSize,
        D3D12_DESCRIPTOR_HEAP_TYPE heapType)
        : mDevice(device),
          mSizeIncrement(device->GetD3D12Device()->GetDescriptorHandleIncrementSize(heapType)),
          mBlockSize(descriptorCount * mSizeIncrement),
          mHeapSize(RoundUp(heapSize, descriptorCount)),
          mHeapType(heapType) {
        ASSERT(heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
               heapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        ASSERT(descriptorCount <= heapSize);
    }

    ResultOrError<NonShaderVisibleHeapAllocation>
    NonShaderVisibleDescriptorAllocator::AllocateCPUDescriptors() {
        if (mAvailableHeaps.empty()) {
            DAWN_TRY(AllocateCPUHeap());
        }

        ASSERT(mAvailableHeaps.size());

        const uint32_t heapIndex = mAvailableHeaps.back();
        NonShaderVisibleBuffer& buffer = mPool[heapIndex];

        ASSERT(buffer.freeAllocationIndicies.size());

        const uint64_t allocationIndex = buffer.freeAllocationIndicies.back();

        buffer.freeAllocationIndicies.pop_back();

        if (buffer.freeAllocationIndicies.empty()) {
            mAvailableHeaps.pop_back();
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
            buffer.heap->GetCPUDescriptorHandleForHeapStart();

        return NonShaderVisibleHeapAllocation{
            mSizeIncrement, {heapStart.ptr + (allocationIndex * mBlockSize)}, heapIndex};
    }

    MaybeError NonShaderVisibleDescriptorAllocator::AllocateCPUHeap() {
        D3D12_DESCRIPTOR_HEAP_DESC heapDescriptor;
        heapDescriptor.Type = mHeapType;
        heapDescriptor.NumDescriptors = mHeapSize;
        heapDescriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heapDescriptor.NodeMask = 0;

        ComPtr<ID3D12DescriptorHeap> heap;
        DAWN_TRY(CheckOutOfMemoryHRESULT(
            mDevice->GetD3D12Device()->CreateDescriptorHeap(&heapDescriptor, IID_PPV_ARGS(&heap)),
            "ID3D12Device::CreateDescriptorHeap"));

        NonShaderVisibleBuffer newBuffer;
        newBuffer.heap = std::move(heap);

        for (uint32_t allocationIndex = 0;
             allocationIndex < ((mHeapSize * mSizeIncrement) / mBlockSize); allocationIndex++) {
            newBuffer.freeAllocationIndicies.push_back(allocationIndex);
        }

        mAvailableHeaps.push_back(mPool.size());
        mPool.emplace_back(newBuffer);

        return {};
    }

    void NonShaderVisibleDescriptorAllocator::Deallocate(
        D3D12_CPU_DESCRIPTOR_HANDLE* baseDescriptor,
        uint32_t heapIndex) {
        ASSERT(baseDescriptor->ptr != 0);
        ASSERT(heapIndex >= 0);
        ASSERT(heapIndex < mPool.size());

        // Insert the deallocated block back into the free-list. Order does not matter. However,
        // having blocks be non-contigious could slow down future allocations due to poor cache
        // locality.
        // TODO(dawn:155): Consider more optimization.
        std::vector<uint64_t>& freeAllocationIndicies = mPool[heapIndex].freeAllocationIndicies;
        if (freeAllocationIndicies.empty()) {
            mAvailableHeaps.emplace_back(heapIndex);
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
            mPool[heapIndex].heap->GetCPUDescriptorHandleForHeapStart();

        const uint64_t allocationIndex = (baseDescriptor->ptr - heapStart.ptr) / mBlockSize;

        freeAllocationIndicies.emplace_back(allocationIndex);

        // Invalidate the handle in case the developer accidentally uses it again.
        *baseDescriptor = {0};
    }
}}  // namespace dawn_native::d3d12