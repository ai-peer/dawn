// Copyright 2022 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
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

// TEST_F(DirectVariableAccessTest, ShouldRunEmptyModule) {
//   auto* src = R"()";
//
//   EXPECT_FALSE(ShouldRun<DirectVariableAccess>(src));
// }

TEST_F(DirectVariableAccessTest, Param_ptr_unused) {
    auto* src = R"(
var<private> keep_me = 42;

fn u(p : ptr<uniform, i32>) -> i32 {
  return *p;
}

fn s(p : ptr<storage, i32>) -> i32 {
  return *p;
}

fn w(p : ptr<workgroup, i32>) -> i32 {
  return *p;
}

fn f(p : ptr<function, i32>) -> i32 {
  return *p;
}
)";

    auto* expect = R"(
var<private> keep_me = 42;

fn f(p : ptr<function, i32>) -> i32 {
  return *(p);
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Param_ptr_uniform_i32_read) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : i32;

fn a(p : ptr<uniform, i32>) -> i32 {
  return *p;
}

fn b() {
  a(&U);
}
)";

    auto* expect = R"(
@group(0) @binding(0) var<uniform> U : i32;

fn a_U() -> i32 {
  return U;
}

fn b() {
  a_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Param_ptr_storage_i32_Via_struct_read) {
    auto* src = R"(
struct str {
  i : i32,
};

@group(0) @binding(0) var<storage> S : str;

fn a(p : ptr<storage, i32>) -> i32 {
  return *p;
}

fn b() {
  a(&S.i);
}
)";

    auto* expect = R"(
struct str {
  i : i32,
}

@group(0) @binding(0) var<storage> S : str;

fn a_S_i() -> i32 {
  return S.i;
}

fn b() {
  a_S_i();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Param_ptr_storage_arr_i32_Via_struct_write) {
    auto* src = R"(
struct str {
  arr : array<i32, 4>,
};

@group(0) @binding(0) var<storage, read_write> S : str;

fn a(p : ptr<storage, array<i32, 4>, read_write>) {
  *p = array<i32, 4>();
}

fn b() {
  a(&S.arr);
}
)";

    auto* expect = R"(
struct str {
  arr : array<i32, 4>,
}

@group(0) @binding(0) var<storage, read_write> S : str;

fn a_S_arr() {
  S.arr = array<i32, 4>();
}

fn b() {
  a_S_arr();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Param_ptr_workgroup_vec4i32_Via_array_StaticRead) {
    auto* src = R"(
var<workgroup> W : array<vec4<i32>, 8>;

fn a(p : ptr<workgroup, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  a(&W[3]);
}
)";

    auto* expect =
        R"(
var<workgroup> W : array<vec4<i32>, 8>;

type W_X = array<u32, 1u>;

fn a_W_X(p : W_X) -> vec4<i32> {
  return W[p[0]];
}

fn b() {
  a_W_X(W_X(3));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Param_ptr_workgroup_vec4i32_Via_array_StaticWrite) {
    auto* src = R"(
var<workgroup> W : array<vec4<i32>, 8>;

fn a(p : ptr<workgroup, vec4<i32>>) {
  *p = vec4<i32>();
}

fn b() {
  a(&W[3]);
}
)";

    auto* expect =
        R"(
var<workgroup> W : array<vec4<i32>, 8>;

type W_X = array<u32, 1u>;

fn a_W_X(p : W_X) {
  W[p[0]] = vec4<i32>();
}

fn b() {
  a_W_X(W_X(3));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Param_ptr_uniform_vec4i32_Via_array_DynamicRead) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<vec4<i32>, 8>;

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  let I = 3;
  a(&U[I]);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<vec4<i32>, 8>;

type U_X = array<u32, 1u>;

fn a_U_X(p : U_X) -> vec4<i32> {
  return U[p[0]];
}

fn b() {
  let I = 3;
  a_U_X(U_X(u32(I)));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Param_ptr_storage_vec4i32_Via_array_DynamicWrite) {
    auto* src = R"(
@group(0) @binding(0) var<storage, read_write> S : array<vec4<i32>, 8>;

fn a(p : ptr<storage, vec4<i32>, read_write>) {
  *p = vec4<i32>();
}

fn b() {
  let I = 3;
  a(&S[I]);
}
)";

    auto* expect = R"(
@group(0) @binding(0) var<storage, read_write> S : array<vec4<i32>, 8>;

type S_X = array<u32, 1u>;

fn a_S_X(p : S_X) {
  S[p[0]] = vec4<i32>();
}

