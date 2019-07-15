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

#include <gtest/gtest.h>

#include "dawn_native/ResourceHeap.h"
#include "dawn_native/ResourceHeapAllocator.h"

using namespace dawn_native;

// Mock objects used to test allocators without requiring a device.
class DummyResourceHeap : public ResourceHeapBase {
  public:
    DummyResourceHeap() = default;
    ~DummyResourceHeap() = default;

    MaybeError MapImpl() override {
        return DAWN_UNIMPLEMENTED_ERROR("Cannot map a dummy resource");
    }
    void UnmapImpl() override {
    }
};

class DummyResourceHeapAllocator {
  public:
    ResourceHeapBase* CreateHeap(size_t heapSize, int heapFlags = 0) {
        return new DummyResourceHeap();
    }
    void FreeHeap(ResourceHeapBase* heap) { /*unused*/
        delete heap;
    }
};

class ResourceAllocatorTests : public testing::Test {
  public:
    void CheckBlockValid(size_t offset, size_t expectedOffset) {
        ASSERT_EQ(offset, expectedOffset);
    }

    void CheckBlockInvalid(size_t offset) {
        ASSERT_EQ(offset, INVALID_OFFSET);
    }

    typedef BuddyResourceMemoryAllocator<DummyResourceHeapAllocator> BuddyResourceMemoryAllocator;
    typedef DirectResourceMemoryAllocator<DummyResourceHeapAllocator> DirectResourceMemoryAllocator;
};

class BuddyAllocatorTests : public ResourceAllocatorTests {};

TEST_F(BuddyAllocatorTests, SingleBlock) {
    // After one 32 byte allocation:
    //
    //  Level          --------------------------------
    //      0       32 |               A              |
    //                 --------------------------------
    //
    constexpr size_t sizeInBytes = 32;
    BuddyAllocator allocator(sizeInBytes);

    // Check that we cannot allocate a block too large.
    CheckBlockInvalid(allocator.Allocate(sizeInBytes * 2));

    // Allocate the block.
    size_t blockOffset = allocator.Allocate(sizeInBytes);
    CheckBlockValid(blockOffset, 0u);

    // Check that we are full.
    CheckBlockInvalid(allocator.Allocate(sizeInBytes));

    // Deallocate the block.
    allocator.Deallocate(blockOffset);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);
}

// Verify multiple allocations succeeds using a buddy allocator.
TEST_F(BuddyAllocatorTests, MultipleBlocks) {
    // Fill every level in the allocator (order-n = 2^n)
    const size_t maxSizeInBytes = (1 << 16);
    for (uint32_t order = 1; (1 << order) <= maxSizeInBytes; order++) {
        BuddyAllocator allocator(maxSizeInBytes);

        size_t blockSize = (1 << order);
        for (uint32_t blocki = 0; blocki < (maxSizeInBytes / blockSize); blocki++) {
            CheckBlockValid(allocator.Allocate(blockSize), blockSize * blocki);
        }
    }
}

// Verify that a single allocation succeeds using a buddy allocator.
TEST_F(BuddyAllocatorTests, SingleSplitBlock) {
    //  After one 8 byte allocation:
    //
    //  Level          --------------------------------
    //      0       32 |               S              |
    //                 --------------------------------
    //      1       16 |       S       |       F      |        S - split
    //                 --------------------------------        F - free
    //      2       8  |   A   |   F   |       |      |        A - allocated
    //                 --------------------------------
    //
    constexpr size_t sizeInBytes = 32;
    BuddyAllocator allocator(sizeInBytes);

    // Allocate block (splits two blocks).
    size_t blockOffset = allocator.Allocate(8);
    CheckBlockValid(blockOffset, 0u);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 2u);

    // Deallocate block (merges two blocks).
    allocator.Deallocate(blockOffset);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);

    // Check that we cannot allocate a block that is too large.
    CheckBlockInvalid(allocator.Allocate(sizeInBytes * 2));

    // Re-allocate the largest block allowed after merging.
    blockOffset = allocator.Allocate(sizeInBytes);
    CheckBlockValid(blockOffset, 0u);

    allocator.Deallocate(blockOffset);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);
}

