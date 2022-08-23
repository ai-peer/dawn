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
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;
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
  m__0 : vec2<f32>,
  m__1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_Load) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn f() {
  let I = 0;

  let l_s = s;
  let l_s_m = s.m;
  let l_s_m_0 = s.m[0];
  let l_s_m_1 = s.m[1];
  let l_s_m_I = s.m[I];
  let l_s_m_0_0 = s.m[0][0];
  let l_s_m_1_0 = s.m[1][0];
  let l_s_m_I_0 = s.m[I][0];
  let l_s_m_0_1 = s.m[0][1];
  let l_s_m_1_1 = s.m[1][1];
  let l_s_m_I_1 = s.m[I][1];
  let l_s_m_0_I = s.m[0][I];
  let l_s_m_1_I = s.m[1][I];
  let l_s_m_I_I = s.m[I][I];
}
)";

    auto* expect = R"(
struct S {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> s : S;

fn load_s_m() -> mat2x2<f32> {
  let l = &(s);
  return mat2x2((*(l)).m_0, (*(l)).m_1);
}

fn load_s_m_0() -> vec2<f32> {
  let l_1 = &(s);
  return mat2x2((*(l_1)).m_0, (*(l_1)).m_1)[0u];
}

fn load_s_m_1() -> vec2<f32> {
  let l_2 = &(s);
  return mat2x2((*(l_2)).m_0, (*(l_2)).m_1)[1u];
}

fn load_s_m_p0(p0 : i32) -> vec2<f32> {
  let l_3 = &(s);
  return mat2x2((*(l_3)).m_0, (*(l_3)).m_1)[p0];
}

fn load_s_m_0_0() -> f32 {
  let l_4 = &(s);
  return mat2x2((*(l_4)).m_0, (*(l_4)).m_1)[0u][0u];
}

fn load_s_m_1_0() -> f32 {
  let l_5 = &(s);
  return mat2x2((*(l_5)).m_0, (*(l_5)).m_1)[1u][0u];
}

fn load_s_m_p0_0(p0 : i32) -> f32 {
  let l_6 = &(s);
  return mat2x2((*(l_6)).m_0, (*(l_6)).m_1)[p0][0u];
}

fn load_s_m_0_1() -> f32 {
  let l_7 = &(s);
  return mat2x2((*(l_7)).m_0, (*(l_7)).m_1)[0u][1u];
}

fn load_s_m_1_1() -> f32 {
  let l_8 = &(s);
  return mat2x2((*(l_8)).m_0, (*(l_8)).m_1)[1u][1u];
}

fn load_s_m_p0_1(p0 : i32) -> f32 {
  let l_9 = &(s);
  return mat2x2((*(l_9)).m_0, (*(l_9)).m_1)[p0][1u];
}

fn load_s_m_0_p0(p0 : i32) -> f32 {
  let l_10 = &(s);
  return mat2x2((*(l_10)).m_0, (*(l_10)).m_1)[0u][p0];
}

fn load_s_m_1_p0(p0 : i32) -> f32 {
  let l_11 = &(s);
  return mat2x2((*(l_11)).m_0, (*(l_11)).m_1)[1u][p0];
}

fn load_s_m_p0_p1(p0 : i32, p1 : i32) -> f32 {
  let l_12 = &(s);
  return mat2x2((*(l_12)).m_0, (*(l_12)).m_1)[p0][p1];
}

fn f() {
  let I = 0;
  let l_s = s;
  let l_s_m = load_s_m();
  let l_s_m_0 = load_s_m_0();
  let l_s_m_1 = load_s_m_1();
  let l_s_m_I = load_s_m_p0(I);
  let l_s_m_0_0 = load_s_m_0_0();
  let l_s_m_1_0 = load_s_m_1_0();
  let l_s_m_I_0 = load_s_m_p0_0(I);
  let l_s_m_0_1 = load_s_m_0_1();
  let l_s_m_1_1 = load_s_m_1_1();
  let l_s_m_I_1 = load_s_m_p0_1(I);
  let l_s_m_0_I = load_s_m_0_p0(I);
  let l_s_m_1_I = load_s_m_1_p0(I);
  let l_s_m_I_I = load_s_m_p0_p1(I, I);
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, StructMat2x2Uniform_Store) {
    auto* src = R"(
struct S {
  m : mat2x2<f32>,
}

@group(0) @binding(0) var<storage, read_write> s : S;

@group(0) @binding(1) var<uniform> u : S;

fn f() {
  let I = 0;

  let l_s = S();
  let l_s_m = mat2x2<f32>();
  let l_s_m_0 = vec2<f32>();
  let l_s_m_1 = vec2<f32>();
  let l_s_m_I = vec2<f32>();
  let l_s_m_0_0 = 0.0;
  let l_s_m_1_0 = 0.0;
  let l_s_m_I_0 = 0.0;
  let l_s_m_0_1 = 0.0;
  let l_s_m_1_1 = 0.0;
  let l_s_m_I_1 = 0.0;
  let l_s_m_0_I = 0.0;
  let l_s_m_1_I = 0.0;
  let l_s_m_I_I = 0.0;

  s = l_s;
  s.m = l_s_m;
  s.m[0] = l_s_m_0;
  s.m[1] = l_s_m_1;
  s.m[I] = l_s_m_I;
  s.m[0][0] = l_s_m_0_0;
  s.m[1][0] = l_s_m_1_0;
  s.m[I][0] = l_s_m_I_0;
  s.m[0][1] = l_s_m_0_1;
  s.m[1][1] = l_s_m_1_1;
  s.m[I][1] = l_s_m_I_1;
  s.m[0][I] = l_s_m_0_I;
  s.m[1][I] = l_s_m_1_I;
  s.m[I][I] = l_s_m_I_I;
}
)";

