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

#include "dawn_native/ResourceHeap.h"
#include "dawn_native/ResourceHeapAllocator.h"

using namespace dawn_native;

class DirectResourceMemoryAllocatorTests : public testing::Test {
  public:
    class DummyResourceHeapAllocator {
      public:
        ResourceHeapBase* CreateHeap(uint64_t heapSize, int heapFlags = 0) {
            return new ResourceHeapBase();
        }
        void FreeHeap(ResourceHeapBase* heap) {
            delete heap;
        }
    };

    typedef DirectResourceMemoryAllocator<DummyResourceHeapAllocator> DirectResourceMemoryAllocator;
};

// Verify direct allocation with a single resource.
TEST_F(DirectResourceMemoryAllocatorTests, SingleResourceHeap) {
    DirectResourceMemoryAllocator allocator;

    constexpr uint64_t allocationSize = 5;  // NPOT allowed in direct allocation.
    ResourceMemoryAllocation allocation = allocator.Allocate(allocationSize);
    ASSERT_EQ(allocation.GetOffset(), 0u);

    ASSERT_TRUE(allocation.IsDirect());
    ASSERT_TRUE(allocation.GetResourceHeap() != nullptr);

    allocator.Deallocate(allocation);
}

// Verify direct allocation using multiple resources.
TEST_F(DirectResourceMemoryAllocatorTests, MultiResourceHeap) {
    DirectResourceMemoryAllocator allocator;

    // Allocate two 4-byte blocks on seperate resource heaps.
    ResourceMemoryAllocation allocationA = allocator.Allocate(5);
    ASSERT_EQ(allocationA.GetOffset(), 0u);
    ASSERT_TRUE(allocationA.IsDirect());
    ASSERT_TRUE(allocationA.GetResourceHeap() != nullptr);

    ResourceMemoryAllocation allocationB = allocator.Allocate(10);
    ASSERT_EQ(allocationB.GetOffset(), 0u);
    ASSERT_TRUE(allocationB.IsDirect());
    ASSERT_TRUE(allocationA.GetResourceHeap() != nullptr);

    // Both allocations must be backed by seperate resource heaps.
    ASSERT_TRUE(allocationB.GetResourceHeap() != allocationA.GetResourceHeap());

    allocator.Deallocate(allocationA);
    allocator.Deallocate(allocationB);
}