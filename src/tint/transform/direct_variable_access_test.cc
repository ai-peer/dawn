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

#include <memory>
#include <utility>

#include "src/tint/transform/test_helper.h"
#include "src/tint/utils/string.h"

namespace tint::transform {
namespace {

/// @returns a DataMap with DirectVariableAccess::Config::transform_private enabled.
static DataMap EnablePrivate() {
    DirectVariableAccess::Options opts;
    opts.transform_private = true;

    DataMap inputs;
    inputs.Add<DirectVariableAccess::Config>(opts);
    return inputs;
}

/// @returns a DataMap with DirectVariableAccess::Config::transform_function enabled.
static DataMap EnableFunction() {
    DirectVariableAccess::Options opts;
    opts.transform_function = true;

    DataMap inputs;
    inputs.Add<DirectVariableAccess::Config>(opts);
    return inputs;
}

////////////////////////////////////////////////////////////////////////////////
// ShouldRun tests
////////////////////////////////////////////////////////////////////////////////
namespace should_run {

using DirectVariableAccessShouldRunTest = TransformTest;

// TEST_F(DirectVariableAccessShouldRunTest, EmptyModule) {
//   auto* src = R"()";
//
//   EXPECT_FALSE(ShouldRun<DirectVariableAccess>(src));
// }

}  // namespace should_run

////////////////////////////////////////////////////////////////////////////////
// remove uncalled
////////////////////////////////////////////////////////////////////////////////
namespace remove_uncalled {

using DirectVariableAccessRemoveUncalledTest = TransformTest;

TEST_F(DirectVariableAccessRemoveUncalledTest, PtrUniform) {
    auto* src = R"(
var<private> keep_me = 42;

fn u(p : ptr<uniform, i32>) -> i32 {
  return *(p);
}

)";

    auto* expect = R"(
var<private> keep_me = 42;
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessRemoveUncalledTest, PtrStorage) {
    auto* src = R"(
var<private> keep_me = 42;

fn s(p : ptr<storage, i32>) -> i32 {
  return *(p);
}
)";

    auto* expect = R"(
var<private> keep_me = 42;
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessRemoveUncalledTest, PtrWorkgroup) {
    auto* src = R"(
var<private> keep_me = 42;

fn w(p : ptr<workgroup, i32>) -> i32 {
  return *(p);
}

)";

    auto* expect = R"(
var<private> keep_me = 42;
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessRemoveUncalledTest, PtrPrivate_Disabled) {
    auto* src = R"(
var<private> keep_me = 42;

fn f(p : ptr<private, i32>) -> i32 {
  return *(p);
}
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessRemoveUncalledTest, PtrPrivate_Enabled) {
    auto* src = R"(
var<private> keep_me = 42;
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src, EnablePrivate());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessRemoveUncalledTest, PtrFunction_Disabled) {
    auto* src = R"(
var<private> keep_me = 42;

fn f(p : ptr<function, i32>) -> i32 {
  return *(p);
}
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessRemoveUncalledTest, PtrFunction_Enabled) {
    auto* src = R"(
var<private> keep_me = 42;
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src, EnableFunction());

    EXPECT_EQ(expect, str(got));
}

}  // namespace remove_uncalled

