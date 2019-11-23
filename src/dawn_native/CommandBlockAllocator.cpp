// Copyright 2019 The Dawn Authors
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

#include "dawn_native/CommandBlockAllocator.h"

namespace dawn_native {

    CommandBlockAllocator::CommandBlockAllocator() {}
    CommandBlockAllocator::~CommandBlockAllocator() {
        while (mFreeBlockList.mNext != nullptr) {
            CommandBlock* ptr = mFreeBlockList.mNext;
            mFreeBlockList.mNext = ptr->mNext;
            free(ptr);
        }
    }

    CommandBlock* CommandBlockAllocator::Allocate(size_t minimumSize, CommandBlock* previousBlock) {
        CommandBlock* block = nullptr;
        while (block == nullptr && mFreeBlockList.mNext != nullptr) {
            const size_t size = mFreeBlockList.mNext->GetSize();
            if (size >= minimumSize) {
                block = mFreeBlockList.mNext;

                // Set the head to the next block
                mFreeBlockList.mNext = block->mNext;
                if (block == mFreeBlockTail) {
                    mFreeBlockTail = &mFreeBlockList;
                }

                block->mNext = nullptr;
            } else {
                CommandBlock* ptr = mFreeBlockList.mNext;
                mFreeBlockList.mNext = ptr->mNext;
                free(ptr);
            }
        }

        if (block == nullptr) {
            static constexpr size_t maxBlockGrowthSize = 64 * 1024;
            mLastBlockSize =
                std::max(minimumSize, std::min(mLastBlockSize * 2, maxBlockGrowthSize));

            size_t size = mLastBlockSize + sizeof(CommandBlock);
            void* ptr = malloc(size);

            block = new (ptr) CommandBlock(size);
        }

        if (previousBlock != nullptr) {
            previousBlock->mNext = block;
        }

        return block;
    }

    void CommandBlockAllocator::Deallocate(CommandBlock* block) {
        mFreeBlockTail->mNext = block;
        mFreeBlockTail = block;
    }

}  // namespace dawn_native