fn b() {
  let I = 3;
  a_S_X(S_X(u32(I)));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Param_ptr_mixed_vec4i32_ViaMultiple) {
    auto* src = R"(
struct str {
  i : vec4<i32>,
};

@group(0) @binding(0) var<uniform> U     : vec4<i32>;
@group(0) @binding(1) var<uniform> U_str   : str;
@group(0) @binding(2) var<uniform> U_arr   : array<vec4<i32>, 8>;
@group(0) @binding(3) var<uniform> U_arr_arr : array<array<vec4<i32>, 8>, 4>;

@group(1) @binding(0) var<storage> S     : vec4<i32>;
@group(1) @binding(1) var<storage> S_str   : str;
@group(1) @binding(2) var<storage> S_arr   : array<vec4<i32>, 8>;
@group(1) @binding(3) var<storage> S_arr_arr : array<array<vec4<i32>, 8>, 4>;

          var<workgroup> W     : vec4<i32>;
          var<workgroup> W_str   : str;
          var<workgroup> W_arr   : array<vec4<i32>, 8>;
          var<workgroup> W_arr_arr : array<array<vec4<i32>, 8>, 4>;

fn fn_u(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn fn_s(p : ptr<storage, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn fn_w(p : ptr<workgroup, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  let I = 3;
  let J = 4;

  let u           = fn_u(&U);
  let u_str       = fn_u(&U_str.i);
  let u_arr0      = fn_u(&U_arr[0]);
  let u_arr1      = fn_u(&U_arr[1]);
  let u_arrI      = fn_u(&U_arr[I]);
  let u_arr1_arr0 = fn_u(&U_arr_arr[1][0]);
  let u_arr2_arrI = fn_u(&U_arr_arr[2][I]);
  let u_arrI_arr2 = fn_u(&U_arr_arr[I][2]);
  let u_arrI_arrJ = fn_u(&U_arr_arr[I][J]);

  let s           = fn_s(&S);
  let s_str       = fn_s(&S_str.i);
  let s_arr0      = fn_s(&S_arr[0]);
  let s_arr1      = fn_s(&S_arr[1]);
  let s_arrI      = fn_s(&S_arr[I]);
  let s_arr1_arr0 = fn_s(&S_arr_arr[1][0]);
  let s_arr2_arrI = fn_s(&S_arr_arr[2][I]);
  let s_arrI_arr2 = fn_s(&S_arr_arr[I][2]);
  let s_arrI_arrJ = fn_s(&S_arr_arr[I][J]);

  let w           = fn_w(&W);
  let w_str       = fn_w(&W_str.i);
  let w_arr0      = fn_w(&W_arr[0]);
  let w_arr1      = fn_w(&W_arr[1]);
  let w_arrI      = fn_w(&W_arr[I]);
  let w_arr1_arr0 = fn_w(&W_arr_arr[1][0]);
  let w_arr2_arrI = fn_w(&W_arr_arr[2][I]);
  let w_arrI_arr2 = fn_w(&W_arr_arr[I][2]);
  let w_arrI_arrJ = fn_w(&W_arr_arr[I][J]);
}
)";

    auto* expect =
        R"(
struct str {
  i : vec4<i32>,
}

@group(0) @binding(0) var<uniform> U : vec4<i32>;

@group(0) @binding(1) var<uniform> U_str : str;

@group(0) @binding(2) var<uniform> U_arr : array<vec4<i32>, 8>;

@group(0) @binding(3) var<uniform> U_arr_arr : array<array<vec4<i32>, 8>, 4>;

@group(1) @binding(0) var<storage> S : vec4<i32>;

@group(1) @binding(1) var<storage> S_str : str;

@group(1) @binding(2) var<storage> S_arr : array<vec4<i32>, 8>;

@group(1) @binding(3) var<storage> S_arr_arr : array<array<vec4<i32>, 8>, 4>;

var<workgroup> W : vec4<i32>;

var<workgroup> W_str : str;

var<workgroup> W_arr : array<vec4<i32>, 8>;

var<workgroup> W_arr_arr : array<array<vec4<i32>, 8>, 4>;

fn fn_u_U() -> vec4<i32> {
  return U;
}

fn fn_u_U_str_i() -> vec4<i32> {
  return U_str.i;
}

type U_arr_X = array<u32, 1u>;

fn fn_u_U_arr_X(p : U_arr_X) -> vec4<i32> {
  return U_arr[p[0]];
}

type U_arr_arr_X_X = array<u32, 2u>;

fn fn_u_U_arr_arr_X_X(p : U_arr_arr_X_X) -> vec4<i32> {
  return U_arr_arr[p[0]][p[1]];
}

fn fn_s_S() -> vec4<i32> {
  return S;
}

fn fn_s_S_str_i() -> vec4<i32> {
  return S_str.i;
}

type S_arr_X = array<u32, 1u>;

fn fn_s_S_arr_X(p : S_arr_X) -> vec4<i32> {
  return S_arr[p[0]];
}

type S_arr_arr_X_X = array<u32, 2u>;

fn fn_s_S_arr_arr_X_X(p : S_arr_arr_X_X) -> vec4<i32> {
  return S_arr_arr[p[0]][p[1]];
}

fn fn_w_W() -> vec4<i32> {
  return W;
}

fn fn_w_W_str_i() -> vec4<i32> {
  return W_str.i;
}

type W_arr_X = array<u32, 1u>;

fn fn_w_W_arr_X(p : W_arr_X) -> vec4<i32> {
  return W_arr[p[0]];
}

type W_arr_arr_X_X = array<u32, 2u>;

fn fn_w_W_arr_arr_X_X(p : W_arr_arr_X_X) -> vec4<i32> {
  return W_arr_arr[p[0]][p[1]];
}

fn b() {
  let I = 3;
  let J = 4;
  let u = fn_u_U();
  let u_str = fn_u_U_str_i();
  let u_arr0 = fn_u_U_arr_X(U_arr_X(0));
  let u_arr1 = fn_u_U_arr_X(U_arr_X(1));
  let u_arrI = fn_u_U_arr_X(U_arr_X(u32(I)));
  let u_arr1_arr0 = fn_u_U_arr_arr_X_X(U_arr_arr_X_X(1, 0));
  let u_arr2_arrI = fn_u_U_arr_arr_X_X(U_arr_arr_X_X(2, u32(I)));
  let u_arrI_arr2 = fn_u_U_arr_arr_X_X(U_arr_arr_X_X(u32(I), 2));
  let u_arrI_arrJ = fn_u_U_arr_arr_X_X(U_arr_arr_X_X(u32(I), u32(J)));
  let s = fn_s_S();
  let s_str = fn_s_S_str_i();
  let s_arr0 = fn_s_S_arr_X(S_arr_X(0));
  let s_arr1 = fn_s_S_arr_X(S_arr_X(1));
  let s_arrI = fn_s_S_arr_X(S_arr_X(u32(I)));
  let s_arr1_arr0 = fn_s_S_arr_arr_X_X(S_arr_arr_X_X(1, 0));
  let s_arr2_arrI = fn_s_S_arr_arr_X_X(S_arr_arr_X_X(2, u32(I)));
  let s_arrI_arr2 = fn_s_S_arr_arr_X_X(S_arr_arr_X_X(u32(I), 2));
  let s_arrI_arrJ = fn_s_S_arr_arr_X_X(S_arr_arr_X_X(u32(I), u32(J)));
  let w = fn_w_W();
  let w_str = fn_w_W_str_i();
  let w_arr0 = fn_w_W_arr_X(W_arr_X(0));
  let w_arr1 = fn_w_W_arr_X(W_arr_X(1));
  let w_arrI = fn_w_W_arr_X(W_arr_X(u32(I)));
  let w_arr1_arr0 = fn_w_W_arr_arr_X_X(W_arr_arr_X_X(1, 0));
  let w_arr2_arrI = fn_w_W_arr_arr_X_X(W_arr_arr_X_X(2, u32(I)));
  let w_arrI_arr2 = fn_w_W_arr_arr_X_X(W_arr_arr_X_X(u32(I), 2));
  let w_arrI_arrJ = fn_w_W_arr_arr_X_X(W_arr_arr_X_X(u32(I), u32(J)));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, CallChaining) {
    auto* src = R"(
struct Inner {
  mat : mat3x4<f32>,
};

type InnerArr = array<Inner, 4>;

struct Outer {
  arr : InnerArr,
  mat : mat3x4<f32>,
};

@group(0) @binding(0) var<storage> S : Outer;

fn f0(p : ptr<storage, vec4<f32>>) -> f32 {
  return (*p).x;
}

fn f1(p : ptr<storage, mat3x4<f32>>) -> f32 {
  var res : f32;
  {
    // call f0() with inline usage of p
    res += f0(&(*p)[1]);
  }
  {
    // call f0() with pointer-let usage of p
    let p_vec = &(*p)[1];
    res += f0(p_vec);
  }
  {
    // call f0() with inline usage of S
    res += f0(&S.arr[2].mat[1]);
  }
  {
    // call f0() with pointer-let usage of S
    let p_vec = &S.arr[2].mat[1];
    res += f0(p_vec);
  }
  return res;
}

fn f2(p : ptr<storage, Inner>) -> f32 {
  let p_mat = &(*p).mat;
  return f1(p_mat);
}

fn f3(p0 : ptr<storage, InnerArr>, p1 : ptr<storage, mat3x4<f32>>) -> f32 {
  let p0_inner = &(*p0)[3];
  return f2(p0_inner) + f1(p1);
}

fn f4(p : ptr<storage, Outer>) -> f32 {
  return f3(&(*p).arr, &S.mat);
}

fn b() {
  f4(&S);
}
)";

    auto* expect =
        R"(
struct Inner {
  mat : mat3x4<f32>,
}

type InnerArr = array<Inner, 4>;

struct Outer {
  arr : InnerArr,
  mat : mat3x4<f32>,
}

@group(0) @binding(0) var<storage> S : Outer;

type S_mat_X = array<u32, 1u>;

fn f0_S_mat_X(p : S_mat_X) -> f32 {
  return S.mat[p[0]].x;
}

type S_arr_X_mat_X = array<u32, 2u>;

fn f0_S_arr_X_mat_X(p : S_arr_X_mat_X) -> f32 {
  return S.arr[p[0]].mat[p[0]].x;
}

type S_arr_X_mat_X_1 = array<u32, 2u>;

fn f0_S_arr_X_mat_X_1(p : S_arr_X_mat_X_1) -> f32 {
  return S.arr[p[0]].mat[p[1]].x;
}

fn f1_S_mat() -> f32 {
  var res : f32;
  {
    res += f0_S_mat_X(S_mat_X(1));
  }
  {
    let p_vec = &(S.mat[1]);
    res += f0_S_mat_X(S_mat_X(1));
  }
  {
    res += f0_S_arr_X_mat_X_1(S_arr_X_mat_X_1(2, 1));
  }
  {
    let p_vec = &(S.arr[2].mat[1]);
    res += f0_S_arr_X_mat_X_1(S_arr_X_mat_X_1(2, 1));
  }
  return res;
}

type S_arr_X_mat = array<u32, 1u>;

fn f1_S_arr_X_mat(p : S_arr_X_mat) -> f32 {
  var res : f32;
  {
    res += f0_S_arr_X_mat_X(S_arr_X_mat_X(p[0u], 1));
  }
  {
    let p_vec = &(S.arr[p[0]].mat[1]);
    res += f0_S_arr_X_mat_X(S_arr_X_mat_X(p[0u], 1));
  }
  {
    res += f0_S_arr_X_mat_X_1(S_arr_X_mat_X_1(2, 1));
  }
  {
    let p_vec = &(S.arr[2].mat[1]);
    res += f0_S_arr_X_mat_X_1(S_arr_X_mat_X_1(2, 1));
  }
  return res;
}

type S_arr_X = array<u32, 1u>;

fn f2_S_arr_X(p : S_arr_X) -> f32 {
  let p_mat = &(S.arr[p[0]].mat);
  return f1_S_arr_X_mat(S_arr_X_mat(p[0u]));
}

fn f3_S_arr_S_mat() -> f32 {
  let p0_inner = &(S.arr[3]);
  return (f2_S_arr_X(S_arr_X(3)) + f1_S_mat());
}

fn f4_S() -> f32 {
  return f3_S_arr_S_mat();
}

fn b() {
  f4_S();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, ComplexIndexing) {
    auto* src = R"(
@group(0) @binding(0) var<storage> S : array<array<array<array<i32, 9>, 9>, 9>, 50>;

fn a(i : i32) -> i32 { return i; }

fn b(p : ptr<storage, array<array<array<i32, 9>, 9>, 9>>) -> i32 {
  return (*p) [ a( (*p)[0][1][2]    )]
              [ a( (*p)[a(3)][4][5] )]
              [ a( (*p)[6][a(7)][8] )];
}

fn c() {
  let v = b(&S[42]);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<storage> S : array<array<array<array<i32, 9>, 9>, 9>, 50>;

fn a(i : i32) -> i32 {
  return i;
}

type S_X = array<u32, 1u>;

fn b_S_X(p : S_X) -> i32 {
  let tint_symbol = a(S[p[0]][0][1][2]);
  let tint_symbol_1 = a(3);
  let tint_symbol_2 = a(S[p[0]][u32(tint_symbol_1)][4][5]);
  let tint_symbol_3 = a(7);
  let tint_symbol_4 = a(S[p[0]][6][u32(tint_symbol_3)][8]);
  return S[p[0]][u32(tint_symbol)][u32(tint_symbol_2)][u32(tint_symbol_4)];
}

fn c() {
  let v = b_S_X(S_X(42));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, ComplexIndexingInPtrCall) {
    auto* src = R"(
@group(0) @binding(0) var<storage> S : array<array<array<array<i32, 9>, 9>, 9>, 50>;

fn a(i : ptr<storage, i32>) -> i32 {
  return *i;
}

fn b(p : ptr<storage, array<array<array<i32, 9>, 9>, 9>>) -> i32 {
  return a(&(*p)[ a( &(*p)[0][1][2] )]
                [ a( &(*p)[3][4][5] )]
                [ a( &(*p)[6][7][8] )]);
}

fn c() {
  let v = b(&S[42]);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<storage> S : array<array<array<array<i32, 9>, 9>, 9>, 50>;

type S_X_X_X_X = array<u32, 4u>;

fn a_S_X_X_X_X(i : S_X_X_X_X) -> i32 {
  return S[i[0]][i[0]][i[1]][i[2]];
}

type S_X = array<u32, 1u>;

fn b_S_X(p : S_X) -> i32 {
  let tint_symbol = a_S_X_X_X_X(S_X_X_X_X(p[0u], 0, 1, 2));
  let tint_symbol_1 = a_S_X_X_X_X(S_X_X_X_X(p[0u], 3, 4, 5));
  let tint_symbol_2 = a_S_X_X_X_X(S_X_X_X_X(p[0u], 6, 7, 8));
  return a_S_X_X_X_X(S_X_X_X_X(p[0u], u32(tint_symbol), u32(tint_symbol_1), u32(tint_symbol_2)));
}

fn c() {
  let v = b_S_X(S_X(42));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, ComplexIndexingDualPointers) {
    auto* src = R"(
@group(0) @binding(0) var<storage> S : array<array<array<i32, 9>, 9>, 50>;
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 9>, 9>, 50>;

fn a(i : i32) -> i32 { return i; }

fn b(s : ptr<storage, array<array<i32, 9>, 9>>,
     u : ptr<uniform, array<array<vec4<i32>, 9>, 9>>) -> i32 {
  return (*s) [ a( (*u)[0][1].x    )]
              [ a( (*u)[a(3)][4].y )];
}

fn c() {
  let v = b(&S[42], &U[24]);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<storage> S : array<array<array<i32, 9>, 9>, 50>;

@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 9>, 9>, 50>;

fn a(i : i32) -> i32 {
  return i;
}

type S_X = array<u32, 1u>;

type U_X = array<u32, 1u>;

fn b_S_X_U_X(s : S_X, u : U_X) -> i32 {
  let tint_symbol = a(U[u[0]][0][1].x);
  let tint_symbol_1 = a(3);
  let tint_symbol_2 = a(U[u[0]][u32(tint_symbol_1)][4].y);
  return S[s[0]][u32(tint_symbol)][u32(tint_symbol_2)];
}

fn c() {
  let v = b_S_X_U_X(S_X(42), U_X(24));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, FunctionPtr) {
    auto* src = R"(
fn f() {
  var v : i32;
  let p : ptr<function, i32> = &(v);
  var x : i32 = *(p);
}
)";

    auto* expect = src;  // Nothing changes

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, PtrChains_ConstantIndices) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  let p0 = &U;
  let p1 = &(*p0)[1];
  let p2 = &(*p1)[1+1];
  let p3 = &(*p2)[2*2 - 1];
  a(p3);
}

fn c(p : ptr<uniform, array<array<array<vec4<i32>, 8>, 8>, 8>>) {
  let p0 = p;
  let p1 = &(*p0)[1];
  let p2 = &(*p1)[1+1];
  let p3 = &(*p2)[2*2 - 1];
  a(p3);
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

type U_X_X_X = array<u32, 3u>;

fn a_U_X_X_X(p : U_X_X_X) -> vec4<i32> {
  return U[p[0]][p[1]][p[2]];
}

fn b() {
  let p0 = &(U);
  let p1 = &(U[1]);
  let p2 = &(U[1][2]);
  let p3 = &(U[1][2][3]);
  a_U_X_X_X(U_X_X_X(1, 2, 3));
}

fn c_U() {
  let p0 = &(U);
  let p1 = &(U[1]);
  let p2 = &(U[1][2]);
  let p3 = &(U[1][2][3]);
  a_U_X_X_X(U_X_X_X(1, 2, 3));
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, PtrChains_SideEffectingIndices) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

var<private> i : i32;
fn first() -> i32 {
  i++;
  return i;
}
fn second() -> i32 {
  i++;
  return i;
}
fn third() -> i32 {
  i++;
  return i;
}

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  let p0 = &U;
  let p1 = &(*p0)[first()];
  let p2 = &(*p1)[second()][third()];
  a(p2);
}

fn c(p : ptr<uniform, array<array<array<vec4<i32>, 8>, 8>, 8>>) {
  let p0 = &U;
  let p1 = &(*p0)[first()];
  let p2 = &(*p1)[second()][third()];
  a(p2);
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

var<private> i : i32;

fn first() -> i32 {
  i++;
  return i;
}

fn second() -> i32 {
  i++;
  return i;
}

fn third() -> i32 {
  i++;
  return i;
}

type U_X_X_X = array<u32, 3u>;

fn a_U_X_X_X(p : U_X_X_X) -> vec4<i32> {
  return U[p[0]][p[1]][p[2]];
}

fn b() {
  let p0 = &(U);
  let tint_symbol = first();
  let p1 = &(U[u32(tint_symbol)]);
  let tint_symbol_1 = second();
  let tint_symbol_2 = third();
  let p2 = &(U[u32(tint_symbol)][u32(tint_symbol_1)][u32(tint_symbol_2)]);
  a_U_X_X_X(U_X_X_X(u32(tint_symbol), u32(tint_symbol_1), u32(tint_symbol_2)));
}

fn c_U() {
  let p0 = &(U);
  let tint_symbol_3 = first();
  let p1 = &(U[u32(tint_symbol_3)]);
  let tint_symbol_4 = second();
  let tint_symbol_5 = third();
  let p2 = &(U[u32(tint_symbol_3)][u32(tint_symbol_4)][u32(tint_symbol_5)]);
  a_U_X_X_X(U_X_X_X(u32(tint_symbol_3), u32(tint_symbol_4), u32(tint_symbol_5)));
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, PtrChains_SideEffectInForLoopInit) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<vec4<i32>, 8>, 8>;

var<private> i : i32;
fn first() -> i32 {
  i++;
  return i;
}
fn second() -> i32 {
  i++;
  return i;
}

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  for (let p1 = &U[first()]; true; ) {
    a(&(*p1)[second()]);
  }
}

fn c(p : ptr<uniform, array<array<vec4<i32>, 8>, 8>>) {
  for (let p1 = &(*p)[first()]; true; ) {
    a(&(*p1)[second()]);
  }
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<vec4<i32>, 8>, 8>;

var<private> i : i32;

fn first() -> i32 {
  i++;
  return i;
}

fn second() -> i32 {
  i++;
  return i;
}

type U_X_X = array<u32, 2u>;

fn a_U_X_X(p : U_X_X) -> vec4<i32> {
  return U[p[0]][p[1]];
}

fn b() {
  let tint_symbol = first();
  for(let p1 = &(U[u32(tint_symbol)]); true; ) {
    let tint_symbol_1 = second();
    a_U_X_X(U_X_X(u32(tint_symbol), u32(tint_symbol_1)));
  }
}

fn c_U() {
  let tint_symbol_2 = first();
  for(let p1 = &(U[u32(tint_symbol_2)]); true; ) {
    let tint_symbol_3 = second();
    a_U_X_X(U_X_X(u32(tint_symbol_2), u32(tint_symbol_3)));
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, PtrChains_SideEffectInForLoopCond) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<vec4<i32>, 8>, 8>;

var<private> i : i32;
fn first() -> i32 {
  i++;
  return i;
}
fn second() -> i32 {
  i++;
  return i;
}

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  for (; a(&U[first()][second()]).x < 4; ) {
    let body = 1;
  }
}

fn c(p : ptr<uniform, array<array<vec4<i32>, 8>, 8>>) {
  for (; a(&(*p)[first()][second()]).x < 4; ) {
    let body = 1;
  }
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<vec4<i32>, 8>, 8>;

var<private> i : i32;

fn first() -> i32 {
  i++;
  return i;
}

fn second() -> i32 {
  i++;
  return i;
}

type U_X_X = array<u32, 2u>;

fn a_U_X_X(p : U_X_X) -> vec4<i32> {
  return U[p[0]][p[1]];
}

fn b() {
  loop {
    let tint_symbol = first();
    let tint_symbol_1 = second();
    if (!((a_U_X_X(U_X_X(u32(tint_symbol), u32(tint_symbol_1))).x < 4))) {
      break;
    }
    {
      let body = 1;
    }
  }
}

fn c_U() {
  loop {
    let tint_symbol_2 = first();
    let tint_symbol_3 = second();
    if (!((a_U_X_X(U_X_X(u32(tint_symbol_2), u32(tint_symbol_3))).x < 4))) {
      break;
    }
    {
      let body = 1;
    }
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, PtrChains_SideEffectInForLoopCont) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<vec4<i32>, 8>, 8>;

var<private> i : i32;
fn first() -> i32 {
  i++;
  return i;
}
fn second() -> i32 {
  i++;
  return i;
}

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  for (var i = 0; i < 3; a(&U[first()][second()])) {
    i++;
  }
}

fn c(p : ptr<uniform, array<array<vec4<i32>, 8>, 8>>) {
  for (var i = 0; i < 3; a(&(*p)[first()][second()])) {
    i++;
  }
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<vec4<i32>, 8>, 8>;

var<private> i : i32;

fn first() -> i32 {
  i++;
  return i;
}

fn second() -> i32 {
  i++;
  return i;
}

type U_X_X = array<u32, 2u>;

fn a_U_X_X(p : U_X_X) -> vec4<i32> {
  return U[p[0]][p[1]];
}

fn b() {
  {
    var i = 0;
    loop {
      if (!((i < 3))) {
        break;
      }
      {
        i++;
      }

      continuing {
        let tint_symbol = first();
        let tint_symbol_1 = second();
        a_U_X_X(U_X_X(u32(tint_symbol), u32(tint_symbol_1)));
      }
    }
  }
}

fn c_U() {
  {
    var i = 0;
    loop {
      if (!((i < 3))) {
        break;
      }
      {
        i++;
      }

      continuing {
        let tint_symbol_2 = first();
        let tint_symbol_3 = second();
        a_U_X_X(U_X_X(u32(tint_symbol_2), u32(tint_symbol_3)));
      }
    }
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, PtrChains_SideEffectInWhileCond) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<vec4<i32>, 8>, 8>;

var<private> i : i32;
fn first() -> i32 {
  i++;
  return i;
}
fn second() -> i32 {
  i++;
  return i;
}

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  while (a(&U[first()][second()]).x < 4) {
    let body = 1;
  }
}

fn c(p : ptr<uniform, array<array<vec4<i32>, 8>, 8>>) {
  while (a(&(*p)[first()][second()]).x < 4) {
    let body = 1;
  }
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<vec4<i32>, 8>, 8>;

var<private> i : i32;

fn first() -> i32 {
  i++;
  return i;
}

fn second() -> i32 {
  i++;
  return i;
}

type U_X_X = array<u32, 2u>;

fn a_U_X_X(p : U_X_X) -> vec4<i32> {
  return U[p[0]][p[1]];
}

fn b() {
  loop {
    let tint_symbol = first();
    let tint_symbol_1 = second();
    if (!((a_U_X_X(U_X_X(u32(tint_symbol), u32(tint_symbol_1))).x < 4))) {
      break;
    }
    {
      let body = 1;
    }
  }
}

fn c_U() {
  loop {
    let tint_symbol_2 = first();
    let tint_symbol_3 = second();
    if (!((a_U_X_X(U_X_X(u32(tint_symbol_2), u32(tint_symbol_3))).x < 4))) {
      break;
    }
    {
      let body = 1;
    }
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Shadow_PtrChains) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  let p0 = &U;
  if (true) {
    let I = 1;
    let p1 = &(*p0)[I];
    if (true) {
      let I = 2;
      let p2 = &(*p1)[I];
      if (true) {
        let I = 3;
        let p3 = &(*p2)[I];
        if (true) {
          let I = 4;
          a(p3);
        }
      }
    }
  }
}

fn c(p0 : ptr<uniform, array<array<array<vec4<i32>, 8>, 8>, 8>>) {
  if (true) {
    let I = 1;
    let p1 = &(*p0)[I];
    if (true) {
      let I = 2;
      let p2 = &(*p1)[I];
      if (true) {
        let I = 3;
        let p3 = &(*p2)[I];
        if (true) {
          let I = 4;
          a(p3);
        }
      }
    }
  }
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

type U_X_X_X = array<u32, 3u>;

fn a_U_X_X_X(p : U_X_X_X) -> vec4<i32> {
  return U[p[0]][p[1]][p[2]];
}

fn b() {
  let p0 = &(U);
  if (true) {
    let I = 1;
    let p1 = &(U[u32(I)]);
    if (true) {
      let I = 2;
      let p2 = &(U[u32(I)][u32(I)]);
      if (true) {
        let I = 3;
        let p3 = &(U[u32(I)][u32(I)][u32(I)]);
        if (true) {
          let I = 4;
          a_U_X_X_X(U_X_X_X(u32(I), u32(I), u32(I)));
        }
      }
    }
  }
}

fn c_U() {
  if (true) {
    let I = 1;
    let p1 = &(U[u32(I)]);
    if (true) {
      let I = 2;
      let p2 = &(U[u32(I)][u32(I)]);
      if (true) {
        let I = 3;
        let p3 = &(U[u32(I)][u32(I)][u32(I)]);
        if (true) {
          let I = 4;
          a_U_X_X_X(U_X_X_X(u32(I), u32(I), u32(I)));
        }
      }
    }
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, NoShadowLet_PtrChains) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  let p0 = &U;
  if (true) {
    let I1 = 1;
    let p1 = &(*p0)[I1];
    if (true) {
      let I2 = 2;
      let p2 = &(*p1)[I2];
      if (true) {
        let I3 = 3;
        let p3 = &(*p2)[I3];
        if (true) {
          let I = 4;
          a(p3);
        }
      }
    }
  }
}

fn c(p0 : ptr<uniform, array<array<array<vec4<i32>, 8>, 8>, 8>>) {
  if (true) {
    let I1 = 1;
    let p1 = &(*p0)[I1];
    if (true) {
      let I2 = 2;
      let p2 = &(*p1)[I2];
      if (true) {
        let I3 = 3;
        let p3 = &(*p2)[I3];
        if (true) {
          let I = 4;
          a(p3);
        }
      }
    }
  }
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

type U_X_X_X = array<u32, 3u>;

fn a_U_X_X_X(p : U_X_X_X) -> vec4<i32> {
  return U[p[0]][p[1]][p[2]];
}

fn b() {
  let p0 = &(U);
  if (true) {
    let I1 = 1;
    let p1 = &(U[u32(I1)]);
    if (true) {
      let I2 = 2;
      let p2 = &(U[u32(I1)][u32(I2)]);
      if (true) {
        let I3 = 3;
        let p3 = &(U[u32(I1)][u32(I2)][u32(I3)]);
        if (true) {
          let I = 4;
          a_U_X_X_X(U_X_X_X(u32(I1), u32(I2), u32(I3)));
        }
      }
    }
  }
}

fn c_U() {
  if (true) {
    let I1 = 1;
    let p1 = &(U[u32(I1)]);
    if (true) {
      let I2 = 2;
      let p2 = &(U[u32(I1)][u32(I2)]);
      if (true) {
        let I3 = 3;
        let p3 = &(U[u32(I1)][u32(I2)][u32(I3)]);
        if (true) {
          let I = 4;
          a_U_X_X_X(U_X_X_X(u32(I1), u32(I2), u32(I3)));
        }
      }
    }
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, NoShadowVar_PtrChains) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  let p0 = &U;
  if (true) {
    var I1 = 1;
    let p1 = &(*p0)[I1];
    if (true) {
      var I2 = 2;
      let p2 = &(*p1)[I2];
      if (true) {
        var I3 = 3;
        let p3 = &(*p2)[I3];
        if (true) {
          var I = 4;
          a(p3);
        }
      }
    }
  }
}

fn c(p0 : ptr<uniform, array<array<array<vec4<i32>, 8>, 8>, 8>>) {
  if (true) {
    var I1 = 1;
    let p1 = &(*p0)[I1];
    if (true) {
      var I2 = 2;
      let p2 = &(*p1)[I2];
      if (true) {
        var I3 = 3;
        let p3 = &(*p2)[I3];
        if (true) {
          var I = 4;
          a(p3);
        }
      }
    }
  }
}

fn d() {
  c(&U);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

type U_X_X_X = array<u32, 3u>;

fn a_U_X_X_X(p : U_X_X_X) -> vec4<i32> {
  return U[p[0]][p[1]][p[2]];
}

fn b() {
  let p0 = &(U);
  if (true) {
    var I1 = 1;
    let tint_symbol = I1;
    let p1 = &(U[u32(tint_symbol)]);
    if (true) {
      var I2 = 2;
      let tint_symbol_1 = I2;
      let p2 = &(U[u32(tint_symbol)][u32(tint_symbol_1)]);
      if (true) {
        var I3 = 3;
        let tint_symbol_2 = I3;
        let p3 = &(U[u32(tint_symbol)][u32(tint_symbol_1)][u32(tint_symbol_2)]);
        if (true) {
          var I = 4;
          a_U_X_X_X(U_X_X_X(u32(tint_symbol), u32(tint_symbol_1), u32(tint_symbol_2)));
        }
      }
    }
  }
}

fn c_U() {
  if (true) {
    var I1 = 1;
    let tint_symbol_3 = I1;
    let p1 = &(U[u32(tint_symbol_3)]);
    if (true) {
      var I2 = 2;
      let tint_symbol_4 = I2;
      let p2 = &(U[u32(tint_symbol_3)][u32(tint_symbol_4)]);
      if (true) {
        var I3 = 3;
        let tint_symbol_5 = I3;
        let p3 = &(U[u32(tint_symbol_3)][u32(tint_symbol_4)][u32(tint_symbol_5)]);
        if (true) {
          var I = 4;
          a_U_X_X_X(U_X_X_X(u32(tint_symbol_3), u32(tint_symbol_4), u32(tint_symbol_5)));
        }
      }
    }
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Shadow_Globals) {
    auto* src = R"(
fn a(p : ptr<storage, f32>) -> f32 {
  let S = 123;
  return *p;
}

@group(0) @binding(0) var<storage> S : f32;

fn b() {
  a(&S);
}
)";

    auto* expect = R"(
fn a_S() -> f32 {
  let S_1 = &(S);
  let S = 123;
  return *(S_1);
}

@group(0) @binding(0) var<storage> S : f32;

fn b() {
  a_S();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Shadow_Param) {
    auto* src = R"(
fn a(p : ptr<storage, f32>) -> f32 {
  let param = p;
  if (true) {
    let p = 2;
    return *param;
  }
  return 0;
}

@group(0) @binding(0) var<storage> S : array<array<f32, 4>, 4>;

fn b() {
  a(&S[1][2]);
}
)";

    auto* expect = R"(
type S_X_X = array<u32, 2u>;

fn a_S_X_X(p : S_X_X) -> f32 {
  let p_1 = p;
  let param = &(S[p_1[0]][p_1[1]]);
  if (true) {
    let p = 2;
    return S[p_1[0]][p_1[1]];
  }
  return 0;
}

@group(0) @binding(0) var<storage> S : array<array<f32, 4>, 4>;

fn b() {
  a_S_X_X(S_X_X(1, 2));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
