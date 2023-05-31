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

#include "src/tint/ir/validate.h"
#include "gmock/gmock.h"
#include "src/tint/ir/builder.h"
#include "src/tint/type/pointer.h"

namespace tint::ir {
namespace {

/// Helper base class for testing
template <typename BASE>
class TestHelperBase : public BASE, public Builder {
  public:
    TestHelperBase() : Builder(mod) {}

    Module mod;
};

/// Helper class for testing that derives from testing::Test.
using TestHelper = TestHelperBase<testing::Test>;

using namespace tint::number_suffixes;  // NOLINT

using IR_ValidateTest = TestHelper;

TEST_F(IR_ValidateTest, RootBlockVar) {
    CreateRootBlockIfNeeded();
    mod.root_block->Instructions().Push(Declare(ir.Types().Get<type::Pointer>(
        ir.Types().i32(), builtin::AddressSpace::kPrivate, builtin::Access::kReadWrite)));
    EXPECT_TRUE(ir::Validate(mod));
}

TEST_F(IR_ValidateTest, RootBlockNonVar) {
    CreateRootBlockIfNeeded();
    mod.root_block->Instructions().Push(CreateLoop());

    auto res = ir::Validate(mod);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.Failure(), "error: root block: invalid instruction: tint::ir::Loop");
}

TEST_F(IR_ValidateTest, RootBlockVarBadType) {
    CreateRootBlockIfNeeded();
    mod.root_block->Instructions().Push(Declare(ir.Types().i32()));
    auto res = ir::Validate(mod);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.Failure(), "error: root block: 'var' type is not a pointer: tint::type::I32");
}

}  // namespace
}  // namespace tint::ir
