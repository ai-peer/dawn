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

#ifndef DAWNNATIVE_VULKAN_MEMORYALLOCATOR2VK_H_
#define DAWNNATIVE_VULKAN_MEMORYALLOCATOR2VK_H_

#include "dawn_native/ResourceAllocator.cpp"  // Required for allocator definitions.

#include "common/SerialQueue.h"
#include "common/vulkan_platform.h"

namespace dawn_native {
    namespace vulkan {

        class Device;

        class MemoryAllocator2 {
          public:
            MemoryAllocator2() = default;
            MemoryAllocator2(Device* device, uint32_t heapTypeIndex);
            ~MemoryAllocator2() = default;

            ResourceHeapBase* Allocate(size_t heapSize);
            void Deallocate(ResourceHeapBase* heap);

            void Tick(uint64_t lastCompletedSerial);

            // TODO(bryan.bernhart@intel.com): Figure out these values.
            static const size_t MIN_RESOURCE_SIZE = 64 * 1024LL;

          private:
            Device* mDevice;
            uint32_t mHeapTypeIndex = -1;  // Determines the heap type used.

#if defined(_DEBUG)
            size_t mAllocationCount = 0;
#endif
        };
    }  // namespace vulkan

    // Block-based allocators.
    typedef DirectAllocator<HeapSubAllocationBlock, vulkan::MemoryAllocator2>
        DirectResourceAllocator;

    typedef BuddyPoolAllocator<HeapSubAllocationBlock,
                               vulkan::MemoryAllocator2,
                               BuddyBlockAllocator<HeapSubAllocationBlock>,
                               vulkan::MemoryAllocator2::MIN_RESOURCE_SIZE>
        BuddyResourceAllocator;

    // Device allocator.
    typedef ConditionalAllocator<HeapSubAllocationBlock,
                                 BuddyResourceAllocator,
                                 DirectResourceAllocator>
        BufferAllocator;

}  // namespace dawn_native

#endif  // DAWNNATIVE_VULKAN_MEMORYALLOCATOR2VK_H_