////////////////////////////////////////////////////////////////////////////////
// pointer chains
////////////////////////////////////////////////////////////////////////////////
namespace pointer_chains_tests {

using DirectVariableAccessPtrChainsTest = TransformTest;

TEST_F(DirectVariableAccessPtrChainsTest, ConstantIndices) {
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
  let p1 = &((*(p0))[1]);
  let p2 = &((*(p1))[(1 + 1)]);
  let p3 = &((*(p2))[((2 * 2) - 1)]);
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

TEST_F(DirectVariableAccessPtrChainsTest, HoistIndices) {
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
  let ptr_index_save = first();
  let p1 = &((*(p0))[ptr_index_save]);
  let ptr_index_save_1 = second();
  let ptr_index_save_2 = third();
  let p2 = &((*(p1))[ptr_index_save_1][ptr_index_save_2]);
  a_U_X_X_X(U_X_X_X(u32(ptr_index_save), u32(ptr_index_save_1), u32(ptr_index_save_2)));
}

fn c_U() {
  let p0 = &(U);
  let ptr_index_save_3 = first();
  let p1 = &((*(p0))[ptr_index_save_3]);
  let ptr_index_save_4 = second();
  let ptr_index_save_5 = third();
  let p2 = &((*(p1))[ptr_index_save_4][ptr_index_save_5]);
  a_U_X_X_X(U_X_X_X(u32(ptr_index_save_3), u32(ptr_index_save_4), u32(ptr_index_save_5)));
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPtrChainsTest, HoistInForLoopInit) {
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
  let ptr_index_save = first();
  for(let p1 = &(U[ptr_index_save]); true; ) {
    a_U_X_X(U_X_X(u32(ptr_index_save), u32(second())));
  }
}

fn c_U() {
  let ptr_index_save_1 = first();
  for(let p1 = &(U[ptr_index_save_1]); true; ) {
    a_U_X_X(U_X_X(u32(ptr_index_save_1), u32(second())));
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPtrChainsTest, HoistInForLoopCond) {
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
  for(; (a_U_X_X(U_X_X(u32(first()), u32(second()))).x < 4); ) {
    let body = 1;
  }
}

fn c_U() {
  for(; (a_U_X_X(U_X_X(u32(first()), u32(second()))).x < 4); ) {
    let body = 1;
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPtrChainsTest, HoistInForLoopCont) {
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
  for(var i = 0; (i < 3); a_U_X_X(U_X_X(u32(first()), u32(second())))) {
    i++;
  }
}

fn c_U() {
  for(var i = 0; (i < 3); a_U_X_X(U_X_X(u32(first()), u32(second())))) {
    i++;
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPtrChainsTest, HoistInWhileCond) {
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
  while((a_U_X_X(U_X_X(u32(first()), u32(second()))).x < 4)) {
    let body = 1;
  }
}

fn c_U() {
  while((a_U_X_X(U_X_X(u32(first()), u32(second()))).x < 4)) {
    let body = 1;
  }
}

fn d() {
  c_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace pointer_chains_tests

////////////////////////////////////////////////////////////////////////////////
// 'uniform' address space
////////////////////////////////////////////////////////////////////////////////
namespace uniform_as_tests {

using DirectVariableAccessUniformASTest = TransformTest;

TEST_F(DirectVariableAccessUniformASTest, Param_ptr_i32_read) {
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

TEST_F(DirectVariableAccessUniformASTest, Param_ptr_vec4i32_Via_array_DynamicRead) {
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

TEST_F(DirectVariableAccessUniformASTest, CallChaining) {
    auto* src = R"(
struct Inner {
  mat : mat3x4<f32>,
};

type InnerArr = array<Inner, 4>;

struct Outer {
  arr : InnerArr,
  mat : mat3x4<f32>,
};

@group(0) @binding(0) var<uniform> U : Outer;

fn f0(p : ptr<uniform, vec4<f32>>) -> f32 {
  return (*p).x;
}

fn f1(p : ptr<uniform, mat3x4<f32>>) -> f32 {
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
    // call f0() with inline usage of U
    res += f0(&U.arr[2].mat[1]);
  }
  {
    // call f0() with pointer-let usage of U
    let p_vec = &U.arr[2].mat[1];
    res += f0(p_vec);
  }
  return res;
}

fn f2(p : ptr<uniform, Inner>) -> f32 {
  let p_mat = &(*p).mat;
  return f1(p_mat);
}

fn f3(p0 : ptr<uniform, InnerArr>, p1 : ptr<uniform, mat3x4<f32>>) -> f32 {
  let p0_inner = &(*p0)[3];
  return f2(p0_inner) + f1(p1);
}

fn f4(p : ptr<uniform, Outer>) -> f32 {
  return f3(&(*p).arr, &U.mat);
}

fn b() {
  f4(&U);
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

@group(0) @binding(0) var<uniform> U : Outer;

type U_mat_X = array<u32, 1u>;

fn f0_U_mat_X(p : U_mat_X) -> f32 {
  return U.mat[p[0]].x;
}

type U_arr_X_mat_X = array<u32, 2u>;

fn f0_U_arr_X_mat_X(p : U_arr_X_mat_X) -> f32 {
  return U.arr[p[0]].mat[p[0]].x;
}

type U_arr_X_mat_X_1 = array<u32, 2u>;

fn f0_U_arr_X_mat_X_1(p : U_arr_X_mat_X_1) -> f32 {
  return U.arr[p[0]].mat[p[1]].x;
}

fn f1_U_mat() -> f32 {
  var res : f32;
  {
    res += f0_U_mat_X(U_mat_X(1));
  }
  {
    let p_vec = &(U.mat[1]);
    res += f0_U_mat_X(U_mat_X(1));
  }
  {
    res += f0_U_arr_X_mat_X_1(U_arr_X_mat_X_1(2, 1));
  }
  {
    let p_vec = &(U.arr[2].mat[1]);
    res += f0_U_arr_X_mat_X_1(U_arr_X_mat_X_1(2, 1));
  }
  return res;
}

type U_arr_X_mat = array<u32, 1u>;

fn f1_U_arr_X_mat(p : U_arr_X_mat) -> f32 {
  var res : f32;
  {
    res += f0_U_arr_X_mat_X(U_arr_X_mat_X(p[0u], 1));
  }
  {
    let p_vec = &(U.arr[p[0]].mat[1]);
    res += f0_U_arr_X_mat_X(U_arr_X_mat_X(p[0u], 1));
  }
  {
    res += f0_U_arr_X_mat_X_1(U_arr_X_mat_X_1(2, 1));
  }
  {
    let p_vec = &(U.arr[2].mat[1]);
    res += f0_U_arr_X_mat_X_1(U_arr_X_mat_X_1(2, 1));
  }
  return res;
}

type U_arr_X = array<u32, 1u>;

fn f2_U_arr_X(p : U_arr_X) -> f32 {
  let p_mat = &(U.arr[p[0]].mat);
  return f1_U_arr_X_mat(U_arr_X_mat(p[0u]));
}

fn f3_U_arr_U_mat() -> f32 {
  let p0_inner = &(U.arr[3]);
  return (f2_U_arr_X(U_arr_X(3)) + f1_U_mat());
}

fn f4_U() -> f32 {
  return f3_U_arr_U_mat();
}

fn b() {
  f4_U();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace uniform_as_tests

////////////////////////////////////////////////////////////////////////////////
// 'storage' address space
////////////////////////////////////////////////////////////////////////////////
namespace storage_as_tests {

using DirectVariableAccessStorageASTest = TransformTest;

TEST_F(DirectVariableAccessStorageASTest, Param_ptr_i32_Via_struct_read) {
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

TEST_F(DirectVariableAccessStorageASTest, Param_ptr_arr_i32_Via_struct_write) {
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

TEST_F(DirectVariableAccessStorageASTest, Param_ptr_vec4i32_Via_array_DynamicWrite) {
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

TEST_F(DirectVariableAccessStorageASTest, CallChaining) {
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

}  // namespace storage_as_tests

////////////////////////////////////////////////////////////////////////////////
// 'workgroup' address space
////////////////////////////////////////////////////////////////////////////////
namespace workgroup_as_tests {

using DirectVariableAccessWorkgroupASTest = TransformTest;

TEST_F(DirectVariableAccessWorkgroupASTest, Param_ptr_vec4i32_Via_array_StaticRead) {
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

TEST_F(DirectVariableAccessWorkgroupASTest, Param_ptr_vec4i32_Via_array_StaticWrite) {
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

TEST_F(DirectVariableAccessWorkgroupASTest, CallChaining) {
    auto* src = R"(
struct Inner {
  mat : mat3x4<f32>,
};

type InnerArr = array<Inner, 4>;

struct Outer {
  arr : InnerArr,
  mat : mat3x4<f32>,
};

var<workgroup> W : Outer;

fn f0(p : ptr<workgroup, vec4<f32>>) -> f32 {
  return (*p).x;
}

fn f1(p : ptr<workgroup, mat3x4<f32>>) -> f32 {
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
    // call f0() with inline usage of W
    res += f0(&W.arr[2].mat[1]);
  }
  {
    // call f0() with pointer-let usage of W
    let p_vec = &W.arr[2].mat[1];
    res += f0(p_vec);
  }
  return res;
}

fn f2(p : ptr<workgroup, Inner>) -> f32 {
  let p_mat = &(*p).mat;
  return f1(p_mat);
}

fn f3(p0 : ptr<workgroup, InnerArr>, p1 : ptr<workgroup, mat3x4<f32>>) -> f32 {
  let p0_inner = &(*p0)[3];
  return f2(p0_inner) + f1(p1);
}

fn f4(p : ptr<workgroup, Outer>) -> f32 {
  return f3(&(*p).arr, &W.mat);
}

fn b() {
  f4(&W);
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

var<workgroup> W : Outer;

type W_mat_X = array<u32, 1u>;

fn f0_W_mat_X(p : W_mat_X) -> f32 {
  return W.mat[p[0]].x;
}

type W_arr_X_mat_X = array<u32, 2u>;

fn f0_W_arr_X_mat_X(p : W_arr_X_mat_X) -> f32 {
  return W.arr[p[0]].mat[p[0]].x;
}

type W_arr_X_mat_X_1 = array<u32, 2u>;

fn f0_W_arr_X_mat_X_1(p : W_arr_X_mat_X_1) -> f32 {
  return W.arr[p[0]].mat[p[1]].x;
}

fn f1_W_mat() -> f32 {
  var res : f32;
  {
    res += f0_W_mat_X(W_mat_X(1));
  }
  {
    let p_vec = &(W.mat[1]);
    res += f0_W_mat_X(W_mat_X(1));
  }
  {
    res += f0_W_arr_X_mat_X_1(W_arr_X_mat_X_1(2, 1));
  }
  {
    let p_vec = &(W.arr[2].mat[1]);
    res += f0_W_arr_X_mat_X_1(W_arr_X_mat_X_1(2, 1));
  }
  return res;
}

type W_arr_X_mat = array<u32, 1u>;

fn f1_W_arr_X_mat(p : W_arr_X_mat) -> f32 {
  var res : f32;
  {
    res += f0_W_arr_X_mat_X(W_arr_X_mat_X(p[0u], 1));
  }
  {
    let p_vec = &(W.arr[p[0]].mat[1]);
    res += f0_W_arr_X_mat_X(W_arr_X_mat_X(p[0u], 1));
  }
  {
    res += f0_W_arr_X_mat_X_1(W_arr_X_mat_X_1(2, 1));
  }
  {
    let p_vec = &(W.arr[2].mat[1]);
    res += f0_W_arr_X_mat_X_1(W_arr_X_mat_X_1(2, 1));
  }
  return res;
}

type W_arr_X = array<u32, 1u>;

fn f2_W_arr_X(p : W_arr_X) -> f32 {
  let p_mat = &(W.arr[p[0]].mat);
  return f1_W_arr_X_mat(W_arr_X_mat(p[0u]));
}

fn f3_W_arr_W_mat() -> f32 {
  let p0_inner = &(W.arr[3]);
  return (f2_W_arr_X(W_arr_X(3)) + f1_W_mat());
}

fn f4_W() -> f32 {
  return f3_W_arr_W_mat();
}

fn b() {
  f4_W();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace workgroup_as_tests

////////////////////////////////////////////////////////////////////////////////
// 'private' address space
////////////////////////////////////////////////////////////////////////////////
namespace private_as_tests {

using DirectVariableAccessPrivateASTest = TransformTest;

TEST_F(DirectVariableAccessPrivateASTest, Enabled_Param_ptr_i32_read) {
    auto* src = R"(
fn a(p : ptr<private, i32>) -> i32 {
  return *(p);
}

var<private> P : i32;

fn b() {
  a(&(P));
}
)";

    auto* expect = R"(
fn a_F(p_base : ptr<private, i32>) -> i32 {
  return *(p_base);
}

var<private> P : i32;

fn b() {
  a_F(&(P));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnablePrivate());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Enabled_Param_ptr_i32_write) {
    auto* src = R"(
fn a(p : ptr<private, i32>) {
  *(p) = 42;
}

var<private> P : i32;

fn b() {
  a(&(P));
}
)";

    auto* expect = R"(
fn a_F(p_base : ptr<private, i32>) {
  *(p_base) = 42;
}

var<private> P : i32;

fn b() {
  a_F(&(P));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnablePrivate());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Enabled_Param_ptr_i32_Via_struct_read) {
    auto* src = R"(
struct str {
  i : i32,
};

fn a(p : ptr<private, i32>) -> i32 {
  return *p;
}

var<private> P : str;

fn b() {
  a(&P.i);
}
)";

    auto* expect =
        R"(
struct str {
  i : i32,
}

fn a_F_i(p_base : ptr<private, str>) -> i32 {
  return (*(p_base)).i;
}

var<private> P : str;

fn b() {
  a_F_i(&(P));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnablePrivate());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Disabled_Param_ptr_i32_Via_struct_read) {
    auto* src = R"(
struct str {
  i : i32,
}

fn a(p : ptr<private, i32>) -> i32 {
  return *(p);
}

var<private> P : str;

fn b() {
  a(&(P.i));
}
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Enabled_Param_ptr_arr_i32_Via_struct_write) {
    auto* src = R"(
struct str {
  arr : array<i32, 4>,
};

fn a(p : ptr<private, array<i32, 4>>) {
  *p = array<i32, 4>();
}

var<private> P : str;

fn b() {
  a(&P.arr);
}
)";

    auto* expect =
        R"(
struct str {
  arr : array<i32, 4>,
}

fn a_F_arr(p_base : ptr<private, str>) {
  (*(p_base)).arr = array<i32, 4>();
}

var<private> P : str;

fn b() {
  a_F_arr(&(P));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnablePrivate());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Disabled_Param_ptr_arr_i32_Via_struct_write) {
    auto* src = R"(
struct str {
  arr : array<i32, 4>,
}

fn a(p : ptr<private, array<i32, 4>, read_write>) {
  *(p) = array<i32, 4>();
}

var<private> P : str;

fn b() {
  a(&(P.arr));
}
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Enabled_Param_ptr_i32_mixed) {
    auto* src = R"(
struct str {
  i : i32,
};

fn a(p : ptr<private, i32>) -> i32 {
  return *p;
}

var<private> Pi : i32;
var<private> Ps : str;
var<private> Pa : array<i32, 4>;

fn b() {
  a(&Pi);
  a(&Ps.i);
  a(&Pa[2]);
}
)";

    auto* expect =
        R"(
struct str {
  i : i32,
}

fn a_F(p_base : ptr<private, i32>) -> i32 {
  return *(p_base);
}

fn a_F_i(p_base_1 : ptr<private, str>) -> i32 {
  return (*(p_base_1)).i;
}

type F_X = array<u32, 1u>;

fn a_F_X(p_base_2 : ptr<private, array<i32, 4u>>, p : F_X) -> i32 {
  return (*(p_base_2))[p[0]];
}

var<private> Pi : i32;

var<private> Ps : str;

var<private> Pa : array<i32, 4>;

type Pa_X = array<u32, 1u>;

fn b() {
  a_F(&(Pi));
  a_F_i(&(Ps));
  a_F_X(&(Pa), Pa_X(2));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnablePrivate());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Disabled_Param_ptr_i32_mixed) {
    auto* src = R"(
struct str {
  i : i32,
}

fn a(p : ptr<private, i32>) -> i32 {
  return *(p);
}

var<private> Pi : i32;

var<private> Ps : str;

var<private> Pa : array<i32, 4>;

fn b() {
  a(&(Pi));
  a(&(Ps.i));
  a(&(Pa[2]));
}
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Enabled_CallChaining) {
    auto* src = R"(
struct Inner {
  mat : mat3x4<f32>,
};

type InnerArr = array<Inner, 4>;

struct Outer {
  arr : InnerArr,
  mat : mat3x4<f32>,
};

var<private> P : Outer;

fn f0(p : ptr<private, vec4<f32>>) -> f32 {
  return (*p).x;
}

fn f1(p : ptr<private, mat3x4<f32>>) -> f32 {
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
    // call f0() with inline usage of P
    res += f0(&P.arr[2].mat[1]);
  }
  {
    // call f0() with pointer-let usage of P
    let p_vec = &P.arr[2].mat[1];
    res += f0(p_vec);
  }
  return res;
}

fn f2(p : ptr<private, Inner>) -> f32 {
  let p_mat = &(*p).mat;
  return f1(p_mat);
}

fn f3(p0 : ptr<private, InnerArr>, p1 : ptr<private, mat3x4<f32>>) -> f32 {
  let p0_inner = &(*p0)[3];
  return f2(p0_inner) + f1(p1);
}

fn f4(p : ptr<private, Outer>) -> f32 {
  return f3(&(*p).arr, &P.mat);
}

fn b() {
  f4(&P);
}
)";

    auto* expect = R"(error: unknown identifier: 'p'
error: unknown identifier: 'p'
error: unknown identifier: 'p0'
error: unknown identifier: 'p1'
error: unknown identifier: 'p')";

    auto got = Run<DirectVariableAccess>(src, EnablePrivate());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessPrivateASTest, Disabled_CallChaining) {
    auto* src = R"(
struct Inner {
  mat : mat3x4<f32>,
}

type InnerArr = array<Inner, 4>;

struct Outer {
  arr : InnerArr,
  mat : mat3x4<f32>,
}

var<private> P : Outer;

fn f0(p : ptr<private, vec4<f32>>) -> f32 {
  return (*(p)).x;
}

fn f1(p : ptr<private, mat3x4<f32>>) -> f32 {
  var res : f32;
  {
    res += f0(&((*(p))[1]));
  }
  {
    let p_vec = &((*(p))[1]);
    res += f0(p_vec);
  }
  {
    res += f0(&(P.arr[2].mat[1]));
  }
  {
    let p_vec = &(P.arr[2].mat[1]);
    res += f0(p_vec);
  }
  return res;
}

fn f2(p : ptr<private, Inner>) -> f32 {
  let p_mat = &((*(p)).mat);
  return f1(p_mat);
}

fn f3(p0 : ptr<private, InnerArr>, p1 : ptr<private, mat3x4<f32>>) -> f32 {
  let p0_inner = &((*(p0))[3]);
  return (f2(p0_inner) + f1(p1));
}

fn f4(p : ptr<private, Outer>) -> f32 {
  return f3(&((*(p)).arr), &(P.mat));
}

fn b() {
  f4(&(P));
}
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace private_as_tests

////////////////////////////////////////////////////////////////////////////////
// 'function' address space
////////////////////////////////////////////////////////////////////////////////
namespace function_as_tests {

using DirectVariableAccessFunctionASTest = TransformTest;

TEST_F(DirectVariableAccessFunctionASTest, Enabled_LocalPtr) {
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

TEST_F(DirectVariableAccessFunctionASTest, Enabled_Param_ptr_i32_read) {
    auto* src = R"(
fn a(p : ptr<function, i32>) -> i32 {
  return *(p);
}

fn b() {
  var F : i32;
  a(&(F));
}
)";

    auto* expect = R"(
fn a_F(p_base : ptr<function, i32>) -> i32 {
  return *(p_base);
}

fn b() {
  var F : i32;
  a_F(&(F));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnableFunction());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessFunctionASTest, Enabled_Param_ptr_i32_write) {
    auto* src = R"(
fn a(p : ptr<function, i32>) {
  *(p) = 42;
}

fn b() {
  var F : i32;
  a(&(F));
}
)";

    auto* expect = R"(
fn a_F(p_base : ptr<function, i32>) {
  *(p_base) = 42;
}

fn b() {
  var F : i32;
  a_F(&(F));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnableFunction());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessFunctionASTest, Enabled_Param_ptr_i32_Via_struct_read) {
    auto* src = R"(
struct str {
  i : i32,
};

fn a(p : ptr<function, i32>) -> i32 {
  return *p;
}

fn b() {
  var F : str;
  a(&F.i);
}
)";

    auto* expect = R"(
struct str {
  i : i32,
}

fn a_F_i(p_base : ptr<function, str>) -> i32 {
  return (*(p_base)).i;
}

fn b() {
  var F : str;
  a_F_i(&(F));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnableFunction());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessFunctionASTest, Enabled_Param_ptr_arr_i32_Via_struct_write) {
    auto* src = R"(
struct str {
  arr : array<i32, 4>,
};

fn a(p : ptr<function, array<i32, 4>>) {
  *p = array<i32, 4>();
}

fn b() {
  var F : str;
  a(&F.arr);
}
)";

    auto* expect = R"(
struct str {
  arr : array<i32, 4>,
}

fn a_F_arr(p_base : ptr<function, str>) {
  (*(p_base)).arr = array<i32, 4>();
}

fn b() {
  var F : str;
  a_F_arr(&(F));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnableFunction());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessFunctionASTest, Enabled_Param_ptr_i32_mixed) {
    auto* src = R"(
struct str {
  i : i32,
};

fn a(p : ptr<function, i32>) -> i32 {
  return *p;
}

fn b() {
  var Fi : i32;
  var Fs : str;
  var Fa : array<i32, 4>;

  a(&Fi);
  a(&Fs.i);
  a(&Fa[2]);
}
)";

    auto* expect = R"(
struct str {
  i : i32,
}

fn a_F(p_base : ptr<function, i32>) -> i32 {
  return *(p_base);
}

fn a_F_i(p_base_1 : ptr<function, str>) -> i32 {
  return (*(p_base_1)).i;
}

type F_X = array<u32, 1u>;

fn a_F_X(p_base_2 : ptr<function, array<i32, 4u>>, p : F_X) -> i32 {
  return (*(p_base_2))[p[0]];
}

type Fa_X = array<u32, 1u>;

fn b() {
  var Fi : i32;
  var Fs : str;
  var Fa : array<i32, 4>;
  a_F(&(Fi));
  a_F_i(&(Fs));
  a_F_X(&(Fa), Fa_X(2));
}
)";

    auto got = Run<DirectVariableAccess>(src, EnableFunction());

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessFunctionASTest, Disabled_Param_ptr_i32_Via_struct_read) {
    auto* src = R"(
struct str {
  i : i32,
}

fn a(p : ptr<function, i32>) -> i32 {
  return *(p);
}

fn b() {
  var F : str;
  a(&(F.i));
}
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessFunctionASTest, Disabled_Param_ptr_arr_i32_Via_struct_write) {
    auto* src = R"(
struct str {
  arr : array<i32, 4>,
}

fn a(p : ptr<function, array<i32, 4>, read_write>) {
  *(p) = array<i32, 4>();
}

fn b() {
  var F : str;
  a(&(F.arr));
}
)";

    auto* expect = src;

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace function_as_tests

////////////////////////////////////////////////////////////////////////////////
// complex tests
////////////////////////////////////////////////////////////////////////////////
namespace complex_tests {

using DirectVariableAccessComplexTest = TransformTest;

TEST_F(DirectVariableAccessComplexTest, Param_ptr_mixed_vec4i32_ViaMultiple) {
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

TEST_F(DirectVariableAccessComplexTest, Indexing) {
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
  return S[p[0]][a(S[p[0]][0][1][2])][a(S[p[0]][a(3)][4][5])][a(S[p[0]][6][a(7)][8])];
}

fn c() {
  let v = b_S_X(S_X(42));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessComplexTest, IndexingInPtrCall) {
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
  return a_S_X_X_X_X(S_X_X_X_X(p[0u], u32(a_S_X_X_X_X(S_X_X_X_X(p[0u], 0, 1, 2))), u32(a_S_X_X_X_X(S_X_X_X_X(p[0u], 3, 4, 5))), u32(a_S_X_X_X_X(S_X_X_X_X(p[0u], 6, 7, 8)))));
}

fn c() {
  let v = b_S_X(S_X(42));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessComplexTest, IndexingDualPointers) {
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
  return S[s[0]][a(U[u[0]][0][1].x)][a(U[u[0]][a(3)][4].y)];
}

fn c() {
  let v = b_S_X_U_X(S_X(42), U_X(24));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace complex_tests

////////////////////////////////////////////////////////////////////////////////
// shadowing tests
////////////////////////////////////////////////////////////////////////////////
namespace shadow_tests {

using DirectVariableAccessShadowTest = TransformTest;

TEST_F(DirectVariableAccessShadowTest, PtrChains) {
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
    let ptr_index_save = I;
    let p1 = &((*(p0))[ptr_index_save]);
    if (true) {
      let I = 2;
      let ptr_index_save_1 = I;
      let p2 = &((*(p1))[ptr_index_save_1]);
      if (true) {
        let I = 3;
        let ptr_index_save_2 = I;
        let p3 = &((*(p2))[ptr_index_save_2]);
        if (true) {
          let I = 4;
          a_U_X_X_X(U_X_X_X(u32(ptr_index_save), u32(ptr_index_save_1), u32(ptr_index_save_2)));
        }
      }
    }
  }
}

fn c_U() {
  if (true) {
    let I = 1;
    let ptr_index_save_3 = I;
    let p1 = &(U[ptr_index_save_3]);
    if (true) {
      let I = 2;
      let ptr_index_save_4 = I;
      let p2 = &(U[ptr_index_save_3][ptr_index_save_4]);
      if (true) {
        let I = 3;
        let ptr_index_save_5 = I;
        let p3 = &(U[ptr_index_save_3][ptr_index_save_4][ptr_index_save_5]);
        if (true) {
          let I = 4;
          a_U_X_X_X(U_X_X_X(u32(ptr_index_save_3), u32(ptr_index_save_4), u32(ptr_index_save_5)));
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

TEST_F(DirectVariableAccessShadowTest, NoShadowLet_PtrChains) {
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
    let p1 = &((*(p0))[I1]);
    if (true) {
      let I2 = 2;
      let p2 = &((*(p1))[I2]);
      if (true) {
        let I3 = 3;
        let p3 = &((*(p2))[I3]);
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
    let p1 = &(U[I1]);
    if (true) {
      let I2 = 2;
      let p2 = &(U[I1][I2]);
      if (true) {
        let I3 = 3;
        let p3 = &(U[I1][I2][I3]);
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

TEST_F(DirectVariableAccessShadowTest, NoShadowVar_PtrChains) {
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
    let ptr_index_save = I1;
    let p1 = &((*(p0))[ptr_index_save]);
    if (true) {
      var I2 = 2;
      let ptr_index_save_1 = I2;
      let p2 = &((*(p1))[ptr_index_save_1]);
      if (true) {
        var I3 = 3;
        let ptr_index_save_2 = I3;
        let p3 = &((*(p2))[ptr_index_save_2]);
        if (true) {
          var I = 4;
          a_U_X_X_X(U_X_X_X(u32(ptr_index_save), u32(ptr_index_save_1), u32(ptr_index_save_2)));
        }
      }
    }
  }
}

fn c_U() {
  if (true) {
    var I1 = 1;
    let ptr_index_save_3 = I1;
    let p1 = &(U[ptr_index_save_3]);
    if (true) {
      var I2 = 2;
      let ptr_index_save_4 = I2;
      let p2 = &(U[ptr_index_save_3][ptr_index_save_4]);
      if (true) {
        var I3 = 3;
        let ptr_index_save_5 = I3;
        let p3 = &(U[ptr_index_save_3][ptr_index_save_4][ptr_index_save_5]);
        if (true) {
          var I = 4;
          a_U_X_X_X(U_X_X_X(u32(ptr_index_save_3), u32(ptr_index_save_4), u32(ptr_index_save_5)));
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

TEST_F(DirectVariableAccessShadowTest, Globals) {
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

TEST_F(DirectVariableAccessShadowTest, Param) {
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

}  // namespace shadow_tests

}  // namespace
}  // namespace tint::transform
