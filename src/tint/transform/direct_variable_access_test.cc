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

TEST_F(DirectVariableAccessTest, Fn_ptr_uniform_i32) {
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
  return *(&(U));
}

fn b() {
  a_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Fn_ptr_storage_i32_via_struct) {
    auto* src = R"(
struct S {
  i : i32,
};

@group(0) @binding(0) var<storage> U : S;

fn a(p : ptr<storage, i32>) -> i32 {
  return *p;
}

fn b() {
  a(&U.i);
}
)";

    auto* expect = R"(
struct S {
  i : i32,
}

@group(0) @binding(0) var<storage> U : S;

fn a_U_i() -> i32 {
  return *(&(U.i));
}

fn b() {
  a_U_i();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Fn_ptr_workgroup_vec4i32_via_array_static) {
    auto* src = R"(
var<workgroup> U : array<vec4<i32>, 8>;

fn a(p : ptr<workgroup, vec4<i32>>) -> vec4<i32> {
  return *p;
}

fn b() {
  a(&U[3]);
}
)";

    auto* expect =
        R"(
var<workgroup> U : array<vec4<i32>, 8>;

type U_X = array<u32, 1u>;

fn a_U_X(p : U_X) -> vec4<i32> {
  return *(&(U[p[0]]));
}

fn b() {
  a_U_X(U_X(3));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Fn_ptr_uniform_vec4i32_via_array_dynamic) {
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
  return *(&(U[p[0]]));
}

fn b() {
  let I = 3;
  let tint_symbol = I;
  a_U_X(U_X(u32(tint_symbol)));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Fn_ptr_mixed_vec4i32_via_multiple) {
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

  let u       = fn_u(&U);
  let u_str     = fn_u(&U_str.i);
  let u_arr0    = fn_u(&U_arr[0]);
  let u_arr1    = fn_u(&U_arr[1]);
  let u_arrI    = fn_u(&U_arr[I]);
  let u_arr1_arr0 = fn_u(&U_arr_arr[1][0]);
  let u_arr2_arrI = fn_u(&U_arr_arr[2][I]);
  let u_arrI_arr2 = fn_u(&U_arr_arr[I][2]);
  let u_arrI_arrJ = fn_u(&U_arr_arr[I][J]);

  let s       = fn_s(&S);
  let s_str     = fn_s(&S_str.i);
  let s_arr0    = fn_s(&S_arr[0]);
  let s_arr1    = fn_s(&S_arr[1]);
  let s_arrI    = fn_s(&S_arr[I]);
  let s_arr1_arr0 = fn_s(&S_arr_arr[1][0]);
  let s_arr2_arrI = fn_s(&S_arr_arr[2][I]);
  let s_arrI_arr2 = fn_s(&S_arr_arr[I][2]);
  let s_arrI_arrJ = fn_s(&S_arr_arr[I][J]);

  let w       = fn_w(&W);
  let w_str     = fn_w(&W_str.i);
  let w_arr0    = fn_w(&W_arr[0]);
  let w_arr1    = fn_w(&W_arr[1]);
  let w_arrI    = fn_w(&W_arr[I]);
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
  return *(&(U));
}

fn fn_u_U_str_i() -> vec4<i32> {
  return *(&(U_str.i));
}

type U_arr_X = array<u32, 1u>;

fn fn_u_U_arr_X(p : U_arr_X) -> vec4<i32> {
  return *(&(U_arr[p[0]]));
}

type U_arr_arr_X_X = array<u32, 2u>;

fn fn_u_U_arr_arr_X_X(p : U_arr_arr_X_X) -> vec4<i32> {
  return *(&(U_arr_arr[p[0]][p[1]]));
}

fn fn_s_S() -> vec4<i32> {
  return *(&(S));
}

fn fn_s_S_str_i() -> vec4<i32> {
  return *(&(S_str.i));
}

type S_arr_X = array<u32, 1u>;

fn fn_s_S_arr_X(p : S_arr_X) -> vec4<i32> {
  return *(&(S_arr[p[0]]));
}

type S_arr_arr_X_X = array<u32, 2u>;

