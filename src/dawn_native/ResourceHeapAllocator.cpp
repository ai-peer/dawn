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

#include "dawn_native/ResourceHeapAllocator.h"
#include "dawn_native/ResourceHeap.h"

#include "common/Math.h"

namespace dawn_native {

    // DirectResourceMemoryAllocator

    template <class TResourceHeapAllocator>
    template <typename... ResourceAllocatorArgs>
    DirectResourceMemoryAllocator<TResourceHeapAllocator>::DirectResourceMemoryAllocator(
        ResourceAllocatorArgs&&... resourceCreationArgs)
        : mResourceHeapAllocator(std::forward<ResourceAllocatorArgs>(resourceCreationArgs)...) {
    }

    template <class TResourceHeapAllocator>
    ResourceMemoryAllocation DirectResourceMemoryAllocator<TResourceHeapAllocator>::Allocate(
        size_t allocationSize,
        int memoryFlags) {
        std::unique_ptr<ResourceHeapBase> newResourceHeap(
            mResourceHeapAllocator.CreateHeap(allocationSize, memoryFlags));

        if (newResourceHeap == nullptr) {
            return ResourceMemoryAllocation{INVALID_OFFSET};
        };

        return ResourceMemoryAllocation{/*offset*/ 0, newResourceHeap.release(), /*IsDirect*/ true};
    }

    template <class TResourceHeapAllocator>
    void DirectResourceMemoryAllocator<TResourceHeapAllocator>::Deallocate(
        ResourceMemoryAllocation allocation) {
        mResourceHeapAllocator.FreeHeap(allocation.GetResourceHeap());
    }

    template <class TResourceHeapAllocator>
    TResourceHeapAllocator&
    DirectResourceMemoryAllocator<TResourceHeapAllocator>::GetResourceHeapAllocator() {
        return mResourceHeapAllocator;
    }

    // BuddyResourceMemoryAllocator

    template <class TResourceHeapAllocator>
    template <typename... ResourceAllocatorArgs>
    BuddyResourceMemoryAllocator<TResourceHeapAllocator>::BuddyResourceMemoryAllocator(
        size_t maxBlockSize,
        size_t mResourceHeapSize,
        ResourceAllocatorArgs&&... resourceCreationArgs)
        : mResourceHeapSize(mResourceHeapSize),
          mBlockAllocator(maxBlockSize),
          mResourceHeapAllocator(std::forward<ResourceAllocatorArgs>(resourceCreationArgs)...) {
        ASSERT(IsPowerOfTwo(mResourceHeapSize));
        ASSERT(maxBlockSize % mResourceHeapSize == 0);
    }

    template <class TResourceHeapAllocator>
    BuddyResourceMemoryAllocator<TResourceHeapAllocator>::~BuddyResourceMemoryAllocator() {
        // Verify resource heaps have been released
        for (auto& resourceHeap : mTrackedSubAllocations) {
            ASSERT(resourceHeap.mResourceHeap == nullptr);
        }
    }

    template <class TResourceHeapAllocator>
    size_t BuddyResourceMemoryAllocator<TResourceHeapAllocator>::GetResourceHeapIndex(
        size_t offset) const {
        return offset / mResourceHeapSize;
    }

    template <class TResourceHeapAllocator>
    ResourceMemoryAllocation BuddyResourceMemoryAllocator<TResourceHeapAllocator>::Allocate(
        size_t allocationSize,
        int memoryFlags) {
        const ResourceMemoryAllocation invalid = ResourceMemoryAllocation{INVALID_OFFSET};

        // Allocation cannot exceed the resource heap size.
        if (allocationSize > mResourceHeapSize) {
            return invalid;
        }

        // Attempt to sub-allocate a block of the requested size.
        const size_t offset = mBlockAllocator.Allocate(allocationSize);
        if (offset == INVALID_OFFSET) {
            return invalid;
        }

        // Ensure the allocated block can be mapped back to the resource.
        // Additional resources are created if not.
        const size_t resourceHeapIndex = GetResourceHeapIndex(offset);

        // Re-size tracked resource allocation.
        if (resourceHeapIndex >= mTrackedSubAllocations.size()) {
            const size_t numOfResourcesRequired =
                (resourceHeapIndex + 1) - mTrackedSubAllocations.size();
            for (uint32_t i = 0; i < numOfResourcesRequired; i++) {
                // Transfer ownership of the new resource heap to this allocator.
                std::unique_ptr<ResourceHeapBase> newResourceHeap(
                    mResourceHeapAllocator.CreateHeap(mResourceHeapSize, memoryFlags));
                ASSERT(newResourceHeap != nullptr);
                mTrackedSubAllocations.push_back({/*refcount*/ 0, std::move(newResourceHeap)});
            }
        }
        // Allocate resource that was previously de-allocated.
        else if (mTrackedSubAllocations[resourceHeapIndex].refcount == 0) {
            // Transfer ownership of the new resource heap to this allocator.
            std::unique_ptr<ResourceHeapBase> newResourceHeap(
                mResourceHeapAllocator.CreateHeap(mResourceHeapSize, memoryFlags));

            if (newResourceHeap == nullptr) {
                return ResourceMemoryAllocation{INVALID_OFFSET};
            };

            mTrackedSubAllocations[resourceHeapIndex] = {/*refcount*/ 0,
                                                         std::move(newResourceHeap)};
        }

        mTrackedSubAllocations[resourceHeapIndex].refcount++;

        return ResourceMemoryAllocation(
            offset, mTrackedSubAllocations[resourceHeapIndex].mResourceHeap.get());
    }  // namespace dawn_native

    template <class TResourceHeapAllocator>
    void BuddyResourceMemoryAllocator<TResourceHeapAllocator>::Deallocate(
        ResourceMemoryAllocation allocation) {
        const size_t resourceHeapIndex = GetResourceHeapIndex(allocation.GetOffset());

        ASSERT(mTrackedSubAllocations[resourceHeapIndex].refcount > 0);

        mTrackedSubAllocations[resourceHeapIndex].refcount--;

        if (mTrackedSubAllocations[resourceHeapIndex].refcount == 0) {
            mResourceHeapAllocator.FreeHeap(
                mTrackedSubAllocations[resourceHeapIndex].mResourceHeap.release());
        }

        mBlockAllocator.Deallocate(allocation.GetOffset());
    }

    template <class TResourceHeapAllocator>
    TResourceHeapAllocator&
    BuddyResourceMemoryAllocator<TResourceHeapAllocator>::GetResourceHeapAllocator() {
        return mResourceHeapAllocator;
    }

    template <class TResourceHeapAllocator>
    size_t BuddyResourceMemoryAllocator<TResourceHeapAllocator>::GetResourceHeapSize() const {
        return mResourceHeapSize;
    }

    template <class TResourceHeapAllocator>
    size_t BuddyResourceMemoryAllocator<TResourceHeapAllocator>::GetResourceHeapCount() const {
        size_t allocatedCount = 0;
        for (const auto& trackedResource : mTrackedSubAllocations) {
            if (trackedResource.mResourceHeap)
                allocatedCount++;
        }
        return allocatedCount;
    }
}  // namespace dawn_native