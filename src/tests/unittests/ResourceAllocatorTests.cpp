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

#include "dawn_native/ResourceAllocator2.cpp"  // Required for allocator definitions.
#include "dawn_native/ResourceHeap.h"

using namespace dawn_native;

// Mock objects used to test allocators without requring a device.
class DummyResource : public ResourceHeapBase {
  public:
    DummyResource(size_t size) : ResourceHeapBase(size) {
    }
    ResultOrError<void*> Map() override {
        return DAWN_UNIMPLEMENTED_ERROR("Cannot map a dummy resource");
    }
    void Unmap() override { /*unused*/
    }
};

class DummyAllocator {
  public:
    std::unique_ptr<ResourceHeapBase> Allocate(size_t heapSize) {
        return std::make_unique<DummyResource>(heapSize);
    }
    void Deallocate(ResourceHeapBase* heap){/*unused*/};
};

class ResourceAllocatorTests : public testing::Test {
  public:
    void CheckBlockValid(HeapSubAllocationBlock block, size_t expectedSize, size_t expectedOffset) {
        ASSERT_EQ(block.GetSize(), expectedSize);
        ASSERT_EQ(block.GetOffset(), expectedOffset);
    }

    void CheckBlockInvalid(HeapSubAllocationBlock block) {
        ASSERT_EQ(block.GetSize(), 0u);
        ASSERT_EQ(block.GetOffset(), 0u);
    }
};

// Renamed for testing per allocator.
class DirectAllocatorTests : public ResourceAllocatorTests {};

// Verify the direct allocator succeeds by ensuring a single block is the whole resource.
TEST_F(DirectAllocatorTests, BasicDirectAllocatorTest) {
    DirectAllocator<HeapSubAllocationBlock, DummyAllocator> allocator;

    constexpr size_t sizeInBytes = 64;
    HeapSubAllocationBlock block = allocator.Allocate(sizeInBytes);

    CheckBlockValid(block, sizeInBytes, 0u);

    allocator.Deallocate(block);
}

typedef LinearPoolAllocator<BuddyBlock, DummyAllocator, BuddyAllocator<>> BuddyPoolAllocator;

class BuddyAllocatorTests : public ResourceAllocatorTests {};

TEST_F(BuddyAllocatorTests, SingleAllocation) {
    // After one 32 byte allocation:
    //
    //  Level          --------------------------------
    //      0       32 |               A              |
    //                 --------------------------------
    //
    constexpr size_t sizeInBytes = 32;
    BuddyPoolAllocator allocator(/*maxSize*/ sizeInBytes, /*resourceSize*/ sizeInBytes);

    // Check that we cannot allocate a block too large.
    CheckBlockInvalid(allocator.Allocate(sizeInBytes * 2));

    // Allocate the block.
    BuddyBlock block = allocator.Allocate(sizeInBytes);
    CheckBlockValid(block, sizeInBytes, 0u);

    // Check that we are full.
    CheckBlockInvalid(allocator.Allocate(sizeInBytes));

    // Deallocate the block.
    allocator.Deallocate(block);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 1u);
}

// Verify that a single allocation succeeds using a buddy allocator.
TEST_F(BuddyAllocatorTests, SingleSplitAllocation) {
    //  After one 8 byte allocation:
    //
    //  Level          -----------------
    //      0       32 |       S       |
    //                 -----------------
    //      1       16 |   S   |   F   |                       S - split
    //                 -----------------                       F - free
    //      2       8  | A | F |                               A - allocated
    //                 ---------
    //
    constexpr size_t sizeInBytes = 32;
    BuddyPoolAllocator allocator(/*maxSize*/ sizeInBytes, /*resourceSize*/ sizeInBytes);

    // Allocate block (splits two blocks).
    BuddyBlock block = allocator.Allocate(8);
    CheckBlockValid(block, 8u, 0u);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 2u);

    // Deallocate block (merges two blocks).
    allocator.Deallocate(block);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 1u);

    // Check that we cannot allocate a block that is too large.
    CheckBlockInvalid(allocator.Allocate(sizeInBytes * 2));

    // Re-allocate the largest block allowed after merging.
    CheckBlockValid(allocator.Allocate(sizeInBytes), sizeInBytes, 0u);
}