fn fn_s_S_arr_arr_X_X(p : S_arr_arr_X_X) -> vec4<i32> {
  return *(&(S_arr_arr[p[0]][p[1]]));
}

fn fn_w_W() -> vec4<i32> {
  return *(&(W));
}

fn fn_w_W_str_i() -> vec4<i32> {
  return *(&(W_str.i));
}

type W_arr_X = array<u32, 1u>;

fn fn_w_W_arr_X(p : W_arr_X) -> vec4<i32> {
  return *(&(W_arr[p[0]]));
}

type W_arr_arr_X_X = array<u32, 2u>;

fn fn_w_W_arr_arr_X_X(p : W_arr_arr_X_X) -> vec4<i32> {
  return *(&(W_arr_arr[p[0]][p[1]]));
}

fn b() {
  let I = 3;
  let J = 4;
  let u = fn_u_U();
  let u_str = fn_u_U_str_i();
  let u_arr0 = fn_u_U_arr_X(U_arr_X(0));
  let u_arr1 = fn_u_U_arr_X(U_arr_X(1));
  let tint_symbol = I;
  let u_arrI = fn_u_U_arr_X(U_arr_X(u32(tint_symbol)));
  let u_arr1_arr0 = fn_u_U_arr_arr_X_X(U_arr_arr_X_X(1, 0));
  let tint_symbol_1 = I;
  let u_arr2_arrI = fn_u_U_arr_arr_X_X(U_arr_arr_X_X(2, u32(tint_symbol_1)));
  let tint_symbol_2 = I;
  let u_arrI_arr2 = fn_u_U_arr_arr_X_X(U_arr_arr_X_X(u32(tint_symbol_2), 2));
  let tint_symbol_4 = I;
  let tint_symbol_3 = J;
  let u_arrI_arrJ = fn_u_U_arr_arr_X_X(U_arr_arr_X_X(u32(tint_symbol_4), u32(tint_symbol_3)));
  let s = fn_s_S();
  let s_str = fn_s_S_str_i();
  let s_arr0 = fn_s_S_arr_X(S_arr_X(0));
  let s_arr1 = fn_s_S_arr_X(S_arr_X(1));
  let tint_symbol_5 = I;
  let s_arrI = fn_s_S_arr_X(S_arr_X(u32(tint_symbol_5)));
  let s_arr1_arr0 = fn_s_S_arr_arr_X_X(S_arr_arr_X_X(1, 0));
  let tint_symbol_6 = I;
  let s_arr2_arrI = fn_s_S_arr_arr_X_X(S_arr_arr_X_X(2, u32(tint_symbol_6)));
  let tint_symbol_7 = I;
  let s_arrI_arr2 = fn_s_S_arr_arr_X_X(S_arr_arr_X_X(u32(tint_symbol_7), 2));
  let tint_symbol_9 = I;
  let tint_symbol_8 = J;
  let s_arrI_arrJ = fn_s_S_arr_arr_X_X(S_arr_arr_X_X(u32(tint_symbol_9), u32(tint_symbol_8)));
  let w = fn_w_W();
  let w_str = fn_w_W_str_i();
  let w_arr0 = fn_w_W_arr_X(W_arr_X(0));
  let w_arr1 = fn_w_W_arr_X(W_arr_X(1));
  let tint_symbol_10 = I;
  let w_arrI = fn_w_W_arr_X(W_arr_X(u32(tint_symbol_10)));
  let w_arr1_arr0 = fn_w_W_arr_arr_X_X(W_arr_arr_X_X(1, 0));
  let tint_symbol_11 = I;
  let w_arr2_arrI = fn_w_W_arr_arr_X_X(W_arr_arr_X_X(2, u32(tint_symbol_11)));
  let tint_symbol_12 = I;
  let w_arrI_arr2 = fn_w_W_arr_arr_X_X(W_arr_arr_X_X(u32(tint_symbol_12), 2));
  let tint_symbol_14 = I;
  let tint_symbol_13 = J;
  let w_arrI_arrJ = fn_w_W_arr_arr_X_X(W_arr_arr_X_X(u32(tint_symbol_14), u32(tint_symbol_13)));
}
)";

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
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

