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

#include "dawn_native/ResourceAllocator2.h"
#include "dawn_native/ResourceHeap.h"

#include "common/Math.h"

// Note: This file must be included by dawn_native/[backend]/[*]Allocator.h.
namespace dawn_native {

    // ResourceAllocation

    ResourceAllocation::ResourceAllocation(ResourceHeapBase* resourceHeap,
                                           const HeapSubAllocationBlock& allocationBlock)
        : mResourceHeap(resourceHeap), mSubAllocationBlock(allocationBlock) {
    }

    size_t ResourceAllocation::GetOffset() const {
        return mSubAllocationBlock.GetOffset();
    }

    void ResourceAllocation::Reset() {
        mResourceHeap = nullptr;
    }

    ResultOrError<void*> ResourceAllocation::Map() {
        void* mappedPointer = nullptr;
        DAWN_TRY_ASSIGN(mappedPointer, mResourceHeap->Map());
        return static_cast<uint8_t*>(mappedPointer) + mSubAllocationBlock.GetOffset();
    }

    void ResourceAllocation::Unmap() {
        mResourceHeap->Unmap();
    }

    ResourceHeapBase* ResourceAllocation::GetResourceHeap() const {
        return mResourceHeap;
    }

    const HeapSubAllocationBlock& ResourceAllocation::GetSubAllocationBlock() const {
        return mSubAllocationBlock;
    }

    // HeapSubAllocationBlock

    HeapSubAllocationBlock::HeapSubAllocationBlock(size_t size,
                                                   size_t offset,
                                                   ResourceHeapBase* resourceHeap)
        : Block(size, offset), mResourceHeap(resourceHeap){};

    ResourceHeapBase* HeapSubAllocationBlock::GetResourceHeap() const {
        return mResourceHeap;
    }

    // DirectAllocator

    template <class TBlock, class TResourceAllocator>
    template <typename... ResourceAllocatorArgs>
    DirectAllocator<TBlock, TResourceAllocator>::DirectAllocator(
        ResourceAllocatorArgs&&... resourceArgs)
        : mResourceAllocator(std::forward<ResourceAllocatorArgs>(resourceArgs)...) {
    }

    template <class TBlock, class TResourceAllocator>
    TBlock DirectAllocator<TBlock, TResourceAllocator>::Allocate(size_t allocationSize) {
        return TBlock(allocationSize, /*offset*/ 0,
                      mResourceAllocator.Allocate(allocationSize).get());
    }

    template <class TBlock, class TResourceAllocator>
    void DirectAllocator<TBlock, TResourceAllocator>::Deallocate(TBlock block) {
        mResourceAllocator.Deallocate(block.GetResourceHeap());
    }

    // LinearPoolAllocator

    template <class TBlock, class TResourceAllocator, class TBlockAllocator>
    template <typename... ResourceAllocatorArgs>
    LinearPoolAllocator<TBlock, TResourceAllocator, TBlockAllocator>::LinearPoolAllocator(
        size_t maxSize,
        size_t resourceSize,
        ResourceAllocatorArgs&&... resourceArgs)
        : mMaxResourceSize(resourceSize),
          mBlockAllocator(maxSize),
          mResourceAllocator(std::forward<ResourceAllocatorArgs>(resourceArgs)...) {
        ASSERT(IsPowerOfTwo(maxSize));
        ASSERT(IsPowerOfTwo(resourceSize));
        ASSERT(mBlockAllocator.GetMaxBlockSize() % mMaxResourceSize == 0);
    }

    template <class TBlock, class TResourceAllocator, class TBlockAllocator>
    size_t LinearPoolAllocator<TBlock, TResourceAllocator, TBlockAllocator>::GetResourceIndex(
        size_t offset) const {
        return offset / mMaxResourceSize;
    }