    auto* expect = R"(error: cannot assign to value of type 'mat2x2<f32>')";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x2Uniform_Load) {
    auto* src = R"(
struct S {
  @size(64) m : mat3x2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let I = 0;

  let l_a = a;
  let l_a_0 = a[0];
  let l_a_1 = a[1];
  let l_a_I = a[I];
  let l_a_0_m = a[0].m;
  let l_a_1_m = a[1].m;
  let l_a_I_m = a[I].m;
  let l_a_0_m_0 = a[0].m[0];
  let l_a_1_m_0 = a[1].m[0];
  let l_a_I_m_0 = a[I].m[0];
  let l_a_0_m_1 = a[0].m[1];
  let l_a_1_m_1 = a[1].m[1];
  let l_a_I_m_1 = a[I].m[1];
  let l_a_0_m_I = a[0].m[I];
  let l_a_1_m_I = a[1].m[I];
  let l_a_I_m_I = a[I].m[I];
}
)";

    auto* expect =
        R"(
struct S {
  m_0 : vec2<f32>,
  m_1 : vec2<f32>,
  @size(48)
  m_2 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn load_a_0_m() -> mat3x2<f32> {
  let l = &(a[0u]);
  return mat3x2((*(l)).m_0, (*(l)).m_1, (*(l)).m_2);
}

fn load_a_1_m() -> mat3x2<f32> {
  let l_1 = &(a[1u]);
  return mat3x2((*(l_1)).m_0, (*(l_1)).m_1, (*(l_1)).m_2);
}

fn load_a_p0_m(p0 : i32) -> mat3x2<f32> {
  let l_2 = &(a[p0]);
  return mat3x2((*(l_2)).m_0, (*(l_2)).m_1, (*(l_2)).m_2);
}

fn load_a_0_m_0() -> vec2<f32> {
  let l_3 = &(a[0u]);
  return mat3x2((*(l_3)).m_0, (*(l_3)).m_1, (*(l_3)).m_2)[0u];
}

fn load_a_1_m_0() -> vec2<f32> {
  let l_4 = &(a[1u]);
  return mat3x2((*(l_4)).m_0, (*(l_4)).m_1, (*(l_4)).m_2)[0u];
}

fn load_a_p0_m_0(p0 : i32) -> vec2<f32> {
  let l_5 = &(a[p0]);
  return mat3x2((*(l_5)).m_0, (*(l_5)).m_1, (*(l_5)).m_2)[0u];
}

fn load_a_0_m_1() -> vec2<f32> {
  let l_6 = &(a[0u]);
  return mat3x2((*(l_6)).m_0, (*(l_6)).m_1, (*(l_6)).m_2)[1u];
}

fn load_a_1_m_1() -> vec2<f32> {
  let l_7 = &(a[1u]);
  return mat3x2((*(l_7)).m_0, (*(l_7)).m_1, (*(l_7)).m_2)[1u];
}

fn load_a_p0_m_1(p0 : i32) -> vec2<f32> {
  let l_8 = &(a[p0]);
  return mat3x2((*(l_8)).m_0, (*(l_8)).m_1, (*(l_8)).m_2)[1u];
}

fn load_a_0_m_p0(p0 : i32) -> vec2<f32> {
  let l_9 = &(a[0u]);
  return mat3x2((*(l_9)).m_0, (*(l_9)).m_1, (*(l_9)).m_2)[p0];
}

fn load_a_1_m_p0(p0 : i32) -> vec2<f32> {
  let l_10 = &(a[1u]);
  return mat3x2((*(l_10)).m_0, (*(l_10)).m_1, (*(l_10)).m_2)[p0];
}

fn load_a_p0_m_p1(p0 : i32, p1 : i32) -> vec2<f32> {
  let l_11 = &(a[p0]);
  return mat3x2((*(l_11)).m_0, (*(l_11)).m_1, (*(l_11)).m_2)[p1];
}

fn f() {
  let I = 0;
  let l_a = a;
  let l_a_0 = a[0];
  let l_a_1 = a[1];
  let l_a_I = a[I];
  let l_a_0_m = load_a_0_m();
  let l_a_1_m = load_a_1_m();
  let l_a_I_m = load_a_p0_m(I);
  let l_a_0_m_0 = load_a_0_m_0();
  let l_a_1_m_0 = load_a_1_m_0();
  let l_a_I_m_0 = load_a_p0_m_0(I);
  let l_a_0_m_1 = load_a_0_m_1();
  let l_a_1_m_1 = load_a_1_m_1();
  let l_a_I_m_1 = load_a_p0_m_1(I);
  let l_a_0_m_I = load_a_0_m_p0(I);
  let l_a_1_m_I = load_a_1_m_p0(I);
  let l_a_I_m_I = load_a_p0_m_p1(I, I);
}
)";

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Std140Test, ArrayStructMat3x3Uniform_Load) {
    auto* src = R"(
struct S {
  @size(64)
  m : mat3x3<f32>,
}

@group(0) @binding(0) var<uniform> a : array<S, 3>;

fn f() {
  let I = 0;
  let l_a = a;
  let l_a_0 = a[0];
  let l_a_1 = a[1];
  let l_a_I = a[I];
  let l_a_0_m = a[0].m;
  let l_a_1_m = a[1].m;
  let l_a_I_m = a[I].m;
  let l_a_0_m_0 = a[0].m[0];
  let l_a_1_m_0 = a[1].m[0];
  let l_a_I_m_0 = a[I].m[0];
  let l_a_0_m_1 = a[0].m[1];
  let l_a_1_m_1 = a[1].m[1];
  let l_a_I_m_1 = a[I].m[1];
  let l_a_0_m_I = a[0].m[I];
  let l_a_1_m_I = a[1].m[I];
  let l_a_I_m_I = a[I].m[I];
}
)";

    auto* expect = src;

    auto got = Run<Std140>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
