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

#include "dawn_native/Error.h"
#include "dawn_native/dawn_platform.h"

namespace dawn_native {

    enum class AllocatorHeapType {
        Readback,
        Upload,
        None,  // aka Default
    };

    // Basic block in memory.
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

    // Block that owns a heap.
    // Heaps which contains only one block (same size) are considered "directly allocated".
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
        void Unmap();

        ResourceHeapBase* GetResourceHeap() const;
        const HeapSubAllocationBlock& GetSubAllocationBlock() const;
        size_t GetOffset() const;

        void Reset();

      private:
        ResourceHeapBase* mResourceHeap = nullptr;
        HeapSubAllocationBlock mSubAllocationBlock;
    };

    // Allocators implement Allocate and Deallocate which require two template arguments.
    // Block : wrapper for some byte offset and size in the heap.
    // ResourceAllocator : allocates the resource from the device.

    // Allocator that only allocates a single block for the whole resource (no sub-allocation).
    template <class TBlock, class TResourceAllocator>
    class DirectAllocator {
      public:
        // Constructor usually takes in the back-end device and heap type.
        // However, the required arguments can be generic as the actual device is not required.
        template <typename... ResourceAllocatorArgs>
        DirectAllocator(ResourceAllocatorArgs&&... resourceArgs);
        ~DirectAllocator() = default;

        // Simply forward allocate/deallocate to the backend heap allocator.
        TBlock Allocate(size_t allocationSize);
        void Deallocate(TBlock block);

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

    // TODO(bryan.bernhart@intel.com): Consider adding minSize which would prevent overhead from
    // allocating very small (ie. 1-byte) sub-allocations.

    template <typename TBlock = BuddyBlock>
    class BuddyAllocator {
      public:
        BuddyAllocator() = default;
        BuddyAllocator(size_t size);
        ~BuddyAllocator();

        // Required methods.
        TBlock Allocate(size_t allocationSize);
        void Deallocate(TBlock block);

        size_t GetMaxBlockSize() const;

        // For testing purposes only.
        size_t GetNumOfFreeBlocks() const;

      private:
        size_t ComputeLevelFromBlockSize(size_t blockSize) const;
        size_t GetNextFreeBlock(size_t allocationBlockLevel) const;

        void InsertFreeBlock(TBlock* block, size_t level);
        void RemoveFreeBlock(TBlock* block, size_t level);
        void DeleteBlock(TBlock* block);

        size_t ComputeNumOfFreeBlocks(TBlock* block) const;

        static const uint32_t MAX_LEVELS = 32;  // TODO: template variable or limit 4GB of size?

        // Keep track the head and tail (for faster insertion/removal).
        struct BlockList {
            TBlock* head = nullptr;  // First free block in level.
            // TODO: Track the tail.
        };

        TBlock* mRoot = nullptr;  // Used to deallocate non-free blocks.

        size_t mMaxBlockSize = 0;

        BlockList mFreeLists[MAX_LEVELS];  // List of linked-lists of blocks.
    };

    // Creates a linear pool of resources on-demand using a sub-allocation allocator.
    template <class TBlock, class TResourceAllocator, class TBlockAllocator>
    class LinearPoolAllocator {
      public:
        // Constructor usually takes in a back-end device and heap type.
        // However, the required arguments must be more generic as the actual device is not required
        // for testing. Size: Max size (in bytes) of the resource.
        template <typename... ResourceAllocatorArgs>
        LinearPoolAllocator(size_t maxSize,
                            size_t resourceSize,
                            ResourceAllocatorArgs&&... resourceArgs);

        ~LinearPoolAllocator() = default;

        // Required methods.
        TBlock Allocate(size_t allocationSize);
        void Deallocate(TBlock block);

        ResourceAllocation GetSubAllocation(const TBlock& block) const;

        // For testing purposes.
        const TBlockAllocator& GetBlockAllocator() const;
        size_t GetResourceCount() const;

      private:
        size_t GetResourceIndex(size_t offset) const;

        size_t mMaxResourceSize = 0;  // Size (in bytes) of each resource.

        // Allocator that sub-allocates from a range of memory.
        TBlockAllocator mBlockAllocator;

        // Allocates resources from the device.
        TResourceAllocator mResourceAllocator;

        // Track the sub-allocated blocks to resource.
        struct TrackedResourceAllocation {
            size_t refcount = 0;
            std::unique_ptr<ResourceHeapBase> mResource;
        };

        std::vector<TrackedResourceAllocation> mTrackedResourceAllocations;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_RESOURCEALLOCATOR_H_
