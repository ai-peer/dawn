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

#ifndef DAWNNATIVE_COMMAND_ALLOCATOR_H_
#define DAWNNATIVE_COMMAND_ALLOCATOR_H_

#include "dawn_native/CommandBlockAllocator.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dawn_native {

    // Allocation for command buffers should be fast. To avoid doing an allocation per command
    // or to avoid copying commands when reallocing, we use a linear allocator in a growing set
    // of large memory blocks. We also use this to have the format to be (u32 commandId, command),
    // so that iteration over the commands is easy.

    // Usage of the allocator and iterator:
    //     CommandAllocator allocator;
    //     DrawCommand* cmd = allocator.Allocate<DrawCommand>(CommandType::Draw);
    //     // Fill command
    //     // Repeat allocation and filling commands
    //
    //     CommandIterator commands(allocator);
    //     CommandType type;
    //     while(commands.NextCommandId(&type)) {
    //         switch(type) {
    //              case CommandType::Draw:
    //                  DrawCommand* draw = commands.NextCommand<DrawCommand>();
    //                  // Do the draw
    //                  break;
    //              // other cases
    //         }
    //     }

    // Note that you need to extract the commands from the CommandAllocator before destroying it
    // and must tell the CommandIterator when the allocated commands have been processed for
    // deletion.

    class CommandAllocator;
    class CommandBlock;
    class CommandBlockAllocator;

    // TODO(cwallez@chromium.org): prevent copy for both iterator and allocator
    class CommandIterator {
      public:
        CommandIterator();
        ~CommandIterator();

        CommandIterator(CommandIterator&& other);
        CommandIterator& operator=(CommandIterator&& other);

        CommandIterator(CommandAllocator&& allocator);
        CommandIterator& operator=(CommandAllocator&& allocator);

        template <typename E>
        bool NextCommandId(E* commandId) {
            return NextCommandId(reinterpret_cast<uint32_t*>(commandId));
        }
        template <typename T>
        T* NextCommand() {
            return static_cast<T*>(NextCommand(sizeof(T), alignof(T)));
        }
        template <typename T>
        T* NextData(size_t count) {
            return static_cast<T*>(NextData(sizeof(T) * count, alignof(T)));
        }

        // Needs to be called if iteration was stopped early.
        void Reset();

        void DataWasDestroyed();

      private:
        bool IsEmpty() const;

        bool NextCommandId(uint32_t* commandId);
        void* NextCommand(size_t commandSize, size_t commandAlignment);
        void* NextData(size_t dataSize, size_t dataAlignment);

        CommandBlockAllocator* mBlockAllocator = nullptr;
        CommandBlock* mFirstBlock = nullptr;
        CommandBlock* mCurrentBlock = nullptr;
        uint8_t* mCurrentPtr = nullptr;

        bool mDataWasDestroyed = false;

        // Used to avoid a special case for empty iterators.
        struct EndBlockAllocation {
            EndBlockAllocation();

            CommandBlock block;
            uint32_t data;
        } mEndBlockAllocation = {};
    };

    class CommandAllocator {
      public:
        CommandAllocator(CommandBlockAllocator* blockAllocator);
        ~CommandAllocator();

        template <typename T, typename E>
        T* Allocate(E commandId) {
            static_assert(sizeof(E) == sizeof(uint32_t), "");
            static_assert(alignof(E) == alignof(uint32_t), "");
            static_assert(alignof(T) <= kMaxSupportedAlignment, "");
            T* result = reinterpret_cast<T*>(
                Allocate(static_cast<uint32_t>(commandId), sizeof(T), alignof(T)));
            if (!result) {
                return nullptr;
            }
            new (result) T;
            return result;
        }

        template <typename T>
        T* AllocateData(size_t count) {
            static_assert(alignof(T) <= kMaxSupportedAlignment, "");
            T* result = reinterpret_cast<T*>(AllocateData(sizeof(T) * count, alignof(T)));
            if (!result) {
                return nullptr;
            }
            for (size_t i = 0; i < count; i++) {
                new (result + i) T;
            }
            return result;
        }

      private:
        // This is used for some internal computations and can be any power of two as long as code
        // using the CommandAllocator passes the static_asserts.
        static constexpr size_t kMaxSupportedAlignment = 8;

        friend CommandIterator;
        std::pair<CommandBlockAllocator*, CommandBlock*> AcquireBlocks();

        uint8_t* Allocate(uint32_t commandId, size_t commandSize, size_t commandAlignment);
        uint8_t* AllocateData(size_t dataSize, size_t dataAlignment);
        bool GetNewBlock(size_t minimumSize);

        // Pointers to the current range of allocation in the block. Guaranteed to allow for at
        // least one uint32_t if not nullptr, so that the special EndOfBlock command id can always
        // be written. Nullptr iff the blocks were moved out.
        uint8_t* mCurrentPtr = nullptr;
        uint8_t* mEndPtr = nullptr;

        // Data used for the block range at initialization so that the first call to Allocate sees
        // there is not enough space and calls GetNewBlock. This avoids having to special case the
        // initialization in Allocate.
        uint32_t mDummyEnum[1] = {0};

        CommandBlockAllocator* mBlockAllocator;
        CommandBlock* mFirstBlock = nullptr;
        CommandBlock* mCurrentBlock = nullptr;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_COMMAND_ALLOCATOR_H_
