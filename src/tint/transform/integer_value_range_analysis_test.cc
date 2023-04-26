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

#include "src/tint/transform/integer_value_range_analysis.h"

#include <memory>

#include "src/tint/ast/identifier.h"
#include "src/tint/transform/test_helper.h"

namespace tint::transform {

namespace {

using IntegerValueRangeAnalysisTest = TransformTest;

TEST_F(IntegerValueRangeAnalysisTest, LocalInvocationID) {
    auto* src = R"(
var<workgroup> memShared : array<array<f32, 8>, 16>;

@compute @workgroup_size(16, 8, 1)
fn main(@builtin(local_invocation_id) LocalInvocationID : vec3u) {
    let value = memShared[LocalInvocationID.x][LocalInvocationID.y];
}
)";
    auto file = std::make_unique<Source::File>("test", src);
    auto program = reader::wgsl::Parse(file.get());

    IntegerValueRangeAnalysis analysis;
    const auto& rangedIntegerVariables = analysis.Apply(&program);
    ASSERT_EQ(1u, rangedIntegerVariables.size());

    const auto& iter = rangedIntegerVariables.cbegin();
    const tint::sem::Variable* integerVariable = iter->first->As<tint::sem::Variable>();
    ASSERT_TRUE(integerVariable->Type()->is_unsigned_integer_vector());
    ASSERT_EQ(12u, integerVariable->Type()->Size());
    ASSERT_EQ("LocalInvocationID", integerVariable->Declaration()->name->symbol.Name());
    ASSERT_EQ(15, iter->second[0].maxValue);
    ASSERT_EQ(0, iter->second[0].minValue);
    ASSERT_EQ(7, iter->second[1].maxValue);
    ASSERT_EQ(0, iter->second[1].minValue);
    ASSERT_EQ(0, iter->second[2].maxValue);
    ASSERT_EQ(0, iter->second[2].minValue);
}

}  // anonymous namespace
}  // namespace tint::transform
