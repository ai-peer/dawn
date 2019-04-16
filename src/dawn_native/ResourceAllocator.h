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

#ifndef DAWNNATIVE_RESOURCEALLOCATOR_H_
#define DAWNNATIVE_RESOURCEALLOCATOR_H_

#include <vector>

#include "dawn_native/Error.h"
#include "dawn_native/dawn_platform.h"

namespace dawn_native {

    // Depending on the expected resource usage pattern, optimal allocators may be used.
    enum class AllocatorResourceUsage {
        // For frequently modifying transient resources on CPU. Internally sub-allocates memory
        // using a ring-buffer for optimal allocation.
        // Used for immediate buffer uploads (ie. SetSubData).
        Dynamic,

        // For frequently modifying persistant resources on CPU. Internally sub-allocates using the
        // buddy-system for optimal allocation.
        // Used for mapped buffer uploads (ie. Map[Read|Write]).
        Upload,

        // Other cases which do not expect resources to be made CPU-visible.
        Unknown
    };

    class Block {
      public:
        Block() : mSize(0), mOffset(0) {
        }
        Block(size_t size, size_t offset) : mSize(size), mOffset(offset){};
        size_t GetOffset() const {
            return mOffset;
        }
        size_t GetSize() const {
            return mSize;
        }

      protected:
        size_t mSize;
        size_t mOffset;
    };

    // Block which specifies if it was sub-allocated in a resource heap or not.
    // If the block contains a resource heap, it is not considered sub-allocated where
    // one block represents the whole resource heap.
    class HeapSubAllocationBlock : public Block {
      public:
        HeapSubAllocationBlock() = default;
        HeapSubAllocationBlock(size_t size,
                               size_t offset,
                               ResourceHeapBase* resourceHeap = nullptr);
        ~HeapSubAllocationBlock() = default;

        ResourceHeapBase* GetResourceHeap() const;

      private:
        ResourceHeapBase* mResourceHeap = nullptr;  // Determines if sub-allocated or not.
    };

    // Wrapper to adjust heap by block offset.
    // ResourceHeapBase is the device-allocated resource.
    // HeapSubAllocationBlock is the sub-allocated block.
    class ResourceAllocation {
      public:
        ResourceAllocation() = default;
        ResourceAllocation(ResourceHeapBase* resourceHeap,
                           const HeapSubAllocationBlock& allocationBlock);
        ~ResourceAllocation() = default;

        ResultOrError<void*> Map();
        MaybeError Unmap();

        ResourceHeapBase* GetResourceHeap() const;
        const HeapSubAllocationBlock& GetSubAllocationBlock() const;
        size_t GetOffset() const;

      private:
        ResourceHeapBase* mResourceHeap = nullptr;
        HeapSubAllocationBlock mSubAllocationBlock;
    };

    // Allocators implement Allocate and Deallocate which require two template arguments:
    // Block : wrapper for some byte offset and size in the heap.
    // ResourceAllocator : allocates the resource from the device.

    // Allocator that only allocates a single block for the whole resource (no sub-allocation).
    template <class TBlock, class TResourceAllocator>
    class DirectAllocator {
      public:
        DirectAllocator() = default;
        // Constructor usually takes in the back-end device and heap type.
        // However, the required arguments should be generic as the actual device is not required.
        template <typename... ResourceAllocatorArgs>
        DirectAllocator(ResourceAllocatorArgs&&... resourceArgs);
        ~DirectAllocator() = default;

        // Required methods.
        TBlock Allocate(size_t allocationSize);
        void Deallocate(TBlock block);

        ResourceAllocation GetSubAllocation(const TBlock& block) const;

      private:
        TResourceAllocator mResourceAllocator;
    };

    enum class BlockState { Free, Split, Allocated };

    struct BuddyBlock : public HeapSubAllocationBlock {
        BuddyBlock(size_t size, size_t offset)
            : HeapSubAllocationBlock(size, offset), mState(BlockState::Free) {
        }

        // Pointer to this block's buddy, iff parent is split.
        // Used to quickly merge buddy blocks upon de-allocate.
        BuddyBlock* pBuddy = nullptr;
        BuddyBlock* pParent = nullptr;

        // Track whether this block has been split or not.
        BlockState mState;

        union {
            // Used upon allocation.
            // Avoids searching for the next free block.
            struct {
                BuddyBlock* pPrev = nullptr;
                BuddyBlock* pNext = nullptr;
            } free;

            // Used upon de-allocation.
            // Had this block split upon allocation, it and it's buddy is to be deleted.
            struct {
                BuddyBlock* pLeft = nullptr;
            } split;
        };
    };

