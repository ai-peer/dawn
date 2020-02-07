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

// The SlabAllocator allocates objects out of one or more fixed-size contiguous "slabs" of memory.
// This makes it very quick to allocate and deallocate fixed-size objects because the allocator only
// needs to index an offset into pre-allocated memory. It is similar to a pool-allocator that
// recycles memory from previous allocations, except multiple allocations are hosted contiguously in
// one large slab.
//
// Internally, the SlabAllocator stores slabs as a linked list to avoid extra indirections indexing
// into an std::vector. To service an allocation request, the allocator only needs to know the first
// currently available slab.
//
// Allocated objects are placement-allocated with some extra info at the end (we'll call the Object
// plus the extra bytes a "block") used to specify the constant index of the block in its parent
// slab, as well as the index of the next available block. So, following the block next-indices
// forms a linked list of free blocks.
//
// Slab creation: When a new slab is allocated, sufficient memory is allocated for it, and then the
// slab metadata plus all of it's child blocks are placement-allocated into the memory. Indices and
// next-indices are initialized to form the free-list of blocks.
//
// Allocation: When an object is allocated, if there is no space available in an existing slab, a
// new slab is created (or an old slab is recycled). The first block of the slab is removed and
// returned.
//
// Deallocation: When an object is deallocated, it can compute the pointer to its parent slab
// because it stores the index of its own allocation. That block is then prepended to the slab's
// free list.
//
// Multi-slab-list optimizations: To decrease the amount of pointer-chasing, once a slab becomes
// full, it is moved immediately to a list of full slabs so we can skip checking them entirely. When
// any one deallocation happens in a full slab, it's moved to a list of recycled slabs. This list
// will be used once the allocator needs to look for a new slab.
class SlabAllocatorImpl {
  public:
    // Allocations host their current index and the index of the next free block.
    // Because this is an index, and not a byte offset, it can be much smaller than a size_t.
    // TODO(enga): Is uint8_t sufficient?
    using Index = uint16_t;

  protected:
    struct BlockInfo : PlacementAllocated {
        BlockInfo(Index index, Index nextIndex);

        const Index index;  // The index of this block in the slab.
        Index nextIndex;    // The index of the next available block. kInvalidIndex, if none.
    };

    struct Slab : PlacementAllocated {
        // A slab is placement-allocated into an aligned pointer from a separate allocation.
        // Ownership of the allocation is transferred to the slab on creation.
        // | ---------- allocation --------- |
        // | pad | Slab | data ------------> |
        Slab(std::unique_ptr<char[]> allocation, BlockInfo* head);
        ~Slab();

        Slab* Splice();

        std::unique_ptr<char[]> allocation;
        BlockInfo* freeList;
        Slab* prev;
        Slab* next;
        Index blocksInUse;
    };

    SlabAllocatorImpl(Index count,
                      uint32_t allocationAlignment,
                      uint32_t dataOffset,
                      uint32_t blockSize,
                      uint32_t blockInfoOffset);
    ~SlabAllocatorImpl();

    // Allocate a new block of memory.
    void* Allocate();

    // Deallocate a block of memory.
    void Deallocate(void* ptr);

  private:
    // The maximum value is reserved to indicate there is no value.
    static Index kInvalidIndex;

    // Get the BlockInfo |offset| slots away.
    BlockInfo* OffsetFrom(BlockInfo* info, std::make_signed_t<Index> offset);

    // Compute the pointer to the BlockInfo from an allocated object.
    BlockInfo* InfoFromObject(void* object);

    // Compute the pointer to the object from an BlockInfo.
    void* ObjectFromInfo(BlockInfo* info);

    // The Slab stores a linked-list of free allocations.
    // PushFront/PopFront adds/removes an allocation from the free list.
    void PushFront(Slab* slab, BlockInfo* info);
    BlockInfo* PopFront(Slab* slab);

    // Replace the current slab with a new one, and chain the old one off of it.
    // Both slabs may still be used for for allocation/deallocation, but older slabs
    // will be a little slower to get allocations from.
    void GetNewSlab();

    const uint32_t mAllocationAlignment;

    // | Slab | pad | Object | pad | Info | pad | Object | pad | Info | pad | ....
    // | -----------|                                 mDataOffset
    // |            | ------------------------- |     mBlockSize
    // |            | -------------|                  mBlockInfoOffset
    // | -------------------------------------------> (mDataOffset + mCount * mBlockSize)

    // A Slab is metadata, followed by
    // the aligned memory to allocate out of. |mDataOffset| is the offset to the
    // start of the aligned memory region.
    const uint32_t mDataOffset;

    // Because alignment of allocations may introduce padding, |mBlockSize| is the
    // distance between aligned blocks of (Allocation + BlockInfo)
    const uint32_t mBlockSize;

    // The BlockInfo is stored after the Allocation itself. This is the offset to it.
    const uint32_t mBlockInfoOffset;

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
        struct Block {
            // We do this char[sizeof(T)] trick because if T is a non-standard-layout type,
            // we can't use offsetof on the Block and Storage structs.
            alignas(Alignment) char object[sizeof(T)];
            BlockInfo info;
        } blocks[];
    };

  public:
    static constexpr uint32_t kAlignment = Alignment;

    SlabAllocator(Index count)
        : SlabAllocatorImpl(
              count,
              alignof(Storage),                                             // allocationAlignment
              offsetof(Storage, blocks[0]),                                 // dataOffset
              offsetof(Storage, blocks[1]) - offsetof(Storage, blocks[0]),  // blockSize
              offsetof(typename Storage::Block, info)                       // blockInfoOffset
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