    template <class TBlock, class TResourceAllocator, class TBlockAllocator>
    TBlock LinearPoolAllocator<TBlock, TResourceAllocator, TBlockAllocator>::Allocate(
        size_t allocationSize) {
        ASSERT(IsPowerOfTwo(allocationSize));

        const TBlock empty = TBlock{0, 0};

        // Allocation cannot exceed the specified allocator size.
        if (allocationSize > mBlockAllocator.GetMaxBlockSize()) {
            return empty;
        }

        // Allocation cannot exceed the specified resource size.
        if (allocationSize > mMaxResourceSize) {
            return empty;
        }

        // Attempt to sub-allocate a block of the requested size.
        const TBlock block = mBlockAllocator.Allocate(allocationSize);
        if (block.GetSize() != allocationSize) {  // Return empty block on failure.
            return block;
        }

        // Ensure the allocated block can be mapped back to the resource.
        // Additional resources are created if not.
        const size_t resourceIndex = GetResourceIndex(block.GetOffset());

        // Re-size tracked resource allocation.
        if (resourceIndex >= mTrackedResourceAllocations.size()) {
            const size_t numOfResourcesRequired =
                (resourceIndex + 1) - mTrackedResourceAllocations.size();
            for (uint32_t i = 0; i < numOfResourcesRequired; i++) {
                // Transfer ownership of the resource to this allocator.
                mTrackedResourceAllocations.push_back(
                    {/*refcount*/ 0, std::move(mResourceAllocator.Allocate(mMaxResourceSize))});
            }
        }
        // Allocate resource that was previously de-allocated.
        else if (mTrackedResourceAllocations[resourceIndex].refcount == 0) {
            mTrackedResourceAllocations[resourceIndex] = {
                /*refcount*/ 0, std::move(mResourceAllocator.Allocate(mMaxResourceSize))};
        }

        mTrackedResourceAllocations[resourceIndex].refcount++;

        return block;
    }

    template <class TBlock, class TResourceAllocator, class TBlockAllocator>
    ResourceAllocation
    LinearPoolAllocator<TBlock, TResourceAllocator, TBlockAllocator>::GetSubAllocation(
        const TBlock& block) const {
        const size_t resourceIndex = GetResourceIndex(block.GetOffset());
        return ResourceAllocation(mTrackedResourceAllocations[resourceIndex].mResource, block);
    }

    template <class TBlock, class TResourceAllocator, class TBlockAllocator>
    void LinearPoolAllocator<TBlock, TResourceAllocator, TBlockAllocator>::Deallocate(
        TBlock block) {
        ASSERT(block.mState == BlockState::Allocated);

        const size_t resourceIndex = GetResourceIndex(block.GetOffset());

        ASSERT(mTrackedResourceAllocations[resourceIndex].refcount > 0);

        mTrackedResourceAllocations[resourceIndex].refcount--;

        if (mTrackedResourceAllocations[resourceIndex].refcount == 0) {
            mResourceAllocator.Deallocate(
                mTrackedResourceAllocations[resourceIndex].mResource.get());
            mTrackedResourceAllocations[resourceIndex].mResource = nullptr;  // Invalidate it.
        }

        mBlockAllocator.Deallocate(block);
    }

    template <class TBlock, class TResourceAllocator, class TBlockAllocator>
    const TBlockAllocator&
    LinearPoolAllocator<TBlock, TResourceAllocator, TBlockAllocator>::GetBlockAllocator() const {
        return mBlockAllocator;
    }

    template <class TBlock, class TResourceAllocator, class TBlockAllocator>
    size_t LinearPoolAllocator<TBlock, TResourceAllocator, TBlockAllocator>::GetResourceCount()
        const {
        size_t allocatedCount = 0;
        for (const TrackedResourceAllocation& trackedResource : mTrackedResourceAllocations) {
            if (trackedResource.mResource)
                allocatedCount++;
        }
        return allocatedCount;
    }

    // BuddyAllocator

    static constexpr size_t INVALID_LEVEL = std::numeric_limits<size_t>::max();