    // Buddy block allocator uses the buddy system to sub-allocate a memory address range into
    // blocks. Uses a freelist to track free blocks in the binary tree.
    template <typename TBlock>
    class BuddyBlockAllocator {
      public:
        BuddyBlockAllocator() = default;
        // The ctor takes two arguments:
        // maxSize: size of the largest block allowed to be allocated.
        // minSize: size of the smallest block allowed to be allocated.
        //
        // To prevent overhead of allocating of very small sub-allocations (ie. 1-byte per block),
        // minSize can be set to min. resource alignment requirement.
        BuddyBlockAllocator(size_t maxSize, size_t minSize = 1);
        ~BuddyBlockAllocator();

        // Required methods.
        TBlock Allocate(size_t allocationSize);
        void Deallocate(TBlock block);

        size_t GetMaxBlockSize() const;

        // For testing purposes only.
        size_t GetNumOfFreeBlocks() const;

      private:
        size_t ComputeLevelFromBlockSize(size_t blockSize) const;
        size_t GetNextFreeBlock(size_t allocationBlockLevel) const;

        void InsertFreeBlock(BuddyBlock* block, size_t level);
        void RemoveFreeBlock(BuddyBlock* block, size_t level);
        void DeleteBlock(BuddyBlock* block);

        size_t ComputeNumOfFreeBlocks(BuddyBlock* block) const;

        // Keep track the head and tail (for faster insertion/removal).
        struct BlockList {
            BuddyBlock* head = nullptr;  // First free block in level.
            // TODO: Track the tail.
        };

        BuddyBlock* mRoot = nullptr;  // Used to deallocate non-free blocks.

        size_t mMaxBlockSize = 0;
        size_t mMinBlockSize = 0;

        std::vector<BlockList> mFreeLists;  // List of linked-lists of blocks.
    };

    // Buddy Pool Allocator uses a single buddy allocator with multiple resource heaps.
    // The resource heap index is computed from the sub-allocated block's offset and created
    // on-demand.
    //
    // Requires the following template arguments:
    // Block: allocator block type.
    // ResourceAllocator: resource heap allocator.
    // BlockAllocator: block allocator.
    // minBlockSize: Smallest block size allowed in a resource.
    // minResourceSize: Smallest resource size allowed in the pool.
    template <class TBlock,
              class TResourceAllocator,
              class TBlockAllocator,
              size_t minBlockSize,
              size_t minResourceHeapSize>
    class BuddyPoolAllocator {
      public:
        BuddyPoolAllocator() = default;
        // Constructor usually takes in a back-end device and heap type.
        // However, the required arguments must be more generic as the actual device is not required
        // for testing.
        template <typename... ResourceAllocatorArgs>
        BuddyPoolAllocator(size_t maxBlockSize,
                           size_t resourceSize,
                           ResourceAllocatorArgs&&... resourceArgs);

        ~BuddyPoolAllocator() = default;

        // Required methods.
        TBlock Allocate(size_t allocationSize);
        void Deallocate(TBlock block);

        ResourceAllocation GetSubAllocation(const TBlock& block) const;

        // For testing purposes.
        const TBlockAllocator& GetBlockAllocator() const;
        size_t GetResourceCount() const;

      private:
        size_t GetResourceIndex(size_t offset) const;

        size_t mResourceHeapSize = 0;  // Size (in bytes) of each resource.

        // Allocator that sub-allocates from a range of memory.
        TBlockAllocator mBlockAllocator;

        // Allocates resources from the device.
        TResourceAllocator mResourceAllocator;

        // Track the sub-allocations on the resource.
        struct TrackedResourceAllocation {
            size_t refcount = 0;
            ResourceHeapBase* mResource = nullptr;
        };

        std::vector<TrackedResourceAllocation> mTrackedResourceAllocations;
    };

    // Allocator which could either sub-allocate or not.
    template <class TBlock, class TPoolAllocator, class TDirectAllocator>
    class ConditionalAllocator {
      public:
        template <typename... ResourceAllocatorArgs>
        ConditionalAllocator(size_t allocatorSize,
                             size_t resourceSize,
                             ResourceAllocatorArgs&&... resourceArgs);
        ~ConditionalAllocator() = default;

        // Required methods.
        TBlock Allocate(size_t allocationSize, bool IsDirect = false);
        void Deallocate(TBlock block);

        ResourceAllocation GetSubAllocation(const TBlock& block) const;

      private:
        TPoolAllocator mPoolAllocator;
        TDirectAllocator mDirectAllocator;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_RESOURCEALLOCATOR_H_
