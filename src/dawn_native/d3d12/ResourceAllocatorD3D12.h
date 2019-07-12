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

#include "dawn_native/Allocator.cpp"  // Required for allocator definitions.

#include "common/SerialQueue.h"
#include "dawn_native/d3d12/ResourceHeapD3D12.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    // Wrapper to allocate a D3D12 heap.
    class ResourceHeapAllocator {
      public:
        ResourceHeapAllocator() = default;
        ResourceHeapAllocator(Device* device, D3D12_HEAP_TYPE heapType);
        ~ResourceHeapAllocator() = default;

        ResourceHeapBase* CreateHeap(size_t heapSize, int heapFlags);
        void FreeHeap(ResourceHeapBase* heap);

        void Tick(uint64_t lastCompletedSerial);

      private:
        Device* mDevice;
        D3D12_HEAP_TYPE mHeapType;

        SerialQueue<std::unique_ptr<ResourceHeapBase>> mReleasedHeaps;
    };

    // Wrapper to allocate a D3D12 placed resource.
    // Placed resources require a D3D12 heap to exist before being created.
    // Creates a block within an allocator's address space which corresponds to a physical heap
    // address space. Then a placed resource is created using the offset of the block into the
    // physical heap address space.
    class PlacedResourceAllocator {
      public:
        PlacedResourceAllocator() = default;
        PlacedResourceAllocator(Device* device, D3D12_HEAP_TYPE heapType);

        PlacedResourceAllocator(size_t maxBlockSize,
                                size_t resourceHeapSize,
                                Device* device,
                                D3D12_HEAP_TYPE heapType);

        ~PlacedResourceAllocator() = default;

        ResourceMemoryAllocation Allocate(D3D12_RESOURCE_DESC resourceDescriptor,
                                          size_t allocationSize,
                                          D3D12_HEAP_FLAGS heapFlags);
        void Deallocate(ResourceMemoryAllocation allocation);

        void Tick(uint64_t lastCompletedSerial);

      private:
        Device* mDevice;
        bool mIsDirect;

        BuddyResourceMemoryAllocator<ResourceHeapAllocator> mSubAllocator;
        DirectResourceMemoryAllocator<ResourceHeapAllocator> mDirectAllocator;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RESOURCEALLOCATORD3D12_H_
