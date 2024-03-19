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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
    %17:mat3x2<f32> = access %params, 6u
    %18:vec2<f32> = access %params, 7u
    %19:vec2<f32> = access %params, 8u
    %20:vec2<u32> = access %params, 9u
    %21:vec2<u32> = access %params, 10u
    %22:u32 = access %20, 0u
    %23:u32 = access %20, 1u
    %24:vec2<u32> = access %params, 11u
    %25:u32 = access %24, 0u
    %26:u32 = access %24, 1u
    %27:f32 = convert %22
    %28:vec3<f32> = construct %27, 0.0f, 0.0f
    %29:f32 = convert %23
    %30:vec3<f32> = construct 0.0f, %29, 0.0f
    %31:vec3<f32> = construct 0.0f, 0.0f, 1.0f
    %32:mat3x3<f32> = construct %28, %30, %31
    %33:f32 = convert %25
    %34:f32 = div 1.0f, %33
    %35:vec3<f32> = construct %34, 0.0f, 0.0f
    %36:f32 = convert %26
    %37:f32 = div 1.0f, %36
    %38:vec3<f32> = construct 0.0f, %37, 0.0f
    %39:vec3<f32> = construct 0.0f, 0.0f, 1.0f
    %40:mat3x3<f32> = construct %35, %38, %39
    %41:vec2<f32> = access %17, 0u
    %42:vec3<f32> = construct %41, 0.0f
    %43:vec2<f32> = access %17, 1u
    %44:vec3<f32> = construct %43, 0.0f
    %45:vec2<f32> = access %17, 2u
    %46:vec3<f32> = construct %45, 1.0f
    %47:mat3x3<f32> = construct %42, %44, %46
    %48:mat3x3<f32> = mul %32, %47
    %49:mat3x3<f32> = mul %40, %48
    %50:vec3<f32> = construct %coords_1, 1.0f
    %51:vec3<f32> = mul %49, %50
    %52:vec2<f32> = swizzle %51, xy
    %53:vec2<i32> = convert %52
    %54:vec2<f32> = convert %20
    %55:vec2<f32> = mul %18, %54
    %56:vec2<i32> = convert %55
    %57:vec2<f32> = convert %20
    %58:vec2<f32> = mul %19, %57
    %59:vec2<f32> = sub %58, vec2<f32>(1.0f)
    %60:vec2<i32> = convert %59
    %61:vec2<i32> = clamp %53, %56, %60
    %62:u32 = access %params, 0u
    %63:bool = eq %62, 1u
    %64:vec3<f32>, %65:f32 = if %63 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %66:vec4<f32> = textureLoad %plane_0, %61, 0u
        %67:vec3<f32> = swizzle %66, xyz
        %68:f32 = access %66, 3u
        exit_if %67, %68  # if_1
      }
      %b5 = block {  # false
        %69:vec4<f32> = textureLoad %plane_0, %61, 0u
        %70:f32 = access %69, 0u
        %71:vec2<f32> = convert %20
        %72:vec2<f32> = convert %21
        %73:vec2<f32> = div %71, %72
        %74:vec2<f32> = convert %61
        %75:vec2<f32> = mul %74, %73
        %76:vec2<i32> = convert %75
        %77:vec2<f32> = convert %21
        %78:vec2<f32> = mul %18, %77
        %79:vec2<i32> = convert %78
        %80:vec2<f32> = convert %21
        %81:vec2<f32> = mul %19, %80
        %82:vec2<f32> = sub %81, vec2<f32>(1.0f)
        %83:vec2<i32> = convert %82
        %84:vec2<i32> = clamp %76, %79, %83
        %85:vec4<f32> = textureLoad %plane_1, %84, 0u
        %86:vec2<f32> = swizzle %85, xy
        %87:vec4<f32> = construct %70, %86, 1.0f
        %88:vec3<f32> = mul %87, %16
        exit_if %88, 1.0f  # if_1
      }
    }
    %89:bool = eq %15, 0u
    %90:vec3<f32> = if %89 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %91:tint_GammaTransferParams = access %params, 3u
        %92:tint_GammaTransferParams = access %params, 4u
        %93:mat3x3<f32> = access %params, 5u
        %94:vec3<f32> = call %tint_GammaCorrection, %64, %91
        %96:vec3<f32> = mul %93, %94
        %97:vec3<f32> = call %tint_GammaCorrection, %96, %92
        exit_if %97  # if_2
      }
      %b7 = block {  # false
        exit_if %64  # if_2
      }
    }
    %98:vec4<f32> = construct %90, %65
    ret %98
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %101:f32 = access %params_1, 0u
    %102:f32 = access %params_1, 1u
    %103:f32 = access %params_1, 2u
    %104:f32 = access %params_1, 3u
    %105:f32 = access %params_1, 4u
    %106:f32 = access %params_1, 5u
    %107:f32 = access %params_1, 6u
    %108:vec3<f32> = construct %101
    %109:vec3<f32> = construct %105
    %110:vec3<f32> = abs %v
    %111:vec3<f32> = sign %v
    %112:vec3<bool> = lt %110, %109
    %113:vec3<f32> = mul %104, %110
    %114:vec3<f32> = add %113, %107
    %115:vec3<f32> = mul %111, %114
    %116:vec3<f32> = mul %102, %110
    %117:vec3<f32> = add %116, %103
    %118:vec3<f32> = pow %117, %108
    %119:vec3<f32> = add %118, %106
    %120:vec3<f32> = mul %111, %119
    %121:vec3<f32> = select %120, %115, %112
    ret %121
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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
    %18:mat3x2<f32> = access %params, 6u
    %19:vec2<f32> = access %params, 7u
    %20:vec2<f32> = access %params, 8u
    %21:vec2<u32> = access %params, 9u
    %22:vec2<u32> = access %params, 10u
    %23:u32 = access %21, 0u
    %24:u32 = access %21, 1u
    %25:vec2<u32> = access %params, 11u
    %26:u32 = access %25, 0u
    %27:u32 = access %25, 1u
    %28:f32 = convert %23
    %29:vec3<f32> = construct %28, 0.0f, 0.0f
    %30:f32 = convert %24
    %31:vec3<f32> = construct 0.0f, %30, 0.0f
    %32:vec3<f32> = construct 0.0f, 0.0f, 1.0f
    %33:mat3x3<f32> = construct %29, %31, %32
    %34:f32 = convert %26
    %35:f32 = div 1.0f, %34
    %36:vec3<f32> = construct %35, 0.0f, 0.0f
    %37:f32 = convert %27
    %38:f32 = div 1.0f, %37
    %39:vec3<f32> = construct 0.0f, %38, 0.0f
    %40:vec3<f32> = construct 0.0f, 0.0f, 1.0f
    %41:mat3x3<f32> = construct %36, %39, %40
    %42:vec2<f32> = access %18, 0u
    %43:vec3<f32> = construct %42, 0.0f
    %44:vec2<f32> = access %18, 1u
    %45:vec3<f32> = construct %44, 0.0f
    %46:vec2<f32> = access %18, 2u
    %47:vec3<f32> = construct %46, 1.0f
    %48:mat3x3<f32> = construct %43, %45, %47
    %49:mat3x3<f32> = mul %33, %48
    %50:mat3x3<f32> = mul %41, %49
    %51:vec3<f32> = construct %coords_1, 1.0f
    %52:vec3<f32> = mul %50, %51
    %53:vec2<f32> = swizzle %52, xy
    %54:vec2<i32> = convert %53
    %55:vec2<f32> = convert %21
    %56:vec2<f32> = mul %19, %55
    %57:vec2<i32> = convert %56
    %58:vec2<f32> = convert %21
    %59:vec2<f32> = mul %20, %58
    %60:vec2<f32> = sub %59, vec2<f32>(1.0f)
    %61:vec2<i32> = convert %60
    %62:vec2<i32> = clamp %54, %57, %61
    %63:u32 = access %params, 0u
    %64:bool = eq %63, 1u
    %65:vec3<f32>, %66:f32 = if %64 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %67:vec4<f32> = textureLoad %plane_0, %62, 0u
        %68:vec3<f32> = swizzle %67, xyz
        %69:f32 = access %67, 3u
        exit_if %68, %69  # if_1
      }
      %b5 = block {  # false
        %70:vec4<f32> = textureLoad %plane_0, %62, 0u
        %71:f32 = access %70, 0u
        %72:vec2<f32> = convert %21
        %73:vec2<f32> = convert %22
        %74:vec2<f32> = div %72, %73
        %75:vec2<f32> = convert %62
        %76:vec2<f32> = mul %75, %74
        %77:vec2<i32> = convert %76
        %78:vec2<f32> = convert %22
        %79:vec2<f32> = mul %19, %78
        %80:vec2<i32> = convert %79
        %81:vec2<f32> = convert %22
        %82:vec2<f32> = mul %20, %81
        %83:vec2<f32> = sub %82, vec2<f32>(1.0f)
        %84:vec2<i32> = convert %83
        %85:vec2<i32> = clamp %77, %80, %84
        %86:vec4<f32> = textureLoad %plane_1, %85, 0u
        %87:vec2<f32> = swizzle %86, xy
        %88:vec4<f32> = construct %71, %87, 1.0f
        %89:vec3<f32> = mul %88, %17
        exit_if %89, 1.0f  # if_1
      }
    }
    %90:bool = eq %16, 0u
    %91:vec3<f32> = if %90 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %92:tint_GammaTransferParams = access %params, 3u
        %93:tint_GammaTransferParams = access %params, 4u
        %94:mat3x3<f32> = access %params, 5u
        %95:vec3<f32> = call %tint_GammaCorrection, %65, %92
        %97:vec3<f32> = mul %94, %95
        %98:vec3<f32> = call %tint_GammaCorrection, %97, %93
        exit_if %98  # if_2
      }
      %b7 = block {  # false
        exit_if %65  # if_2
      }
    }
    %99:vec4<f32> = construct %91, %66
    ret %99
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %102:f32 = access %params_1, 0u
    %103:f32 = access %params_1, 1u
    %104:f32 = access %params_1, 2u
    %105:f32 = access %params_1, 3u
    %106:f32 = access %params_1, 4u
    %107:f32 = access %params_1, 5u
    %108:f32 = access %params_1, 6u
    %109:vec3<f32> = construct %102
    %110:vec3<f32> = construct %106
    %111:vec3<f32> = abs %v
    %112:vec3<f32> = sign %v
    %113:vec3<bool> = lt %111, %110
    %114:vec3<f32> = mul %105, %111
    %115:vec3<f32> = add %114, %108
    %116:vec3<f32> = mul %112, %115
    %117:vec3<f32> = mul %103, %111
    %118:vec3<f32> = add %117, %104
    %119:vec3<f32> = pow %118, %109
    %120:vec3<f32> = add %119, %107
    %121:vec3<f32> = mul %112, %120
    %122:vec3<f32> = select %121, %116, %113
    ret %122
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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
    %20:vec2<f32> = access %params, 7u
    %21:vec2<f32> = access %params, 8u
    %22:vec2<u32> = access %params, 9u
    %23:vec2<u32> = access %params, 10u
    %24:vec3<f32> = construct %coords_1, 1.0f
    %25:vec2<f32> = mul %19, %24
    %26:vec2<f32> = convert %22
    %27:vec2<f32> = div vec2<f32>(0.5f), %26
    %28:vec2<f32> = add %20, %27
    %29:vec2<f32> = sub %21, %27
    %30:vec2<f32> = clamp %25, %28, %29
    %31:u32 = access %params, 0u
    %32:bool = eq %31, 1u
    %33:vec3<f32>, %34:f32 = if %32 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %35:vec4<f32> = textureSampleLevel %plane_0, %sampler_1, %30, 0.0f
        %36:vec3<f32> = swizzle %35, xyz
        %37:f32 = access %35, 3u
        exit_if %36, %37  # if_1
      }
      %b5 = block {  # false
        %38:vec4<f32> = textureSampleLevel %plane_0, %sampler_1, %30, 0.0f
        %39:f32 = access %38, 0u
        %40:vec2<f32> = convert %23
        %41:vec2<f32> = div vec2<f32>(0.5f), %40
        %42:vec2<f32> = add %20, %41
        %43:vec2<f32> = sub %21, %41
        %44:vec2<f32> = clamp %25, %42, %43
        %45:vec4<f32> = textureSampleLevel %plane_1, %sampler_1, %44, 0.0f
        %46:vec2<f32> = swizzle %45, xy
        %47:vec4<f32> = construct %39, %46, 1.0f
        %48:vec3<f32> = mul %47, %18
        exit_if %48, 1.0f  # if_1
      }
    }
    %49:bool = eq %17, 0u
    %50:vec3<f32> = if %49 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %51:tint_GammaTransferParams = access %params, 3u
        %52:tint_GammaTransferParams = access %params, 4u
        %53:mat3x3<f32> = access %params, 5u
        %54:vec3<f32> = call %tint_GammaCorrection, %33, %51
        %56:vec3<f32> = mul %53, %54
        %57:vec3<f32> = call %tint_GammaCorrection, %56, %52
        exit_if %57  # if_2
      }
      %b7 = block {  # false
        exit_if %33  # if_2
      }
    }
    %58:vec4<f32> = construct %50, %34
    ret %58
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %61:f32 = access %params_1, 0u
    %62:f32 = access %params_1, 1u
    %63:f32 = access %params_1, 2u
    %64:f32 = access %params_1, 3u
    %65:f32 = access %params_1, 4u
    %66:f32 = access %params_1, 5u
    %67:f32 = access %params_1, 6u
    %68:vec3<f32> = construct %61
    %69:vec3<f32> = construct %65
    %70:vec3<f32> = abs %v
    %71:vec3<f32> = sign %v
    %72:vec3<bool> = lt %70, %69
    %73:vec3<f32> = mul %64, %70
    %74:vec3<f32> = add %73, %67
    %75:vec3<f32> = mul %71, %74
    %76:vec3<f32> = mul %62, %70
    %77:vec3<f32> = add %76, %63
    %78:vec3<f32> = pow %77, %68
    %79:vec3<f32> = add %78, %66
    %80:vec3<f32> = mul %71, %79
    %81:vec3<f32> = select %80, %75, %72
    ret %81
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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
    %27:vec2<f32> = access %params, 7u
    %28:vec2<f32> = access %params, 8u
    %29:vec2<u32> = access %params, 9u
    %30:vec2<u32> = access %params, 10u
    %31:vec3<f32> = construct %coords_2, 1.0f
    %32:vec2<f32> = mul %26, %31
    %33:vec2<f32> = convert %29
    %34:vec2<f32> = div vec2<f32>(0.5f), %33
    %35:vec2<f32> = add %27, %34
    %36:vec2<f32> = sub %28, %34
    %37:vec2<f32> = clamp %32, %35, %36
    %38:u32 = access %params, 0u
    %39:bool = eq %38, 1u
    %40:vec3<f32>, %41:f32 = if %39 [t: %b5, f: %b6] {  # if_1
      %b5 = block {  # true
        %42:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %37, 0.0f
        %43:vec3<f32> = swizzle %42, xyz
        %44:f32 = access %42, 3u
        exit_if %43, %44  # if_1
      }
      %b6 = block {  # false
        %45:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %37, 0.0f
        %46:f32 = access %45, 0u
        %47:vec2<f32> = convert %30
        %48:vec2<f32> = div vec2<f32>(0.5f), %47
        %49:vec2<f32> = add %27, %48
        %50:vec2<f32> = sub %28, %48
        %51:vec2<f32> = clamp %32, %49, %50
        %52:vec4<f32> = textureSampleLevel %plane_1, %sampler_2, %51, 0.0f
        %53:vec2<f32> = swizzle %52, xy
        %54:vec4<f32> = construct %46, %53, 1.0f
        %55:vec3<f32> = mul %54, %25
        exit_if %55, 1.0f  # if_1
      }
    }
    %56:bool = eq %24, 0u
    %57:vec3<f32> = if %56 [t: %b7, f: %b8] {  # if_2
      %b7 = block {  # true
        %58:tint_GammaTransferParams = access %params, 3u
        %59:tint_GammaTransferParams = access %params, 4u
        %60:mat3x3<f32> = access %params, 5u
        %61:vec3<f32> = call %tint_GammaCorrection, %40, %58
        %63:vec3<f32> = mul %60, %61
        %64:vec3<f32> = call %tint_GammaCorrection, %63, %59
        exit_if %64  # if_2
      }
      %b8 = block {  # false
        exit_if %40  # if_2
      }
    }
    %65:vec4<f32> = construct %57, %41
    ret %65
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b9 {  # %params_1: 'params'
  %b9 = block {
    %68:f32 = access %params_1, 0u
    %69:f32 = access %params_1, 1u
    %70:f32 = access %params_1, 2u
    %71:f32 = access %params_1, 3u
    %72:f32 = access %params_1, 4u
    %73:f32 = access %params_1, 5u
    %74:f32 = access %params_1, 6u
    %75:vec3<f32> = construct %68
    %76:vec3<f32> = construct %72
    %77:vec3<f32> = abs %v
    %78:vec3<f32> = sign %v
    %79:vec3<bool> = lt %77, %76
    %80:vec3<f32> = mul %71, %77
    %81:vec3<f32> = add %80, %74
    %82:vec3<f32> = mul %78, %81
    %83:vec3<f32> = mul %69, %77
    %84:vec3<f32> = add %83, %70
    %85:vec3<f32> = pow %84, %75
    %86:vec3<f32> = add %85, %73
    %87:vec3<f32> = mul %78, %86
    %88:vec3<f32> = select %87, %82, %79
    ret %88
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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
    %41:vec2<f32> = access %params, 7u
    %42:vec2<f32> = access %params, 8u
    %43:vec2<u32> = access %params, 9u
    %44:vec2<u32> = access %params, 10u
    %45:vec3<f32> = construct %coords_2, 1.0f
    %46:vec2<f32> = mul %40, %45
    %47:vec2<f32> = convert %43
    %48:vec2<f32> = div vec2<f32>(0.5f), %47
    %49:vec2<f32> = add %41, %48
    %50:vec2<f32> = sub %42, %48
    %51:vec2<f32> = clamp %46, %49, %50
    %52:u32 = access %params, 0u
    %53:bool = eq %52, 1u
    %54:vec3<f32>, %55:f32 = if %53 [t: %b5, f: %b6] {  # if_1
      %b5 = block {  # true
        %56:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %51, 0.0f
        %57:vec3<f32> = swizzle %56, xyz
        %58:f32 = access %56, 3u
        exit_if %57, %58  # if_1
      }
      %b6 = block {  # false
        %59:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %51, 0.0f
        %60:f32 = access %59, 0u
        %61:vec2<f32> = convert %44
        %62:vec2<f32> = div vec2<f32>(0.5f), %61
        %63:vec2<f32> = add %41, %62
        %64:vec2<f32> = sub %42, %62
        %65:vec2<f32> = clamp %46, %63, %64
        %66:vec4<f32> = textureSampleLevel %plane_1, %sampler_2, %65, 0.0f
        %67:vec2<f32> = swizzle %66, xy
        %68:vec4<f32> = construct %60, %67, 1.0f
        %69:vec3<f32> = mul %68, %39
        exit_if %69, 1.0f  # if_1
      }
    }
    %70:bool = eq %38, 0u
    %71:vec3<f32> = if %70 [t: %b7, f: %b8] {  # if_2
      %b7 = block {  # true
        %72:tint_GammaTransferParams = access %params, 3u
        %73:tint_GammaTransferParams = access %params, 4u
        %74:mat3x3<f32> = access %params, 5u
        %75:vec3<f32> = call %tint_GammaCorrection, %54, %72
        %77:vec3<f32> = mul %74, %75
        %78:vec3<f32> = call %tint_GammaCorrection, %77, %73
        exit_if %78  # if_2
      }
      %b8 = block {  # false
        exit_if %54  # if_2
      }
    }
    %79:vec4<f32> = construct %71, %55
    ret %79
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b9 {  # %params_1: 'params'
  %b9 = block {
    %82:f32 = access %params_1, 0u
    %83:f32 = access %params_1, 1u
    %84:f32 = access %params_1, 2u
    %85:f32 = access %params_1, 3u
    %86:f32 = access %params_1, 4u
    %87:f32 = access %params_1, 5u
    %88:f32 = access %params_1, 6u
    %89:vec3<f32> = construct %82
    %90:vec3<f32> = construct %86
    %91:vec3<f32> = abs %v
    %92:vec3<f32> = sign %v
    %93:vec3<bool> = lt %91, %90
    %94:vec3<f32> = mul %85, %91
    %95:vec3<f32> = add %94, %88
    %96:vec3<f32> = mul %92, %95
    %97:vec3<f32> = mul %83, %91
    %98:vec3<f32> = add %97, %84
    %99:vec3<f32> = pow %98, %89
    %100:vec3<f32> = add %99, %87
    %101:vec3<f32> = mul %92, %100
    %102:vec3<f32> = select %101, %96, %93
    ret %102
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
  visibleRectMin:vec2<f32> @offset(200)
  visibleRectMax:vec2<f32> @offset(208)
  plane0Size:vec2<u32> @offset(216)
  plane1Size:vec2<u32> @offset(224)
  displayVisibleSize:vec2<u32> @offset(232)
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
    %31:mat3x2<f32> = access %params, 6u
    %32:vec2<f32> = access %params, 7u
    %33:vec2<f32> = access %params, 8u
    %34:vec2<u32> = access %params, 9u
    %35:vec2<u32> = access %params, 10u
    %36:u32 = access %34, 0u
    %37:u32 = access %34, 1u
    %38:vec2<u32> = access %params, 11u
    %39:u32 = access %38, 0u
    %40:u32 = access %38, 1u
    %41:f32 = convert %36
    %42:vec3<f32> = construct %41, 0.0f, 0.0f
    %43:f32 = convert %37
    %44:vec3<f32> = construct 0.0f, %43, 0.0f
    %45:vec3<f32> = construct 0.0f, 0.0f, 1.0f
    %46:mat3x3<f32> = construct %42, %44, %45
    %47:f32 = convert %39
    %48:f32 = div 1.0f, %47
    %49:vec3<f32> = construct %48, 0.0f, 0.0f
    %50:f32 = convert %40
    %51:f32 = div 1.0f, %50
    %52:vec3<f32> = construct 0.0f, %51, 0.0f
    %53:vec3<f32> = construct 0.0f, 0.0f, 1.0f
    %54:mat3x3<f32> = construct %49, %52, %53
    %55:vec2<f32> = access %31, 0u
    %56:vec3<f32> = construct %55, 0.0f
    %57:vec2<f32> = access %31, 1u
    %58:vec3<f32> = construct %57, 0.0f
    %59:vec2<f32> = access %31, 2u
    %60:vec3<f32> = construct %59, 1.0f
    %61:mat3x3<f32> = construct %56, %58, %60
    %62:mat3x3<f32> = mul %46, %61
    %63:mat3x3<f32> = mul %54, %62
    %64:vec3<f32> = construct %coords_1, 1.0f
    %65:vec3<f32> = mul %63, %64
    %66:vec2<f32> = swizzle %65, xy
    %67:vec2<i32> = convert %66
    %68:vec2<f32> = convert %34
    %69:vec2<f32> = mul %32, %68
    %70:vec2<i32> = convert %69
    %71:vec2<f32> = convert %34
    %72:vec2<f32> = mul %33, %71
    %73:vec2<f32> = sub %72, vec2<f32>(1.0f)
    %74:vec2<i32> = convert %73
    %75:vec2<i32> = clamp %67, %70, %74
    %76:u32 = access %params, 0u
    %77:bool = eq %76, 1u
    %78:vec3<f32>, %79:f32 = if %77 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %80:vec4<f32> = textureLoad %plane_0, %75, 0u
        %81:vec3<f32> = swizzle %80, xyz
        %82:f32 = access %80, 3u
        exit_if %81, %82  # if_1
      }
      %b5 = block {  # false
        %83:vec4<f32> = textureLoad %plane_0, %75, 0u
        %84:f32 = access %83, 0u
        %85:vec2<f32> = convert %34
        %86:vec2<f32> = convert %35
        %87:vec2<f32> = div %85, %86
        %88:vec2<f32> = convert %75
        %89:vec2<f32> = mul %88, %87
        %90:vec2<i32> = convert %89
        %91:vec2<f32> = convert %35
        %92:vec2<f32> = mul %32, %91
        %93:vec2<i32> = convert %92
        %94:vec2<f32> = convert %35
        %95:vec2<f32> = mul %33, %94
        %96:vec2<f32> = sub %95, vec2<f32>(1.0f)
        %97:vec2<i32> = convert %96
        %98:vec2<i32> = clamp %90, %93, %97
        %99:vec4<f32> = textureLoad %plane_1, %98, 0u
        %100:vec2<f32> = swizzle %99, xy
        %101:vec4<f32> = construct %84, %100, 1.0f
        %102:vec3<f32> = mul %101, %30
        exit_if %102, 1.0f  # if_1
      }
    }
    %103:bool = eq %29, 0u
    %104:vec3<f32> = if %103 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %105:tint_GammaTransferParams = access %params, 3u
        %106:tint_GammaTransferParams = access %params, 4u
        %107:mat3x3<f32> = access %params, 5u
        %108:vec3<f32> = call %tint_GammaCorrection, %78, %105
        %110:vec3<f32> = mul %107, %108
        %111:vec3<f32> = call %tint_GammaCorrection, %110, %106
        exit_if %111  # if_2
      }
      %b7 = block {  # false
        exit_if %78  # if_2
      }
    }
    %112:vec4<f32> = construct %104, %79
    ret %112
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %115:f32 = access %params_1, 0u
    %116:f32 = access %params_1, 1u
    %117:f32 = access %params_1, 2u
    %118:f32 = access %params_1, 3u
    %119:f32 = access %params_1, 4u
    %120:f32 = access %params_1, 5u
    %121:f32 = access %params_1, 6u
    %122:vec3<f32> = construct %115
    %123:vec3<f32> = construct %119
    %124:vec3<f32> = abs %v
    %125:vec3<f32> = sign %v
    %126:vec3<bool> = lt %124, %123
    %127:vec3<f32> = mul %118, %124
    %128:vec3<f32> = add %127, %121
    %129:vec3<f32> = mul %125, %128
    %130:vec3<f32> = mul %116, %124
    %131:vec3<f32> = add %130, %117
    %132:vec3<f32> = pow %131, %122
    %133:vec3<f32> = add %132, %120
    %134:vec3<f32> = mul %125, %133
    %135:vec3<f32> = select %134, %129, %126
    ret %135
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
