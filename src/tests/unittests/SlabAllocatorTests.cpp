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
#include "common/SlabAllocator.h"

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

// Test that a slab allocator of a single object works.
TEST(SlabAllocatorTests, Single) {
    SlabAllocator<AlignmentSmall> allocator(1);

    AlignmentSmall* obj1 = allocator.Allocate(4);
    EXPECT_EQ(obj1->value, 4);

    AlignmentSmall* obj2 = allocator.Allocate(5);
    EXPECT_EQ(obj2->value, 5);

    allocator.Deallocate(obj1);
    allocator.Deallocate(obj2);
}

// Allocate multiple objects and check their data is correct.
TEST(SlabAllocatorTests, AllocateSequential) {
    {
        SlabAllocator<AlignmentSmall> allocator(5);

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
        SlabAllocator<AlignmentLarge> allocator(9);

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
TEST(SlabAllocatorTests, ReusesFreedMemory) {
    SlabAllocator<AlignmentSmall> allocator(17);

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
