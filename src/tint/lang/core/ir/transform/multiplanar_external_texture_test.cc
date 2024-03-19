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

#include "src/tint/lang/core/ir/transform/multiplanar_external_texture.h"

#include <utility>

#include "src/tint/lang/core/ir/transform/helper_test.h"
#include "src/tint/lang/core/type/external_texture.h"

namespace tint::core::ir::transform {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

using IR_MultiplanarExternalTextureTest = TransformTest;

TEST_F(IR_MultiplanarExternalTextureTest, NoRootBlock) {
    auto* func = b.Function("foo", ty.void_());
    func->Block()->Append(b.Return(func));

    auto* expect = R"(
%foo = func():void -> %b1 {
  %b1 = block {
    ret
  }
}
)";

    ExternalTextureOptions options;
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, DeclWithNoUses) {
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    mod.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read> = var @binding_point(1, 2)
}

%foo = func():void -> %b2 {
  %b2 = block {
    ret
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
}

%foo = func():void -> %b2 {
  %b2 = block {
    ret
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, LoadWithNoUses) {
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    mod.root_block->Append(var);

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read> = var @binding_point(1, 2)
}

%foo = func():void -> %b2 {
  %b2 = block {
    %3:texture_external = load %texture
    ret
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
}

%foo = func():void -> %b2 {
  %b2 = block {
    %5:texture_2d<f32> = load %texture_plane0
    %6:texture_2d<f32> = load %texture_plane1
    %7:tint_ExternalTextureParams = load %texture_params
    ret
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, TextureDimensions) {
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    mod.root_block->Append(var);

    auto* func = b.Function("foo", ty.vec2<u32>());
    b.Append(func->Block(), [&] {
        auto* load = b.Load(var->Result(0));
        auto* result = b.Call(ty.vec2<u32>(), core::BuiltinFn::kTextureDimensions, load);
        b.Return(func, result);
        mod.SetName(result, "result");
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read> = var @binding_point(1, 2)
}

%foo = func():vec2<u32> -> %b2 {
  %b2 = block {
    %3:texture_external = load %texture
    %result:vec2<u32> = textureDimensions %3
    ret %result
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
}

%foo = func():vec2<u32> -> %b2 {
  %b2 = block {
    %5:texture_2d<f32> = load %texture_plane0
    %6:texture_2d<f32> = load %texture_plane1
    %7:tint_ExternalTextureParams = load %texture_params
    %result:vec2<u32> = textureDimensions %5
    ret %result
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, TextureLoad) {
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    mod.root_block->Append(var);

    auto* func = b.Function("foo", ty.vec4<f32>());
    auto* coords = b.FunctionParam("coords", ty.vec2<u32>());
    func->SetParams({coords});
    b.Append(func->Block(), [&] {
        auto* load = b.Load(var->Result(0));
        auto* result = b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureLoad, load, coords);
        b.Return(func, result);
        mod.SetName(result, "result");
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read> = var @binding_point(1, 2)
}

%foo = func(%coords:vec2<u32>):vec4<f32> -> %b2 {
  %b2 = block {
    %4:texture_external = load %texture
    %result:vec4<f32> = textureLoad %4, %coords
    ret %result
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
}

%foo = func(%coords:vec2<u32>):vec4<f32> -> %b2 {
  %b2 = block {
    %6:texture_2d<f32> = load %texture_plane0
    %7:texture_2d<f32> = load %texture_plane1
    %8:tint_ExternalTextureParams = load %texture_params
    %9:vec4<f32> = call %tint_TextureLoadExternal, %6, %7, %8, %coords
    ret %9
  }
}
%tint_TextureLoadExternal = func(%plane_0:texture_2d<f32>, %plane_1:texture_2d<f32>, %params:tint_ExternalTextureParams, %coords_1:vec2<u32>):vec4<f32> -> %b3 {  # %coords_1: 'coords'
  %b3 = block {
    %15:u32 = access %params, 1u
    %16:mat3x4<f32> = access %params, 2u
    %17:mat3x2<f32> = access %params, 7u
    %18:vec2<u32> = access %params, 12u
    %19:vec2<f32> = access %params, 13u
    %20:vec2<i32> = convert %coords_1
    %21:vec2<i32> = convert %18
    %22:vec2<i32> = clamp vec2<i32>(0i), %20, %21
    %23:vec2<f32> = convert %22
    %24:vec3<f32> = construct %23, 1.0f
    %25:vec2<f32> = mul %17, %24
    %26:vec2<f32> = round %25
    %27:vec2<i32> = convert %26
    %28:u32 = access %params, 0u
    %29:bool = eq %28, 1u
    %30:vec3<f32>, %31:f32 = if %29 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %32:vec4<f32> = textureLoad %plane_0, %27, 0u
        %33:vec3<f32> = swizzle %32, xyz
        %34:f32 = access %32, 3u
        exit_if %33, %34  # if_1
      }
      %b5 = block {  # false
        %35:vec4<f32> = textureLoad %plane_0, %27, 0u
        %36:f32 = access %35, 0u
        %37:vec2<f32> = mul %26, %19
        %38:vec2<i32> = convert %37
        %39:vec4<f32> = textureLoad %plane_1, %38, 0u
        %40:vec2<f32> = swizzle %39, xy
        %41:vec4<f32> = construct %36, %40, 1.0f
        %42:vec3<f32> = mul %41, %16
        exit_if %42, 1.0f  # if_1
      }
    }
    %43:bool = eq %15, 0u
    %44:vec3<f32> = if %43 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %45:tint_GammaTransferParams = access %params, 3u
        %46:tint_GammaTransferParams = access %params, 4u
        %47:mat3x3<f32> = access %params, 5u
        %48:vec3<f32> = call %tint_GammaCorrection, %30, %45
        %50:vec3<f32> = mul %47, %48
        %51:vec3<f32> = call %tint_GammaCorrection, %50, %46
        exit_if %51  # if_2
      }
      %b7 = block {  # false
        exit_if %30  # if_2
      }
    }
    %52:vec4<f32> = construct %44, %31
    ret %52
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %55:f32 = access %params_1, 0u
    %56:f32 = access %params_1, 1u
    %57:f32 = access %params_1, 2u
    %58:f32 = access %params_1, 3u
    %59:f32 = access %params_1, 4u
    %60:f32 = access %params_1, 5u
    %61:f32 = access %params_1, 6u
    %62:vec3<f32> = construct %55
    %63:vec3<f32> = construct %59
    %64:vec3<f32> = abs %v
    %65:vec3<f32> = sign %v
    %66:vec3<bool> = lt %64, %63
    %67:vec3<f32> = mul %58, %64
    %68:vec3<f32> = add %67, %61
    %69:vec3<f32> = mul %65, %68
    %70:vec3<f32> = mul %56, %64
    %71:vec3<f32> = add %70, %57
    %72:vec3<f32> = pow %71, %62
    %73:vec3<f32> = add %72, %60
    %74:vec3<f32> = mul %65, %73
    %75:vec3<f32> = select %74, %69, %66
    ret %75
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, TextureLoad_SignedCoords) {
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    mod.root_block->Append(var);

    auto* func = b.Function("foo", ty.vec4<f32>());
    auto* coords = b.FunctionParam("coords", ty.vec2<i32>());
    func->SetParams({coords});
    b.Append(func->Block(), [&] {
        auto* load = b.Load(var->Result(0));
        auto* result = b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureLoad, load, coords);
        b.Return(func, result);
        mod.SetName(result, "result");
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read> = var @binding_point(1, 2)
}

%foo = func(%coords:vec2<i32>):vec4<f32> -> %b2 {
  %b2 = block {
    %4:texture_external = load %texture
    %result:vec4<f32> = textureLoad %4, %coords
    ret %result
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
}

%foo = func(%coords:vec2<i32>):vec4<f32> -> %b2 {
  %b2 = block {
    %6:texture_2d<f32> = load %texture_plane0
    %7:texture_2d<f32> = load %texture_plane1
    %8:tint_ExternalTextureParams = load %texture_params
    %9:vec2<u32> = convert %coords
    %10:vec4<f32> = call %tint_TextureLoadExternal, %6, %7, %8, %9
    ret %10
  }
}
%tint_TextureLoadExternal = func(%plane_0:texture_2d<f32>, %plane_1:texture_2d<f32>, %params:tint_ExternalTextureParams, %coords_1:vec2<u32>):vec4<f32> -> %b3 {  # %coords_1: 'coords'
  %b3 = block {
    %16:u32 = access %params, 1u
    %17:mat3x4<f32> = access %params, 2u
    %18:mat3x2<f32> = access %params, 7u
    %19:vec2<u32> = access %params, 12u
    %20:vec2<f32> = access %params, 13u
    %21:vec2<i32> = convert %coords_1
    %22:vec2<i32> = convert %19
    %23:vec2<i32> = clamp vec2<i32>(0i), %21, %22
    %24:vec2<f32> = convert %23
    %25:vec3<f32> = construct %24, 1.0f
    %26:vec2<f32> = mul %18, %25
    %27:vec2<f32> = round %26
    %28:vec2<i32> = convert %27
    %29:u32 = access %params, 0u
    %30:bool = eq %29, 1u
    %31:vec3<f32>, %32:f32 = if %30 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %33:vec4<f32> = textureLoad %plane_0, %28, 0u
        %34:vec3<f32> = swizzle %33, xyz
        %35:f32 = access %33, 3u
        exit_if %34, %35  # if_1
      }
      %b5 = block {  # false
        %36:vec4<f32> = textureLoad %plane_0, %28, 0u
        %37:f32 = access %36, 0u
        %38:vec2<f32> = mul %27, %20
        %39:vec2<i32> = convert %38
        %40:vec4<f32> = textureLoad %plane_1, %39, 0u
        %41:vec2<f32> = swizzle %40, xy
        %42:vec4<f32> = construct %37, %41, 1.0f
        %43:vec3<f32> = mul %42, %17
        exit_if %43, 1.0f  # if_1
      }
    }
    %44:bool = eq %16, 0u
    %45:vec3<f32> = if %44 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %46:tint_GammaTransferParams = access %params, 3u
        %47:tint_GammaTransferParams = access %params, 4u
        %48:mat3x3<f32> = access %params, 5u
        %49:vec3<f32> = call %tint_GammaCorrection, %31, %46
        %51:vec3<f32> = mul %48, %49
        %52:vec3<f32> = call %tint_GammaCorrection, %51, %47
        exit_if %52  # if_2
      }
      %b7 = block {  # false
        exit_if %31  # if_2
      }
    }
    %53:vec4<f32> = construct %45, %32
    ret %53
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %56:f32 = access %params_1, 0u
    %57:f32 = access %params_1, 1u
    %58:f32 = access %params_1, 2u
    %59:f32 = access %params_1, 3u
    %60:f32 = access %params_1, 4u
    %61:f32 = access %params_1, 5u
    %62:f32 = access %params_1, 6u
    %63:vec3<f32> = construct %56
    %64:vec3<f32> = construct %60
    %65:vec3<f32> = abs %v
    %66:vec3<f32> = sign %v
    %67:vec3<bool> = lt %65, %64
    %68:vec3<f32> = mul %59, %65
    %69:vec3<f32> = add %68, %62
    %70:vec3<f32> = mul %66, %69
    %71:vec3<f32> = mul %57, %65
    %72:vec3<f32> = add %71, %58
    %73:vec3<f32> = pow %72, %63
    %74:vec3<f32> = add %73, %61
    %75:vec3<f32> = mul %66, %74
    %76:vec3<f32> = select %75, %70, %67
    ret %76
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, TextureSampleBaseClampToEdge) {
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    mod.root_block->Append(var);

    auto* func = b.Function("foo", ty.vec4<f32>());
    auto* sampler = b.FunctionParam("sampler", ty.sampler());
    auto* coords = b.FunctionParam("coords", ty.vec2<f32>());
    func->SetParams({sampler, coords});
    b.Append(func->Block(), [&] {
        auto* load = b.Load(var->Result(0));
        auto* result = b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureSampleBaseClampToEdge, load,
                              sampler, coords);
        b.Return(func, result);
        mod.SetName(result, "result");
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read> = var @binding_point(1, 2)
}

%foo = func(%sampler:sampler, %coords:vec2<f32>):vec4<f32> -> %b2 {
  %b2 = block {
    %5:texture_external = load %texture
    %result:vec4<f32> = textureSampleBaseClampToEdge %5, %sampler, %coords
    ret %result
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
}

%foo = func(%sampler:sampler, %coords:vec2<f32>):vec4<f32> -> %b2 {
  %b2 = block {
    %7:texture_2d<f32> = load %texture_plane0
    %8:texture_2d<f32> = load %texture_plane1
    %9:tint_ExternalTextureParams = load %texture_params
    %10:vec4<f32> = call %tint_TextureSampleExternal, %7, %8, %9, %sampler, %coords
    ret %10
  }
}
%tint_TextureSampleExternal = func(%plane_0:texture_2d<f32>, %plane_1:texture_2d<f32>, %params:tint_ExternalTextureParams, %sampler_1:sampler, %coords_1:vec2<f32>):vec4<f32> -> %b3 {  # %sampler_1: 'sampler', %coords_1: 'coords'
  %b3 = block {
    %17:u32 = access %params, 1u
    %18:mat3x4<f32> = access %params, 2u
    %19:mat3x2<f32> = access %params, 6u
    %20:vec2<f32> = access %params, 8u
    %21:vec2<f32> = access %params, 9u
    %22:vec2<f32> = access %params, 10u
    %23:vec2<f32> = access %params, 11u
    %24:vec3<f32> = construct %coords_1, 1.0f
    %25:vec2<f32> = mul %19, %24
    %26:vec2<f32> = clamp %25, %20, %21
    %27:u32 = access %params, 0u
    %28:bool = eq %27, 1u
    %29:vec3<f32>, %30:f32 = if %28 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %31:vec4<f32> = textureSampleLevel %plane_0, %sampler_1, %26, 0.0f
        %32:vec3<f32> = swizzle %31, xyz
        %33:f32 = access %31, 3u
        exit_if %32, %33  # if_1
      }
      %b5 = block {  # false
        %34:vec4<f32> = textureSampleLevel %plane_0, %sampler_1, %26, 0.0f
        %35:f32 = access %34, 0u
        %36:vec2<f32> = clamp %25, %22, %23
        %37:vec4<f32> = textureSampleLevel %plane_1, %sampler_1, %36, 0.0f
        %38:vec2<f32> = swizzle %37, xy
        %39:vec4<f32> = construct %35, %38, 1.0f
        %40:vec3<f32> = mul %39, %18
        exit_if %40, 1.0f  # if_1
      }
    }
    %41:bool = eq %17, 0u
    %42:vec3<f32> = if %41 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %43:tint_GammaTransferParams = access %params, 3u
        %44:tint_GammaTransferParams = access %params, 4u
        %45:mat3x3<f32> = access %params, 5u
        %46:vec3<f32> = call %tint_GammaCorrection, %29, %43
        %48:vec3<f32> = mul %45, %46
        %49:vec3<f32> = call %tint_GammaCorrection, %48, %44
        exit_if %49  # if_2
      }
      %b7 = block {  # false
        exit_if %29  # if_2
      }
    }
    %50:vec4<f32> = construct %42, %30
    ret %50
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %53:f32 = access %params_1, 0u
    %54:f32 = access %params_1, 1u
    %55:f32 = access %params_1, 2u
    %56:f32 = access %params_1, 3u
    %57:f32 = access %params_1, 4u
    %58:f32 = access %params_1, 5u
    %59:f32 = access %params_1, 6u
    %60:vec3<f32> = construct %53
    %61:vec3<f32> = construct %57
    %62:vec3<f32> = abs %v
    %63:vec3<f32> = sign %v
    %64:vec3<bool> = lt %62, %61
    %65:vec3<f32> = mul %56, %62
    %66:vec3<f32> = add %65, %59
    %67:vec3<f32> = mul %63, %66
    %68:vec3<f32> = mul %54, %62
    %69:vec3<f32> = add %68, %55
    %70:vec3<f32> = pow %69, %60
    %71:vec3<f32> = add %70, %58
    %72:vec3<f32> = mul %63, %71
    %73:vec3<f32> = select %72, %67, %64
    ret %73
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, ViaUserFunctionParameter) {
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    mod.root_block->Append(var);

    auto* foo = b.Function("foo", ty.vec4<f32>());
    {
        auto* texture = b.FunctionParam("texture", ty.Get<core::type::ExternalTexture>());
        auto* sampler = b.FunctionParam("sampler", ty.sampler());
        auto* coords = b.FunctionParam("coords", ty.vec2<f32>());
        foo->SetParams({texture, sampler, coords});
        b.Append(foo->Block(), [&] {
            auto* result = b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureSampleBaseClampToEdge,
                                  texture, sampler, coords);
            b.Return(foo, result);
            mod.SetName(result, "result");
        });
    }

    auto* bar = b.Function("bar", ty.vec4<f32>());
    {
        auto* sampler = b.FunctionParam("sampler", ty.sampler());
        auto* coords = b.FunctionParam("coords", ty.vec2<f32>());
        bar->SetParams({sampler, coords});
        b.Append(bar->Block(), [&] {
            auto* load = b.Load(var->Result(0));
            auto* result = b.Call(ty.vec4<f32>(), foo, load, sampler, coords);
            b.Return(bar, result);
            mod.SetName(result, "result");
        });
    }

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read> = var @binding_point(1, 2)
}

%foo = func(%texture_1:texture_external, %sampler:sampler, %coords:vec2<f32>):vec4<f32> -> %b2 {  # %texture_1: 'texture'
  %b2 = block {
    %result:vec4<f32> = textureSampleBaseClampToEdge %texture_1, %sampler, %coords
    ret %result
  }
}
%bar = func(%sampler_1:sampler, %coords_1:vec2<f32>):vec4<f32> -> %b3 {  # %sampler_1: 'sampler', %coords_1: 'coords'
  %b3 = block {
    %10:texture_external = load %texture
    %result_1:vec4<f32> = call %foo, %10, %sampler_1, %coords_1  # %result_1: 'result'
    ret %result_1
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
}

%foo = func(%texture_plane0_1:texture_2d<f32>, %texture_plane1_1:texture_2d<f32>, %texture_params_1:tint_ExternalTextureParams, %sampler:sampler, %coords:vec2<f32>):vec4<f32> -> %b2 {  # %texture_plane0_1: 'texture_plane0', %texture_plane1_1: 'texture_plane1', %texture_params_1: 'texture_params'
  %b2 = block {
    %10:vec4<f32> = call %tint_TextureSampleExternal, %texture_plane0_1, %texture_plane1_1, %texture_params_1, %sampler, %coords
    ret %10
  }
}
%bar = func(%sampler_1:sampler, %coords_1:vec2<f32>):vec4<f32> -> %b3 {  # %sampler_1: 'sampler', %coords_1: 'coords'
  %b3 = block {
    %15:texture_2d<f32> = load %texture_plane0
    %16:texture_2d<f32> = load %texture_plane1
    %17:tint_ExternalTextureParams = load %texture_params
    %result:vec4<f32> = call %foo, %15, %16, %17, %sampler_1, %coords_1
    ret %result
  }
}
%tint_TextureSampleExternal = func(%plane_0:texture_2d<f32>, %plane_1:texture_2d<f32>, %params:tint_ExternalTextureParams, %sampler_2:sampler, %coords_2:vec2<f32>):vec4<f32> -> %b4 {  # %sampler_2: 'sampler', %coords_2: 'coords'
  %b4 = block {
    %24:u32 = access %params, 1u
    %25:mat3x4<f32> = access %params, 2u
    %26:mat3x2<f32> = access %params, 6u
    %27:vec2<f32> = access %params, 8u
    %28:vec2<f32> = access %params, 9u
    %29:vec2<f32> = access %params, 10u
    %30:vec2<f32> = access %params, 11u
    %31:vec3<f32> = construct %coords_2, 1.0f
    %32:vec2<f32> = mul %26, %31
    %33:vec2<f32> = clamp %32, %27, %28
    %34:u32 = access %params, 0u
    %35:bool = eq %34, 1u
    %36:vec3<f32>, %37:f32 = if %35 [t: %b5, f: %b6] {  # if_1
      %b5 = block {  # true
        %38:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %33, 0.0f
        %39:vec3<f32> = swizzle %38, xyz
        %40:f32 = access %38, 3u
        exit_if %39, %40  # if_1
      }
      %b6 = block {  # false
        %41:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %33, 0.0f
        %42:f32 = access %41, 0u
        %43:vec2<f32> = clamp %32, %29, %30
        %44:vec4<f32> = textureSampleLevel %plane_1, %sampler_2, %43, 0.0f
        %45:vec2<f32> = swizzle %44, xy
        %46:vec4<f32> = construct %42, %45, 1.0f
        %47:vec3<f32> = mul %46, %25
        exit_if %47, 1.0f  # if_1
      }
    }
    %48:bool = eq %24, 0u
    %49:vec3<f32> = if %48 [t: %b7, f: %b8] {  # if_2
      %b7 = block {  # true
        %50:tint_GammaTransferParams = access %params, 3u
        %51:tint_GammaTransferParams = access %params, 4u
        %52:mat3x3<f32> = access %params, 5u
        %53:vec3<f32> = call %tint_GammaCorrection, %36, %50
        %55:vec3<f32> = mul %52, %53
        %56:vec3<f32> = call %tint_GammaCorrection, %55, %51
        exit_if %56  # if_2
      }
      %b8 = block {  # false
        exit_if %36  # if_2
      }
    }
    %57:vec4<f32> = construct %49, %37
    ret %57
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b9 {  # %params_1: 'params'
  %b9 = block {
    %60:f32 = access %params_1, 0u
    %61:f32 = access %params_1, 1u
    %62:f32 = access %params_1, 2u
    %63:f32 = access %params_1, 3u
    %64:f32 = access %params_1, 4u
    %65:f32 = access %params_1, 5u
    %66:f32 = access %params_1, 6u
    %67:vec3<f32> = construct %60
    %68:vec3<f32> = construct %64
    %69:vec3<f32> = abs %v
    %70:vec3<f32> = sign %v
    %71:vec3<bool> = lt %69, %68
    %72:vec3<f32> = mul %63, %69
    %73:vec3<f32> = add %72, %66
    %74:vec3<f32> = mul %70, %73
    %75:vec3<f32> = mul %61, %69
    %76:vec3<f32> = add %75, %62
    %77:vec3<f32> = pow %76, %67
    %78:vec3<f32> = add %77, %65
    %79:vec3<f32> = mul %70, %78
    %80:vec3<f32> = select %79, %74, %71
    ret %80
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, MultipleUses) {
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    mod.root_block->Append(var);

    auto* foo = b.Function("foo", ty.vec4<f32>());
    {
        auto* texture = b.FunctionParam("texture", ty.Get<core::type::ExternalTexture>());
        auto* sampler = b.FunctionParam("sampler", ty.sampler());
        auto* coords = b.FunctionParam("coords", ty.vec2<f32>());
        foo->SetParams({texture, sampler, coords});
        b.Append(foo->Block(), [&] {
            auto* result = b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureSampleBaseClampToEdge,
                                  texture, sampler, coords);
            b.Return(foo, result);
            mod.SetName(result, "result");
        });
    }

    auto* bar = b.Function("bar", ty.vec4<f32>());
    {
        auto* sampler = b.FunctionParam("sampler", ty.sampler());
        auto* coords_f = b.FunctionParam("coords", ty.vec2<f32>());
        bar->SetParams({sampler, coords_f});
        b.Append(bar->Block(), [&] {
            auto* load_a = b.Load(var->Result(0));
            b.Call(ty.vec2<u32>(), core::BuiltinFn::kTextureDimensions, load_a);
            auto* load_b = b.Load(var->Result(0));
            b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureSampleBaseClampToEdge, load_b, sampler,
                   coords_f);
            auto* load_c = b.Load(var->Result(0));
            b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureSampleBaseClampToEdge, load_c, sampler,
                   coords_f);
            auto* load_d = b.Load(var->Result(0));
            auto* result_a = b.Call(ty.vec4<f32>(), foo, load_d, sampler, coords_f);
            auto* result_b = b.Call(ty.vec4<f32>(), foo, load_d, sampler, coords_f);
            b.Return(bar, b.Add(ty.vec4<f32>(), result_a, result_b));
            mod.SetName(result_a, "result_a");
            mod.SetName(result_b, "result_b");
        });
    }

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read> = var @binding_point(1, 2)
}

%foo = func(%texture_1:texture_external, %sampler:sampler, %coords:vec2<f32>):vec4<f32> -> %b2 {  # %texture_1: 'texture'
  %b2 = block {
    %result:vec4<f32> = textureSampleBaseClampToEdge %texture_1, %sampler, %coords
    ret %result
  }
}
%bar = func(%sampler_1:sampler, %coords_1:vec2<f32>):vec4<f32> -> %b3 {  # %sampler_1: 'sampler', %coords_1: 'coords'
  %b3 = block {
    %10:texture_external = load %texture
    %11:vec2<u32> = textureDimensions %10
    %12:texture_external = load %texture
    %13:vec4<f32> = textureSampleBaseClampToEdge %12, %sampler_1, %coords_1
    %14:texture_external = load %texture
    %15:vec4<f32> = textureSampleBaseClampToEdge %14, %sampler_1, %coords_1
    %16:texture_external = load %texture
    %result_a:vec4<f32> = call %foo, %16, %sampler_1, %coords_1
    %result_b:vec4<f32> = call %foo, %16, %sampler_1, %coords_1
    %19:vec4<f32> = add %result_a, %result_b
    ret %19
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
}

%foo = func(%texture_plane0_1:texture_2d<f32>, %texture_plane1_1:texture_2d<f32>, %texture_params_1:tint_ExternalTextureParams, %sampler:sampler, %coords:vec2<f32>):vec4<f32> -> %b2 {  # %texture_plane0_1: 'texture_plane0', %texture_plane1_1: 'texture_plane1', %texture_params_1: 'texture_params'
  %b2 = block {
    %10:vec4<f32> = call %tint_TextureSampleExternal, %texture_plane0_1, %texture_plane1_1, %texture_params_1, %sampler, %coords
    ret %10
  }
}
%bar = func(%sampler_1:sampler, %coords_1:vec2<f32>):vec4<f32> -> %b3 {  # %sampler_1: 'sampler', %coords_1: 'coords'
  %b3 = block {
    %15:texture_2d<f32> = load %texture_plane0
    %16:texture_2d<f32> = load %texture_plane1
    %17:tint_ExternalTextureParams = load %texture_params
    %18:vec2<u32> = textureDimensions %15
    %19:texture_2d<f32> = load %texture_plane0
    %20:texture_2d<f32> = load %texture_plane1
    %21:tint_ExternalTextureParams = load %texture_params
    %22:vec4<f32> = call %tint_TextureSampleExternal, %19, %20, %21, %sampler_1, %coords_1
    %23:texture_2d<f32> = load %texture_plane0
    %24:texture_2d<f32> = load %texture_plane1
    %25:tint_ExternalTextureParams = load %texture_params
    %26:vec4<f32> = call %tint_TextureSampleExternal, %23, %24, %25, %sampler_1, %coords_1
    %27:texture_2d<f32> = load %texture_plane0
    %28:texture_2d<f32> = load %texture_plane1
    %29:tint_ExternalTextureParams = load %texture_params
    %result_a:vec4<f32> = call %foo, %27, %28, %29, %sampler_1, %coords_1
    %result_b:vec4<f32> = call %foo, %27, %28, %29, %sampler_1, %coords_1
    %32:vec4<f32> = add %result_a, %result_b
    ret %32
  }
}
%tint_TextureSampleExternal = func(%plane_0:texture_2d<f32>, %plane_1:texture_2d<f32>, %params:tint_ExternalTextureParams, %sampler_2:sampler, %coords_2:vec2<f32>):vec4<f32> -> %b4 {  # %sampler_2: 'sampler', %coords_2: 'coords'
  %b4 = block {
    %38:u32 = access %params, 1u
    %39:mat3x4<f32> = access %params, 2u
    %40:mat3x2<f32> = access %params, 6u
    %41:vec2<f32> = access %params, 8u
    %42:vec2<f32> = access %params, 9u
    %43:vec2<f32> = access %params, 10u
    %44:vec2<f32> = access %params, 11u
    %45:vec3<f32> = construct %coords_2, 1.0f
    %46:vec2<f32> = mul %40, %45
    %47:vec2<f32> = clamp %46, %41, %42
    %48:u32 = access %params, 0u
    %49:bool = eq %48, 1u
    %50:vec3<f32>, %51:f32 = if %49 [t: %b5, f: %b6] {  # if_1
      %b5 = block {  # true
        %52:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %47, 0.0f
        %53:vec3<f32> = swizzle %52, xyz
        %54:f32 = access %52, 3u
        exit_if %53, %54  # if_1
      }
      %b6 = block {  # false
        %55:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %47, 0.0f
        %56:f32 = access %55, 0u
        %57:vec2<f32> = clamp %46, %43, %44
        %58:vec4<f32> = textureSampleLevel %plane_1, %sampler_2, %57, 0.0f
        %59:vec2<f32> = swizzle %58, xy
        %60:vec4<f32> = construct %56, %59, 1.0f
        %61:vec3<f32> = mul %60, %39
        exit_if %61, 1.0f  # if_1
      }
    }
    %62:bool = eq %38, 0u
    %63:vec3<f32> = if %62 [t: %b7, f: %b8] {  # if_2
      %b7 = block {  # true
        %64:tint_GammaTransferParams = access %params, 3u
        %65:tint_GammaTransferParams = access %params, 4u
        %66:mat3x3<f32> = access %params, 5u
        %67:vec3<f32> = call %tint_GammaCorrection, %50, %64
        %69:vec3<f32> = mul %66, %67
        %70:vec3<f32> = call %tint_GammaCorrection, %69, %65
        exit_if %70  # if_2
      }
      %b8 = block {  # false
        exit_if %50  # if_2
      }
    }
    %71:vec4<f32> = construct %63, %51
    ret %71
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b9 {  # %params_1: 'params'
  %b9 = block {
    %74:f32 = access %params_1, 0u
    %75:f32 = access %params_1, 1u
    %76:f32 = access %params_1, 2u
    %77:f32 = access %params_1, 3u
    %78:f32 = access %params_1, 4u
    %79:f32 = access %params_1, 5u
    %80:f32 = access %params_1, 6u
    %81:vec3<f32> = construct %74
    %82:vec3<f32> = construct %78
    %83:vec3<f32> = abs %v
    %84:vec3<f32> = sign %v
    %85:vec3<bool> = lt %83, %82
    %86:vec3<f32> = mul %77, %83
    %87:vec3<f32> = add %86, %80
    %88:vec3<f32> = mul %84, %87
    %89:vec3<f32> = mul %75, %83
    %90:vec3<f32> = add %89, %76
    %91:vec3<f32> = pow %90, %81
    %92:vec3<f32> = add %91, %79
    %93:vec3<f32> = mul %84, %92
    %94:vec3<f32> = select %93, %88, %85
    ret %94
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

TEST_F(IR_MultiplanarExternalTextureTest, MultipleTextures) {
    auto* var_a = b.Var("texture_a", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var_a->SetBindingPoint(1, 2);
    mod.root_block->Append(var_a);

    auto* var_b = b.Var("texture_b", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var_b->SetBindingPoint(2, 2);
    mod.root_block->Append(var_b);

    auto* var_c = b.Var("texture_c", ty.ptr(handle, ty.Get<core::type::ExternalTexture>()));
    var_c->SetBindingPoint(3, 2);
    mod.root_block->Append(var_c);

    auto* foo = b.Function("foo", ty.void_());
    auto* coords = b.FunctionParam("coords", ty.vec2<u32>());
    foo->SetParams({coords});
    b.Append(foo->Block(), [&] {
        auto* load_a = b.Load(var_a->Result(0));
        b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureLoad, load_a, coords);
        auto* load_b = b.Load(var_b->Result(0));
        b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureLoad, load_b, coords);
        auto* load_c = b.Load(var_c->Result(0));
        b.Call(ty.vec4<f32>(), core::BuiltinFn::kTextureLoad, load_c, coords);
        b.Return(foo);
    });

    auto* src = R"(
%b1 = block {  # root
  %texture_a:ptr<handle, texture_external, read> = var @binding_point(1, 2)
  %texture_b:ptr<handle, texture_external, read> = var @binding_point(2, 2)
  %texture_c:ptr<handle, texture_external, read> = var @binding_point(3, 2)
}

%foo = func(%coords:vec2<u32>):void -> %b2 {
  %b2 = block {
    %6:texture_external = load %texture_a
    %7:vec4<f32> = textureLoad %6, %coords
    %8:texture_external = load %texture_b
    %9:vec4<f32> = textureLoad %8, %coords
    %10:texture_external = load %texture_c
    %11:vec4<f32> = textureLoad %10, %coords
    ret
  }
}
)";
    auto* expect = R"(
tint_GammaTransferParams = struct @align(4) {
  G:f32 @offset(0)
  A:f32 @offset(4)
  B:f32 @offset(8)
  C:f32 @offset(12)
  D:f32 @offset(16)
  E:f32 @offset(20)
  F:f32 @offset(24)
  padding:u32 @offset(28)
}

tint_ExternalTextureParams = struct @align(16) {
  numPlanes:u32 @offset(0)
  doYuvToRgbConversionOnly:u32 @offset(4)
  yuvToRgbConversionMatrix:mat3x4<f32> @offset(16)
  gammaDecodeParams:tint_GammaTransferParams @offset(64)
  gammaEncodeParams:tint_GammaTransferParams @offset(96)
  gamutConversionMatrix:mat3x3<f32> @offset(128)
  coordTransformationMatrix:mat3x2<f32> @offset(176)
  loadTransformationMatrix:mat3x2<f32> @offset(200)
  samplePlane0RectMin:vec2<f32> @offset(224)
  samplePlane0RectMax:vec2<f32> @offset(232)
  samplePlane1RectMin:vec2<f32> @offset(240)
  samplePlane1RectMax:vec2<f32> @offset(248)
  displayVisibleRectMax:vec2<u32> @offset(256)
  plane1CoordFactor:vec2<f32> @offset(264)
}

%b1 = block {  # root
  %texture_a_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 2)
  %texture_a_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(1, 3)
  %texture_a_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(1, 4)
  %texture_b_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(2, 2)
  %texture_b_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(2, 3)
  %texture_b_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(2, 4)
  %texture_c_plane0:ptr<handle, texture_2d<f32>, read> = var @binding_point(3, 2)
  %texture_c_plane1:ptr<handle, texture_2d<f32>, read> = var @binding_point(3, 3)
  %texture_c_params:ptr<uniform, tint_ExternalTextureParams, read> = var @binding_point(3, 4)
}

%foo = func(%coords:vec2<u32>):void -> %b2 {
  %b2 = block {
    %12:texture_2d<f32> = load %texture_a_plane0
    %13:texture_2d<f32> = load %texture_a_plane1
    %14:tint_ExternalTextureParams = load %texture_a_params
    %15:vec4<f32> = call %tint_TextureLoadExternal, %12, %13, %14, %coords
    %17:texture_2d<f32> = load %texture_b_plane0
    %18:texture_2d<f32> = load %texture_b_plane1
    %19:tint_ExternalTextureParams = load %texture_b_params
    %20:vec4<f32> = call %tint_TextureLoadExternal, %17, %18, %19, %coords
    %21:texture_2d<f32> = load %texture_c_plane0
    %22:texture_2d<f32> = load %texture_c_plane1
    %23:tint_ExternalTextureParams = load %texture_c_params
    %24:vec4<f32> = call %tint_TextureLoadExternal, %21, %22, %23, %coords
    ret
  }
}
%tint_TextureLoadExternal = func(%plane_0:texture_2d<f32>, %plane_1:texture_2d<f32>, %params:tint_ExternalTextureParams, %coords_1:vec2<u32>):vec4<f32> -> %b3 {  # %coords_1: 'coords'
  %b3 = block {
    %29:u32 = access %params, 1u
    %30:mat3x4<f32> = access %params, 2u
    %31:mat3x2<f32> = access %params, 7u
    %32:vec2<u32> = access %params, 12u
    %33:vec2<f32> = access %params, 13u
    %34:vec2<i32> = convert %coords_1
    %35:vec2<i32> = convert %32
    %36:vec2<i32> = clamp vec2<i32>(0i), %34, %35
    %37:vec2<f32> = convert %36
    %38:vec3<f32> = construct %37, 1.0f
    %39:vec2<f32> = mul %31, %38
    %40:vec2<f32> = round %39
    %41:vec2<i32> = convert %40
    %42:u32 = access %params, 0u
    %43:bool = eq %42, 1u
    %44:vec3<f32>, %45:f32 = if %43 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %46:vec4<f32> = textureLoad %plane_0, %41, 0u
        %47:vec3<f32> = swizzle %46, xyz
        %48:f32 = access %46, 3u
        exit_if %47, %48  # if_1
      }
      %b5 = block {  # false
        %49:vec4<f32> = textureLoad %plane_0, %41, 0u
        %50:f32 = access %49, 0u
        %51:vec2<f32> = mul %40, %33
        %52:vec2<i32> = convert %51
        %53:vec4<f32> = textureLoad %plane_1, %52, 0u
        %54:vec2<f32> = swizzle %53, xy
        %55:vec4<f32> = construct %50, %54, 1.0f
        %56:vec3<f32> = mul %55, %30
        exit_if %56, 1.0f  # if_1
      }
    }
    %57:bool = eq %29, 0u
    %58:vec3<f32> = if %57 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %59:tint_GammaTransferParams = access %params, 3u
        %60:tint_GammaTransferParams = access %params, 4u
        %61:mat3x3<f32> = access %params, 5u
        %62:vec3<f32> = call %tint_GammaCorrection, %44, %59
        %64:vec3<f32> = mul %61, %62
        %65:vec3<f32> = call %tint_GammaCorrection, %64, %60
        exit_if %65  # if_2
      }
      %b7 = block {  # false
        exit_if %44  # if_2
      }
    }
    %66:vec4<f32> = construct %58, %45
    ret %66
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %69:f32 = access %params_1, 0u
    %70:f32 = access %params_1, 1u
    %71:f32 = access %params_1, 2u
    %72:f32 = access %params_1, 3u
    %73:f32 = access %params_1, 4u
    %74:f32 = access %params_1, 5u
    %75:f32 = access %params_1, 6u
    %76:vec3<f32> = construct %69
    %77:vec3<f32> = construct %73
    %78:vec3<f32> = abs %v
    %79:vec3<f32> = sign %v
    %80:vec3<bool> = lt %78, %77
    %81:vec3<f32> = mul %72, %78
    %82:vec3<f32> = add %81, %75
    %83:vec3<f32> = mul %79, %82
    %84:vec3<f32> = mul %70, %78
    %85:vec3<f32> = add %84, %71
    %86:vec3<f32> = pow %85, %76
    %87:vec3<f32> = add %86, %74
    %88:vec3<f32> = mul %79, %87
    %89:vec3<f32> = select %88, %83, %80
    ret %89
  }
}
)";

    EXPECT_EQ(src, str());

    ExternalTextureOptions options;
    options.bindings_map[{1u, 2u}] = {{1u, 3u}, {1u, 4u}};
    options.bindings_map[{2u, 2u}] = {{2u, 3u}, {2u, 4u}};
    options.bindings_map[{3u, 2u}] = {{3u, 3u}, {3u, 4u}};
    Run(MultiplanarExternalTexture, options);
    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::core::ir::transform