    template <class TBlock>
    BuddyAllocator<TBlock>::BuddyAllocator(size_t size) : mMaxBlockSize(size) {
        // Insert the level0 free block.
        mRoot = new TBlock(size, /*offset*/ 0);
        mRoot->free.pNext = nullptr;
        mRoot->free.pPrev = nullptr;
        mFreeLists[0] = {mRoot};
    }

    template <class TBlock>
    BuddyAllocator<TBlock>::~BuddyAllocator() {
        DeleteBlock(mRoot);
    }

    template <class TBlock>
    size_t BuddyAllocator<TBlock>::GetMaxBlockSize() const {
        return mMaxBlockSize;
    }

    template <class TBlock>
    size_t BuddyAllocator<TBlock>::GetNumOfFreeBlocks() const {
        return ComputeNumOfFreeBlocks(mRoot);
    }

    template <class TBlock>
    size_t BuddyAllocator<TBlock>::ComputeNumOfFreeBlocks(TBlock* block) const {
        if (block->mState == BlockState::Free)
            return 1;
        else if (block->mState == BlockState::Split) {
            return ComputeNumOfFreeBlocks(block->split.pLeft) +
                   ComputeNumOfFreeBlocks(block->split.pLeft->pBuddy);
        }
        return 0;
    }

    template <class TBlock>
    size_t BuddyAllocator<TBlock>::ComputeLevelFromBlockSize(size_t blockSize) const {
        // Every level in the buddy system can be indexed by order-n where n = log2(blockSize).
        // However, mFreeList zero-indexed by level.
        // For example, blockSize=4 is Level1 if MAX_BLOCK is 8.
        return Log2(mMaxBlockSize) - Log2(blockSize);
    }

    template <class TBlock>
    size_t BuddyAllocator<TBlock>::GetNextFreeBlock(size_t allocationBlockLevel) const {
        // Go-up level-by-level until a free block exists.
        // Check if Freelist[level] is empty since lower level blocks only exist when upper blocks
        // split.
        for (size_t currLevel = allocationBlockLevel; currLevel >= 0; currLevel--) {
            if (mFreeLists[currLevel].head)
                return currLevel;
        }
        return INVALID_LEVEL;  // No free block exists at any level.
    }

    // Inserts existing free block into the free-list.
    // Called by allocate upon splitting to insert a child block into a free-list.
    // Note: Always insert into the head of the free-list. As when a larger free block at a lower
    // level was split, there were no smaller free blocks at a higher level to allocate.
    template <class TBlock>
    void BuddyAllocator<TBlock>::InsertFreeBlock(TBlock* block, size_t level) {
        ASSERT(block->mState == BlockState::Free);

        // Block already in HEAD position (ex. right child was inserted first).
        if (mFreeLists[level].head) {
            // Inserted block is now the front (no prev).
            block->free.pPrev = nullptr;

            // Old head is now the inserted block's next.
            block->free.pNext = mFreeLists[level].head;

            // Old head's previous is the inserted block.
            mFreeLists[level].head->free.pPrev = block;

            mFreeLists[level].head = block;
        } else {
            block->free.pPrev = nullptr;
            block->free.pNext = nullptr;

            mFreeLists[level].head = block;
        }
    }

    template <class TBlock>
    void BuddyAllocator<TBlock>::RemoveFreeBlock(TBlock* block, size_t level) {
        ASSERT(block->mState == BlockState::Free);

        // Block is HEAD position.
        if (mFreeLists[level].head == block) {
            mFreeLists[level].head = mFreeLists[level].head->free.pNext;
            // Block is after HEAD position.
        } else {
            TBlock* pPrev = block->free.pPrev;
            TBlock* pNext = block->free.pNext;

            // When inserting after front, there must be a previous block.
            ASSERT(pPrev != nullptr);

            pPrev->free.pNext = pNext;

            if (pNext)
                pNext->free.pPrev = pPrev;
        }
    }

