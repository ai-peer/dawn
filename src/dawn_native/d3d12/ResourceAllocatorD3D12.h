// Copyright 2018 The Dawn Authors
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

#ifndef DAWNNATIVE_D3D12_RESOURCEALLOCATORD3D12_H_
#define DAWNNATIVE_D3D12_RESOURCEALLOCATORD3D12_H_

#include "dawn_native/ResourceAllocator.cpp"  // Required for allocator definitions.

#include "common/SerialQueue.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native {
    namespace d3d12 {

        class Device;

        // Wrapper to allocate a D3D heap.
        // TODO(bryan.bernhart@intel.com): Rename to ResourceAllocator.
        class ResourceAllocator2 {
          public:
            ResourceAllocator2() = default;
            ResourceAllocator2(Device* device, uint32_t heapTypeIndex);
            ~ResourceAllocator2() = default;

            ResourceHeapBase* Allocate(size_t heapSize);
            void Deallocate(ResourceHeapBase* heap);

            void Tick(uint64_t lastCompletedSerial);

          private:
            D3D12_RESOURCE_STATES GetHeapState(D3D12_HEAP_TYPE heapType) const;
            D3D12_HEAP_PROPERTIES GetHeapProperties(D3D12_HEAP_TYPE heapType) const;

            Device* mDevice;
            uint32_t mHeapTypeIndex = -1;  // Determines the heap type used.

#if defined(_DEBUG)
            size_t mAllocationCount = 0;
#endif
            // TODO(bryan.bernhart@intel.com): Combine queue for all resource heap usages.
            SerialQueue<ComPtr<ID3D12Resource>> mReleasedResources;
        };
    }  // namespace d3d12

    // Block-based allocators.
    typedef DirectAllocator<HeapSubAllocationBlock, d3d12::ResourceAllocator2>
        DirectResourceAllocator;

    typedef BuddyPoolAllocator<HeapSubAllocationBlock,
                               d3d12::ResourceAllocator2,
                               BuddyBlockAllocator<HeapSubAllocationBlock>,
                               D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES>
        BuddyResourceAllocator;

    // Device allocator.
    typedef ConditionalAllocator<HeapSubAllocationBlock,
                                 BuddyResourceAllocator,
                                 DirectResourceAllocator>
        BufferAllocator;  // default and upload

}  // namespace dawn_native

#endif  // DAWNNATIVE_D3D12_RESOURCEALLOCATORD3D12_H_
