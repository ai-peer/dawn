// Copyright 2019 The Dawn Authors
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

#include "dawn_native/d3d12/BuddyPlacedResourceAllocatorD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/HeapAllocatorD3D12.h"
#include "dawn_native/d3d12/HeapD3D12.h"

namespace dawn_native { namespace d3d12 {

    BuddyPlacedResourceAllocator::BuddyPlacedResourceAllocator(uint64_t maxResourceSize,
                                                               uint64_t heapSize,
                                                               Device* device,
                                                               D3D12_HEAP_TYPE heapType)
        : mDevice(device),
          mBuddyMemoryAllocator(maxResourceSize,
                                heapSize,
                                std::make_unique<HeapAllocator>(device, heapType)) {
    }

    ResultOrError<ResourceHeapAllocation> BuddyPlacedResourceAllocator::Allocate(
        const D3D12_RESOURCE_DESC& resourceDescriptor,
        uint64_t allocationSize,
        uint64_t allocationAlignment,
        D3D12_RESOURCE_STATES initialUsage,
        D3D12_HEAP_FLAGS heapFlags) {
        ResourceMemoryAllocation allocation;
        DAWN_TRY_ASSIGN(allocation, mBuddyMemoryAllocator.Allocate(allocationSize,
                                                                   allocationAlignment, heapFlags));

        ComPtr<ID3D12Heap> heap = static_cast<Heap*>(allocation.GetResourceHeap())->GetD3D12Heap();

        // heapFlags must be compatible or match the heap flags used by the heap
        // allocation. It is the responsibly of the caller to ensure these flags are correct
        // or CreatePlacedResource will fail.
        ASSERT(heap->GetDesc().Flags == heapFlags);

        ComPtr<ID3D12Resource> placedResource;
        if (FAILED(mDevice->GetD3D12Device()->CreatePlacedResource(
                heap.Get(), allocation.GetOffset(), &resourceDescriptor, initialUsage, nullptr,
                IID_PPV_ARGS(&placedResource)))) {
            return DAWN_OUT_OF_MEMORY_ERROR("Unable to allocate resource");
        }

        return ResourceHeapAllocation{allocation.GetInfo(), allocation.GetOffset(),
                                      std::move(placedResource)};
    }

    void BuddyPlacedResourceAllocator::Deallocate(ResourceHeapAllocation& allocation) {
        mDevice->ReferenceUntilUnused(allocation.GetD3D12Resource());
        mBuddyMemoryAllocator.Deallocate(allocation);
    }
}}  // namespace dawn_native::d3d12
