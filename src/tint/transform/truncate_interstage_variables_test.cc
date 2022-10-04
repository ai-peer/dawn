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

#include "src/tint/transform/truncate_interstage_variables.h"

#include <memory>

#include "gmock/gmock.h"
#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using ::testing::ContainerEq;

using TruncateInterstageVariablesTest = TransformTest;

TEST_F(TruncateInterstageVariablesTest, ShouldRunVertex) {
    auto* src = R"(
struct ShaderIO {
  @builtin(position) pos: vec4<f32>,
  @location(0) f_0: f32,
  @location(2) f_2: f32,
}
@vertex
fn f() -> ShaderIO {
  var io: ShaderIO;
  io.f_0 = 1.0;
  io.f_2 = io.f_2 + 3.0;
  return io;
}
)";

    EXPECT_FALSE(ShouldRun<TruncateInterstageVariables>(src));

    {
        TruncateInterstageVariables::Config cfg;
        cfg.interstage_locations[2] = true;
        DataMap data;
        data.Add<TruncateInterstageVariables::Config>(cfg);
        EXPECT_TRUE(ShouldRun<TruncateInterstageVariables>(src, data));
    }
}

TEST_F(TruncateInterstageVariablesTest, ShouldRunFragment) {
    auto* src = R"(
struct ShaderIO {
  @location(0) f_0: f32,
  @location(2) f_2: f32,
}
@fragment
fn f(io: ShaderIO) -> @location(1) vec4<f32> {
  return vec4<f32>(io.f_0, io.f_2, 0.0, 1.0);
}
)";

    EXPECT_FALSE(ShouldRun<TruncateInterstageVariables>(src));

    TruncateInterstageVariables::Config cfg;
    // cfg.interstage_variables.emplace_back(1, sem::InterStageComponentType::Float, 4);
    // cfg.interstage_variables.push_back({1, sem::InterStageComponentType::Float, 4});

    cfg.interstage_locations[2] = true;

    DataMap data;
    data.Add<TruncateInterstageVariables::Config>(cfg);

    EXPECT_FALSE(ShouldRun<TruncateInterstageVariables>(src, data));
}

TEST_F(TruncateInterstageVariablesTest, BasicVertex) {
    auto* src = R"(
struct ShaderIO {
  @builtin(position) pos: vec4<f32>,
  @location(1) f_1: f32,
  @location(3) f_3: f32,
}
@vertex
fn f() -> ShaderIO {
  var io: ShaderIO;
  io.pos = vec4<f32>(1.0, 1.0, 1.0, 1.0);
  io.f_1 = 1.0;
  io.f_3 = io.f_1 + 3.0;
  return io;
}
)";

    auto* expect = R"(
struct TruncatedShaderIO {
  @location(3)
  f_3 : f32,
  @builtin(position)
  pos : vec4<f32>,
}

fn truncate_shader_output(io : ShaderIO) -> TruncatedShaderIO {
  var result : TruncatedShaderIO;
  result.pos = io.pos;
  result.f_3 = io.f_3;
  return result;
}

struct ShaderIO {
  pos : vec4<f32>,
  f_1 : f32,
  f_3 : f32,
}

@vertex
fn f() -> TruncatedShaderIO {
  var io : ShaderIO;
  io.pos = vec4<f32>(1.0, 1.0, 1.0, 1.0);
  io.f_1 = 1.0;
  io.f_3 = (io.f_1 + 3.0);
  return truncate_shader_output(io);
}
)";

    TruncateInterstageVariables::Config cfg;
    // fragment has input at @location(3)
    cfg.interstage_locations[3] = true;

    DataMap data;
    data.Add<TruncateInterstageVariables::Config>(cfg);

    auto got = Run<TruncateInterstageVariables>(src, data);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
