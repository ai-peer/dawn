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

#include "src/tint/transform/direct_variable_access.h"

#include "src/tint/transform/test_helper.h"
#include "src/tint/utils/string.h"

namespace tint::transform {
namespace {

using DirectVariableAccessTest = TransformTest;

// TEST_F(DirectVariableAccessTest, ShouldRunEmptyModule) {
//     auto* src = R"()";
//
//     EXPECT_FALSE(ShouldRun<DirectVariableAccess>(src));
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

fn a_1() -> i32 {
  return *(&(U));
}

fn b() {
  a_1();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Fn_ptr_uniform_i32_via_struct) {
    auto* src = R"(
struct S {
    i : i32,
};

@group(0) @binding(0) var<uniform> U : S;

fn a(p : ptr<uniform, i32>) -> i32 {
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

@group(0) @binding(0) var<uniform> U : S;

fn a_1() -> i32 {
  return *(&(U.i));
}

fn b() {
  a_1();
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Fn_ptr_uniform_vec4i32_via_array_static) {
    auto* src = R"(
@group(0) @binding(0) var<uniform> U : array<vec4<i32>, 8>;

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
    return *p;
}

fn b() {
    a(&U[3]);
}
)";

    auto* expect =
        R"(
@group(0) @binding(0) var<uniform> U : array<vec4<i32>, 8>;

type Up0 = array<u32, 1u>;

fn a_1(p : Up0) -> vec4<i32> {
  return *(&(U[p[0u]]));
}

fn b() {
  a_1(Up0(3));
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

type Up0 = array<u32, 1u>;

fn a_1(p : Up0) -> vec4<i32> {
  return *(&(U[p[0u]]));
}

fn b() {
  let I = 3;
  a_1(Up0(u32(I)));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(DirectVariableAccessTest, Fn_ptr_uniform_vec4i32_via_multiple) {
    auto* src = R"(
struct S {
    i : vec4<i32>,
};

@group(0) @binding(0) var<uniform> U : vec4<i32>;
@group(0) @binding(1) var<uniform> U_str : S;
@group(0) @binding(2) var<uniform> U_arr : array<vec4<i32>, 8>;
@group(0) @binding(3) var<uniform> U_arr_arr : array<array<vec4<i32>, 8>, 4>;

fn a(p : ptr<uniform, vec4<i32>>) -> vec4<i32> {
    return *p;
}

fn b() {
    let I = 3;
    let J = 4;

    let u         = a(&U);
    let str       = a(&U_str.i);
    let arr0      = a(&U_arr[0]);
    let arr1      = a(&U_arr[1]);
    let arrI      = a(&U_arr[I]);
    let arr1_arr0 = a(&U_arr_arr[1][0]);
    let arr2_arrI = a(&U_arr_arr[2][I]);
    let arrI_arr2 = a(&U_arr_arr[I][2]);
    let arrI_arrJ = a(&U_arr_arr[I][J]);
}
)";

    auto* expect =
        R"(
struct S {
  i : vec4<i32>,
}

@group(0) @binding(0) var<uniform> U : vec4<i32>;

@group(0) @binding(1) var<uniform> U_str : S;

@group(0) @binding(2) var<uniform> U_arr : array<vec4<i32>, 8>;

@group(0) @binding(3) var<uniform> U_arr_arr : array<array<vec4<i32>, 8>, 4>;

type U_arrp0 = array<u32, 1u>;

fn a_3(p : U_arrp0) -> vec4<i32> {
  return *(&(U_arr[p[0u]]));
}

fn a_1() -> vec4<i32> {
  return *(&(U));
}

fn a_2() -> vec4<i32> {
  return *(&(U_str.i));
}

type U_arr_arrp0p1 = array<u32, 2u>;

fn a_4(p : U_arr_arrp0p1) -> vec4<i32> {
  return *(&(U_arr_arr[p[0u]][p[1u]]));
}

fn b() {
  let I = 3;
  let J = 4;
  let u = a_1();
  let str = a_2();
  let arr0 = a_3(U_arrp0(0));
  let arr1 = a_3(U_arrp0(1));
  let arrI = a_3(U_arrp0(u32(I)));
  let arr1_arr0 = a_4(U_arr_arrp0p1(1, 0));
  let arr2_arrI = a_4(U_arr_arrp0p1(2, u32(I)));
  let arrI_arr2 = a_4(U_arr_arrp0p1(u32(I), 2));
  let arrI_arrJ = a_4(U_arr_arrp0p1(u32(I), u32(J)));
}
)";

    auto got = Run<DirectVariableAccess>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::transform
