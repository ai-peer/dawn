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

#include "src/tint/transform/direct_variable_access.h"

#include "src/tint/transform/test_helper.h"
#include "src/tint/utils/string.h"

namespace tint::transform {
namespace {

using DirectVariableAccessTest = TransformTest;

TEST_F(DirectVariableAccessTest, ShouldRunEmptyModule) {
    auto* src = R"()";

    EXPECT_FALSE(ShouldRun<DirectVariableAccess>(src));
}

TEST_F(DirectVariableAccessTest, UniformPointer_i32) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : i32;

fn a(p : ptr<uniform, i32>) -> i32 {
    return *p;
}

fn b() {
  a(&U);
}
)";

    auto* expect = R"(xxxx)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
