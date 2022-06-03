// Copyright 2022 The Dawn Authors
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

#include <utility>

#include "dawn/native/Blob.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace dawn::native {

// static
Blob Blob::CreateForTesting(testing::StrictMock<testing::MockFunction<void()>>* mockDeleter) {
    return Blob(nullptr, 0, [=]() { mockDeleter->Call(); });
}

namespace {

// Test that a blob starts empty.
TEST(BlobTests, DefaultEmpty) {
    Blob b;
    EXPECT_TRUE(b.Empty());
    EXPECT_EQ(b.Data(), nullptr);
    EXPECT_EQ(b.Size(), 0u);
}

// Test that you can create a blob with a size in bytes and write/read its contents.
TEST(BlobTests, SizedCreation) {
    Blob b = Blob::Create(10);
    EXPECT_FALSE(b.Empty());
    EXPECT_EQ(b.Size(), 10u);
    ASSERT_NE(b.Data(), nullptr);
    // We should be able to copy 10 bytes into the blob.
    char data[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    memcpy(b.Data(), data, sizeof(data));
    // And retrieve the exact contents back.
    EXPECT_EQ(memcmp(b.Data(), data, sizeof(data)), 0);
}

// Test that ou can create a zero-sized blob.
TEST(BlobTests, EmptySizedCreation) {
    Blob b = Blob::Create(0);
    EXPECT_TRUE(b.Empty());
    EXPECT_EQ(b.Data(), nullptr);
    EXPECT_EQ(b.Size(), 0u);
}

// Test that move construction moves the data from one blob into the new one.
TEST(BlobTests, MoveConstruct) {
    // Create the blob.
    Blob b1 = Blob::Create(10);
    char data[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    memcpy(b1.Data(), data, sizeof(data));

    // Move construct b2 from b1.
    Blob b2(std::move(b1));

    // Data should be moved.
    EXPECT_FALSE(b2.Empty());
    EXPECT_EQ(b2.Size(), 10u);
    ASSERT_NE(b2.Data(), nullptr);
    EXPECT_EQ(memcmp(b2.Data(), data, sizeof(data)), 0);
}

// Test that move assignment moves the data from one blob into another.
TEST(BlobTests, MoveAssign) {
    // Create the blob.
    Blob b1 = Blob::Create(10);
    char data[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    memcpy(b1.Data(), data, sizeof(data));

    // Move assign b2 from b1.
    Blob b2;
    b2 = std::move(b1);

    // Data should be moved.
    EXPECT_FALSE(b2.Empty());
    EXPECT_EQ(b2.Size(), 10u);
    ASSERT_NE(b2.Data(), nullptr);
    EXPECT_EQ(memcmp(b2.Data(), data, sizeof(data)), 0);
}

// Test that move assignment can replace the contents of the moved-to blob.
TEST(BlobTests, MoveAssignOver) {
    // Create the blob.
    Blob b1 = Blob::Create(10);
    char data[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    memcpy(b1.Data(), data, sizeof(data));

    // Create another blob with a mock deleter.
    testing::StrictMock<testing::MockFunction<void()>> mockDeleter;
    Blob b2 = Blob::CreateForTesting(&mockDeleter);

    // Move b1 into b2, replacing b2's contents, and expect the deleter to be called.
    EXPECT_CALL(mockDeleter, Call());
    b2 = std::move(b1);

    // Data should be moved.
    EXPECT_FALSE(b2.Empty());
    EXPECT_EQ(b2.Size(), 10u);
    ASSERT_NE(b2.Data(), nullptr);
    EXPECT_EQ(memcmp(b2.Data(), data, sizeof(data)), 0);
}

}  // namespace

}  // namespace dawn::native