// Verify that a multiple allocated blocks can be removed in the free-list.
TEST_F(BuddyAllocatorTests, MultipleSplitBlocks) {
    //  After four 16 byte allocations:
    //
    //  Level          --------------------------------
    //      0       32 |               S              |
    //                 --------------------------------
    //      1       16 |       S       |       S      |        S - split
    //                 --------------------------------        F - free
    //      2       8  |   Aa  |   Ab  |  Ac  |   Ad  |        A - allocated
    //                 --------------------------------
    //
    constexpr size_t sizeInBytes = 32;
    BuddyAllocator allocator(sizeInBytes);

    // Populates the free-list with four blocks at Level2.

    // Allocate "a" block (two splits).
    constexpr size_t blockSizeInBytes = 8;
    size_t blockOffsetA = allocator.Allocate(blockSizeInBytes);
    CheckBlockValid(blockOffsetA, 0u);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 2u);

    // Allocate "b" block.
    size_t blockOffsetB = allocator.Allocate(blockSizeInBytes);
    CheckBlockValid(blockOffsetB, blockSizeInBytes);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);

    // Allocate "c" block (three splits).
    size_t blockOffsetC = allocator.Allocate(blockSizeInBytes);
    CheckBlockValid(blockOffsetC, blockOffsetB + blockSizeInBytes);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);

    // Allocate "d" block.
    size_t blockOffsetD = allocator.Allocate(blockSizeInBytes);
    CheckBlockValid(blockOffsetD, blockOffsetC + blockSizeInBytes);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 0u);

    // Deallocate "d" block.
    // FreeList[Level2] = [BlockD] -> x
    allocator.Deallocate(blockOffsetD);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);

    // Deallocate "b" block.
    // FreeList[Level2] = [BlockB] -> [BlockD] -> x
    allocator.Deallocate(blockOffsetB);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 2u);

    // Deallocate "c" block (one merges).
    // FreeList[Level1] = [BlockCD] -> x
    // FreeList[Level2] = [BlockB] -> x
    allocator.Deallocate(blockOffsetC);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 2u);

    // Deallocate "a" block (two merges).
    // FreeList[Level0] = [BlockABCD] -> x
    allocator.Deallocate(blockOffsetA);
    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);
}

// Verify the buddy allocator can handle allocations of various sizes.
TEST_F(BuddyAllocatorTests, MultipleSplitBlockIncreasingSize) {
    //  After four L4-to-L1 byte then one L4 block allocations:
    //
    //  Level          -----------------------------------------------------------------
    //      0      512 |                               S                               |
    //                 -----------------------------------------------------------------
    //      1      256 |               S               |               A               |
    //                 -----------------------------------------------------------------
    //      2      128 |       S       |       A       |               |               |
    //                 -----------------------------------------------------------------
    //      3       64 |   S   |   A   |       |       |       |       |       |       |
    //                 -----------------------------------------------------------------
    //      4       32 | A | F |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
    //                 -----------------------------------------------------------------
    //
    constexpr size_t maxSizeInBytes = 512;
    BuddyAllocator allocator(maxSizeInBytes);

    CheckBlockValid(allocator.Allocate(32), 0);
    CheckBlockValid(allocator.Allocate(64), 64);
    CheckBlockValid(allocator.Allocate(128), 128);
    CheckBlockValid(allocator.Allocate(256), 256);

    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);

    // Fill in the last free block.
    CheckBlockValid(allocator.Allocate(32), 32);

    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 0u);

    // Check if we're full.
    CheckBlockInvalid(allocator.Allocate(32));
}

// Verify very small allocations using a larger allocator works correctly.
TEST_F(BuddyAllocatorTests, MultipleSplitBlocksVariousSizes) {
    //  After allocating four 1x64B then 2x32B blocks:
    //
    //  Level          -----------------------------------------------------------------
    //      0      512 |                               S                               |
    //                 -----------------------------------------------------------------
    //      1      256 |               S               |               S               |
    //                 -----------------------------------------------------------------
    //      2      128 |       S       |       S       |       S       |       F       |
    //                 -----------------------------------------------------------------
    //      3       64 |   A   |   S   |   A   |   A   |   S   |   A   |       |       |
    //                 -----------------------------------------------------------------
    //      4       32 |   |   | A | A |   |   |   |   | A | A |   |   |   |   |   |   |
    //                 -----------------------------------------------------------------
    //
    constexpr size_t maxSizeInBytes = 512;
    BuddyAllocator allocator(maxSizeInBytes);

    CheckBlockValid(allocator.Allocate(64), 0);
    CheckBlockValid(allocator.Allocate(32), 64);

    CheckBlockValid(allocator.Allocate(64), 128);
    CheckBlockValid(allocator.Allocate(32), 96);

    CheckBlockValid(allocator.Allocate(64), 192);
    CheckBlockValid(allocator.Allocate(32), 256);

    CheckBlockValid(allocator.Allocate(64), 320);
    CheckBlockValid(allocator.Allocate(32), 288);

    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 1u);
}

