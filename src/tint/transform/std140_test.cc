// Copyright 2021 The Tint Authors.
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

#include "src/tint/transform/std140.h"

#include <utility>

#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using Std140Test = TransformTest;

TEST_F(Std140Test, ShouldRunEmptyModule) {
    auto* src = R"()";

    EXPECT_FALSE(ShouldRun<Std140>(src));
}

TEST_F(Std140Test, ShouldRunStructMat2x2Unused) {
    auto* src = R"(
struct Unused {
  m : mat2x2<f32>,
}
)";

    EXPECT_FALSE(ShouldRun<Std140>(src));
}

TEST_F(Std140Test, ShouldRunStructMat2x2Storage) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<storage> s : S;
)";

    EXPECT_FALSE(ShouldRun<Std140>(src));
}

TEST_F(Std140Test, ShouldRunStructMat2x2Uniform) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;
)";

    EXPECT_TRUE(ShouldRun<Std140>(src));
}

TEST_F(Std140Test, ShouldRunArrayMat2x2Storage) {
    auto* src = R"(
@group(0) @binding(0) var<storage> s : array<mat2x2<f32>, 2>;
)";

    EXPECT_FALSE(ShouldRun<Std140>(src));
}

TEST_F(Std140Test, ShouldRunArrayMat2x2Uniform) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> s : array<mat2x2<f32>, 2>;
)";

    EXPECT_FALSE(ShouldRun<Std140>(src));
}

TEST_F(Std140Test, EmptyModule) {
    auto* src = R"()";

    auto* expect = R"()";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_NameCollision) {
    auto* src = R"(
struct S {
  m_1 : i32,
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;
)";

    auto* expect = R"(
struct S {
  m_1 : i32,
  m : mat2x2<f32>,
}

struct S_std140 {
  m_1 : i32,
  m__0 : vec2<f32>,
  m__1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadStruct) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let l = s;
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn conv_S(val : S_std140) -> S {
  return S(mat2x2<f32>(val.m_0, val.m_1));
}

fn f() {
  let l = conv_S(s);
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadMatrix) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let l = s.m;
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn load_s_m() -> mat2x2<f32> {
  let s = &(s);
  return mat2x2<f32>((*(s)).m_0, (*(s)).m_1);
}

fn f() {
  let l = load_s_m();
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadColumn0) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let l = s.m[0];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn f() {
  let l = s.m_0;
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadColumn1) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let l = s.m[1];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn f() {
  let l = s.m_1;
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadColumnI) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let I = 0;
  let l = s.m[I];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn load_s_p0(p0 : u32) -> vec2<f32> {
  switch(p0) {
    case 0u: {
      return s.m_0;
    }
    case 1u: {
      return s.m_1;
    }
    default: {
      return vec2<f32>();
    }
  }
}

fn f() {
  let I = 0;
  let l = load_s_p0(u32(I));
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalar00) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let l = s.m[0][0];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn f() {
  let l = s.m_0[0u];
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalar10) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let l = s.m[1][0];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn f() {
  let l = s.m_1[0u];
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalarI0) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let I = 0;
  let l = s.m[I][0];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn load_s_p0_0(p0 : u32) -> f32 {
  switch(p0) {
    case 0u: {
      return s.m_0[0u];
    }
    case 1u: {
      return s.m_1[0u];
    }
    default: {
      return f32();
    }
  }
}

fn f() {
  let I = 0;
  let l = load_s_p0_0(u32(I));
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalar01) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let l = s.m[0][1];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn f() {
  let l = s.m_0[1u];
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalar11) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let l = s.m[1][1];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn f() {
  let l = s.m_1[1u];
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalarI1) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let I = 0;
  let l = s.m[I][1];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn load_s_p0_1(p0 : u32) -> f32 {
  switch(p0) {
    case 0u: {
      return s.m_0[1u];
    }
    case 1u: {
      return s.m_1[1u];
    }
    default: {
      return f32();
    }
  }
}

fn f() {
  let I = 0;
  let l = load_s_p0_1(u32(I));
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalar0I) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let I = 0;
  let l = s.m[0][I];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn f() {
  let I = 0;
  let l = s.m_0[I];
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalar1I) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let I = 0;
  let l = s.m[1][I];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn f() {
  let I = 0;
  let l = s.m_1[I];
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_LoadScalarII) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let I = 0;
  let l = s.m[I][I];
}
)";

    auto* expect = R"(
