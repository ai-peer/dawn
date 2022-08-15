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

#include "src/tint/transform/fork_uniform_structs.h"

#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using ForkUniformStructsTest = TransformTest;

TEST_F(ForkUniformStructsTest, NoOp) {
    auto* src = "";
    auto* expect = "";

    DataMap data;
    auto got = Run<ForkUniformStructs>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(ForkUniformStructsTest, ShouldNotRunForNonUniform) {
    DataMap data;
    std::string src =
        "struct M {\n  m : mat2x2<f32>,\n}\n\n@group(0) @binding(0) var<storage> u : M;";
    EXPECT_FALSE(ShouldRun<ForkUniformStructs>(src, data));
}

TEST_F(ForkUniformStructsTest, AggregateAssignment) {
    DataMap data;
    auto* src = R"(
struct S {
  f : f32,
  i : i32,
  u : u32,
  v : vec4<f32>,
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S;

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  s = u;
}

)";

    auto* expect = R"(
struct S_1 {
  f : f32,
  i : i32,
  u : u32,
  v : vec4<f32>,
  m : mat2x2<f32>,
}

struct S {
  f : f32,
  i : i32,
  u : u32,
  v : vec4<f32>,
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S_1;

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  s = S(u.f, u.i, u.u, u.v, u.m);
}
)";

    auto got = Run<ForkUniformStructs>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(ForkUniformStructsTest, FunctionCall) {
    DataMap data;
    auto* src = R"(
struct S {
  f : f32,
  i : i32,
  u : u32,
  v : vec4<f32>,
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S;

@group(0) @binding(1) var<storage, read_write> s : S;

fn f(p : S) {
  s = p;
}

fn g() {
  f(u);
}

)";

    auto* expect = R"(
struct S_1 {
  f : f32,
  i : i32,
  u : u32,
  v : vec4<f32>,
  m : mat2x2<f32>,
}

struct S {
  f : f32,
  i : i32,
  u : u32,
  v : vec4<f32>,
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S_1;

@group(0) @binding(1) var<storage, read_write> s : S;

fn f(p : S) {
  s = p;
}

fn g() {
  f(S(u.f, u.i, u.u, u.v, u.m));
}
)";

    auto got = Run<ForkUniformStructs>(src, data);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
