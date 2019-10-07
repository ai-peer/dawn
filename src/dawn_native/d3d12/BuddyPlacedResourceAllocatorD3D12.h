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

#ifndef DAWNNATIVE_D3D12_BUDDYPLACEDRESOURCEALLOCATORD3D12_H_
#define DAWNNATIVE_D3D12_BUDDYPLACEDRESOURCEALLOCATORD3D12_H_

#include "dawn_native/BuddyMemoryAllocator.h"
#include "dawn_native/d3d12/ResourceHeapAllocationD3D12.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    // Wrapper to allocates a D3D12 placed resource with the buddy allocator.
    class BuddyPlacedResourceAllocator {
      public:
        BuddyPlacedResourceAllocator(uint64_t maxResourceSize,
                                     uint64_t heapSize,
                                     Device* device,
                                     D3D12_HEAP_TYPE heapType);

        ~BuddyPlacedResourceAllocator() = default;

        ResultOrError<ResourceHeapAllocation> Allocate(
            const D3D12_RESOURCE_DESC& resourceDescriptor,
            uint64_t allocationSize,
            uint64_t allocationAlignment,
            D3D12_RESOURCE_STATES initialUsage,
            D3D12_HEAP_FLAGS heapFlags);

        void Deallocate(ResourceHeapAllocation& allocation);

      private:
        Device* mDevice;
        BuddyMemoryAllocator mBuddyMemoryAllocator;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_BUDDYPLACEDRESOURCEALLOCATORD3D12_H_