struct S {
  m : mat2x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S_std140;

fn load_s_p0_p0(p0 : u32, p1 : u32) -> f32 {
  switch(p0) {
    case 0u: {
      return s.m_0[p0];
    }
    case 1u: {
      return s.m_1[p0];
    }
    default: {
      return f32();
    }
  }
}

fn f() {
  let I = 0;
  let l = load_s_p0_p0(u32(I), u32(I));
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadArray) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a;
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn conv_S(val : S_std140) -> S {
  return S(mat3x2<f32>(val.m_0, val.m_1, val.m_2));
}

fn conv_arr_3_S(val : array<S_std140, 3u>) -> array<S, 3u> {
  var arr : array<S, 3u>;
  for(var i : u32; (i < 3u); i++) {
    arr[i] = conv_S(val[i]);
  }
  return arr;
}

fn f() {
  let l = conv_arr_3_S(a);
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadStruct0) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a[0];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn conv_S(val : S_std140) -> S {
  return S(mat3x2<f32>(val.m_0, val.m_1, val.m_2));
}

fn f() {
  let l = conv_S(a[0u]);
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadStruct1) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a[1];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn conv_S(val : S_std140) -> S {
  return S(mat3x2<f32>(val.m_0, val.m_1, val.m_2));
}

fn f() {
  let l = conv_S(a[1u]);
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadStructI) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let I = 1;
  let l = a[I];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn conv_S(val : S_std140) -> S {
  return S(mat3x2<f32>(val.m_0, val.m_1, val.m_2));
}

fn f() {
  let I = 1;
  let l = conv_S(a[I]);
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrix0) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a[0].m;
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn load_a_0_m() -> mat3x2<f32> {
  let s = &(a[0u]);
  return mat3x2<f32>((*(s)).m_0, (*(s)).m_1, (*(s)).m_2);
}

fn f() {
  let l = load_a_0_m();
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrix1) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a[1].m;
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn load_a_1_m() -> mat3x2<f32> {
  let s = &(a[1u]);
  return mat3x2<f32>((*(s)).m_0, (*(s)).m_1, (*(s)).m_2);
}

fn f() {
  let l = load_a_1_m();
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrixI) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let I = 1;
  let l = a[I].m;
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn load_a_p0_m(p0 : u32) -> mat3x2<f32> {
  let s = &(a[p0]);
  return mat3x2<f32>((*(s)).m_0, (*(s)).m_1, (*(s)).m_2);
}

fn f() {
  let I = 1;
  let l = load_a_p0_m(u32(I));
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrix0Column0) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a[0].m[0];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn f() {
  let l = a[0u].m_0;
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrix1Column0) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a[1].m[0];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn f() {
  let l = a[1u].m_0;
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrixIColumn0) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let I = 1;
  let l = a[I].m[0];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn f() {
  let I = 1;
  let l = a[I].m_0;
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrix0Column1) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a[0].m[1];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn f() {
  let l = a[0u].m_1;
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrix1Column1) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let l = a[1].m[1];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn f() {
  let l = a[1u].m_1;
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_LoadMatrixIColumnI) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let I = 1;
  let l = a[I].m[I];
}
)";

    auto* expect = R"(
struct S {
  @size(64)
  m : mat3x2<f32>,
}

struct S_std140 {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S_std140, 3u>;

fn load_a_p0_p0(p0 : u32, p1 : u32) -> vec2<f32> {
  switch(p0) {
    case 0u: {
      return a[p0].m_0;
    }
    case 1u: {
      return a[p0].m_1;
    }
    case 2u: {
      return a[p0].m_2;
    }
    default: {
      return vec2<f32>();
    }
  }
}

fn f() {
  let I = 1;
  let l = load_a_p0_p0(u32(I), u32(I));
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
