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
#include "src/tint/ir/builder.h"
#include "src/tint/ir/instruction.h"
#include "src/tint/ir/ir_test_helper.h"

namespace tint::ir {
namespace {

using namespace tint::number_suffixes;  // NOLINT

using IR_VarTest = IRTestHelper;

TEST_F(IR_VarTest, CreateVar) {
    auto* inst = b.Declare(mod.Types().pointer(mod.Types().i32(), builtin::AddressSpace::kPrivate,
                                               builtin::Access::kReadWrite));

    ASSERT_TRUE(inst->Is<ir::Var>());
    EXPECT_EQ(inst->Initializer(), nullptr);
    EXPECT_FALSE(inst->BindingPoint());
}

TEST_F(IR_VarTest, SetBindingPoint) {
    auto* inst = b.Declare(mod.Types().pointer(mod.Types().i32(), builtin::AddressSpace::kStorage,
                                               builtin::Access::kReadWrite));
    inst->SetBindingPoint(1u, 2u);

    ASSERT_TRUE(inst->Is<ir::Var>());
    ASSERT_TRUE(inst->BindingPoint());
    EXPECT_EQ(inst->BindingPoint()->group, 1u);
    EXPECT_EQ(inst->BindingPoint()->binding, 2u);
}

TEST_F(IR_VarTest, SetInitializer) {
    auto* inst = b.Declare(mod.Types().pointer(mod.Types().i32(), builtin::AddressSpace::kPrivate,
                                               builtin::Access::kReadWrite));
    auto* init = b.Constant(42_i);
    inst->SetInitializer(init);

    ASSERT_TRUE(inst->Is<ir::Var>());
    EXPECT_EQ(inst->Initializer(), init);
    EXPECT_THAT(init->Usages(), testing::UnorderedElementsAre(Usage{inst, 0u}));
    inst->SetInitializer(nullptr);
    EXPECT_TRUE(init->Usages().IsEmpty());
}

}  // namespace
}  // namespace tint::ir
