// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/hlsl/writer/raise/promote_initializers.h"

#include <utility>

#include "gtest/gtest.h"
#include "src/tint/lang/core/ir/transform/helper_test.h"

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

namespace tint::hlsl::writer::raise {
namespace {

using HlslWriterPromoteInitializersTest = core::ir::transform::TransformTest;

TEST_F(HlslWriterPromoteInitializersTest, NoStructInitializers) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Var<private_>("a", b.Zero<i32>());
        b.Return(func);
    });

    auto* src = R"(
%foo = @fragment func():void {
  $B1: {
    %a:ptr<private, i32, read_write> = var, 0i
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, StructInVarNoChange) {
    auto* str_ty =
        ty.Struct(mod.symbols.New("S"),
                  {
                      {mod.symbols.New("a"), ty.i32(), core::type::StructMemberAttributes{}},
                  });

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Var<private_>("a", b.Composite(str_ty, 1_i));
        b.Return(func);
    });

    auto* src = R"(
S = struct @align(4) {
  a:i32 @offset(0)
}

%foo = @fragment func():void {
  $B1: {
    %a:ptr<private, S, read_write> = var, S(1i)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, ArrayInVarNoChange) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Var<private_>("a", b.Zero<array<i32, 2>>());
        b.Return(func);
    });

    auto* src = R"(
%foo = @fragment func():void {
  $B1: {
    %a:ptr<private, array<i32, 2>, read_write> = var, array<i32, 2>(0i)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, StructInLetNoChange) {
    auto* str_ty =
        ty.Struct(mod.symbols.New("S"),
                  {
                      {mod.symbols.New("a"), ty.i32(), core::type::StructMemberAttributes{}},
                  });

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Composite(str_ty, 1_i));
        b.Return(func);
    });

    auto* src = R"(
S = struct @align(4) {
  a:i32 @offset(0)
}

%foo = @fragment func():void {
  $B1: {
    %a:S = let S(1i)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, ArrayInLetNoChange) {
    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Let("a", b.Zero<array<i32, 2>>());
        b.Return(func);
    });

    auto* src = R"(
%foo = @fragment func():void {
  $B1: {
    %a:array<i32, 2> = let array<i32, 2>(0i)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, StructInCall) {
    auto* str_ty =
        ty.Struct(mod.symbols.New("S"),
                  {
                      {mod.symbols.New("a"), ty.i32(), core::type::StructMemberAttributes{}},
                  });

    auto* p = b.FunctionParam("p", str_ty);
    auto* dst = b.Function("dst", ty.void_());
    dst->SetParams({p});
    dst->Block()->Append(b.Return(dst));

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Call(dst, b.Composite(str_ty, 1_i));
        b.Return(func);
    });

    auto* src = R"(
S = struct @align(4) {
  a:i32 @offset(0)
}

