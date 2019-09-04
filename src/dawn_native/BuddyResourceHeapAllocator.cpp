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
#include "dawn_native/BuddyResourceHeapAllocator.h"
#include "dawn_native/ResourceHeap.h"

#include "common/Math.h"

namespace dawn_native {

    BuddyResourceHeapAllocator::BuddyResourceHeapAllocator(uint64_t maxBlockSize,
                                                           uint64_t heapSize,
                                                           ResourceMemoryAllocator* client)
        : mHeapSize(heapSize), mBuddyBlockAllocator(maxBlockSize), mClient(client) {
        ASSERT(heapSize <= maxBlockSize);
        ASSERT(IsPowerOfTwo(mHeapSize));
        ASSERT(maxBlockSize % mHeapSize == 0);
    }

    uint64_t BuddyResourceHeapAllocator::GetHeapIndex(uint64_t offset) const {
        ASSERT(offset != kInvalidOffset);
        return offset / mHeapSize;
    }

    ResultOrError<ResourceMemoryAllocation> BuddyResourceHeapAllocator::Allocate(
        uint64_t allocationSize,
        uint64_t alignment,
        int memoryFlags) {
        ResourceMemoryAllocation invalidAllocation = ResourceMemoryAllocation{};

        // Allocation cannot exceed the heap size.
        if (allocationSize == 0 || allocationSize > mHeapSize) {
            return invalidAllocation;
        }

        // Attempt to sub-allocate a block of the requested size.
        const uint64_t offset = mBuddyBlockAllocator.Allocate(allocationSize, alignment);
        if (offset == kInvalidOffset) {
            return invalidAllocation;
        }

        // Heap at index does not exist: create the heaps in the same level up to the offset.
        const uint64_t heapIndex = GetHeapIndex(offset);
        if (heapIndex >= mTrackedSubAllocations.size()) {
            const uint64_t numRequired = (heapIndex + 1) - mTrackedSubAllocations.size();
            for (uint64_t i = 0; i < numRequired; i++) {
                // Transfer ownership to this allocator
                ResourceMemoryAllocation newHeap;
                DAWN_TRY_ASSIGN(newHeap, mClient->Allocate(mHeapSize, memoryFlags));
                mTrackedSubAllocations.push_back({/*refcount*/ 0, std::move(newHeap)});
            }
        }
        // Heap at index previously existed but was de-allocated.
        else if (mTrackedSubAllocations[heapIndex].refcount == 0) {
            // Transfer ownership to this allocator
            ResourceMemoryAllocation newHeap;
            DAWN_TRY_ASSIGN(newHeap, mClient->Allocate(mHeapSize, memoryFlags));
            mTrackedSubAllocations[heapIndex] = {/*refcount*/ 0, std::move(newHeap)};
        }

        mTrackedSubAllocations[heapIndex].refcount++;

        return ResourceMemoryAllocation(
            offset, mTrackedSubAllocations[heapIndex].mHeapAllocation.GetResourceHeap(),
            mTrackedSubAllocations[heapIndex].mHeapAllocation.GetAllocationMethod());
    }  // namespace dawn_native

    void BuddyResourceHeapAllocator::Deallocate(ResourceMemoryAllocation& allocation) {
        ASSERT(allocation.GetAllocationMethod() == AllocationMethod::kSubAllocated);
        const uint64_t heapIndex = GetHeapIndex(allocation.GetOffset());

        ASSERT(mTrackedSubAllocations[heapIndex].refcount > 0);

        mTrackedSubAllocations[heapIndex].refcount--;

        if (mTrackedSubAllocations[heapIndex].refcount == 0) {
            mClient->Deallocate(mTrackedSubAllocations[heapIndex].mHeapAllocation);
            mTrackedSubAllocations[heapIndex].mHeapAllocation.Invalidate();
        }

        mBuddyBlockAllocator.Deallocate(allocation.GetOffset());
    }

    uint64_t BuddyResourceHeapAllocator::GetHeapSize() const {
        return mHeapSize;
    }

    uint64_t BuddyResourceHeapAllocator::ComputeTotalNumOfHeapsForTesting() const {
        uint64_t count = 0;
        for (const TrackedSubAllocations& allocation : mTrackedSubAllocations) {
            if (allocation.refcount > 0) {
                count++;
            }
        }
        return count;
    }
}  // namespace dawn_native