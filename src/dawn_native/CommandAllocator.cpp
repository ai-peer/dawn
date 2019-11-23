// Copyright 2017 The Dawn Authors
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

#include "dawn_native/CommandAllocator.h"

#include "common/Assert.h"
#include "common/Math.h"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace dawn_native {

    // TODO(cwallez@chromium.org): figure out a way to have more type safety for the iterator

    CommandIterator::CommandIterator() {
        Reset();
    }

    CommandIterator::~CommandIterator() {
        ASSERT(mDataWasDestroyed);

        if (!IsEmpty()) {
            ASSERT(mBlockAllocator != nullptr);
            CommandBlock* current = mFirstBlock;
            while (current != nullptr) {
                mBlockAllocator->Deallocate(current);
                current = current->GetNext();
            }
        }
    }

    CommandIterator::CommandIterator(CommandIterator&& other) {
        if (!other.IsEmpty()) {
            mBlockAllocator = other.mBlockAllocator;
            mFirstBlock = other.mFirstBlock;

            other.mBlockAllocator = nullptr;
            other.mFirstBlock = nullptr;

            other.Reset();
        }
        other.DataWasDestroyed();
        Reset();
    }

    CommandIterator& CommandIterator::operator=(CommandIterator&& other) {
        if (!other.IsEmpty()) {
            mBlockAllocator = other.mBlockAllocator;
            mFirstBlock = other.mFirstBlock;

            other.mBlockAllocator = nullptr;
            other.mFirstBlock = nullptr;

            other.Reset();
        } else {
            mBlockAllocator = nullptr;
            mFirstBlock = nullptr;
        }
        other.DataWasDestroyed();
        Reset();
        return *this;
    }

    CommandIterator::CommandIterator(CommandAllocator&& allocator) {
        std::tie(mBlockAllocator, mFirstBlock) = allocator.AcquireBlocks();
        Reset();
    }

    CommandIterator& CommandIterator::operator=(CommandAllocator&& allocator) {
        std::tie(mBlockAllocator, mFirstBlock) = allocator.AcquireBlocks();
        Reset();
        return *this;
    }

    void CommandIterator::Reset() {
        if (mFirstBlock == nullptr) {
            mCurrentBlock = &mEndBlockAllocation.block;
            mCurrentPtr = mCurrentBlock->GetPointer();
        } else {
            ASSERT(mBlockAllocator != nullptr);
            mCurrentBlock = mFirstBlock;
            mCurrentPtr = AlignPtr(mCurrentBlock->GetPointer(), alignof(uint32_t));
        }
    }

    void CommandIterator::DataWasDestroyed() {
        mDataWasDestroyed = true;
    }

    bool CommandIterator::IsEmpty() const {
        return mFirstBlock == nullptr;
    }

    // Potential TODO(cwallez@chromium.org):
    //  - Host the size and pointer to next block in the block itself to avoid having an allocation
    //    in the vector
    //  - Assume T's alignof is, say 64bits, static assert it, and make commandAlignment a constant
    //    in Allocate
    //  - Be able to optimize allocation to one block, for command buffers expected to live long to
    //    avoid cache misses
    //  - Better block allocation, maybe have Dawn API to say command buffer is going to have size
    //    close to another

    CommandAllocator::CommandAllocator(CommandBlockAllocator* blockAllocator)
        : mCurrentPtr(reinterpret_cast<uint8_t*>(&mDummyEnum[0])),
          mEndPtr(reinterpret_cast<uint8_t*>(&mDummyEnum[1])),
          mBlockAllocator(blockAllocator) {
    }

    CommandAllocator::~CommandAllocator() {
        ASSERT(mFirstBlock == nullptr);
    }

    std::pair<CommandBlockAllocator*, CommandBlock*> CommandAllocator::AcquireBlocks() {
        ASSERT(mCurrentPtr != nullptr && mEndPtr != nullptr);
        ASSERT(IsPtrAligned(mCurrentPtr, alignof(uint32_t)));
        ASSERT(mCurrentPtr + sizeof(uint32_t) <= mEndPtr);
        *reinterpret_cast<uint32_t*>(mCurrentPtr) = EndOfBlock;

        auto result = std::make_pair(mBlockAllocator, mFirstBlock);
        mFirstBlock = nullptr;
        mCurrentBlock = nullptr;
        mCurrentPtr = reinterpret_cast<uint8_t*>(&mDummyEnum[0]);
        mEndPtr = reinterpret_cast<uint8_t*>(&mDummyEnum[1]);

        return result;
    }

    bool CommandAllocator::GetNewBlock(size_t minimumSize) {
        mCurrentBlock = mBlockAllocator->Allocate(minimumSize, mCurrentBlock);
        if (mFirstBlock == nullptr) {
            mFirstBlock = mCurrentBlock;
        }

        if (DAWN_UNLIKELY(mCurrentBlock == nullptr)) {
            return false;
        }

        mCurrentPtr = AlignPtr(mCurrentBlock->GetPointer(), alignof(uint32_t));
        mEndPtr = mCurrentBlock->GetPointer() + mCurrentBlock->GetSize();
        return true;
    }

}  // namespace dawn_native
