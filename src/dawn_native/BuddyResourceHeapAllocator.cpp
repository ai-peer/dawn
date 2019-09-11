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
#include "dawn_native/Resource.h"

#include "common/Math.h"

namespace dawn_native {

    BuddyResourceHeapAllocator::BuddyResourceHeapAllocator(uint64_t maxBlockSize,
                                                           uint64_t heapSize,
                                                           ResourceMemoryAllocator* client)
        : mHeapSize(heapSize), mBuddyBlockAllocator(maxBlockSize), mClient(client) {
        ASSERT(heapSize <= maxBlockSize);
        ASSERT(IsPowerOfTwo(mHeapSize));
        ASSERT(maxBlockSize % mHeapSize == 0);

        mTrackedSubAllocations.resize(maxBlockSize / mHeapSize);
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
        const uint64_t blockOffset = mBuddyBlockAllocator.Allocate(allocationSize, alignment);
        if (blockOffset == kInvalidOffset) {
            return invalidAllocation;
        }

        const uint64_t heapIndex = GetHeapIndex(blockOffset);
        if (mTrackedSubAllocations[heapIndex].refcount == 0) {
            // Transfer ownership to this allocator
            std::unique_ptr<HeapBase> heapAllocation;
            DAWN_TRY_ASSIGN(heapAllocation, mClient->Allocate(mHeapSize, memoryFlags));
            mTrackedSubAllocations[heapIndex] = {/*refcount*/ 0, std::move(heapAllocation)};
        }

        mTrackedSubAllocations[heapIndex].refcount++;

        AllocationInfo info;
        info.mBlockOffset = blockOffset;
        info.mMethod = AllocationMethod::kSubAllocated;

        // Allocation offset is always local to the heap.
        const uint64_t heapOffset = blockOffset % mHeapSize;

        return ResourceMemoryAllocation{info, heapOffset, nullptr, nullptr};
    }  // namespace dawn_native

    void BuddyResourceHeapAllocator::Deallocate(ResourceMemoryAllocation& allocation) {
        const AllocationInfo info = allocation.GetInfo();

        ASSERT(info.mMethod == AllocationMethod::kSubAllocated);

        const uint64_t heapIndex = GetHeapIndex(info.mBlockOffset);

        ASSERT(mTrackedSubAllocations[heapIndex].refcount > 0);

        mTrackedSubAllocations[heapIndex].refcount--;

        if (mTrackedSubAllocations[heapIndex].refcount == 0) {
            mClient->Deallocate(std::move(mTrackedSubAllocations[heapIndex].mHeapAllocation));
        }

        mBuddyBlockAllocator.Deallocate(info.mBlockOffset);
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