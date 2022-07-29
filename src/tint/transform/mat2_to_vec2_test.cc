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

TEST_F(Mat2ToVec2Test, CustomSize2x2) {
    auto* src = R"(
struct U {
  @size(64) m : mat2x2<f32>,
  f : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  @size(56)
  m1 : vec2<f32>,
  f : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, CustomSize3x2) {
    auto* src = R"(
struct U {
  @size(64) m : mat3x2<f32>,
  f : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  @size(48)
  m2 : vec2<f32>,
  f : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, CustomSize4x2) {
    auto* src = R"(
struct U {
  @size(64) m : mat4x2<f32>,
  f : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  m2 : vec2<f32>,
  @size(40)
  m3 : vec2<f32>,
  f : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, CustomAlign2x2) {
    auto* src = R"(
struct U {
  f : f32,
  @align(32)
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  f : f32,
  @align(32)
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, CustomAlign3x2) {
    auto* src = R"(
struct U {
  f : f32,
  @align(32)
  m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  f : f32,
  @align(32)
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

TEST_F(Mat2ToVec2Test, CustomAlign4x2) {
    auto* src = R"(
struct U {
  f : f32,
  @align(32)
  m : mat4x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  f : f32,
  @align(32)
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

TEST_F(Mat2ToVec2Test, NestledScalarsMat2x2) {
    auto* src = R"(
struct U {
  m : mat2x2<f32>,
  f0 : f32,
  f1 : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  f0 : f32,
  f1 : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, NestledScalarsMat3x2) {
    auto* src = R"(
struct U {
  m : mat3x2<f32>,
  f0 : f32,
  f1 : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  m2 : vec2<f32>,
  f0 : f32,
  f1 : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, NestledScalarsMat4x2) {
    auto* src = R"(
struct U {
  m : mat3x2<f32>,
  f0 : f32,
  f1 : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
  m2 : vec2<f32>,
  f0 : f32,
  f1 : f32,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, NameCollision) {
    auto* src = R"(
struct U {
  m0 : f32,
  m : mat4x2<f32>,
  m2: u32,
}

@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : f32,
  m0_1 : vec2<f32>,
  m1 : vec2<f32>,
  m2_1 : vec2<f32>,
  m3 : vec2<f32>,
  m2 : u32,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, ShouldRunForRows2Only) {
    DataMap data;
    for (int rows = 2; rows <= 4; ++rows) {
        for (int cols = 2; cols <= 4; ++cols) {
            std::string src = "struct M {\n  m : mat" + std::to_string(cols) + "x" +
                              std::to_string(rows) +
                              "<f32>,\n}\n\n@group(0) @binding(0) var<uniform> u : M;";
            if (rows == 2) {
                EXPECT_TRUE(ShouldRun<Mat2ToVec2>(src, data));
            } else {
                EXPECT_FALSE(ShouldRun<Mat2ToVec2>(src, data));
            }
        }
    }
}

TEST_F(Mat2ToVec2Test, ShouldNotRunForNonUniform) {
    DataMap data;
    std::string src =
        "struct M {\n  m : mat2x2<f32>,\n}\n\n@group(0) @binding(0) var<storage> u : M;";
    EXPECT_FALSE(ShouldRun<Mat2ToVec2>(src, data));
}

TEST_F(Mat2ToVec2Test, Mat2AsFunctionArg) {
    DataMap data;
    auto* src = R"(
struct U {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;

fn g(m : mat2x2<f32>) {
  let a = m[0][0];
  let b = m[0][1];
  let c = m[1][0];
  let d = m[1][1];
}

fn f() {
  g(u.m);
}
)";

    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;

fn g(m : mat2x2<f32>) {
  let a = m[0][0];
  let b = m[0][1];
  let c = m[1][0];
  let d = m[1][1];
}

fn f() {
  g(mat2x2<f32>(u.m0, u.m1));
}
)";

    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, UniformAndStorageUsage) {
    DataMap data;
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S;

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  s.m = u.m;
}

)";

    auto* expect = R"(
struct S_1 {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S_1;

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  s.m = mat2x2<f32>(u.m0, u.m1);
}
)";

    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, UniformAndPrivateUsage) {
    DataMap data;
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S;

var<private> p : S;

fn f() {
  p.m = u.m;
}

)";

    auto* expect = R"(
struct S_1 {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S_1;

var<private> p : S;

fn f() {
  p.m = mat2x2<f32>(u.m0, u.m1);
}
)";

    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, StorageAndPrivateUsage) {
    DataMap data;
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<storage, read_write> s : S;

var<private> p : S;

fn f() {
  p.m = s.m;
}

)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<storage, read_write> s : S;

var<private> p : S;

fn f() {
  p.m = s.m;
}
)";

    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, UniformAndStorageUsageInAForLoop) {
    DataMap data;
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S;

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  for (s.m = u.m; ; ) {
    break;
  }
}

)";

    auto* expect = R"(
struct S_1 {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> u : S_1;

@group(0) @binding(1) var<storage, read_write> s : S;

fn f() {
  for(s.m = mat2x2<f32>(u.m0, u.m1); ; ) {
    break;
  }
}
)";

    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