type U_X_X_X = array<u32, 3u>;

fn a_U_X_X_X(p : U_X_X_X) -> vec4<i32> {
  return *(&(U[p[0]][p[1]][p[2]]));
}

fn b() {
  let p0 = &(U);
  let p1 = &(U[1]);
  let p2 = &(U[1][2]);
  let p3 = &(U[1][2][3]);
  a_U_X_X_X(U_X_X_X(1, 2, 3));
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
  return *(&(U[p[0]][p[1]][p[2]]));
}

fn b() {
  let p0 = &(U);
  let tint_symbol = first();
  let p1 = &(U[u32(tint_symbol)]);
  let tint_symbol_2 = second();
  let tint_symbol_1 = third();
  let p2 = &(U[u32(tint_symbol)][u32(tint_symbol_2)][u32(tint_symbol_1)]);
  a_U_X_X_X(U_X_X_X(u32(tint_symbol), u32(tint_symbol_2), u32(tint_symbol_1)));
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
  let p0 = &U;
  for (let p1 = &(*p0)[first()]; true; ) {
    a(&(*p1)[second()]);
  }
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
  return *(&(U[p[0]][p[1]]));
}

fn b() {
  let p0 = &(U);
  let tint_symbol = first();
  for(let p1 = &(U[u32(tint_symbol)]); true; ) {
    let tint_symbol_1 = second();
    a_U_X_X(U_X_X(u32(tint_symbol), u32(tint_symbol_1)));
  }
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
  let p0 = &U;
  for (; a(&(*p0)[first()][second()]).x < 4; ) {
    let body = 1;
  }
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
  return *(&(U[p[0]][p[1]]));
}

fn b() {
  let p0 = &(U);
  loop {
    let tint_symbol_1 = first();
    let tint_symbol = second();
    if (!((a_U_X_X(U_X_X(u32(tint_symbol_1), u32(tint_symbol))).x < 4))) {
      break;
    }
    {
      let body = 1;
    }
  }
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
  let p0 = &U;
  for (var i = 0; i < 3; a(&(*p0)[first()][second()])) {
    i++;
  }
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
  return *(&(U[p[0]][p[1]]));
}

fn b() {
  let p0 = &(U);
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
        let tint_symbol_1 = first();
        let tint_symbol = second();
        a_U_X_X(U_X_X(u32(tint_symbol_1), u32(tint_symbol)));
      }
    }
  }
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
  let p0 = &U;
  while (a(&(*p0)[first()][second()]).x < 4) {
    let body = 1;
  }
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
  return *(&(U[p[0]][p[1]]));
}

fn b() {
  let p0 = &(U);
  loop {
    let tint_symbol_1 = first();
    let tint_symbol = second();
    if (!((a_U_X_X(U_X_X(u32(tint_symbol_1), u32(tint_symbol))).x < 4))) {
      break;
    }
    {
      let body = 1;
    }
  }
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, PtrChains_Shadowing) {
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
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<array<array<vec4<i32>, 8>, 8>, 8>;

type U_X_X_X = array<u32, 3u>;

fn a_U_X_X_X(p : U_X_X_X) -> vec4<i32> {
  return *(&(U[p[0]][p[1]][p[2]]));
}

fn b() {
  let p0 = &(U);
  if (true) {
    let I = 1;
    let tint_symbol = I;
    let p1 = &(U[u32(tint_symbol)]);
    if (true) {
      let I = 2;
      let tint_symbol_1 = I;
      let p2 = &(U[u32(tint_symbol)][u32(tint_symbol_1)]);
      if (true) {
        let I = 3;
        let tint_symbol_2 = I;
        let p3 = &(U[u32(tint_symbol)][u32(tint_symbol_1)][u32(tint_symbol_2)]);
        if (true) {
          let I = 4;
          a_U_X_X_X(U_X_X_X(u32(tint_symbol), u32(tint_symbol_1), u32(tint_symbol_2)));
        }
      }
    }
  }
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
