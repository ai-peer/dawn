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

#include "common/PoolAllocator.h"

#include "common/Math.h"

#include <algorithm>

struct PoolAllocatorImpl::AllocationInfo {
    AllocationInfo(Index index, Index nextIndex) : index(index), nextIndex(nextIndex) {
    }

    const Index index;
    Index nextIndex;
};

struct PoolAllocatorImpl::Pool {
    Pool(AllocationInfo* head) : freeList(head), prev(nullptr), next(nullptr), blocksInUse(0) {
    }

    AllocationInfo* freeList;
    Pool* prev;
    std::unique_ptr<Pool> next;
    Index blocksInUse;
};

PoolAllocatorImpl::PoolAllocatorImpl(Index initialCount,
                                     uint32_t objectSize,
                                     uint32_t objectAlignment,
                                     size_t maxPoolSize)
    : mDataOffset(Align(sizeof(Pool), objectAlignment)),
      mAllocationInfoOffset(Align(mDataOffset + objectSize, alignof(AllocationInfo)) - mDataOffset),
      mChunkSize(
          Align(mDataOffset + mAllocationInfoOffset + sizeof(AllocationInfo), objectAlignment) -
          mDataOffset),
      mTotalCount(initialCount),
      mMaxCount(std::max(maxPoolSize, static_cast<size_t>(kInvalidIndex - 1) * mChunkSize) /
                mChunkSize),
      mPool(nullptr) {
    ASSERT(mTotalCount <= mMaxCount);

    // | Pool | pad | Object | pad | Info | pad | Object | pad | Info | pad | ....
    // | -----------|                                 dataOffset
    // |            | -------------|                  allocationInfoOffset
    // |            | ------------------------- |     chunkSize
    // | -------------------------------------------> (dataOffset + elementCount * chunkSize)
    GetNewBlock();
}

PoolAllocatorImpl::~PoolAllocatorImpl() = default;

PoolAllocatorImpl::AllocationInfo* PoolAllocatorImpl::OffsetFrom(AllocationInfo* info,
                                                                 int16_t offset) {
    return reinterpret_cast<AllocationInfo*>(reinterpret_cast<char*>(info) +
                                             static_cast<intptr_t>(mChunkSize) * offset);
}

PoolAllocatorImpl::AllocationInfo* PoolAllocatorImpl::InfoFromAllocation(void* allocation) {
    return reinterpret_cast<PoolAllocatorImpl::AllocationInfo*>(static_cast<char*>(allocation) +
                                                                mAllocationInfoOffset);
}

void* PoolAllocatorImpl::AllocationFromInfo(PoolAllocatorImpl::AllocationInfo* info) {
    return static_cast<void*>(reinterpret_cast<char*>(info) - mAllocationInfoOffset);
}

void PoolAllocatorImpl::PushFront(Pool* pool, AllocationInfo* info) {
    AllocationInfo* head = pool->freeList;
    if (head == nullptr) {
        info->nextIndex = kInvalidIndex;
    } else {
        info->nextIndex = head->index;
    }
    pool->freeList = info;
}

PoolAllocatorImpl::AllocationInfo* PoolAllocatorImpl::PopFront(Pool* pool) {
    if (pool->freeList == nullptr) {
        return nullptr;
    }
    AllocationInfo* head = pool->freeList;
    if (head->nextIndex == kInvalidIndex) {
        pool->freeList = nullptr;
    } else {
        pool->freeList = OffsetFrom(head, head->nextIndex - head->index);
    }
    return head;
}

void* PoolAllocatorImpl::Allocate() {
    Pool* pool = mPool.get();
    while (pool != nullptr) {
        if (AllocationInfo* info = PopFront(pool)) {
            pool->blocksInUse++;
            return AllocationFromInfo(info);
        }
        pool = pool->next.get();
    }

    if (mTotalCount <= mMaxCount - mTotalCount) {
        mTotalCount *= 2;
    }
    GetNewBlock();
    return AllocationFromInfo(PopFront(mPool.get()));
}

void PoolAllocatorImpl::Deallocate(void* ptr) {
    AllocationInfo* info = InfoFromAllocation(ptr);

    ASSERT(info->index >= 0);
    void* firstAllocation = AllocationFromInfo(OffsetFrom(info, -info->index));
    Pool* pool = reinterpret_cast<Pool*>(static_cast<char*>(firstAllocation) - mDataOffset);
    ASSERT(pool != nullptr);

    PushFront(pool, info);

    ASSERT(pool->blocksInUse != 0);
    pool->blocksInUse--;

    if (pool->blocksInUse == 0 && pool->prev != nullptr) {
        // Remove |pool| from the linked list.
        // First, move it out so the parent pool doesn't own it.
        ASSERT(pool->prev->next.get() == pool);
        std::unique_ptr<Pool> toDelete = std::move(pool->prev->next);

        // Now, set the child pool as the parent's new child.
        pool->prev->next = std::move(pool->next);
    }
}

void PoolAllocatorImpl::GetNewBlock() {
    size_t allocationSize = static_cast<size_t>(mDataOffset) + mTotalCount * mChunkSize;

    char* ptr = static_cast<char*>(malloc(allocationSize));
    char* dataStart = ptr + mDataOffset;

    AllocationInfo* info = InfoFromAllocation(dataStart);
    for (uint32_t i = 0; i < mTotalCount; ++i) {
        new (OffsetFrom(info, i)) AllocationInfo(i, i + 1);
    }

    AllocationInfo* lastInfo = OffsetFrom(info, mTotalCount - 1);
    lastInfo->nextIndex = kInvalidIndex;

    Pool* pool = new (ptr) Pool(info);
    if (mPool) {
        mPool->prev = pool;
        pool->next = std::move(mPool);
    }
    mPool.reset(pool);
}
