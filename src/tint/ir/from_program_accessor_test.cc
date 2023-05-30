// Copyright 2023 The Tint Authors.
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

#include "src/tint/ir/test_helper.h"

#include "gmock/gmock.h"
#include "src/tint/ast/case_selector.h"
#include "src/tint/ast/int_literal_expression.h"
#include "src/tint/constant/scalar.h"
#include "src/tint/ir/block.h"
#include "src/tint/ir/constant.h"
#include "src/tint/ir/var.h"

namespace tint::ir {
namespace {

using namespace tint::number_suffixes;  // NOLINT

using IR_BuilderImplTest = TestHelper;

TEST_F(IR_BuilderImplTest, SingleIndexAccessor) {
    // let b = a[2]
    GTEST_SKIP();

    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, vec3<u32>, read_write> = var
    //    %1:ptr<function, u32, read_write> = access %a 2
    //    %b:u32 = load %1
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, MultiIndexAccessor) {
    // let b = a[2][3]
    GTEST_SKIP();

    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, mat3x4<u32>, read_write> = var
    //    %1:ptr<function, u32, read_write> = access %a 2, 3
    //    %b:u32 = load %1
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, SingleMemberAccess) {
    // struct MyStruct { foo: i32 }
    // let b = a.foo
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, MyStruct, read_write> = var
    //    %1:ptr<function, i32, read_write> = access %a 0
    //    %b:i32 = load %1
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, MultiMemberAccess) {
    // struct Inner { bar: f32 }
    // struct Outer { a: i32, foo: Inner }
    // let b = a.foo.bar
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, Outer, read_write> = var
    //    %1:ptr<function, f32, read_write> = access %a 1, 0
    //    %b:f32 = load %1
    // }
    // )");
    //
    //);
}

TEST_IF(IR_BuilderImplTest, MixedAccessor) {
    // struct Inner { a: i32, foo: array<Outer, 4> }
    // struct Outer { b: i32, c: f32, bar: vec4<f32> }
    // let b = a[0].foo[1].bar
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, array<Inner, 4>, read_write> = var
    //    %1:ptr<function, vec4<f32>, read_write> = access %a 0, 1, 1, 2
    //    %b = load %1
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, SingleElementSwizzle) {
    // let b = a.y
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, vec2<f32>, read_write> = var
    //    %1:ptr<function, f32, read_write> = access %a 1
    //    %b:f32 = load %1
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, MultiElementSwizzle) {
    // let b = a.zyxz
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, vec3<f32>, read_write> = var
    //    %1:vec3<f32> = load %a
    //    %b:vec4<f32> = swizzle %1, zyxz
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, MultiElementSwizzleOfSwizzle) {
    // let b = a.zyx.yy
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, vec3<f32>, read_write> = var
    //    %1:vec3<f32> = load %a
    //    %2:vec3<f32> = swizzle %1, zyx
    //    %b:vec2<f32> = swizzle %2, yy
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, MultiElementSwizzle_MiddleOfChain) {
    // struct MyArray { a: i32; foo: vec4<f32> }
    // let b = a.foo.zyx.yx[0]
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, MyArray, read_write> = var
    //    %1:ptr<function, vec4<f32>, read_write> = access %a, 1
    //    %2:vec4<f32> = load %1
    //    %3:vec3<f32> = swizzle %2, zxy
    //    %4:vec2<f32> = swizzle %3, xy
    //    %5:ptr<function, vec2<f32>, read_write> = var
    //    store %5, %4
    //    %6:ptr<function, f32, read_write> = access %5, 0
    //    %b:f32 = load %6
    // }
    // )");
    //
    //);
}

}  // namespace
}  // namespace tint::ir
