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

#ifndef DAWNNATIVE_BUDDYALLOCATOR_H_
#define DAWNNATIVE_BUDDYALLOCATOR_H_

#include <vector>
#include <cstddef>

namespace dawn_native {

    static constexpr size_t INVALID_OFFSET = std::numeric_limits<size_t>::max();

    enum class BlockState { Free, Split, Allocated };

    struct BuddyBlock {
        BuddyBlock(size_t size, size_t offset)
            : mOffset(offset), mSize(size), mState(BlockState::Free) {
        }

        size_t mOffset = INVALID_OFFSET;
        size_t mSize = 0;

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

    // Buddy block allocator uses the buddy system to sub-divide a memory address range into
    // binary-sized blocks. Internally, it manages a freelist to track free blocks in a binary tree.
    class BuddyAllocator {
        public:
        BuddyAllocator() = default;
        BuddyAllocator(size_t maxSize);
        ~BuddyAllocator();

        // Required methods.
        size_t Allocate(size_t allocationSize);
        void Deallocate(size_t offset);

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
            // TODO(bryan.bernhart@intel.com): Track the tail.
        };

        BuddyBlock* mRoot = nullptr;  // Used to deallocate non-free blocks.

        size_t mMaxBlockSize = 0;

        std::vector<BlockList> mFreeLists;  // List of linked-lists of blocks.
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_BUDDYALLOCATOR_H_