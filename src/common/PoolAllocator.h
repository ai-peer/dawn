// Copyright 2020 The Dawn Authors
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

#ifndef COMMON_POOLALLOCATOR_H_
#define COMMON_POOLALLOCATOR_H_

#include "common/PlacementAllocated.h"

#include <cstdint>
#include <memory>

class PoolAllocatorImpl {
  public:
    struct AllocationInfo;
    struct Pool;

  protected:
    // Allocations host their current index and the index of the next free block.
    using Index = uint16_t;

    PoolAllocatorImpl(Index initialCount,
                      uint32_t objectSize,
                      uint32_t objectAlignment,
                      size_t maxPoolSize);
    ~PoolAllocatorImpl();

    // Allocate a new block of memory.
    void* Allocate();

    // Deallocate a block of memory.
    void Deallocate(void* ptr);

  private:
    // The maximum value is reserved to indicate there is no value.
    static constexpr Index kInvalidIndex = std::numeric_limits<Index>::max();

    // Get the AllocationInfo |offset| slots away.
    AllocationInfo* OffsetFrom(AllocationInfo* info, std::make_signed_t<Index> offset);

    // Compute the pointer to the AllocationInfo from an allocation.
    AllocationInfo* InfoFromAllocation(void* allocation);

    // Compute the pointer to the allocation from an AllocaitonInfo
    void* AllocationFromInfo(AllocationInfo* info);

    // The Pool stores a linked-list of free allocations.
    // PushFront/PopFront adds/removes an allocation from the free list.
    void PushFront(Pool* pool, AllocationInfo* info);
    AllocationInfo* PopFront(Pool* pool);

    // Replace the current pool with a new one, and chain the old one off of it.
    // Both pools may still be used for for allocation/deallocation, but older pools
    // will be a little slower to get allocations from.
    void GetNewBlock();

    // A Pool is Pool metadata (freeList and next pointer), followed by
    // the aligned memory to allocate out of. |mDataOffset| is the offset to the
    // start of the aligned memory region.
    const uint32_t mDataOffset;

    // The AllocationInfo is stored after the Allocation itself. This is the offset to it.
    const uint32_t mAllocationInfoOffset;

    // Because alignment of allocations may introduce padding, |mChunkSize| is the
    // distance between aligned blocks of (Allocation + AllocationInfo)
    const uint32_t mChunkSize;

    Index mTotalCount;            // The total number of blocks in the current pool.
    const Index mMaxCount;        // The maximum number of blocks that can be in any pool.
    std::unique_ptr<Pool> mPool;  // The current pool.
};

template <typename T>
class PoolAllocator : public PoolAllocatorImpl {
  public:
    PoolAllocator(Index initialCount, size_t maxPoolSize = std::numeric_limits<size_t>::max())
        : PoolAllocatorImpl(initialCount, sizeof(T), alignof(T), maxPoolSize) {
    }

    template <typename... Args>
    T* Allocate(Args&&... args) {
        void* ptr = PoolAllocatorImpl::Allocate();
        return new (ptr) T(std::forward<Args>(args)...);
    }

    void Deallocate(T* object) {
        PoolAllocatorImpl::Deallocate(object);
    }
};

#endif  // COMMON_POOLALLOCATOR_H_
