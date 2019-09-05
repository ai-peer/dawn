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

#include <gtest/gtest.h>

#include "dawn_native/BuddyResourceHeapAllocator.h"

using namespace dawn_native;

// Dummy allocator that allocates a non-existent heap for testing purposes.
class DummyResourceMemoryAllocator : public ResourceMemoryAllocator {
  public:
    ResultOrError<ResourceMemoryAllocation> Allocate(uint64_t size, int memoryFlags = 0) override {
        return ResourceMemoryAllocation{kInvalidOffset, nullptr, AllocationMethod::kSubAllocated};
    }
    void Deallocate(ResourceMemoryAllocation& allocation) override { /*unused*/
    }
};

class DummyBuddyResourceAllocator {
  public:
    DummyBuddyResourceAllocator(uint64_t maxBlockSize, uint64_t heapSize)
        : mAllocator(maxBlockSize, heapSize, new DummyResourceMemoryAllocator()) {
    }

    ResourceMemoryAllocation Allocate(uint64_t size, uint64_t alignment = 1) {
        ResultOrError<ResourceMemoryAllocation> result = mAllocator.Allocate(size, alignment);
        return (result.IsSuccess()) ? result.AcquireSuccess() : ResourceMemoryAllocation{};
    }

    void Deallocate(ResourceMemoryAllocation& allocation) {
        mAllocator.Deallocate(allocation);
    }

    uint64_t ComputeTotalNumOfHeapsForTesting() const {
        return mAllocator.ComputeTotalNumOfHeapsForTesting();
    }

  private:
    BuddyResourceHeapAllocator mAllocator;
};

// Verify a single resource allocation in a single heap.
TEST(BuddyResourceHeapAllocatorTests, SingleHeap) {
    // After one 128 byte resource allocation:
    //
    // max block size -> ---------------------------
    //                   |          A1/H0          |       Hi - Heap at index i
    // max heap size  -> ---------------------------       An - Resource allocation n
    //
    constexpr uint64_t heapSize = 128;
    constexpr uint64_t maxBlockSize = heapSize;
    DummyBuddyResourceAllocator allocator(maxBlockSize, heapSize);

    // Cannot allocate greater than heap size.
    ResourceMemoryAllocation invalidAllocation = allocator.Allocate(heapSize * 2);
    ASSERT_EQ(invalidAllocation.GetAllocationMethod(), AllocationMethod::kInvalid);

    // Allocate one 128 byte allocation (same size as heap).
    ResourceMemoryAllocation allocation1 = allocator.Allocate(128);
    ASSERT_EQ(allocation1.GetOffset(), 0u);
    ASSERT_EQ(allocation1.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);

    // Cannot allocate when allocator is full.
    invalidAllocation = allocator.Allocate(128);
    ASSERT_EQ(invalidAllocation.GetAllocationMethod(), AllocationMethod::kInvalid);

    allocator.Deallocate(allocation1);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 0u);
}

// Verify that multiple allocation are created in separate heaps.
TEST(BuddyResourceHeapAllocatorTests, MultipleHeaps) {
    // After two 128 byte allocations with 128 byte heaps.
    //
    // max block size -> ---------------------------
    //                   |                         |       Hi - Heap at index i
    // max heap size  -> ---------------------------       An - Resource allocation n
    //                   |   A1/H0    |    A2/H1   |
    //                   ---------------------------
    //
    constexpr uint64_t maxBlockSize = 256;
    constexpr uint64_t heapSize = 128;
    DummyBuddyResourceAllocator allocator(maxBlockSize, heapSize);

    // Cannot allocate greater than heap size.
    ResourceMemoryAllocation invalidAllocation = allocator.Allocate(heapSize * 2);
    ASSERT_EQ(invalidAllocation.GetAllocationMethod(), AllocationMethod::kInvalid);

    // Cannot allocate greater than max block size.
    invalidAllocation = allocator.Allocate(maxBlockSize * 2);
    ASSERT_EQ(invalidAllocation.GetAllocationMethod(), AllocationMethod::kInvalid);

    // Allocate two 128 byte allocations.
    ResourceMemoryAllocation allocation1 = allocator.Allocate(heapSize);
    ASSERT_EQ(allocation1.GetOffset(), 0u);
    ASSERT_EQ(allocation1.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    // First allocation creates first heap.
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);

    ResourceMemoryAllocation allocation2 = allocator.Allocate(heapSize);
    ASSERT_EQ(allocation2.GetOffset(), heapSize);
    ASSERT_EQ(allocation2.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    // Second allocation creates second heap.
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 2u);

    // Deallocate both allocations
    allocator.Deallocate(allocation1);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);  // Released H0

    allocator.Deallocate(allocation2);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 0u);  // Released H1
}

