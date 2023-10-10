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

#include "src/tint/utils/bytes/reader.h"

#include "gtest/gtest.h"

namespace tint::bytes {
namespace {

TEST(BytesReaderTest, IntegerBigEndian) {
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40};
    auto u32 = Reader{Slice{data}, 0, Endianness::kBig}.Int<uint32_t>();
    EXPECT_EQ(u32, 0x10203040u);
    auto i32 = Reader{Slice{data}, 0, Endianness::kBig}.Int<int32_t>();
    EXPECT_EQ(i32, 0x10203040);
}

TEST(BytesReaderTest, IntegerBigEndian_Offset) {
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    auto u32 = Reader{Slice{data}, 2, Endianness::kBig}.Int<uint32_t>();
    EXPECT_EQ(u32, 0x30405060u);
    auto i32 = Reader{Slice{data}, 2, Endianness::kBig}.Int<int32_t>();
    EXPECT_EQ(i32, 0x30405060);
}

TEST(BytesReaderTest, IntegerBigEndian_Clipped) {
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40};
    auto u32 = Reader{Slice{data}, 2, Endianness::kBig}.Int<uint32_t>();
    EXPECT_EQ(u32, 0x30400000u);
    auto i32 = Reader{Slice{data}, 2, Endianness::kBig}.Int<int32_t>();
    EXPECT_EQ(i32, 0x30400000);
}

TEST(BytesReaderTest, IntegerLittleEndian) {
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40};
    auto u32 = Reader{Slice{data}, 0, Endianness::kLittle}.Int<uint32_t>();
    EXPECT_EQ(u32, 0x40302010u);
    auto i32 = Reader{Slice{data}, 0, Endianness::kLittle}.Int<int32_t>();
    EXPECT_EQ(i32, 0x40302010);
}

TEST(BytesReaderTest, IntegerLittleEndian_Offset) {
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    auto u32 = Reader{Slice{data}, 2, Endianness::kLittle}.Int<uint32_t>();
    EXPECT_EQ(u32, 0x60504030u);
    auto i32 = Reader{Slice{data}, 2, Endianness::kLittle}.Int<int32_t>();
    EXPECT_EQ(i32, 0x60504030);
}

TEST(BytesReaderTest, IntegerLittleEndian_Clipped) {
    uint8_t data[] = {0x10, 0x20, 0x30, 0x40};
    auto u32 = Reader{Slice{data}, 2, Endianness::kLittle}.Int<uint32_t>();
    EXPECT_EQ(u32, 0x00004030u);
    auto i32 = Reader{Slice{data}, 2, Endianness::kLittle}.Int<int32_t>();
    EXPECT_EQ(i32, 0x00004030);
}

TEST(BytesReaderTest, Float) {
    uint8_t data[] = {0x00, 0x00, 0x08, 0x41};
    float f32 = Reader{Slice{data}}.Float<float>();
    EXPECT_EQ(f32, 8.5f);
}

TEST(BytesReaderTest, Float_Offset) {
    uint8_t data[] = {0x00, 0x00, 0x08, 0x41, 0x80, 0x3e};
    float f32 = Reader{Slice{data}, 2}.Float<float>();
    EXPECT_EQ(f32, 0.25049614f);
}

TEST(BytesReaderTest, Float_Clipped) {
    uint8_t data[] = {0x00, 0x00, 0x08, 0x41};
    float f32 = Reader{Slice{data}, 2}.Float<float>();
    EXPECT_EQ(f32, 2.3329e-41f);
}

}  // namespace
}  // namespace tint::bytes
