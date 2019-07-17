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

#ifndef DAWNNATIVE_RESOURCEHEAPALLOCATOR_H_
#define DAWNNATIVE_RESOURCEHEAPALLOCATOR_H_

#include <memory>
#include <vector>

#include "dawn_native/BuddyAllocator.h"
#include "dawn_native/ResourceMemoryAllocation.h"

namespace dawn_native {
    //
    // Resource allocator overview.
    //
    // There are two supported modes of allocation: sub-allocation or not (otherwise called direct
    // allocation). Sub-allocation sub-divides a larger memory space enabling heap reuse while
    // direct allocation does not (heaps are sized to the allocation request). In the former, the
    // sub-allocator controls the lifetime of the heap while in the latter, the client controls the
    // lifetime. In either case, the client is always responsible to call Deallocate to avoid
    // leaking memory.
    //
    // This class uses a nested allocator design. A innermost allocator allocates physical memory
    // or resource heaps using a back-end device used in combination with a "front-end" or block
    // allocator to calculate the offset within a resource heap.
    //
    // In sub-allocation, the outtermost allocator is used to control resource heap lifetimes and is
    // seperated so we can test each allocator individually.
    //
    //  [dawn_native::SubResourceMemoryAllocator]
    //    => [dawn_native::SubAllocator]
    //    => [dawn_native::Backend::ResourceHeapAllocator]
    //
    //  In direct allocation, there only exists one heap nor uses a sub-allocator by definition.
    //
    //  [dawn_native::DirectResourceMemoryAllocator]
    //    => [dawn_native::Backend::ResourceHeapAllocator]
    //
    // The resource heap abstracts the backend memory type can could handle mappings between
    // physical or virtual address spaces. If this is the case,
    // the back-end allocator becomess the outtermost allocator in the chain.

    // Sub-allocator that uses a large buddy system with one or more resource heaps.
    // Internally, manages a free-list of blocks in a binary tree.
    template <class TResourceHeapAllocator>
    class BuddyResourceMemoryAllocator {
      public:
        BuddyResourceMemoryAllocator() = default;
        // Constructor usually takes in a back-end device and heap type.
        // However, the required arguments must be more generic as the actual device is not required
        // for testing.
        template <typename... ResourceAllocatorArgs>
        BuddyResourceMemoryAllocator(size_t maxBlockSize,
                                     size_t resourceHeapSize,
                                     ResourceAllocatorArgs&&... resourceCreationArgs);

        ~BuddyResourceMemoryAllocator();

        // Required methods.
        ResourceMemoryAllocation Allocate(size_t allocationSize, int memoryFlags = 0);
        void Deallocate(ResourceMemoryAllocation allocation);

        TResourceHeapAllocator& GetResourceHeapAllocator();
        size_t GetResourceHeapSize() const;

        // For testing purposes.
        size_t GetResourceHeapCount() const;

      private:
        size_t GetResourceHeapIndex(size_t offset) const;

        size_t mResourceHeapSize = 0;  // Size (in bytes) of each resource.

        BuddyAllocator mBlockAllocator;
        TResourceHeapAllocator mResourceHeapAllocator;

        // Track the sub-allocations on the resource.
        struct TrackedSubAllocations {
            size_t refcount = 0;
            std::unique_ptr<ResourceHeapBase> mResourceHeap;
        };

        std::vector<TrackedSubAllocations> mTrackedSubAllocations;
    };

    // Allocator that uses only a single resource heap.
    template <class TResourceHeapAllocator>
    class DirectResourceMemoryAllocator {
      public:
        DirectResourceMemoryAllocator() = default;
        // Constructor usually takes in the back-end device and heap type.
        // However, the required arguments should be generic as the actual device is not required.
        template <typename... ResourceAllocatorArgs>
        DirectResourceMemoryAllocator(ResourceAllocatorArgs&&... resourceCreationArgs);
        ~DirectResourceMemoryAllocator() = default;

        // Required methods.
        ResourceMemoryAllocation Allocate(size_t allocationSize, int memoryFlags = 0);
        void Deallocate(ResourceMemoryAllocation allocation);

        TResourceHeapAllocator& GetResourceHeapAllocator();

      private:
        TResourceHeapAllocator mResourceHeapAllocator;
    };
}  // namespace dawn_native

#include "dawn_native/ResourceHeapAllocator.cpp"
#endif  // DAWNNATIVE_RESOURCEHEAPALLOCATOR_H_
