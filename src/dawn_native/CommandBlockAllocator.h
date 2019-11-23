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

#ifndef DAWNNATIVE_COMMAND_BLOCK_ALLOCATOR_H_
#define DAWNNATIVE_COMMAND_BLOCK_ALLOCATOR_H_

#include <vector>

namespace dawn_native {

    class CommandBlock {
        friend class CommandBlockAllocator;

      public:
        CommandBlock(size_t size) : mTotalSize(size) {
        }
        inline size_t GetSize() const {
            return mTotalSize - sizeof(CommandBlock);
        }
        inline uint8_t* GetPointer() {
            return reinterpret_cast<uint8_t*>(this) + sizeof(CommandBlock);
        }
        inline CommandBlock* GetNext() {
            return mNext;
        }

      private:
        CommandBlock* mNext = nullptr;
        size_t mTotalSize;
    };
    static_assert(sizeof(CommandBlock) % alignof(uint32_t) == 0, "");

    class CommandBlockAllocator {
      public:
        CommandBlockAllocator();
        ~CommandBlockAllocator();

        CommandBlock* Allocate(size_t minimumSize, CommandBlock* previousBlock);
        void Deallocate(CommandBlock* block);

      private:
        CommandBlock mFreeBlockList {0};
        CommandBlock* mFreeBlockTail = &mFreeBlockList;

        size_t mLastBlockSize = 2048;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_COMMAND_BLOCK_ALLOCATOR_H_
