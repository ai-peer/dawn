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

#include "src/tint/transform/mat2_to_vec2.h"

#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using Mat2ToVec2Test = TransformTest;

TEST_F(Mat2ToVec2Test, NoOp) {
    auto* src = "";
    auto* expect = "";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, Simple2x2) {
    auto* src = R"(
struct U {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, Simple3x2) {
    auto* src = R"(
struct U {
  m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  m2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, Simple4x2) {
    auto* src = R"(
struct U {
  m : mat4x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  m2 : vec2<f32>,
  m3 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, Load2x2) {
    auto* src = R"(
struct U {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;

fn f() {
  let a = u.m[0][0];
  let b = u.m[0][1];
  let c = u.m[1][0];
  let d = u.m[1][1];
}
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;

fn f() {
  let a = mat2x2<f32>(u.m0, u.m1)[0][0];
  let b = mat2x2<f32>(u.m0, u.m1)[0][1];
  let c = mat2x2<f32>(u.m0, u.m1)[1][0];
  let d = mat2x2<f32>(u.m0, u.m1)[1][1];
}
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, Load3x2) {
    auto* src = R"(
struct U {
  m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;

fn f() {
  let a = u.m[0][0];
  let b = u.m[0][1];
  let c = u.m[1][0];
  let d = u.m[1][1];
  let e = u.m[2][0];
  let f = u.m[2][1];
}
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  m2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;

fn f() {
  let a = mat3x2<f32>(u.m0, u.m1, u.m2)[0][0];
  let b = mat3x2<f32>(u.m0, u.m1, u.m2)[0][1];
  let c = mat3x2<f32>(u.m0, u.m1, u.m2)[1][0];
  let d = mat3x2<f32>(u.m0, u.m1, u.m2)[1][1];
  let e = mat3x2<f32>(u.m0, u.m1, u.m2)[2][0];
  let f = mat3x2<f32>(u.m0, u.m1, u.m2)[2][1];
}
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, Load4x2) {
    auto* src = R"(
struct U {
  m : mat4x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;

fn f() {
  let a = u.m[0][0];
  let b = u.m[0][1];
  let c = u.m[1][0];
  let d = u.m[1][1];
  let e = u.m[2][0];
  let f = u.m[2][1];
  let g = u.m[3][0];
  let h = u.m[3][1];
}
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  m2 : vec2<f32>,
  m3 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;

fn f() {
  let a = mat4x2<f32>(u.m0, u.m1, u.m2, u.m3)[0][0];
  let b = mat4x2<f32>(u.m0, u.m1, u.m2, u.m3)[0][1];
  let c = mat4x2<f32>(u.m0, u.m1, u.m2, u.m3)[1][0];
  let d = mat4x2<f32>(u.m0, u.m1, u.m2, u.m3)[1][1];
  let e = mat4x2<f32>(u.m0, u.m1, u.m2, u.m3)[2][0];
  let f = mat4x2<f32>(u.m0, u.m1, u.m2, u.m3)[2][1];
  let g = mat4x2<f32>(u.m0, u.m1, u.m2, u.m3)[3][0];
  let h = mat4x2<f32>(u.m0, u.m1, u.m2, u.m3)[3][1];
}
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
