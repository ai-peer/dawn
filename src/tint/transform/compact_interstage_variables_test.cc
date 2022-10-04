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

#include "src/tint/transform/compact_interstage_variables.h"

#include <memory>

#include "gmock/gmock.h"
#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using ::testing::ContainerEq;

using CompactInterstageVariablesTest = TransformTest;

// TEST_F(CompactInterstageVariablesTest, EmptyModule) {
//     auto* src = "";
//     auto* expect = "";

//     auto got = Run<CompactInterstageVariables>(src);

//     EXPECT_EQ(expect, str(got));

//     auto* data = got.data.Get<CompactInterstageVariables::Data>();

//     ASSERT_EQ(data->remappings.size(), 0u);
// }

TEST_F(CompactInterstageVariablesTest, ShouldNotRunVertex) {
    {
        auto* src = R"(
struct ShaderIO {
  @builtin(position) pos: vec4<f32>,
  @location(0) f_0: f32,
  @location(1) f_1: f32,
}
@vertex
fn f() -> ShaderIO {
  var io: ShaderIO;
  io.f_0 = 1.0;
  io.f_1 = io.f_1 + 3.0;
  return io;
}
)";

        EXPECT_FALSE(ShouldRun<CompactInterstageVariables>(src));
    }
}

TEST_F(CompactInterstageVariablesTest, ShouldNotRunFragment) {
    {
        auto* src = R"(
struct ShaderIO {
  @location(0) f_0: f32,
  @location(1) f_1: f32,
}
@fragment
fn f(io: ShaderIO) -> vec4<f32> {
  return vec4<f32>(io.f_0, io.f_1, 0.0, 1.0);
}
)";

        EXPECT_FALSE(ShouldRun<CompactInterstageVariables>(src));
    }
}

TEST_F(CompactInterstageVariablesTest, BasicVertex) {
    auto* src = R"(
struct ShaderIO {
  @builtin(position) pos: vec4<f32>,
  @location(1) f_1: f32,
  @location(3) f_3: f32,
}
@vertex
fn f() -> ShaderIO {
  var io: ShaderIO;
  io.f_1 = 1.0;
  io.f_3 = io.f_1 + 3.0;
  return io;
}
)";

    auto* expect = R"(
struct ShaderIO {
  @builtin(position) pos: vec4<f32>,
  @location(0) tint_symbol_0: f32,
  @location(1) f_1: f32,
  @location(2) tint_symbol_2: f32,
  @location(3) f_3: f32,
}
@vertex
fn f() -> ShaderIO {
  var io: ShaderIO;
  io.f_1 = 1.0;
  io.f_3 = io.f_1 + 3.0;
  return io;
}
)";

    DataMap data;
    auto got = Run<CompactInterstageVariables>(src, data);

    EXPECT_EQ(expect, str(got));

    // auto* data = got.data.Get<Renamer::Data>();

    // ASSERT_NE(data, nullptr);
    // Renamer::Data::Remappings expected_remappings = {
    //     {"vert_idx", "tint_symbol_1"},
    //     {"test", "tint_symbol"},
    //     {"entry", "tint_symbol_2"},
    // };
    // EXPECT_THAT(data->remappings, ContainerEq(expected_remappings));
}

TEST_F(CompactInterstageVariablesTest, BasicFragment) {
    auto* src = R"(
struct ShaderIO {
  @location(1) f_0: f32,
  @location(3) f_1: f32,
}
@fragment
fn f(io: ShaderIO) -> vec4<f32> {
  return vec4<f32>(io.f_0, io.f_1, 0.0, 1.0);
}
)";

    auto* expect = R"(
struct ShaderIO {
  @location(1) f_0: f32,
  @location(3) f_1: f32,
}
@fragment
fn f(io: ShaderIO) -> vec4<f32> {
  return vec4<f32>(io.f_0, io.f_1, 0.0, 1.0);
}
)";

    DataMap data;
    auto got = Run<CompactInterstageVariables>(src, data);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
