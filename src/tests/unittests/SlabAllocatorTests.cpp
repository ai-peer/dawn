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

    struct Foo : public PlacementAllocated {
        Foo(int value) : value(value) {
        }

        int value;
    };

}  // namespace

// Test that a slab allocator of a single object works.
TEST(SlabAllocatorTests, Single) {
    SlabAllocator<Foo> allocator(1);

    Foo* obj1 = allocator.Allocate(4);
    EXPECT_EQ(obj1->value, 4);

    Foo* obj2 = allocator.Allocate(5);
    EXPECT_EQ(obj2->value, 5);

    allocator.Deallocate(obj1);
    allocator.Deallocate(obj2);
}

// Allocate multiple objects and check their data is correct.
TEST(SlabAllocatorTests, AllocateSequential) {
    // Check default alignment
    {
        SlabAllocator<Foo> allocator(5);

        std::vector<Foo*> objects;
        for (int i = 0; i < 10; ++i) {
            auto* ptr = allocator.Allocate(i);
            EXPECT_TRUE(std::find(objects.begin(), objects.end(), ptr) == objects.end());
            objects.push_back(ptr);
        }

        for (int i = 0; i < 10; ++i) {
            // Check that the value is correct and hasn't been trampled.
            EXPECT_EQ(objects[i]->value, i);

            // Check that the alignment is correct.
            EXPECT_TRUE(IsPtrAligned(objects[i], allocator.kAlignment));
        }
    }

    // Check large alignment
    {
        SlabAllocator<Foo, 256> allocator(9);

        std::vector<Foo*> objects;
        for (int i = 0; i < 21; ++i) {
            auto* ptr = allocator.Allocate(i);
            EXPECT_TRUE(std::find(objects.begin(), objects.end(), ptr) == objects.end());
            objects.push_back(ptr);
        }

        for (int i = 0; i < 21; ++i) {
            // Check that the value is correct and hasn't been trampled.
            EXPECT_EQ(objects[i]->value, i);

            // Check that the alignment is correct.
            EXPECT_TRUE(IsPtrAligned(objects[i], allocator.kAlignment));
        }
    }
}

// Test that when reallocating a number of objects <= pool size, all memory is reused.
TEST(SlabAllocatorTests, ReusesFreedMemory) {
    SlabAllocator<Foo> allocator(17);

    // Allocate a number of objects.
    std::set<Foo*> objects;
    for (int i = 0; i < 17; ++i) {
        EXPECT_TRUE(objects.insert(allocator.Allocate(i)).second);
    }

    // Deallocate all of the objects.
    for (Foo* object : objects) {
        allocator.Deallocate(object);
    }

    std::set<Foo*> reallocatedObjects;
    // Allocate objects again. All of the pointers should be the same.
    for (int i = 0; i < 17; ++i) {
        Foo* ptr = allocator.Allocate(i);
        EXPECT_TRUE(reallocatedObjects.insert(ptr).second);
        EXPECT_TRUE(std::find(objects.begin(), objects.end(), ptr) != objects.end());
    }
}
