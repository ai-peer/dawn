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

#ifndef COMMON_SLABALLOCATOR_H_
#define COMMON_SLABALLOCATOR_H_

#include "common/PlacementAllocated.h"

#include <cstdint>
#include <memory>
#include <type_traits>

class SlabAllocatorImpl {
  public:
    // Allocations host their current index and the index of the next free block.
    // Because this is an index, and not a byte offset, it can be much smaller than a size_t.
    // TODO(enga): Is uint8_t sufficient?
    using Index = uint16_t;

  protected:
    struct AllocationInfo : PlacementAllocated {
        AllocationInfo(Index index, Index nextIndex);

        const Index index;
        Index nextIndex;
    };

    struct Slab : PlacementAllocated {
        Slab(std::unique_ptr<char[]> allocation, AllocationInfo* head);
        ~Slab();

        Slab* Splice();

        std::unique_ptr<char[]> allocation;
        AllocationInfo* freeList;
        Slab* prev;
        Slab* next;
        Index blocksInUse;
    };

    SlabAllocatorImpl(Index count,
                      uint32_t allocationAlignment,
                      uint32_t dataOffset,
                      uint32_t chunkSize,
                      uint32_t allocationInfoOffset);
    ~SlabAllocatorImpl();

    // Allocate a new block of memory.
    void* Allocate();

    // Deallocate a block of memory.
    void Deallocate(void* ptr);

  private:
    // The maximum value is reserved to indicate there is no value.
    static Index kInvalidIndex;

    // Get the AllocationInfo |offset| slots away.
    AllocationInfo* OffsetFrom(AllocationInfo* info, std::make_signed_t<Index> offset);

    // Compute the pointer to the AllocationInfo from an allocation.
    AllocationInfo* InfoFromAllocation(void* allocation);

    // Compute the pointer to the allocation from an AllocaitonInfo
    void* AllocationFromInfo(AllocationInfo* info);

    // The Slab stores a linked-list of free allocations.
    // PushFront/PopFront adds/removes an allocation from the free list.
    void PushFront(Slab* slab, AllocationInfo* info);
    AllocationInfo* PopFront(Slab* slab);

    // Replace the current slab with a new one, and chain the old one off of it.
    // Both slabs may still be used for for allocation/deallocation, but older slabs
    // will be a little slower to get allocations from.
    void GetNewBlock();

    const uint32_t mAllocationAlignment;

    // | Slab | pad | Object | pad | Info | pad | Object | pad | Info | pad | ....
    // | -----------|                                 mDataOffset
    // |            | ------------------------- |     mChunkSize
    // |            | -------------|                  mAllocationInfoOffset
    // | -------------------------------------------> (mDataOffset + mCount * mChunkSize)

    // A Slab is metadata, followed by
    // the aligned memory to allocate out of. |mDataOffset| is the offset to the
    // start of the aligned memory region.
    const uint32_t mDataOffset;

    // Because alignment of allocations may introduce padding, |mChunkSize| is the
    // distance between aligned blocks of (Allocation + AllocationInfo)
    const uint32_t mChunkSize;

    // The AllocationInfo is stored after the Allocation itself. This is the offset to it.
    const uint32_t mAllocationInfoOffset;

    const Index mCount;  // The total number of blocks in a slab.

    struct SentinelSlab : Slab {
        SentinelSlab() : Slab(nullptr, nullptr) {
        }

        void Prepend(Slab* slab);
    };

    SentinelSlab mAvailableSlabs;  // Available slabs to service allocations.
    SentinelSlab mFullSlabs;       // Full slabs. Stored here so we can skip checking them.
    SentinelSlab mRecycledSlabs;   // Recycled slabs. Not immediately added to |mAvailableSlabs| so
                                   // we don't thrash the current "active" slab.
};

template <typename T, uint32_t Alignment>
class SlabAllocator : public SlabAllocatorImpl {
    // Helper struct for computing alignments
    struct Storage {
        Slab slab;
        struct ObjectAndInfo {
            alignas(Alignment) char object[sizeof(T)];
            AllocationInfo info;
        } data[];
    };

  public:
    static constexpr uint32_t kAlignment = Alignment;

    SlabAllocator(Index count)
        : SlabAllocatorImpl(count,
                            alignof(Storage),            // allocationAlignment
                            offsetof(Storage, data[0]),  // dataOffset
                            offsetof(Storage, data[1]) - offsetof(Storage, data[0]),  // chunkSize
                            offsetof(typename Storage::ObjectAndInfo, info)  // allocationInfoOffset
          ) {
        static_assert(Alignment >= alignof(T), "");
    }

    template <typename... Args>
    T* Allocate(Args&&... args) {
        void* ptr = SlabAllocatorImpl::Allocate();
        return new (ptr) T(std::forward<Args>(args)...);
    }

    void Deallocate(T* object) {
        SlabAllocatorImpl::Deallocate(object);
    }
};

#endif  // COMMON_SLABALLOCATOR_H_
