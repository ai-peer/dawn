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

#include "gmock/gmock.h"
#include "gtest/gtest-spi.h"
#include "src/tint/ir/builder.h"
#include "src/tint/ir/instruction.h"
#include "src/tint/ir/ir_test_helper.h"

namespace tint::ir {
namespace {

using namespace tint::number_suffixes;  // NOLINT

using IR_UndefValueTest = IRTestHelper;

TEST_F(IR_UndefValueTest, Create) {
    auto* val = b.Undef(mod.Types().i32());
    ASSERT_TRUE(val->Is<UndefValue>());
    EXPECT_EQ(val->Type(), mod.Types().i32());
}

TEST_F(IR_UndefValueTest, UndefValue_Usage) {
    auto* val = b.Undef(mod.Types().i32());
    ASSERT_NE(val, nullptr);

    auto* add = b.Add(mod.Types().i32(), val, val);
    EXPECT_THAT(val->Usages(), testing::UnorderedElementsAre(Usage{add, 0u}, Usage{add, 1u}));

    add->SetOperand(0u, b.Value(42_i));
    EXPECT_THAT(val->Usages(), testing::UnorderedElementsAre(Usage{add, 1u}));
}

TEST_F(IR_UndefValueTest, Fail_NullType) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            b.Undef(nullptr);
        },
        "");
}

}  // namespace
}  // namespace tint::ir