// Verify the buddy allocator can deal with bad fragmentation.
TEST_F(BuddyAllocatorTests, MultipleSplitBlocksInterleaved) {
    //  Allocate every leaf then de-allocate every other of those allocations.
    //
    //  Level          -----------------------------------------------------------------
    //      0      512 |                               S                               |
    //                 -----------------------------------------------------------------
    //      1      256 |               S               |               S               |
    //                 -----------------------------------------------------------------
    //      2      128 |       S       |       S       |        S       |        S     |
    //                 -----------------------------------------------------------------
    //      3       64 |   S   |   S   |   S   |   S   |   S   |   S   |   S   |   S   |
    //                 -----------------------------------------------------------------
    //      4       32 | A | F | A | F | A | F | A | F | A | F | A | F | A | F | A | F |
    //                 -----------------------------------------------------------------
    //
    constexpr size_t maxSizeInBytes = 512;
    BuddyAllocator allocator(maxSizeInBytes);

    // Allocate leaf blocks
    constexpr size_t minBlockSizeInBytes = 32;
    std::vector<size_t> blockOffsets;
    for (size_t i = 0; i < maxSizeInBytes / minBlockSizeInBytes; i++) {
        blockOffsets.push_back(allocator.Allocate(minBlockSizeInBytes));
    }

    // Free every other leaf block.
    for (size_t count = 1; count <= blockOffsets.size(); count++) {
        if (count % 2 == 0)
            allocator.Deallocate(blockOffsets[count - 1]);
    }

    ASSERT_EQ(allocator.GetNumOfFreeBlocks(), 8u);
}

class BuddyResourceMemoryAllocatorTests : public ResourceAllocatorTests {};

// Verify allocation of a few blocks over multple resources.
TEST_F(BuddyResourceMemoryAllocatorTests, SmallPool) {
    constexpr size_t resourceSizeInBytes = 128;
    constexpr size_t maxSizeInBytes = 512;
    BuddyResourceMemoryAllocator allocator(maxSizeInBytes, resourceSizeInBytes);

    // Cannot allocate block greater than allocator size.
    ResourceMemoryAllocation invalidAllocation1 = allocator.Allocate(maxSizeInBytes * 2);
    ASSERT_EQ(invalidAllocation1.GetOffset(), INVALID_OFFSET);

    // Cannot allocate block greater than resource size.
    ResourceMemoryAllocation invalidAllocation2 = allocator.Allocate(resourceSizeInBytes * 2);
    ASSERT_EQ(invalidAllocation2.GetOffset(), INVALID_OFFSET);

    // Allocate two blocks: implicitly backed by two resources.

    ResourceMemoryAllocation validAllocation1 = allocator.Allocate(resourceSizeInBytes);
    ASSERT_EQ(validAllocation1.GetOffset(), 0u);

    ResourceMemoryAllocation validAllocation2 = allocator.Allocate(resourceSizeInBytes);
    ASSERT_EQ(validAllocation2.GetOffset(), resourceSizeInBytes);

    ASSERT_EQ(allocator.GetResourceHeapCount(), 2u);

    // Deallocate both blocks: implicitly de-allocates both resources.
    allocator.Deallocate(validAllocation1);
    ASSERT_EQ(allocator.GetResourceHeapCount(), 1u);

    allocator.Deallocate(validAllocation2);
    ASSERT_EQ(allocator.GetResourceHeapCount(), 0u);
}

// Verify allocation of many blocks over multiple resources.
TEST_F(BuddyResourceMemoryAllocatorTests, LargePool) {
    constexpr size_t resourceSizeInBytes = 64 * 1024LL;            // 64KB
    constexpr size_t allocatorSizeInBytes = 16 * 1024LL * 1024LL;  // 16GB
    BuddyResourceMemoryAllocator allocator(allocatorSizeInBytes, resourceSizeInBytes);

    // Sub-allocate 1KB blocks in 64KB resources.
    constexpr size_t allocationSize = 1024;
    size_t offset = 0;
    std::vector<ResourceMemoryAllocation> allocations;
    for (size_t i = 0; i < allocatorSizeInBytes / allocationSize; i++) {
        ResourceMemoryAllocation allocation = allocator.Allocate(allocationSize);
        ASSERT_EQ(allocation.GetOffset(), offset);
        offset += allocationSize;

        allocations.push_back(allocation);
    }

    ASSERT_EQ(allocator.GetResourceHeapCount(), allocatorSizeInBytes / resourceSizeInBytes);

    // Deallocate allocation and resources.
    for (ResourceMemoryAllocation& allocation : allocations) {
        allocator.Deallocate(allocation);
    }

    ASSERT_EQ(allocator.GetResourceHeapCount(), 0u);
}

class DirectResourceMemoryAllocatorTests : public ResourceAllocatorTests {};

// Verify the direct allocator allocates correctly for a single resource.
TEST_F(DirectResourceMemoryAllocatorTests, SingleResource) {
    DirectResourceMemoryAllocator allocator;

    constexpr size_t allocationSize = 4;
    ResourceMemoryAllocation allocation = allocator.Allocate(allocationSize);
    CheckBlockValid(allocation.GetOffset(), 0u);

    ASSERT_TRUE(allocation.IsDirect());
    ASSERT_TRUE(allocation.GetResourceHeap() != nullptr);

    allocator.Deallocate(allocation);
}