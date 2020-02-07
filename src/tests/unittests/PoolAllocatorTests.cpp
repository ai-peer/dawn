// Copyright 2020 The Dawn Authors
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

#include "common/Math.h"
#include "common/PoolAllocator.h"

namespace {

    struct alignas(8) AlignmentSmall : public PlacementAllocated {
        AlignmentSmall(int value) : value(value) {
        }

        int value;
    };

    struct alignas(256) AlignmentLarge : public PlacementAllocated {
        AlignmentLarge(int value) : value(value) {
        }

        int value;
    };
}  // namespace

TEST(PoolAllocatorTests, AllocateSequential) {
    {
        PoolAllocator<AlignmentSmall> allocator(5);

        std::vector<AlignmentSmall*> objects;
        for (int i = 0; i < 10; ++i) {
            auto* ptr = allocator.Allocate(i);
            EXPECT_TRUE(std::find(objects.begin(), objects.end(), ptr) == objects.end());
            objects.push_back(ptr);
        }

        for (int i = 0; i < 10; ++i) {
            // Check that the value is correct and hasn't been trampled.
            EXPECT_EQ(objects[i]->value, i);

            // Check that the alignment is correct.
            EXPECT_TRUE(IsPtrAligned(objects[i], alignof(AlignmentSmall)));
        }
    }
    {
        PoolAllocator<AlignmentLarge> allocator(9);

        std::vector<AlignmentLarge*> objects;
        for (int i = 0; i < 21; ++i) {
            auto* ptr = allocator.Allocate(i);
            EXPECT_TRUE(std::find(objects.begin(), objects.end(), ptr) == objects.end());
            objects.push_back(ptr);
        }

        for (int i = 0; i < 21; ++i) {
            // Check that the value is correct and hasn't been trampled.
            EXPECT_EQ(objects[i]->value, i);

            // Check that the alignment is correct.
            EXPECT_TRUE(IsPtrAligned(objects[i], alignof(AlignmentLarge)));
        }
    }
}

// Test that when reallocating a number of objects <= pool size, all memory is reused.
TEST(PoolAllocatorTests, ReusesFreedMemory) {
    PoolAllocator<AlignmentSmall> allocator(17);

    // Allocate a number of objects.
    std::set<AlignmentSmall*> objects;
    for (int i = 0; i < 17; ++i) {
        EXPECT_TRUE(objects.insert(allocator.Allocate(i)).second);
    }

    // Deallocate all of the objects.
    for (AlignmentSmall* object : objects) {
        allocator.Deallocate(object);
    }

    std::set<AlignmentSmall*> reallocatedObjects;
    // Allocate objects again. All of the pointers should be the same.
    for (int i = 0; i < 17; ++i) {
        AlignmentSmall* ptr = allocator.Allocate(i);
        EXPECT_TRUE(reallocatedObjects.insert(ptr).second);
        EXPECT_TRUE(std::find(objects.begin(), objects.end(), ptr) != objects.end());
    }
}

// Test that when reallocating a number of objects <= pool size, all memory is reused.
TEST(PoolAllocatorTests, NewPoolAndDeallocateFormer) {
    PoolAllocator<AlignmentSmall> allocator(4);

    // Allocate a number of objects.
    std::vector<AlignmentSmall*> objects;
    for (int i = 0; i < 5; ++i) {
        objects.push_back(allocator.Allocate(i));
    }

    for (int i = 0; i < 4; ++i) {
        allocator.Deallocate(objects[i]);
    }

    for (int i = 5; i < 7; ++i) {
        objects.push_back(allocator.Allocate(i));
    }
}