// Verify that a multiple split allocations succeeds using a buddy allocator.
TEST_F(BuddyAllocatorTests, MultipleSplitAllocation) {
    //  After four 16 byte allocations:
    //
    //  Level          -----------------
    //      0       32 |       S       |
    //                 -----------------
    //      1       16 |   S   |   S   |                       S - split
    //                 -----------------                       F - free
    //      2       8  |Aa |Ab |Ac |Ad |                       A - allocated
    //                 -----------------
    //
    constexpr size_t sizeInBytes = 32;
    BuddyPoolAllocator allocator(/*maxSize*/ sizeInBytes, /*resourceSize*/ sizeInBytes);

    // Allocate 1st block (two splits).
    constexpr size_t blockSizeInBytes = 8;
    BuddyBlock blockA = allocator.Allocate(blockSizeInBytes);
    CheckBlockValid(blockA, blockSizeInBytes, 0u);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 2u);

    // Allocate 2nd block.
    BuddyBlock blockB = allocator.Allocate(blockSizeInBytes);
    CheckBlockValid(blockB, blockSizeInBytes, blockSizeInBytes);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 1u);

    // Allocate 3rd block (three splits).
    BuddyBlock blockC = allocator.Allocate(blockSizeInBytes);
    CheckBlockValid(blockC, blockSizeInBytes, blockB.GetOffset() + blockSizeInBytes);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 1u);

    // Allocate 4th block.
    BuddyBlock blockD = allocator.Allocate(blockSizeInBytes);
    CheckBlockValid(blockD, blockSizeInBytes, blockC.GetOffset() + blockSizeInBytes);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 0u);

    // Re-populate the free-list upon de-allocating.

    // Deallocate 4th block.
    allocator.Deallocate(blockD);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 1u);

    // Deallocate 2nd block.
    allocator.Deallocate(blockB);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 2u);

    // Deallocate 3rd block (one merges).
    allocator.Deallocate(blockC);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 2u);

    // Deallocate 1st block (two merges).
    allocator.Deallocate(blockA);
    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 1u);
}

// Verify multiple allocations succeeds using a buddy allocator.
TEST_F(BuddyAllocatorTests, MultipleSplitAllocationEqualSize) {
    //  After sixteen 32 byte allocations:
    //
    //  Level          -----------------------------------------------------------------
    //      0      512 |                               S                               |
    //                 -----------------------------------------------------------------
    //      1      256 |               S               |               S               |
    //                 -----------------------------------------------------------------
    //      2      128 |       S       |       S       |       S       |       S       |
    //                 -----------------------------------------------------------------
    //      3       64 |   S   |   S   |   S   |   S   |   S   |   S   |   S   |   S   |
    //                 -----------------------------------------------------------------
    //      4       32 | A | A | A | A | A | A | A | A | A | A | A | A | A | A | A | A |
    //                 -----------------------------------------------------------------
    //
    constexpr size_t maxSizeInBytes = 512;
    BuddyPoolAllocator allocator(
        /*allocatorSize*/ maxSizeInBytes, /*resourceSize*/ maxSizeInBytes);

    // Fill-up with 32B blocks.
    size_t allocatedBlockSize = 32u;
    for (size_t i = 0; i < maxSizeInBytes / allocatedBlockSize; i++) {
        BuddyBlock block = allocator.Allocate(allocatedBlockSize);
        CheckBlockValid(block, allocatedBlockSize, allocatedBlockSize * i);
    }

    // Check if we're full.
    CheckBlockInvalid(allocator.Allocate(allocatedBlockSize));
}

// Verify the buddy allocator can handle allocations of various sizes.
TEST_F(BuddyAllocatorTests, MultipleSplitAllocationIncreasingSize) {
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
    BuddyPoolAllocator allocator(
        /*allocatorSize*/ maxSizeInBytes, /*resourceSize*/ maxSizeInBytes);

    CheckBlockValid(allocator.Allocate(32), 32, 0);
    CheckBlockValid(allocator.Allocate(64), 64, 64);
    CheckBlockValid(allocator.Allocate(128), 128, 128);
    CheckBlockValid(allocator.Allocate(256), 256, 256);

    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 1u);

    // Fill in the last free block.
    CheckBlockValid(allocator.Allocate(32), 32, 32);

    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 0u);

    // Check if we're full.
    CheckBlockInvalid(allocator.Allocate(32));
}

