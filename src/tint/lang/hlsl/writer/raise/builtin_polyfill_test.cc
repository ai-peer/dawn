// Copyright 2023 The Dawn & Tint Authors
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

#include "src/tint/lang/hlsl/writer/raise/builtin_polyfill.h"

#include "gtest/gtest.h"
#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/core/ir/transform/helper_test.h"
#include "src/tint/lang/core/number.h"
#include "src/tint/lang/core/type/builtin_structs.h"
#include "src/tint/lang/core/type/storage_texture.h"

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

namespace tint::hlsl::writer::raise {
namespace {

using HlslWriter_BuiltinPolyfillTest = core::ir::transform::TransformTest;

TEST_F(HlslWriter_BuiltinPolyfillTest, BitcastIdentity) {
    auto* a = b.FunctionParam<i32>("a");
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({a});
    b.Append(func->Block(), [&] { b.Return(func, b.Bitcast<i32>(a)); });

    auto* src = R"(
%foo = func(%a:i32):i32 {
  $B1: {
    %3:i32 = bitcast %a
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%a:i32):i32 {
  $B1: {
    ret %a
  }
}
)";

    Run(BuiltinPolyfill);
    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriter_BuiltinPolyfillTest, Asuint) {
    auto* a = b.FunctionParam<i32>("a");
    auto* func = b.Function("foo", ty.u32());
    func->SetParams({a});
    b.Append(func->Block(), [&] { b.Return(func, b.Bitcast<u32>(a)); });

    auto* src = R"(
%foo = func(%a:i32):u32 {
  $B1: {
    %3:u32 = bitcast %a
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%a:i32):u32 {
  $B1: {
    %3:u32 = hlsl.asuint %a
    ret %3
  }
}
)";

    Run(BuiltinPolyfill);
    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriter_BuiltinPolyfillTest, Asint) {
    auto* a = b.FunctionParam<u32>("a");
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({a});
    b.Append(func->Block(), [&] { b.Return(func, b.Bitcast<i32>(a)); });

    auto* src = R"(
%foo = func(%a:u32):i32 {
  $B1: {
    %3:i32 = bitcast %a
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%a:u32):i32 {
  $B1: {
    %3:i32 = hlsl.asint %a
    ret %3
  }
}
)";

    Run(BuiltinPolyfill);
    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriter_BuiltinPolyfillTest, Asfloat) {
    auto* a = b.FunctionParam<i32>("a");
    auto* func = b.Function("foo", ty.f32());
    func->SetParams({a});
    b.Append(func->Block(), [&] { b.Return(func, b.Bitcast<f32>(a)); });

    auto* src = R"(
%foo = func(%a:i32):f32 {
  $B1: {
    %3:f32 = bitcast %a
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%a:i32):f32 {
  $B1: {
    %3:f32 = hlsl.asfloat %a
    ret %3
  }
}
)";

    Run(BuiltinPolyfill);
    EXPECT_EQ(expect, str());
}

TEST_F(HlslWriter_BuiltinPolyfillTest, AsfloatVec) {
    auto* a = b.FunctionParam<vec3<i32>>("a");
    auto* func = b.Function("foo", ty.vec<f32, 3>());
    func->SetParams({a});
    b.Append(func->Block(), [&] { b.Return(func, b.Bitcast(ty.vec(ty.f32(), 3), a)); });

    auto* src = R"(
%foo = func(%a:vec3<i32>):vec3<f32> {
  $B1: {
    %3:vec3<f32> = bitcast %a
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%a:vec3<i32>):vec3<f32> {
  $B1: {
    %3:vec3<f32> = hlsl.asfloat %a
    ret %3
  }
}
)";

    Run(BuiltinPolyfill);
    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::hlsl::writer::raise
