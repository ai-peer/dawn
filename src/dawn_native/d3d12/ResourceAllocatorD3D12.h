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

#ifndef DAWNNATIVE_D3D12_RESOURCEALLOCATORD3D12_H_
#define DAWNNATIVE_D3D12_RESOURCEALLOCATORD3D12_H_

#include "common/SerialQueue.h"
#include "dawn_native/ResourceMemoryAllocator.h"
#include "dawn_native/d3d12/ResourceHeapD3D12.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    // Wrapper to allocate a D3D12 heap.
    class ResourceHeapAllocator : public ResourceMemoryAllocator {
      public:
        ResourceHeapAllocator(Device* device, D3D12_HEAP_TYPE heapType);
        virtual ~ResourceHeapAllocator() = default;

        ResultOrError<ResourceMemoryAllocation> Allocate(uint64_t allocationSize,
                                                         uint64_t alignment,
                                                         int memoryFlags) override;
        void Deallocate(ResourceMemoryAllocation allocation) override;

        void Tick(uint64_t lastCompletedSerial) override;

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
        PlacedResourceAllocator(Device* device, D3D12_HEAP_TYPE heapType);
        virtual ~PlacedResourceAllocator() = default;

        ResultOrError<ResourceMemoryAllocation> Allocate(
            const D3D12_RESOURCE_DESC& resourceDescriptor,
            uint64_t allocationSize,
            uint64_t allocationAlignment,
            D3D12_HEAP_FLAGS heapFlags);
        void Deallocate(ResourceMemoryAllocation allocation);

        void Tick(uint64_t lastCompletedSerial);

      private:
        Device* mDevice;

        ResourceHeapAllocator mDirectAllocator;

        SerialQueue<std::unique_ptr<ResourceHeapBase>> mReleasedResources;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RESOURCEALLOCATORD3D12_H_