    template <class TBlock>
    TBlock BuddyAllocator<TBlock>::Allocate(size_t allocationSize) {
        ASSERT(IsPowerOfTwo(allocationSize));

        // Compute the levels
        const size_t allocationSizeToLevel = ComputeLevelFromBlockSize(allocationSize);

        const TBlock empty = TBlock{0, 0};

        // Error when limit is too small.
        if (allocationSizeToLevel > MAX_LEVELS)
            return empty;

        size_t currBlockLevel = GetNextFreeBlock(allocationSizeToLevel);

        // Error when no free blocks exist.
        if (currBlockLevel == INVALID_LEVEL)
            return empty;

        // Split blocks level-by-level.
        // Terminate when the current block level is equal the computed level of the requested
        // allocation.
        TBlock* currBlock = mFreeLists[currBlockLevel].head;

        while (currBlockLevel < allocationSizeToLevel) {
            ASSERT(currBlock->mState == BlockState::Free);

            // Remove curr block (about to be split).
            RemoveFreeBlock(currBlock, currBlockLevel);

            // Create two free child blocks (the buddies).
            const size_t nextLevelSize = currBlock->GetSize() / 2;
            TBlock* leftChildBlock = new TBlock(nextLevelSize, currBlock->GetOffset());
            TBlock* rightChildBlock =
                new TBlock(nextLevelSize, currBlock->GetOffset() + nextLevelSize);

            // Remeber the parent to merge these back upon de-allocation.
            rightChildBlock->pParent = currBlock;
            leftChildBlock->pParent = currBlock;

            // Make them buddies.
            leftChildBlock->pBuddy = rightChildBlock;
            rightChildBlock->pBuddy = leftChildBlock;

            // Insert the children back into the free list into the next level.
            // Cannot update the left child's next free block until right child gets inserted.
            InsertFreeBlock(rightChildBlock, currBlockLevel + 1);
            InsertFreeBlock(leftChildBlock, currBlockLevel + 1);

            // Curr block is now split.
            currBlock->mState = BlockState::Split;
            currBlock->split.pLeft = leftChildBlock;

            // Decend down into the next level (the left child block).
            currBlock = mFreeLists[++currBlockLevel].head;
        }

        // Remove curr block from free-list (as its now allocated).
        RemoveFreeBlock(currBlock, currBlockLevel);
        currBlock->mState = BlockState::Allocated;

        return *currBlock;
    }

    template <class TBlock>
    void BuddyAllocator<TBlock>::Deallocate(TBlock block) {
        ASSERT(block.mState == BlockState::Allocated);

        TBlock* curr = mRoot;

        // Search for the free block node that corresponds to the block offset.
        size_t currBlockLevel = 0;

        while (curr->mState == BlockState::Split) {
            if (block.GetOffset() < curr->split.pLeft->pBuddy->GetOffset()) {
                curr = curr->split.pLeft;
            } else {
                curr = curr->split.pLeft->pBuddy;
            }

            currBlockLevel++;
        }

        // Mark curr free so we can merge.
        curr->mState = BlockState::Free;

        // Merge the buddies (LevelN-to-Level0).
        while (currBlockLevel > 0 && curr->pBuddy->mState == BlockState::Free) {
            // Remove the buddy.
            RemoveFreeBlock(curr->pBuddy, currBlockLevel);

            TBlock* parent = curr->pParent;

            // Delete the pair in order we inserted.
            DeleteBlock(curr->pBuddy);
            DeleteBlock(curr);

            // Parent is now free.
            parent->mState = BlockState::Free;

            // Go up one level
            curr = parent;

            currBlockLevel--;
        }

        InsertFreeBlock(curr, currBlockLevel);
    }

    // Helper which deletes a block node recursively (post-order binary tree traversal).
    template <class TBlock>
    void BuddyAllocator<TBlock>::DeleteBlock(TBlock* block) {
        ASSERT(block != nullptr);

        if (block->mState == BlockState::Split) {
            // Delete the pair in same order we inserted.
            DeleteBlock(block->split.pLeft->pBuddy);
            DeleteBlock(block->split.pLeft);
        }
        // Else, block must be allocated or free.
        delete block;
    }
}  // namespace dawn_native