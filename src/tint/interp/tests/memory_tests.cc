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

#include "gtest/gtest.h"

#include "tint/interp/memory.h"

namespace tint::interp {
namespace {

using MemoryTest = testing::Test;

TEST_F(MemoryTest, Create) {
    Memory alloc(4);
    EXPECT_EQ(alloc.Size(), 4u);
}

TEST_F(MemoryTest, LoadStore) {
    Memory alloc(8);
    uint32_t store1 = 0x91B7C3DA;
    uint32_t store2 = 0xF2C05E18;
    alloc.Store(&store1, 0);
    alloc.Store(&store2, 4);
    uint32_t load1 = 0;
    uint32_t load2 = 0;
    alloc.Load(&load1, 0);
    alloc.Load(&load2, 4);
    EXPECT_EQ(load1, store1);
    EXPECT_EQ(load2, store2);
}

TEST_F(MemoryTest, Load_OOB) {
    Memory alloc(4);
    uint32_t store1 = 0x91B7C3DA;
    alloc.Store(&store1, 0);
    uint32_t load1 = 0xFFFFFFFF;
    uint32_t load2 = 0xFFFFFFFF;
    uint32_t load3 = 0xFFFFFFFF;
    alloc.Load(&load1, 0);
    alloc.Load(&load2, 4);
    alloc.Load(&load3, (1 << 20u));
    EXPECT_EQ(load1, store1);
    EXPECT_EQ(load2, 0u);
    EXPECT_EQ(load3, 0u);
}

TEST_F(MemoryTest, Store_OOB) {
    Memory alloc(4);
    uint32_t store1 = 0x91B7C3DA;
    uint32_t store2 = 0xF2C05E18;
    uint32_t store3 = 0xDEADBEEF;
    alloc.Store(&store1, 0);
    alloc.Store(&store2, 4);
    alloc.Store(&store3, (1 << 20u));
    uint32_t load1 = 0xFFFFFFFF;
    alloc.Load(&load1, 0);
    EXPECT_EQ(load1, store1);
}

}  // namespace
}  // namespace tint::interp