// Verify multiple sub-allocations can re-use heaps.
TEST(BuddyResourceHeapAllocatorTests, MultipleSplitHeaps) {
    // After two 64 byte allocations with 128 byte heaps.
    //
    // max block size -> ---------------------------
    //                   |                         |       Hi - Heap at index i
    // max heap size  -> ---------------------------       An - Resource allocation n
    //                   |     H0     |     H1     |
    //                   ---------------------------
    //                   |  A1 |  A2  |  A3 |      |
    //                   ---------------------------
    //
    constexpr uint64_t maxBlockSize = 256;
    constexpr uint64_t heapSize = 128;
    DummyBuddyResourceAllocator allocator(maxBlockSize, heapSize);

    // Allocate two 64 byte sub-allocations.
    ResourceMemoryAllocation allocation1 = allocator.Allocate(heapSize / 2);
    ASSERT_EQ(allocation1.GetOffset(), 0u);
    ASSERT_EQ(allocation1.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    // First sub-allocation creates first heap.
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);

    ResourceMemoryAllocation allocation2 = allocator.Allocate(heapSize / 2);
    ASSERT_EQ(allocation2.GetOffset(), heapSize / 2);
    ASSERT_EQ(allocation2.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    // Second allocation re-uses first heap.
    ASSERT_EQ(allocation1.GetResourceHeap(), allocation2.GetResourceHeap());
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);

    ResourceMemoryAllocation allocation3 = allocator.Allocate(heapSize / 2);
    ASSERT_EQ(allocation3.GetOffset(), heapSize);
    ASSERT_EQ(allocation3.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    // Third allocation creates second heap.
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 2u);

    // Deallocate all allocations in reverse order.
    allocator.Deallocate(allocation1);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(),
              2u);  // A2 pins H0.

    allocator.Deallocate(allocation2);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);  // Released H0

    allocator.Deallocate(allocation3);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 0u);  // Released H1
}

// Verify resource sub-allocation of various sizes over multiple heaps.
TEST(BuddyResourceHeapAllocatorTests, MultiplSplitHeapsVariableSizes) {
    // After three 64 byte allocations and two 128 byte allocations.
    //
    // max block size -> -------------------------------------------------------
    //                   |                                                     |
    //                   -------------------------------------------------------
    //                   |                         |                           |
    // max heap size  -> -------------------------------------------------------
    //                   |     H0     |    A3/H1   |      H2     |    A5/H3    |
    //                   -------------------------------------------------------
    //                   |  A1 |  A2  |            |   A4  |     |             |
    //                   -------------------------------------------------------
    //
    constexpr uint64_t heapSize = 128;
    constexpr uint64_t maxBlockSize = 512;
    DummyBuddyResourceAllocator allocator(maxBlockSize, heapSize);

    // Allocate two 64-byte allocations.
    ResourceMemoryAllocation allocation1 = allocator.Allocate(64);
    ASSERT_EQ(allocation1.GetOffset(), 0u);
    ASSERT_EQ(allocation1.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ResourceMemoryAllocation allocation2 = allocator.Allocate(64);
    ASSERT_EQ(allocation2.GetOffset(), 64u);
    ASSERT_EQ(allocation2.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    // A1 and A2 share H0
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);
    ASSERT_EQ(allocation1.GetResourceHeap(), allocation2.GetResourceHeap());

    ResourceMemoryAllocation allocation3 = allocator.Allocate(128);
    ASSERT_EQ(allocation3.GetOffset(), 128u);
    ASSERT_EQ(allocation3.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    // A3 creates and fully occupies a new heap.
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 2u);

    ResourceMemoryAllocation allocation4 = allocator.Allocate(64);
    ASSERT_EQ(allocation4.GetOffset(), 256u);
    ASSERT_EQ(allocation4.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 3u);

    // R5 size forms 64 byte hole after R4.
    ResourceMemoryAllocation allocation5 = allocator.Allocate(128);
    ASSERT_EQ(allocation5.GetOffset(), 384u);
    ASSERT_EQ(allocation5.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 4u);

    // Deallocate allocations in staggered order.
    allocator.Deallocate(allocation1);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 4u);  // A2 pins H0

    allocator.Deallocate(allocation5);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 3u);  // Released H3

    allocator.Deallocate(allocation2);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 2u);  // Released H0

    allocator.Deallocate(allocation4);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);  // Released H2

    allocator.Deallocate(allocation3);
    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 0u);  // Released H1
}

