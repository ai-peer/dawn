// Copyright 2021 The Tint Authors.
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

#include "src/tint/utils/list.h"

#include <string>

#include "gtest/gtest.h"

namespace tint::utils {
namespace {

TEST(ListTest, Empty) {
    List<int, 2> vec;
    EXPECT_EQ(vec.Length(), 0u);
}

TEST(ListTest, PushPop_NoSpill) {
    List<std::string, 2> vec;
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

TEST(ListTest, PushPop_WithSpill) {
    List<std::string, 1> vec;
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

TEST(ListTest, MoveCtor_N2_to_N2) {
    List<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    List<std::string, 2> vec_b{std::move(vec_a)};
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(ListTest, MoveCtor_N2_to_N1) {
    List<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    List<std::string, 1> vec_b{std::move(vec_a)};
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(ListTest, MoveCtor_N2_to_N3) {
    List<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    List<std::string, 3> vec_b{std::move(vec_a)};
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(ListTest, MoveAssign_N2_to_N2) {
    List<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    List<std::string, 2> vec_b;
    vec_b = std::move(vec_a);
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(ListTest, MoveAssign_N2_to_N1) {
    List<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    List<std::string, 1> vec_b;
    vec_b = std::move(vec_a);
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

TEST(ListTest, MoveAssign_N2_to_N3) {
    List<std::string, 2> vec_a(2);
    vec_a[0] = "hello";
    vec_a[1] = "world";

    List<std::string, 3> vec_b;
    vec_b = std::move(vec_a);
    EXPECT_EQ(vec_b[0], "hello");
    EXPECT_EQ(vec_b[1], "world");
}

}  // namespace
}  // namespace tint::utils
