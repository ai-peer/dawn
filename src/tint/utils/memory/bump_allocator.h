// Copyright 2023 The Tint Authors.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_TINT_UTILS_MEMORY_BUMP_ALLOCATOR_H_
#define SRC_TINT_UTILS_MEMORY_BUMP_ALLOCATOR_H_

#include <array>
#include <cstring>
#include <utility>

#include "src/tint/utils/math/math.h"
#include "src/tint/utils/memory/bitcast.h"

namespace tint {

/// A allocator for chunks of memory. The memory is owned by the BumpAllocator. When the
/// BumpAllocator is freed all of the allocated memory is freed.
class BumpAllocator {
    static constexpr size_t kBlockSize = 64 * 1024;

    /// Block is linked list of memory blocks.
    /// Blocks are allocated out of heap memory.
    struct Block {
        uint8_t data[kBlockSize];
        Block* next;
    };

  public:
    /// Constructor
    BumpAllocator() = default;

    /// Move constructor
    /// @param rhs the BumpAllocator to move
    BumpAllocator(BumpAllocator&& rhs) { std::swap(data, rhs.data); }

    /// Move assignment operator
    /// @param rhs the BumpAllocator to move
    /// @return this BumpAllocator
    BumpAllocator& operator=(BumpAllocator&& rhs) {
        if (this != &rhs) {
            Reset();
            std::swap(data, rhs.data);
        }
        return *this;
    }

    /// Destructor
    ~BumpAllocator() { Reset(); }

    /// Allocates @p size_in_bytes from the current block, or from a newly allocated block if the
    /// current block is full.
    /// @param size_in_bytes the number of bytes to allocate
    /// @returns the pointer to the allocated memory or |nullptr| if the memory can not be allocated
    char* Allocate(size_t size_in_bytes) {
        auto& block = data.block;
        if (block.current_offset + size_in_bytes > kBlockSize) {
            // Allocate a new block from the heap
            auto* prev_block = block.current;
            block.current = new Block;
            if (!block.current) {
                return nullptr;  // out of memory
            }
            block.current->next = nullptr;
            block.current_offset = 0;
            if (prev_block) {
                prev_block->next = block.current;
            } else {
                block.root = block.current;
            }
        }

        auto* base = &block.current->data[0];
        auto* ptr = reinterpret_cast<char*>(base + block.current_offset);
        block.current_offset += size_in_bytes;
        data.count++;
        return ptr;
    }

    /// Frees all allocations from the allocator.
    void Reset() {
        auto* block = data.block.root;
        while (block != nullptr) {
            auto* next = block->next;
            delete block;
            block = next;
        }
        data = {};
    }

    /// @returns the total number of allocations
    size_t Count() const { return data.count; }

  private:
    BumpAllocator(const BumpAllocator&) = delete;
    BumpAllocator& operator=(const BumpAllocator&) = delete;

    struct {
        struct {
            /// The root block of the block linked list
            Block* root = nullptr;
            /// The current (end) block of the blocked linked list.
            /// New allocations come from this block
            Block* current = nullptr;
            /// The byte offset in #current for the next allocation.
            /// Initialized with kBlockSize so that the first allocation triggers a block
            /// allocation.
            size_t current_offset = kBlockSize;
        } block;

        size_t count = 0;
    } data;
};

}  // namespace tint

#endif  // SRC_TINT_UTILS_MEMORY_BUMP_ALLOCATOR_H_
