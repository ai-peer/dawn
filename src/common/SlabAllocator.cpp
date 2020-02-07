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

#include "common/SlabAllocator.h"

#include "common/Assert.h"
#include "common/Math.h"

#include <cstdlib>
#include <limits>
#include <new>

// AllocationInfo

SlabAllocatorImpl::AllocationInfo::AllocationInfo(Index index, Index nextIndex)
    : index(index), nextIndex(nextIndex) {
}

// Slab

SlabAllocatorImpl::Slab::Slab(std::unique_ptr<char[]> allocation, AllocationInfo* head)
    : allocation(std::move(allocation)),
      freeList(head),
      prev(nullptr),
      next(nullptr),
      blocksInUse(0) {
}

SlabAllocatorImpl::Slab::~Slab() {
    if (next != nullptr) {
        delete next;
    }
}

// SlabAllocatorImpl

SlabAllocatorImpl::Index SlabAllocatorImpl::kInvalidIndex =
    std::numeric_limits<SlabAllocatorImpl::Index>::max();

SlabAllocatorImpl::SlabAllocatorImpl(Index count,
                                     uint32_t allocationAlignment,
                                     uint32_t dataOffset,
                                     uint32_t chunkSize,
                                     uint32_t allocationInfoOffset)
    : mAllocationAlignment(allocationAlignment),
      mDataOffset(dataOffset),
      mChunkSize(chunkSize),
      mAllocationInfoOffset(allocationInfoOffset),
      mCount(count) {
    GetNewBlock();
}

SlabAllocatorImpl::~SlabAllocatorImpl() = default;

SlabAllocatorImpl::AllocationInfo* SlabAllocatorImpl::OffsetFrom(AllocationInfo* info,
                                                                 std::make_signed_t<Index> offset) {
    return reinterpret_cast<AllocationInfo*>(reinterpret_cast<char*>(info) +
                                             static_cast<intptr_t>(mChunkSize) * offset);
}

SlabAllocatorImpl::AllocationInfo* SlabAllocatorImpl::InfoFromAllocation(void* allocation) {
    return reinterpret_cast<SlabAllocatorImpl::AllocationInfo*>(static_cast<char*>(allocation) +
                                                                mAllocationInfoOffset);
}

void* SlabAllocatorImpl::AllocationFromInfo(SlabAllocatorImpl::AllocationInfo* info) {
    return static_cast<void*>(reinterpret_cast<char*>(info) - mAllocationInfoOffset);
}

void SlabAllocatorImpl::PushFront(Slab* slab, AllocationInfo* info) {
    AllocationInfo* head = slab->freeList;
    if (head == nullptr) {
        info->nextIndex = kInvalidIndex;
    } else {
        info->nextIndex = head->index;
    }
    slab->freeList = info;

    ASSERT(slab->blocksInUse != 0);
    slab->blocksInUse--;
}

SlabAllocatorImpl::AllocationInfo* SlabAllocatorImpl::PopFront(Slab* slab) {
    ASSERT(slab->freeList != nullptr);

    AllocationInfo* head = slab->freeList;
    if (head->nextIndex == kInvalidIndex) {
        slab->freeList = nullptr;
    } else {
        slab->freeList = OffsetFrom(head, head->nextIndex - head->index);
    }
    slab->blocksInUse++;
    return head;
}

void SlabAllocatorImpl::SentinelSlab::Prepend(SlabAllocatorImpl::Slab* slab) {
    if (this->next != nullptr) {
        this->next->prev = slab;
    }
    slab->prev = this;
    slab->next = this->next;
    this->next = slab;
}

SlabAllocatorImpl::Slab* SlabAllocatorImpl::Slab::Splice() {
    SlabAllocatorImpl::Slab* parent = this->prev;
    SlabAllocatorImpl::Slab* child = this->next;

    this->prev = nullptr;
    this->next = nullptr;

    ASSERT(parent != nullptr);

    // Set the child's prev pointer.
    if (child != nullptr) {
        child->prev = parent;
    }

    // Now, set the child slab as the parent's new child.
    parent->next = child;

    return this;
}

void* SlabAllocatorImpl::Allocate() {
    if (mAvailableSlabs.next == nullptr) {
        GetNewBlock();
    }

    Slab* slab = mAvailableSlabs.next;
    AllocationInfo* info = PopFront(slab);
    ASSERT(info != nullptr);

    // Move full slabs to a separate list, so allocate can always return quickly.
    if (slab->blocksInUse == mCount) {
        mFullSlabs.Prepend(slab->Splice());
    }

    return AllocationFromInfo(info);
}

void SlabAllocatorImpl::Deallocate(void* ptr) {
    AllocationInfo* info = InfoFromAllocation(ptr);

    ASSERT(info->index >= 0);
    void* firstAllocation = AllocationFromInfo(OffsetFrom(info, -info->index));
    Slab* slab = reinterpret_cast<Slab*>(static_cast<char*>(firstAllocation) - mDataOffset);
    ASSERT(slab != nullptr);

    bool slabWasFull = slab->blocksInUse == mCount;

    ASSERT(slab->blocksInUse != 0);
    PushFront(slab, info);

    if (slabWasFull) {
        // Slab is in the full list. Move it to the recycled list.
        ASSERT(slab->freeList != nullptr);
        mRecycledSlabs.Prepend(slab->Splice());
    }

    // TODO(enga): Occasionally prune slabs if |blocksInUse == 0|.
    // Doing so eagerly hurts performance.
}

void SlabAllocatorImpl::GetNewBlock() {
    if (mRecycledSlabs.next) {
        mAvailableSlabs.next = mRecycledSlabs.next;
        mAvailableSlabs.next->prev = &mAvailableSlabs;
        mRecycledSlabs.next = nullptr;
        return;
    }

    // Pad the allocation size by the alignment so that the aligned pointer still fulfills the
    // requested size.
    size_t requiredSize = static_cast<size_t>(mDataOffset) + mCount * mChunkSize;
    size_t allocationSize = requiredSize + mAllocationAlignment;

    auto allocation = std::unique_ptr<char[]>(new char[allocationSize]);
    char* alignedPtr = AlignPtr(allocation.get(), mAllocationAlignment);
    ASSERT(alignedPtr + requiredSize <= allocation.get() + allocationSize);

    char* dataStart = alignedPtr + mDataOffset;

    AllocationInfo* info = InfoFromAllocation(dataStart);
    for (uint32_t i = 0; i < mCount; ++i) {
        new (OffsetFrom(info, i)) AllocationInfo(i, i + 1);
    }

    AllocationInfo* lastInfo = OffsetFrom(info, mCount - 1);
    lastInfo->nextIndex = kInvalidIndex;

    mAvailableSlabs.Prepend(new (allocation.get()) Slab(std::move(allocation), info));
}