// Verify very small allocations using a larger allocator works correctly.
TEST_F(BuddyAllocatorTests, MultipleSplitAllocationsVariousSizes) {
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
    BuddyPoolAllocator allocator(
        /*allocatorSize*/ maxSizeInBytes, /*resourceSize*/ maxSizeInBytes);

    CheckBlockValid(allocator.Allocate(64), 64, 0);
    CheckBlockValid(allocator.Allocate(32), 32, 64);

    CheckBlockValid(allocator.Allocate(64), 64, 128);
    CheckBlockValid(allocator.Allocate(32), 32, 96);

    CheckBlockValid(allocator.Allocate(64), 64, 192);
    CheckBlockValid(allocator.Allocate(32), 32, 256);

    CheckBlockValid(allocator.Allocate(64), 64, 320);
    CheckBlockValid(allocator.Allocate(32), 32, 288);

    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 1u);
}

// Verify the buddy allocator can deal with bad fragmentation.
TEST_F(BuddyAllocatorTests, MultipleSplitAllocationsInterleavedSameSize) {
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
    BuddyPoolAllocator allocator(
        /*allocatorSize*/ maxSizeInBytes, /*resourceSize*/ maxSizeInBytes);

    // Allocate leaf blocks
    constexpr size_t minBlockSizeInBytes = 32;
    std::vector<BuddyBlock> blocks;
    for (size_t i = 0; i < maxSizeInBytes / minBlockSizeInBytes; i++) {
        blocks.push_back(allocator.Allocate(minBlockSizeInBytes));
    }

    // Free every other leaf block.
    for (size_t count = 1; count <= blocks.size(); count++) {
        if (count % 2 == 0)
            allocator.Deallocate(blocks[count - 1]);
    }

    ASSERT_EQ(allocator.GetBlockAllocator().GetNumOfFreeBlocks(), 8u);
}

// Verify the buddy allocator can pool large allocations over multiple resources.
TEST_F(BuddyAllocatorTests, SmallAllocationPool) {
    constexpr size_t resourceSizeInBytes = 128;
    constexpr size_t maxSizeInBytes = 512;
    BuddyPoolAllocator allocator(
        /*maxSize*/ maxSizeInBytes, /*resourceSize*/ resourceSizeInBytes);

    // Cannot allocate block greater than allocator size.
    CheckBlockInvalid(allocator.Allocate(maxSizeInBytes * 2));

    // Cannot allocate block greater than resource size.
    CheckBlockInvalid(allocator.Allocate(resourceSizeInBytes * 2));

    // Allocate two blocks: implicitly backed by two resources.

    BuddyBlock block1 = allocator.Allocate(resourceSizeInBytes);
    CheckBlockValid(block1, resourceSizeInBytes, 0);

    BuddyBlock block2 = allocator.Allocate(resourceSizeInBytes);
    CheckBlockValid(block2, resourceSizeInBytes, resourceSizeInBytes);

    ASSERT_EQ(allocator.GetResourceCount(), 2u);

    // Deallocate both blocks: implicitly de-allocates both resources.
    allocator.Deallocate(block1);
    ASSERT_EQ(allocator.GetResourceCount(), 1u);

    allocator.Deallocate(block2);
    ASSERT_EQ(allocator.GetResourceCount(), 0u);
}

// Verify the buddy allocator can pool small allocations over multiple resources.
TEST_F(BuddyAllocatorTests, LargeAllocationPool) {
    constexpr size_t resourceSizeInBytes = 8;
    constexpr size_t allocatorSizeInBytes = 512;
    BuddyPoolAllocator allocator(
        /*maxSize*/ allocatorSizeInBytes, /*resourceSize*/ resourceSizeInBytes);

    constexpr size_t blockSize = 4;
    size_t offset = 0;
    for (size_t i = 0; i < allocatorSizeInBytes / blockSize; i++) {
        BuddyBlock block = allocator.Allocate(blockSize);
        CheckBlockValid(block, blockSize, offset);
        offset += block.GetSize();
    }

    ASSERT_EQ(allocator.GetResourceCount(), allocatorSizeInBytes / resourceSizeInBytes);
}
