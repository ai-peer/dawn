// Copyright 2022 The Tint Authors.
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

#include "src/tint/utils/vector.h"

#include <string>

#include "gtest/gtest.h"

namespace tint::utils {
namespace {

TEST(TintVectorTest, Empty) {
    Vector<int, 2> vec;
    EXPECT_EQ(vec.Length(), 0u);
}

TEST(TintVectorTest, PushPop_NoSpill) {
    Vector<std::string, 2> vec;
    EXPECT_EQ(vec.Length(), 0u);

    vec.Push("hello");
    EXPECT_EQ(vec.Length(), 1u);

    vec.Push("world");
    EXPECT_EQ(vec.Length(), 2u);

    EXPECT_EQ(vec.Pop(), "world");
    EXPECT_EQ(vec.Length(), 1u);

    EXPECT_EQ(vec.Pop(), "hello");
    EXPECT_EQ(vec.Length(), 0u);
}

TEST(TintVectorTest, PushPop_WithSpill) {
    Vector<std::string, 1> vec;
    EXPECT_EQ(vec.Length(), 0u);

    vec.Push("hello");
    EXPECT_EQ(vec.Length(), 1u);

    vec.Push("world");
    EXPECT_EQ(vec.Length(), 2u);

    EXPECT_EQ(vec.Pop(), "world");
    EXPECT_EQ(vec.Length(), 1u);

    EXPECT_EQ(vec.Pop(), "hello");
    EXPECT_EQ(vec.Length(), 0u);
}

TEST(TintVectorTest, MoveCtor_N2_to_N2) {
    Vector<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    Vector<std::string, 2> vec_b{std::move(vec_a)};
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(TintVectorTest, MoveCtor_N2_to_N1) {
    Vector<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    Vector<std::string, 1> vec_b{std::move(vec_a)};
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(TintVectorTest, MoveCtor_N2_to_N3) {
    Vector<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    Vector<std::string, 3> vec_b{std::move(vec_a)};
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(TintVectorTest, MoveAssign_N2_to_N2) {
    Vector<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    Vector<std::string, 2> vec_b;
    vec_b = std::move(vec_a);
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(TintVectorTest, MoveAssign_N2_to_N1) {
    Vector<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    Vector<std::string, 1> vec_b;
    vec_b = std::move(vec_a);
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(TintVectorTest, MoveAssign_N2_to_N3) {
    Vector<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    Vector<std::string, 3> vec_b;
    vec_b = std::move(vec_a);
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

}  // namespace
}  // namespace tint::utils
