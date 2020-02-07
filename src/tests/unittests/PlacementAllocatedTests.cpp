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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/PlacementAllocated.h"

using namespace testing;

namespace {

    enum class DestructedClass {
        Foo,
        Bar,
    };

    class MockDestructor {
      public:
        MOCK_METHOD2(Call, void(void*, DestructedClass));
    };

    std::unique_ptr<StrictMock<MockDestructor>> mockDestructor;

    class PlacementAllocatedTests : public Test {
        void SetUp() override {
            mockDestructor = std::make_unique<StrictMock<MockDestructor>>();
        }

        void TearDown() override {
            mockDestructor = nullptr;
        }
    };

    struct Foo : PlacementAllocated {
        ~Foo() {
            mockDestructor->Call(this, DestructedClass::Foo);
        }
    };

    struct Bar : Foo {
        ~Bar() {
            mockDestructor->Call(this, DestructedClass::Bar);
        }
    };
}  // namespace

// Test that deleting twice is calls the destructor twice and doesn't crash.
// In practice, nothing should be double deleted, but this checks that the memory isn't deallocated.
TEST_F(PlacementAllocatedTests, DeletionDoesNotFreeMemory) {
    void* ptr = malloc(sizeof(Foo));

    Foo* foo = new (ptr) Foo();

    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Foo));
    delete foo;

    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Foo));
    delete foo;

    free(ptr);
}

// Test that destructing an instance of a derived class calls the derived, then base destructor.
TEST_F(PlacementAllocatedTests, DeletingDerivedClassCallsBaseDestructor) {
    void* ptr = malloc(sizeof(Bar));

    Bar* bar = new (ptr) Bar();

    Sequence s1;
    EXPECT_CALL(*mockDestructor, Call(bar, DestructedClass::Bar)).InSequence(s1);
    EXPECT_CALL(*mockDestructor, Call(bar, DestructedClass::Foo)).InSequence(s1);
    delete bar;

    Sequence s2;
    EXPECT_CALL(*mockDestructor, Call(bar, DestructedClass::Bar)).InSequence(s2);
    EXPECT_CALL(*mockDestructor, Call(bar, DestructedClass::Foo)).InSequence(s2);
    delete bar;

    free(ptr);
}

// Test that destructing an instance of a base class calls the derived, then base destructor.
TEST_F(PlacementAllocatedTests, DeletingBaseClassCallsDerivedDestructor) {
    void* ptr = malloc(sizeof(Bar));

    Foo* foo = new (ptr) Bar();

    Sequence s1;
    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Bar)).InSequence(s1);
    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Foo)).InSequence(s1);
    delete foo;

    Sequence s2;
    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Bar)).InSequence(s2);
    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Foo)).InSequence(s2);
    delete foo;

    free(ptr);
}

// Test that destructing an instance of a base class calls the derived, then base destructor.
TEST_F(PlacementAllocatedTests, DeletingPlacementAllocatedClassCallsDerivedAndBaseDestructor) {
    void* ptr = malloc(sizeof(Bar));

    PlacementAllocated* foo = new (ptr) Bar();

    Sequence s1;
    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Bar)).InSequence(s1);
    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Foo)).InSequence(s1);
    delete foo;

    Sequence s2;
    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Bar)).InSequence(s2);
    EXPECT_CALL(*mockDestructor, Call(foo, DestructedClass::Foo)).InSequence(s2);
    delete foo;

    free(ptr);
}
