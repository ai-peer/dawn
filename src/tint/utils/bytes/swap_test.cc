// Copyright 2023 The Tint Authors.
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

#include "src/tint/utils/bytes/swap.h"

#include "gtest/gtest.h"

namespace tint::bytes {
namespace {

TEST(BytesSwapTest, Uint) {
    EXPECT_EQ(Swap<uint8_t>(0x41), static_cast<uint8_t>(0x41));
    EXPECT_EQ(Swap<uint16_t>(0x4152), static_cast<uint16_t>(0x5241));
    EXPECT_EQ(Swap<uint32_t>(0x41526374), static_cast<uint32_t>(0x74635241));
    EXPECT_EQ(Swap<uint64_t>(0x415263748596A7B8), static_cast<uint64_t>(0xB8A7968574635241));
}

TEST(BytesSwapTest, Sint) {
    EXPECT_EQ(Swap<int8_t>(0x41), static_cast<int8_t>(0x41));
    EXPECT_EQ(Swap<int8_t>(-0x41), static_cast<int8_t>(-0x41));
    EXPECT_EQ(Swap<int16_t>(0x4152), static_cast<int16_t>(0x5241));
    EXPECT_EQ(Swap<int16_t>(-0x4152), static_cast<int16_t>(0xAEBE));
    EXPECT_EQ(Swap<int32_t>(0x41526374), static_cast<int32_t>(0x74635241));
    EXPECT_EQ(Swap<int32_t>(-0x41526374), static_cast<int32_t>(0x8C9CADBE));
    EXPECT_EQ(Swap<int64_t>(0x415263748596A7B8), static_cast<int64_t>(0xB8A7968574635241));
    EXPECT_EQ(Swap<int64_t>(-0x415263748596A7B8), static_cast<int64_t>(0x4858697A8B9CADBE));
}

}  // namespace
}  // namespace tint::bytes