%dst = func(%p:S):void {
  $B1: {
    ret
  }
}
%foo = @fragment func():void {
  $B2: {
    %4:void = call %dst, S(1i)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
S = struct @align(4) {
  a:i32 @offset(0)
}

%dst = func(%p:S):void {
  $B1: {
    ret
  }
}
%foo = @fragment func():void {
  $B2: {
    %4:S = let S(1i)
    %5:void = call %dst, %4
    ret
  }
}
)";
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, ArrayInCall) {
    auto* p = b.FunctionParam("p", ty.array<i32, 2>());
    auto* dst = b.Function("dst", ty.void_());
    dst->SetParams({p});
    dst->Block()->Append(b.Return(dst));

    auto* func = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(func->Block(), [&] {
        b.Call(dst, b.Composite(ty.array<i32, 2>(), 1_i));
        b.Return(func);
    });

    auto* src = R"(
%dst = func(%p:array<i32, 2>):void {
  $B1: {
    ret
  }
}
%foo = @fragment func():void {
  $B2: {
    %4:void = call %dst, array<i32, 2>(1i)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%dst = func(%p:array<i32, 2>):void {
  $B1: {
    ret
  }
}
%foo = @fragment func():void {
  $B2: {
    %4:array<i32, 2> = let array<i32, 2>(1i)
    %5:void = call %dst, %4
    ret
  }
}
)";
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, ModuleScopedStruct) {
    capabilities = core::ir::Capabilities{core::ir::Capability::kAllowModuleScopeLets};

    auto* str_ty =
        ty.Struct(mod.symbols.New("S"),
                  {
                      {mod.symbols.New("a"), ty.i32(), core::type::StructMemberAttributes{}},
                  });

    b.ir.root_block->Append(b.Var<private_>("a", b.Composite(str_ty, 1_i)));

    auto* src = R"(
S = struct @align(4) {
  a:i32 @offset(0)
}

$B1: {  # root
  %a:ptr<private, S, read_write> = var, S(1i)
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
S = struct @align(4) {
  a:i32 @offset(0)
}

$B1: {  # root
  %1:S = construct 1i
  %2:S = let %1
  %a:ptr<private, S, read_write> = var, %2
}

)";
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, ModuleScopedArray) {
    capabilities = core::ir::Capabilities{core::ir::Capability::kAllowModuleScopeLets};

    b.ir.root_block->Append(b.Var<private_>("a", b.Zero<array<i32, 2>>()));

    auto* src = R"(
$B1: {  # root
  %a:ptr<private, array<i32, 2>, read_write> = var, array<i32, 2>(0i)
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
$B1: {  # root
  %1:array<i32, 2> = construct 0i
  %2:array<i32, 2> = let %1
  %a:ptr<private, array<i32, 2>, read_write> = var, %2
}

)";
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, ModuleScopedStructNested) {
    capabilities = core::ir::Capabilities{core::ir::Capability::kAllowModuleScopeLets};

    auto* b_ty =
        ty.Struct(mod.symbols.New("B"),
                  {
                      {mod.symbols.New("c"), ty.f32(), core::type::StructMemberAttributes{}},
                  });

    auto* a_ty =
        ty.Struct(mod.symbols.New("A"),
                  {
                      {mod.symbols.New("z"), ty.i32(), core::type::StructMemberAttributes{}},
                      {mod.symbols.New("b"), b_ty, core::type::StructMemberAttributes{}},
                  });

    auto* str_ty = ty.Struct(mod.symbols.New("S"),
                             {
                                 {mod.symbols.New("a"), a_ty, core::type::StructMemberAttributes{}},
                             });

    b.ir.root_block->Append(
        b.Var<private_>("a", b.Composite(str_ty, b.Composite(a_ty, 1_i, b.Composite(b_ty, 1_f)))));

    auto* src = R"(
B = struct @align(4) {
  c:f32 @offset(0)
}

A = struct @align(4) {
  z:i32 @offset(0)
  b:B @offset(4)
}

S = struct @align(4) {
  a:A @offset(0)
}

$B1: {  # root
  %a:ptr<private, S, read_write> = var, S(A(1i, B(1.0f)))
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
B = struct @align(4) {
  c:f32 @offset(0)
}

A = struct @align(4) {
  z:i32 @offset(0)
  b:B @offset(4)
}

S = struct @align(4) {
  a:A @offset(0)
}

$B1: {  # root
  %1:B = construct 1.0f
  %2:B = let %1
  %3:A = construct 1i, %2
  %4:A = let %3
  %5:S = construct %4
  %6:S = let %5
  %a:ptr<private, S, read_write> = var, %6
}

)";
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriterPromoteInitializersTest, ModuleScopedArrayNestedInStruct) {
    capabilities = core::ir::Capabilities{core::ir::Capability::kAllowModuleScopeLets};

    auto* str_ty = ty.Struct(mod.symbols.New("S"), {
                                                       {mod.symbols.New("a"), ty.array<i32, 3>(),
                                                        core::type::StructMemberAttributes{}},
                                                   });

    b.ir.root_block->Append(b.Var<private_>("a", b.Composite(str_ty, b.Zero(ty.array<i32, 3>()))));

    auto* src = R"(
S = struct @align(4) {
  a:array<i32, 3> @offset(0)
}

$B1: {  # root
  %a:ptr<private, S, read_write> = var, S(array<i32, 3>(0i))
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
S = struct @align(4) {
  a:array<i32, 3> @offset(0)
}

$B1: {  # root
  %1:array<i32, 3> = construct 0i
  %2:array<i32, 3> = let %1
  %3:S = construct %2
  %4:S = let %3
  %a:ptr<private, S, read_write> = var, %4
}

)";
    Run(PromoteInitializers);

    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::hlsl::writer::raise
