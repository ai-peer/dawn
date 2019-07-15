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
    // Allocator that uses one or more resource heaps.
    // Internally, it uses a large buddy system with a specified resource heap size.
    // Allocate: the resource heap will be created at the allocated block offset.
    // Deallocate: Last sub-allocation will free the resource heap.
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

    // Allocator that only uses a single resource heap.
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
