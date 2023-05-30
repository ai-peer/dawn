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

TEST_F(IR_BuilderImplTest, Var_SingleIndexAccessor) {
    // var a: vec3<u32>
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

TEST_F(IR_BuilderImplTest, Var_MultiIndexAccessor) {
    // var a: mat3x4<f32>
    // let b = a[2][3]
    GTEST_SKIP();

    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, mat3x4<f32>, read_write> = var
    //    %1:ptr<function, f32, read_write> = access %a 2, 3
    //    %b:f32 = load %1
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Var_SingleMemberAccess) {
    // struct MyStruct { foo: i32 }
    // var a: MyStruct;
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

TEST_F(IR_BuilderImplTest, Var_MultiMemberAccess) {
    // struct Inner { bar: f32 }
    // struct Outer { a: i32, foo: Inner }
    // var a: Outer;
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

TEST_IF(IR_BuilderImplTest, Var_MixedAccessor) {
    // struct Inner { a: i32, foo: array<Outer, 4> }
    // struct Outer { b: i32, c: f32, bar: vec4<f32> }
    // var a: array<Inner, 4>
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

TEST_F(IR_BuilderImplTest, Var_SingleElementSwizzle) {
    // var a: vec2<f32>
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

TEST_F(IR_BuilderImplTest, Var_MultiElementSwizzle) {
    // var a: vec3<f32>
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

TEST_F(IR_BuilderImplTest, Var_MultiElementSwizzleOfSwizzle) {
    // var a: vec3<f32>
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

TEST_F(IR_BuilderImplTest, Var_MultiElementSwizzle_MiddleOfChain) {
    // struct MyStruct { a: i32; foo: vec4<f32> }
    // var a: MyStruct;
    // let b = a.foo.zyx.yx[0]
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %a:ptr<function, MyStruct, read_write> = var
    //    %1:ptr<function, vec4<f32>, read_write> = access %a, 1
    //    %2:vec4<f32> = load %1
    //    %3:vec3<f32> = swizzle %2, zxy
    //    %4:vec2<f32> = swizzle %3, xy
    //    %b:f32 = access %4, 0
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Let_SingleIndexAccessor) {
    // let a: vec3<u32> = vec3(1, 2, 3)
    // let b = a[2]
    GTEST_SKIP();

    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:vec3<u32> = construct vec3<u32>, 1, 2, 3
    //    %b:u32 = access %1 2
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Let_MultiIndexAccessor) {
    // let a: mat3x4<f32> = mat3x4<u32>()
    // let b = a[2][3]
    GTEST_SKIP();

    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:mat3x4<u32> = construct mat2x3<f32> 0
    //    %b:f32 = access %1 2, 3
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Let_SingleMemberAccess) {
    // struct MyStruct { foo: i32 }
    // let a: MyStruct = MyStruct();
    // let b = a.foo
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:MyStruct = construct MyStruct
    //    %b:i32 = access %1 0
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Let_MultiMemberAccess) {
    // struct Inner { bar: f32 }
    // struct Outer { a: i32, foo: Inner }
    // let a: Outer = Outer();
    // let b = a.foo.bar
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:Outer = construt Outer
    //    %b:f32 = access %1 1, 0
    // }
    // )");
    //
    //);
}

TEST_IF(IR_BuilderImplTest, Let_MixedAccessor) {
    // struct Inner { a: i32, foo: array<Outer, 4> }
    // struct Outer { b: i32, c: f32, bar: vec4<f32> }
    // let a: array<Inner, 4> = array();
    // let b = a[0].foo[1].bar
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:array<Inner, 4> = construct array<Inner, 4>
    //    %b:vec4<f32 = access %1 0, 1, 1, 2
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Let_SingleElementSwizzle) {
    // let a: vec2<f32> = vec2()
    // let b = a.y
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:vec2<f32> = construct vec2<f32>
    //    %b:f32 = access %1 1
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Let_MultiElementSwizzle) {
    // let a: vec3<f32 = vec3()>
    // let b = a.zyxz
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:vec3<f32> = construct vec3<f32>
    //    %b:vec4<f32> = swizzle %1, zyxz
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Let_MultiElementSwizzleOfSwizzle) {
    // let a: vec3<f32> = vec3();
    // let b = a.zyx.yy
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:vec3<f32> = construct vec3<f32>
    //    %2:vec3<f32> = swizzle %1, zyx
    //    %b:vec2<f32> = swizzle %2, yy
    // }
    // )");
    //
    //);
}

TEST_F(IR_BuilderImplTest, Let_MultiElementSwizzle_MiddleOfChain) {
    // struct MyStruct { a: i32; foo: vec4<f32> }
    // let a: MyStruct = MyStruct();
    // let b = a.foo.zyx.yx[0]
    GTEST_SKIP();
    // EXPECT_EQ(Disassemble(m),
    //         R"(%test_function = func():void [@compute @workgroup_size(1, 1, 1)] -> %b1 {
    // %b1 = block {
    //    %1:MyStruct = construct MyStruct
    //    %2:vec4<f32> = access %a, 1
    //    %2:vec3<f32> = swizzle %1, zxy
    //    %3:vec2<f32> = swizzle %2, xy
    //    %b:f32 = access %3, 0
    // }
    // )");
    //
    //);
}

}  // namespace
}  // namespace tint::ir
