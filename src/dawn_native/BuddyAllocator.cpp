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

#include "dawn_native/BuddyAllocator.h"

#include "common/Assert.h"
#include "common/Math.h"

namespace dawn_native {

    BuddyAllocator::BuddyAllocator(size_t maxSize) : mMaxBlockSize(maxSize) {
        ASSERT(IsPowerOfTwo(maxSize));

        mFreeLists.resize(Log2(mMaxBlockSize) + 1);

        // Insert the level0 free block.
        mRoot = new BuddyBlock(maxSize, /*offset*/ 0);
        mRoot->free.pNext = nullptr;
        mRoot->free.pPrev = nullptr;
        mFreeLists[0] = {mRoot};
    }

    BuddyAllocator::~BuddyAllocator() {
        if (mRoot) {
            DeleteBlock(mRoot);
        }
    }

    size_t BuddyAllocator::GetNumOfFreeBlocks() const {
        return ComputeNumOfFreeBlocks(mRoot);
    }

    size_t BuddyAllocator::ComputeNumOfFreeBlocks(BuddyBlock* block) const {
        if (block->mState == BlockState::Free)
            return 1;
        else if (block->mState == BlockState::Split) {
            return ComputeNumOfFreeBlocks(block->split.pLeft) +
                   ComputeNumOfFreeBlocks(block->split.pLeft->pBuddy);
        }
        return 0;
    }

    size_t BuddyAllocator::ComputeLevelFromBlockSize(size_t blockSize) const {
        // Every level in the buddy system can be indexed by order-n where n = log2(blockSize).
        // However, mFreeList zero-indexed by level.
        // For example, blockSize=4 is Level1 if MAX_BLOCK is 8.
        return Log2(mMaxBlockSize) - Log2(blockSize);
    }

    size_t BuddyAllocator::GetNextFreeBlock(size_t allocationBlockLevel) const {
        // Go-up level-by-level until a free block exists.
        // Check if Freelist[level] is empty since lower level blocks only exist when upper blocks
        // split.
        for (size_t currLevel = allocationBlockLevel; currLevel >= 0; currLevel--) {
            if (mFreeLists[currLevel].head)
                return currLevel;

            if (currLevel == 0)
                break;
        }
        return INVALID_OFFSET;  // No free block exists at any level.
    }

    // Inserts existing free block into the free-list.
    // Called by allocate upon splitting to insert a child block into a free-list.
    // Note: Always insert into the head of the free-list. As when a larger free block at a lower
    // level was split, there were no smaller free blocks at a higher level to allocate.
    void BuddyAllocator::InsertFreeBlock(BuddyBlock* block, size_t level) {
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

    void BuddyAllocator::RemoveFreeBlock(BuddyBlock* block, size_t level) {
        ASSERT(block->mState == BlockState::Free);

        // Block is HEAD position.
        if (mFreeLists[level].head == block) {
            mFreeLists[level].head = mFreeLists[level].head->free.pNext;
            // Block is after HEAD position.
        } else {
            BuddyBlock* pPrev = block->free.pPrev;
            BuddyBlock* pNext = block->free.pNext;

            // When inserting after front, there must be a previous block.
            ASSERT(pPrev != nullptr);

            pPrev->free.pNext = pNext;

            if (pNext)
                pNext->free.pPrev = pPrev;
        }
    }

    size_t BuddyAllocator::Allocate(size_t allocationSize) {
        ASSERT(IsPowerOfTwo(allocationSize));

        // Allocation cannot be larger than max block.
        if (allocationSize > mMaxBlockSize)
            return INVALID_OFFSET;

        // Compute the levels
        const size_t allocationSizeToLevel = ComputeLevelFromBlockSize(allocationSize);

        ASSERT(allocationSizeToLevel < mFreeLists.size());

        size_t currBlockLevel = GetNextFreeBlock(allocationSizeToLevel);

        // Error when no free blocks exist (allocator is full)
        if (currBlockLevel == INVALID_OFFSET)
            return INVALID_OFFSET;

        // Split free blocks level-by-level.
        // Terminate when the current block level is equal the computed level of the requested
        // allocation.
        BuddyBlock* currBlock = mFreeLists[currBlockLevel].head;

        while (currBlockLevel < allocationSizeToLevel) {
            ASSERT(currBlock->mState == BlockState::Free);

            // Remove curr block (about to be split).
            RemoveFreeBlock(currBlock, currBlockLevel);

            // Create two free child blocks (the buddies).
            const size_t nextLevelSize = currBlock->mSize / 2;
            BuddyBlock* leftChildBlock = new BuddyBlock(nextLevelSize, currBlock->mOffset);
            BuddyBlock* rightChildBlock =
                new BuddyBlock(nextLevelSize, currBlock->mOffset + nextLevelSize);

            // Remember the parent to merge these back upon de-allocation.
            rightChildBlock->pParent = currBlock;
            leftChildBlock->pParent = currBlock;

            // Make them buddies.
            leftChildBlock->pBuddy = rightChildBlock;
            rightChildBlock->pBuddy = leftChildBlock;

            // Insert the children back into the free list into the next level.
            // Order matters. Cannot update the left child's next free block until right child gets
            // inserted.
            InsertFreeBlock(rightChildBlock, currBlockLevel + 1);
            InsertFreeBlock(leftChildBlock, currBlockLevel + 1);

            // Curr block is now split.
            currBlock->mState = BlockState::Split;
            currBlock->split.pLeft = leftChildBlock;

            // Decend down into the next level (the left child block).
            currBlock = mFreeLists[++currBlockLevel].head;
        }

        // Remove curr block from free-list (its now allocated).
        RemoveFreeBlock(currBlock, currBlockLevel);
        currBlock->mState = BlockState::Allocated;

        return currBlock->mOffset;
    }

    void BuddyAllocator::Deallocate(size_t offset) {
        BuddyBlock* curr = mRoot;

        // Search for the free block node that corresponds to the block offset.
        size_t currBlockLevel = 0;
        while (curr->mState == BlockState::Split) {
            if (offset < curr->split.pLeft->pBuddy->mOffset) {
                curr = curr->split.pLeft;
            } else {
                curr = curr->split.pLeft->pBuddy;
            }

            currBlockLevel++;
        }

        ASSERT(curr->mState == BlockState::Allocated);

        // Mark curr free so we can merge.
        curr->mState = BlockState::Free;

        // Merge the buddies (LevelN-to-Level0).
        while (currBlockLevel > 0 && curr->pBuddy->mState == BlockState::Free) {
            // Remove the buddy.
            RemoveFreeBlock(curr->pBuddy, currBlockLevel);

            BuddyBlock* parent = curr->pParent;

            // Order matters. Delete the pair in same order we inserted.
            DeleteBlock(curr->pBuddy);
            DeleteBlock(curr);

            // Parent is now free.
            parent->mState = BlockState::Free;

            // Ascend up to the next level (parent block).
            curr = parent;
            currBlockLevel--;
        }

        InsertFreeBlock(curr, currBlockLevel);
    }

    // Helper which deletes a block in the tree recursively (post-order).
    void BuddyAllocator::DeleteBlock(BuddyBlock* block) {
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