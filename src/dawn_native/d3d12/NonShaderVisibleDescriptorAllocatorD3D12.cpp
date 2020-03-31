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

        const uint32_t heapIndex = mAvailableHeaps.front();
        NonShaderVisibleBuffer& buffer = mPool[heapIndex];
        HeapBlock& freeBlock = buffer.freeList.front();

        const uint64_t startOffset = freeBlock.start;
        freeBlock.start += mBlockSize;

        // No more room, remove the free block and heap, if full.
        if (freeBlock.start == freeBlock.end) {
            buffer.freeList.pop_front();
            if (buffer.freeList.empty()) {
                mAvailableHeaps.pop_front();
            }
        }

        return NonShaderVisibleHeapAllocation{mSizeIncrement, {startOffset}, heapIndex};
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

        const D3D12_CPU_DESCRIPTOR_HANDLE heapStart =
            newBuffer.heap->GetCPUDescriptorHandleForHeapStart();

        newBuffer.freeList = {{heapStart.ptr, heapStart.ptr + (mHeapSize * mSizeIncrement)}};

        mAvailableHeaps.push_front(mPool.size());
        mPool.emplace_back(newBuffer);

        return {};
    }

    void NonShaderVisibleDescriptorAllocator::Deallocate(
        D3D12_CPU_DESCRIPTOR_HANDLE& baseDescriptor,
        uint32_t heapIndex) {
        ASSERT(baseDescriptor.ptr != 0);
        ASSERT(heapIndex >= 0);
        ASSERT(heapIndex < mPool.size());

        // Insert the deallocated block back into the free-list. Order does not matter. However,
        // having blocks be non-contigious could slow down future allocations due to poor cache
        // locality.
        // TODO(dawn:155): Consider more optimization.
        std::list<HeapBlock>& freeList = mPool[heapIndex].freeList;
        if (freeList.empty()) {
            mAvailableHeaps.emplace_back(heapIndex);
        }
        const HeapBlock freeBlock = {baseDescriptor.ptr, baseDescriptor.ptr + mBlockSize};

        freeList.emplace_back(freeBlock);

        // Invalidate the handle in case the developer accidentally uses it again.
        baseDescriptor = {0};
    }
}}  // namespace dawn_native::d3d12