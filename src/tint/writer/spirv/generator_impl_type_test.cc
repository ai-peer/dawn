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

#include "src/tint/type/bool.h"
#include "src/tint/type/f16.h"
#include "src/tint/type/f32.h"
#include "src/tint/type/i32.h"
#include "src/tint/type/type.h"
#include "src/tint/type/u32.h"
#include "src/tint/type/void.h"
#include "src/tint/writer/spirv/test_helper_ir.h"

namespace tint::writer::spirv {
namespace {

// Test that we emit each type correctly.
struct TypeTestCase {
    const type::Type* ty;
    std::string expected;
};
using TypeTest = SpvGeneratorImplTestWithParam<TypeTestCase>;
TEST_P(TypeTest, Emit) {
    auto& params = GetParam();
    ir::Module module;
    GeneratorImplIr generator(&module, false);
    auto id = generator.Type(params.ty);
    EXPECT_EQ(id, 1u);
    auto got = DumpInstructions(generator.Module().Types());
    EXPECT_EQ(got, params.expected);
}
INSTANTIATE_TEST_SUITE_P(SpvGeneratorImplTest,
                         TypeTest,
                         ::testing::ValuesIn({
                             TypeTestCase{new type::Void(), "%1 = OpTypeVoid\n"},
                             TypeTestCase{new type::Bool(), "%1 = OpTypeBool\n"},
                             TypeTestCase{new type::I32(), "%1 = OpTypeInt 32 1\n"},
                             TypeTestCase{new type::U32(), "%1 = OpTypeInt 32 0\n"},
                             TypeTestCase{new type::F32(), "%1 = OpTypeFloat 32\n"},
                             TypeTestCase{new type::F16(), "%1 = OpTypeFloat 16\n"},
                         }),
                         [](testing::TestParamInfo<TypeTestCase> c) {
                             return c.param.ty->FriendlyName();
                         });

// Test that we do not emit the same type more than once.
TEST_F(SpvGeneratorImplTest, Deduplicate) {
    GeneratorImplIr generator(&ir, false);
    auto* i32 = new type::I32();
    EXPECT_EQ(generator.Type(i32), 1u);
    EXPECT_EQ(generator.Type(i32), 1u);
    EXPECT_EQ(generator.Type(i32), 1u);
    auto got = DumpInstructions(generator.Module().Types());
    EXPECT_EQ(got, "%1 = OpTypeInt 32 1\n");
}

}  // namespace
}  // namespace tint::writer::spirv