// Verify resource sub-allocation of same sizes with various alignments.
TEST(BuddyResourceHeapAllocatorTests, SameSizeVariousAlignment) {
    // After three 64 byte allocations and one 128 byte allocations.
    //
    // max block size -> -------------------------------------------------------
    //                   |                                                     |
    //                   -------------------------------------------------------
    //                   |                         |                           |
    // max heap size  -> -------------------------------------------------------
    //                   |     H0     |     H1     |    Ac/H2   |     H3       |
    //                   -------------------------------------------------------
    //                   |  Aa |      |  Ab  |     |            |  Ad  |       |
    //                   -------------------------------------------------------
    //
    constexpr uint64_t heapSize = 128;
    constexpr uint64_t maxBlockSize = 512;
    DummyBuddyResourceAllocator allocator(maxBlockSize, heapSize);

    ResourceMemoryAllocation allocation1 = allocator.Allocate(64, 128);
    ASSERT_EQ(allocation1.GetOffset(), 0u);
    ASSERT_EQ(allocation1.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);

    ResourceMemoryAllocation allocationB = allocator.Allocate(64, 128);
    ASSERT_EQ(allocationB.GetOffset(), 128u);
    ASSERT_EQ(allocationB.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 2u);

    ResourceMemoryAllocation allocationC = allocator.Allocate(128, 128);
    ASSERT_EQ(allocationC.GetOffset(), 256u);
    ASSERT_EQ(allocationC.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 3u);

    ResourceMemoryAllocation allocationD = allocator.Allocate(64, 128);
    ASSERT_EQ(allocationD.GetOffset(), 384u);
    ASSERT_EQ(allocationD.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 4u);

    ResourceMemoryAllocation invalidAllocation = allocator.Allocate(128);
    ASSERT_EQ(invalidAllocation.GetAllocationMethod(), AllocationMethod::kInvalid);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 4u);
}

// Verify resource sub-allocation of various sizes with same alignments.
TEST(BuddyResourceHeapAllocatorTests, VariousSizeSameAlignment) {
    // After two 64 byte and two 128 byte resource allocations:
    //
    // max block size -> -------------------------------------------------------
    //                   |                                                     |
    //                   -------------------------------------------------------
    //                   |                         |                           |
    // max heap size  -> -------------------------------------------------------
    //                   |     H0     |    Ac/H1   |    Ad/H2   |              |
    //                   -------------------------------------------------------
    //                   |  Aa |  Ab  |            |            |              |
    //                   -------------------------------------------------------
    //
    constexpr uint64_t heapSize = 128;
    constexpr uint64_t maxBlockSize = 512;
    DummyBuddyResourceAllocator allocator(maxBlockSize, heapSize);

    constexpr uint64_t alignment = 64;
    ResourceMemoryAllocation allocation1 = allocator.Allocate(64, alignment);
    ASSERT_EQ(allocation1.GetOffset(), 0u);
    ASSERT_EQ(allocation1.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);

    ResourceMemoryAllocation allocationB = allocator.Allocate(64, alignment);
    ASSERT_EQ(allocationB.GetOffset(), 64u);
    ASSERT_EQ(allocationB.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 1u);  // Reuses H0

    ResourceMemoryAllocation allocationC = allocator.Allocate(128, alignment);
    ASSERT_EQ(allocationC.GetOffset(), 128u);
    ASSERT_EQ(allocationC.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 2u);

    ResourceMemoryAllocation allocationD = allocator.Allocate(128, alignment);
    ASSERT_EQ(allocationD.GetOffset(), 256u);
    ASSERT_EQ(allocationD.GetAllocationMethod(), AllocationMethod::kSubAllocated);

    ASSERT_EQ(allocator.ComputeTotalNumOfHeapsForTesting(), 3u);
}