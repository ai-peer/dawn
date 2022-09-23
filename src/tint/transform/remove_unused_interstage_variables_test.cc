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

#include "src/tint/transform/remove_unused_interstage_variables.h"

#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using RemoveUnusedInterstageVariablesTest = TransformTest;

TEST_F(RemoveUnusedInterstageVariablesTest, ShouldRunEmptyModule) {
    auto* src = R"()";

    EXPECT_FALSE(ShouldRun<RemoveUnusedInterstageVariables>(src));
}

// TEST_F(RemoveUnusedInterstageVariablesTest, ShouldRunHasNoUnreachable) {
//     auto* src = R"(
// fn f() {
//   if (true) {
//     var x = 1;
//   }
// }
// )";

//     EXPECT_FALSE(ShouldRun<RemoveUnusedInterstageVariables>(src));
// }

TEST_F(RemoveUnusedInterstageVariablesTest, structMember) {
    auto* src = R"(
struct ShaderIO {
  @builtin(position) pos: vec4<f32>,
  @location(1) f_1: f32,
  @location(3) f_3: f32,
  @location(5) f_5: f32,
}
@vertex
fn f() -> ShaderIO {
  var io: ShaderIO;
  io.f_1 = 1.0;
  io.f_3 = 3.0;
  io.f_5 = io.f_3 + 5.0;
  return io;
}
)";

    auto* expect = R"(
@vertex
fn f() {
}
)";

    RemoveUnusedInterstageVariables::Config cfg;
    cfg.variables.set(3);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
