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

#include "src/tint/lang/core/ir/transform/multiplanar_external_texture.h"

#include <utility>

#include "src/tint/lang/core/ir/transform/helper_test.h"
#include "src/tint/lang/core/type/external_texture.h"

namespace tint::ir::transform {
namespace {

using namespace tint::core::fluent_types;  // NOLINT
using namespace tint::number_suffixes;     // NOLINT

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
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    b.RootBlock()->Append(var);

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read_write> = var @binding_point(1, 2)
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
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(1, 4)
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
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    b.RootBlock()->Append(var);

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read_write> = var @binding_point(1, 2)
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
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(1, 4)
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
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    b.RootBlock()->Append(var);

    auto* func = b.Function("foo", ty.vec2<u32>());
    b.Append(func->Block(), [&] {
        auto* load = b.Load(var->Result());
        auto* result = b.Call(ty.vec2<u32>(), core::Function::kTextureDimensions, load);
        b.Return(func, result);
        mod.SetName(result, "result");
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read_write> = var @binding_point(1, 2)
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
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(1, 4)
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
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    b.RootBlock()->Append(var);

    auto* func = b.Function("foo", ty.vec4<f32>());
    auto* coords = b.FunctionParam("coords", ty.vec2<u32>());
    func->SetParams({coords});
    b.Append(func->Block(), [&] {
        auto* load = b.Load(var->Result());
        auto* result = b.Call(ty.vec4<f32>(), core::Function::kTextureLoad, load, coords);
        b.Return(func, result);
        mod.SetName(result, "result");
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read_write> = var @binding_point(1, 2)
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
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(1, 4)
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
    %17:u32 = access %params, 0u
    %18:bool = eq %17, 1u
    %19:vec3<f32>, %20:f32 = if %18 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %21:vec4<f32> = textureLoad %plane_0, %coords_1, 0u
        %22:vec3<f32> = swizzle %21, xyz
        %23:f32 = access %21, 3u
        exit_if %22, %23  # if_1
      }
      %b5 = block {  # false
        %24:vec4<f32> = textureLoad %plane_0, %coords_1, 0u
        %25:f32 = access %24, 0u
        %26:vec2<u32> = shiftr %coords_1, vec2<u32>(1u)
        %27:vec4<f32> = textureLoad %plane_1, %26, 0u
        %28:vec2<f32> = swizzle %27, xy
        %29:vec4<f32> = construct %25, %28, 1.0f
        %30:vec3<f32> = mul %29, %16
        exit_if %30, 1.0f  # if_1
      }
    }
    %31:bool = eq %15, 0u
    %32:vec3<f32> = if %31 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %33:tint_GammaTransferParams = access %params, 3u
        %34:tint_GammaTransferParams = access %params, 4u
        %35:mat3x3<f32> = access %params, 5u
        %36:vec3<f32> = call %tint_GammaCorrection, %19, %33
        %38:vec3<f32> = mul %35, %36
        %39:vec3<f32> = call %tint_GammaCorrection, %38, %34
        exit_if %39  # if_2
      }
      %b7 = block {  # false
        exit_if %19  # if_2
      }
    }
    %40:vec4<f32> = construct %32, %20
    ret %40
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %43:f32 = access %params_1, 0u
    %44:f32 = access %params_1, 1u
    %45:f32 = access %params_1, 2u
    %46:f32 = access %params_1, 3u
    %47:f32 = access %params_1, 4u
    %48:f32 = access %params_1, 5u
    %49:f32 = access %params_1, 6u
    %50:vec3<f32> = construct %43
    %51:vec3<f32> = construct %47
    %52:vec3<f32> = abs %v
    %53:vec3<f32> = sign %v
    %54:vec3<bool> = lt %52, %51
    %55:vec3<f32> = mul %46, %52
    %56:vec3<f32> = add %55, %49
    %57:vec3<f32> = mul %53, %56
    %58:vec3<f32> = mul %44, %52
    %59:vec3<f32> = add %58, %45
    %60:vec3<f32> = pow %59, %50
    %61:vec3<f32> = add %60, %48
    %62:vec3<f32> = mul %53, %61
    %63:vec3<f32> = select %62, %57, %54
    ret %63
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
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    b.RootBlock()->Append(var);

    auto* func = b.Function("foo", ty.vec4<f32>());
    auto* sampler = b.FunctionParam("sampler", ty.sampler());
    auto* coords = b.FunctionParam("coords", ty.vec2<f32>());
    func->SetParams({sampler, coords});
    b.Append(func->Block(), [&] {
        auto* load = b.Load(var->Result());
        auto* result = b.Call(ty.vec4<f32>(), core::Function::kTextureSampleBaseClampToEdge, load,
                              sampler, coords);
        b.Return(func, result);
        mod.SetName(result, "result");
    });

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read_write> = var @binding_point(1, 2)
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
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(1, 4)
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
    %20:vec3<f32> = construct %coords_1, 1.0f
    %21:vec2<f32> = mul %19, %20
    %22:vec2<u32> = textureDimensions %plane_0
    %23:vec2<f32> = convert %22
    %24:vec2<f32> = div vec2<f32>(0.5f), %23
    %25:vec2<f32> = sub 1.0f, %24
    %26:vec2<f32> = clamp %21, %24, %25
    %27:vec2<u32> = textureDimensions %plane_1
    %28:vec2<f32> = convert %27
    %29:vec2<f32> = div vec2<f32>(0.5f), %28
    %30:vec2<f32> = sub 1.0f, %29
    %31:vec2<f32> = clamp %21, %29, %30
    %32:u32 = access %params, 0u
    %33:bool = eq %32, 1u
    %34:vec3<f32>, %35:f32 = if %33 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %36:vec4<f32> = textureSampleLevel %plane_0, %sampler_1, %26, 0.0f
        %37:vec3<f32> = swizzle %36, xyz
        %38:f32 = access %36, 3u
        exit_if %37, %38  # if_1
      }
      %b5 = block {  # false
        %39:vec4<f32> = textureSampleLevel %plane_0, %sampler_1, %26, 0.0f
        %40:f32 = access %39, 0u
        %41:vec4<f32> = textureSampleLevel %plane_1, %sampler_1, %31, 0.0f
        %42:vec2<f32> = swizzle %41, xy
        %43:vec4<f32> = construct %40, %42, 1.0f
        %44:vec3<f32> = mul %43, %18
        exit_if %44, 1.0f  # if_1
      }
    }
    %45:bool = eq %17, 0u
    %46:vec3<f32> = if %45 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %47:tint_GammaTransferParams = access %params, 3u
        %48:tint_GammaTransferParams = access %params, 4u
        %49:mat3x3<f32> = access %params, 5u
        %50:vec3<f32> = call %tint_GammaCorrection, %34, %47
        %52:vec3<f32> = mul %49, %50
        %53:vec3<f32> = call %tint_GammaCorrection, %52, %48
        exit_if %53  # if_2
      }
      %b7 = block {  # false
        exit_if %34  # if_2
      }
    }
    %54:vec4<f32> = construct %46, %35
    ret %54
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %57:f32 = access %params_1, 0u
    %58:f32 = access %params_1, 1u
    %59:f32 = access %params_1, 2u
    %60:f32 = access %params_1, 3u
    %61:f32 = access %params_1, 4u
    %62:f32 = access %params_1, 5u
    %63:f32 = access %params_1, 6u
    %64:vec3<f32> = construct %57
    %65:vec3<f32> = construct %61
    %66:vec3<f32> = abs %v
    %67:vec3<f32> = sign %v
    %68:vec3<bool> = lt %66, %65
    %69:vec3<f32> = mul %60, %66
    %70:vec3<f32> = add %69, %63
    %71:vec3<f32> = mul %67, %70
    %72:vec3<f32> = mul %58, %66
    %73:vec3<f32> = add %72, %59
    %74:vec3<f32> = pow %73, %64
    %75:vec3<f32> = add %74, %62
    %76:vec3<f32> = mul %67, %75
    %77:vec3<f32> = select %76, %71, %68
    ret %77
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
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    b.RootBlock()->Append(var);

    auto* foo = b.Function("foo", ty.vec4<f32>());
    {
        auto* texture = b.FunctionParam("texture", ty.Get<type::ExternalTexture>());
        auto* sampler = b.FunctionParam("sampler", ty.sampler());
        auto* coords = b.FunctionParam("coords", ty.vec2<f32>());
        foo->SetParams({texture, sampler, coords});
        b.Append(foo->Block(), [&] {
            auto* result = b.Call(ty.vec4<f32>(), core::Function::kTextureSampleBaseClampToEdge,
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
            auto* load = b.Load(var->Result());
            auto* result = b.Call(ty.vec4<f32>(), foo, load, sampler, coords);
            b.Return(bar, result);
            mod.SetName(result, "result");
        });
    }

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read_write> = var @binding_point(1, 2)
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
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(1, 4)
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
    %27:vec3<f32> = construct %coords_2, 1.0f
    %28:vec2<f32> = mul %26, %27
    %29:vec2<u32> = textureDimensions %plane_0
    %30:vec2<f32> = convert %29
    %31:vec2<f32> = div vec2<f32>(0.5f), %30
    %32:vec2<f32> = sub 1.0f, %31
    %33:vec2<f32> = clamp %28, %31, %32
    %34:vec2<u32> = textureDimensions %plane_1
    %35:vec2<f32> = convert %34
    %36:vec2<f32> = div vec2<f32>(0.5f), %35
    %37:vec2<f32> = sub 1.0f, %36
    %38:vec2<f32> = clamp %28, %36, %37
    %39:u32 = access %params, 0u
    %40:bool = eq %39, 1u
    %41:vec3<f32>, %42:f32 = if %40 [t: %b5, f: %b6] {  # if_1
      %b5 = block {  # true
        %43:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %33, 0.0f
        %44:vec3<f32> = swizzle %43, xyz
        %45:f32 = access %43, 3u
        exit_if %44, %45  # if_1
      }
      %b6 = block {  # false
        %46:vec4<f32> = textureSampleLevel %plane_0, %sampler_2, %33, 0.0f
        %47:f32 = access %46, 0u
        %48:vec4<f32> = textureSampleLevel %plane_1, %sampler_2, %38, 0.0f
        %49:vec2<f32> = swizzle %48, xy
        %50:vec4<f32> = construct %47, %49, 1.0f
        %51:vec3<f32> = mul %50, %25
        exit_if %51, 1.0f  # if_1
      }
    }
    %52:bool = eq %24, 0u
    %53:vec3<f32> = if %52 [t: %b7, f: %b8] {  # if_2
      %b7 = block {  # true
        %54:tint_GammaTransferParams = access %params, 3u
        %55:tint_GammaTransferParams = access %params, 4u
        %56:mat3x3<f32> = access %params, 5u
        %57:vec3<f32> = call %tint_GammaCorrection, %41, %54
        %59:vec3<f32> = mul %56, %57
        %60:vec3<f32> = call %tint_GammaCorrection, %59, %55
        exit_if %60  # if_2
      }
      %b8 = block {  # false
        exit_if %41  # if_2
      }
    }
    %61:vec4<f32> = construct %53, %42
    ret %61
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b9 {  # %params_1: 'params'
  %b9 = block {
    %64:f32 = access %params_1, 0u
    %65:f32 = access %params_1, 1u
    %66:f32 = access %params_1, 2u
    %67:f32 = access %params_1, 3u
    %68:f32 = access %params_1, 4u
    %69:f32 = access %params_1, 5u
    %70:f32 = access %params_1, 6u
    %71:vec3<f32> = construct %64
    %72:vec3<f32> = construct %68
    %73:vec3<f32> = abs %v
    %74:vec3<f32> = sign %v
    %75:vec3<bool> = lt %73, %72
    %76:vec3<f32> = mul %67, %73
    %77:vec3<f32> = add %76, %70
    %78:vec3<f32> = mul %74, %77
    %79:vec3<f32> = mul %65, %73
    %80:vec3<f32> = add %79, %66
    %81:vec3<f32> = pow %80, %71
    %82:vec3<f32> = add %81, %69
    %83:vec3<f32> = mul %74, %82
    %84:vec3<f32> = select %83, %78, %75
    ret %84
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
    auto* var = b.Var("texture", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var->SetBindingPoint(1, 2);
    b.RootBlock()->Append(var);

    auto* foo = b.Function("foo", ty.vec4<f32>());
    {
        auto* texture = b.FunctionParam("texture", ty.Get<type::ExternalTexture>());
        auto* sampler = b.FunctionParam("sampler", ty.sampler());
        auto* coords = b.FunctionParam("coords", ty.vec2<f32>());
        foo->SetParams({texture, sampler, coords});
        b.Append(foo->Block(), [&] {
            auto* result = b.Call(ty.vec4<f32>(), core::Function::kTextureSampleBaseClampToEdge,
                                  texture, sampler, coords);
            b.Return(foo, result);
            mod.SetName(result, "result");
        });
    }

    auto* bar = b.Function("bar", ty.vec4<f32>());
    {
        auto* sampler = b.FunctionParam("sampler", ty.sampler());
        auto* coords_u = b.FunctionParam("coords_u", ty.vec2<u32>());
        auto* coords_f = b.FunctionParam("coords_f", ty.vec2<f32>());
        bar->SetParams({sampler, coords_u, coords_f});
        b.Append(bar->Block(), [&] {
            auto* load_a = b.Load(var->Result());
            b.Call(ty.vec2<u32>(), core::Function::kTextureDimensions, load_a);
            auto* load_b = b.Load(var->Result());
            b.Call(ty.vec2<u32>(), core::Function::kTextureLoad, load_b, coords_u);
            auto* load_c = b.Load(var->Result());
            b.Call(ty.vec2<u32>(), core::Function::kTextureSampleBaseClampToEdge, load_c, sampler,
                   coords_f);
            auto* load_d = b.Load(var->Result());
            auto* result_a = b.Call(ty.vec4<f32>(), foo, load_d, sampler, coords_f);
            auto* result_b = b.Call(ty.vec4<f32>(), foo, load_d, sampler, coords_f);
            b.Return(bar, b.Add(ty.vec4<f32>(), result_a, result_b));
            mod.SetName(result_a, "result_a");
            mod.SetName(result_b, "result_b");
        });
    }

    auto* src = R"(
%b1 = block {  # root
  %texture:ptr<handle, texture_external, read_write> = var @binding_point(1, 2)
}

%foo = func(%texture_1:texture_external, %sampler:sampler, %coords:vec2<f32>):vec4<f32> -> %b2 {  # %texture_1: 'texture'
  %b2 = block {
    %result:vec4<f32> = textureSampleBaseClampToEdge %texture_1, %sampler, %coords
    ret %result
  }
}
%bar = func(%sampler_1:sampler, %coords_u:vec2<u32>, %coords_f:vec2<f32>):vec4<f32> -> %b3 {  # %sampler_1: 'sampler'
  %b3 = block {
    %11:texture_external = load %texture
    %12:vec2<u32> = textureDimensions %11
    %13:texture_external = load %texture
    %14:vec2<u32> = textureLoad %13, %coords_u
    %15:texture_external = load %texture
    %16:vec2<u32> = textureSampleBaseClampToEdge %15, %sampler_1, %coords_f
    %17:texture_external = load %texture
    %result_a:vec4<f32> = call %foo, %17, %sampler_1, %coords_f
    %result_b:vec4<f32> = call %foo, %17, %sampler_1, %coords_f
    %20:vec4<f32> = add %result_a, %result_b
    ret %20
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
}

%b1 = block {  # root
  %texture_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 2)
  %texture_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 3)
  %texture_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(1, 4)
}

%foo = func(%texture_plane0_1:texture_2d<f32>, %texture_plane1_1:texture_2d<f32>, %texture_params_1:tint_ExternalTextureParams, %sampler:sampler, %coords:vec2<f32>):vec4<f32> -> %b2 {  # %texture_plane0_1: 'texture_plane0', %texture_plane1_1: 'texture_plane1', %texture_params_1: 'texture_params'
  %b2 = block {
    %10:vec4<f32> = call %tint_TextureSampleExternal, %texture_plane0_1, %texture_plane1_1, %texture_params_1, %sampler, %coords
    ret %10
  }
}
%bar = func(%sampler_1:sampler, %coords_u:vec2<u32>, %coords_f:vec2<f32>):vec4<f32> -> %b3 {  # %sampler_1: 'sampler'
  %b3 = block {
    %16:texture_2d<f32> = load %texture_plane0
    %17:texture_2d<f32> = load %texture_plane1
    %18:tint_ExternalTextureParams = load %texture_params
    %19:vec2<u32> = textureDimensions %16
    %20:texture_2d<f32> = load %texture_plane0
    %21:texture_2d<f32> = load %texture_plane1
    %22:tint_ExternalTextureParams = load %texture_params
    %23:vec4<f32> = call %tint_TextureLoadExternal, %20, %21, %22, %coords_u
    %25:texture_2d<f32> = load %texture_plane0
    %26:texture_2d<f32> = load %texture_plane1
    %27:tint_ExternalTextureParams = load %texture_params
    %28:vec4<f32> = call %tint_TextureSampleExternal, %25, %26, %27, %sampler_1, %coords_f
    %29:texture_2d<f32> = load %texture_plane0
    %30:texture_2d<f32> = load %texture_plane1
    %31:tint_ExternalTextureParams = load %texture_params
    %result_a:vec4<f32> = call %foo, %29, %30, %31, %sampler_1, %coords_f
    %result_b:vec4<f32> = call %foo, %29, %30, %31, %sampler_1, %coords_f
    %34:vec4<f32> = add %result_a, %result_b
    ret %34
  }
}
%tint_TextureLoadExternal = func(%plane_0:texture_2d<f32>, %plane_1:texture_2d<f32>, %params:tint_ExternalTextureParams, %coords_1:vec2<u32>):vec4<f32> -> %b4 {  # %coords_1: 'coords'
  %b4 = block {
    %39:u32 = access %params, 1u
    %40:mat3x4<f32> = access %params, 2u
    %41:u32 = access %params, 0u
    %42:bool = eq %41, 1u
    %43:vec3<f32>, %44:f32 = if %42 [t: %b5, f: %b6] {  # if_1
      %b5 = block {  # true
        %45:vec4<f32> = textureLoad %plane_0, %coords_1, 0u
        %46:vec3<f32> = swizzle %45, xyz
        %47:f32 = access %45, 3u
        exit_if %46, %47  # if_1
      }
      %b6 = block {  # false
        %48:vec4<f32> = textureLoad %plane_0, %coords_1, 0u
        %49:f32 = access %48, 0u
        %50:vec2<u32> = shiftr %coords_1, vec2<u32>(1u)
        %51:vec4<f32> = textureLoad %plane_1, %50, 0u
        %52:vec2<f32> = swizzle %51, xy
        %53:vec4<f32> = construct %49, %52, 1.0f
        %54:vec3<f32> = mul %53, %40
        exit_if %54, 1.0f  # if_1
      }
    }
    %55:bool = eq %39, 0u
    %56:vec3<f32> = if %55 [t: %b7, f: %b8] {  # if_2
      %b7 = block {  # true
        %57:tint_GammaTransferParams = access %params, 3u
        %58:tint_GammaTransferParams = access %params, 4u
        %59:mat3x3<f32> = access %params, 5u
        %60:vec3<f32> = call %tint_GammaCorrection, %43, %57
        %62:vec3<f32> = mul %59, %60
        %63:vec3<f32> = call %tint_GammaCorrection, %62, %58
        exit_if %63  # if_2
      }
      %b8 = block {  # false
        exit_if %43  # if_2
      }
    }
    %64:vec4<f32> = construct %56, %44
    ret %64
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b9 {  # %params_1: 'params'
  %b9 = block {
    %67:f32 = access %params_1, 0u
    %68:f32 = access %params_1, 1u
    %69:f32 = access %params_1, 2u
    %70:f32 = access %params_1, 3u
    %71:f32 = access %params_1, 4u
    %72:f32 = access %params_1, 5u
    %73:f32 = access %params_1, 6u
    %74:vec3<f32> = construct %67
    %75:vec3<f32> = construct %71
    %76:vec3<f32> = abs %v
    %77:vec3<f32> = sign %v
    %78:vec3<bool> = lt %76, %75
    %79:vec3<f32> = mul %70, %76
    %80:vec3<f32> = add %79, %73
    %81:vec3<f32> = mul %77, %80
    %82:vec3<f32> = mul %68, %76
    %83:vec3<f32> = add %82, %69
    %84:vec3<f32> = pow %83, %74
    %85:vec3<f32> = add %84, %72
    %86:vec3<f32> = mul %77, %85
    %87:vec3<f32> = select %86, %81, %78
    ret %87
  }
}
%tint_TextureSampleExternal = func(%plane_0_1:texture_2d<f32>, %plane_1_1:texture_2d<f32>, %params_2:tint_ExternalTextureParams, %sampler_2:sampler, %coords_2:vec2<f32>):vec4<f32> -> %b10 {  # %plane_0_1: 'plane_0', %plane_1_1: 'plane_1', %params_2: 'params', %sampler_2: 'sampler', %coords_2: 'coords'
  %b10 = block {
    %93:u32 = access %params_2, 1u
    %94:mat3x4<f32> = access %params_2, 2u
    %95:mat3x2<f32> = access %params_2, 6u
    %96:vec3<f32> = construct %coords_2, 1.0f
    %97:vec2<f32> = mul %95, %96
    %98:vec2<u32> = textureDimensions %plane_0_1
    %99:vec2<f32> = convert %98
    %100:vec2<f32> = div vec2<f32>(0.5f), %99
    %101:vec2<f32> = sub 1.0f, %100
    %102:vec2<f32> = clamp %97, %100, %101
    %103:vec2<u32> = textureDimensions %plane_1_1
    %104:vec2<f32> = convert %103
    %105:vec2<f32> = div vec2<f32>(0.5f), %104
    %106:vec2<f32> = sub 1.0f, %105
    %107:vec2<f32> = clamp %97, %105, %106
    %108:u32 = access %params_2, 0u
    %109:bool = eq %108, 1u
    %110:vec3<f32>, %111:f32 = if %109 [t: %b11, f: %b12] {  # if_3
      %b11 = block {  # true
        %112:vec4<f32> = textureSampleLevel %plane_0_1, %sampler_2, %102, 0.0f
        %113:vec3<f32> = swizzle %112, xyz
        %114:f32 = access %112, 3u
        exit_if %113, %114  # if_3
      }
      %b12 = block {  # false
        %115:vec4<f32> = textureSampleLevel %plane_0_1, %sampler_2, %102, 0.0f
        %116:f32 = access %115, 0u
        %117:vec4<f32> = textureSampleLevel %plane_1_1, %sampler_2, %107, 0.0f
        %118:vec2<f32> = swizzle %117, xy
        %119:vec4<f32> = construct %116, %118, 1.0f
        %120:vec3<f32> = mul %119, %94
        exit_if %120, 1.0f  # if_3
      }
    }
    %121:bool = eq %93, 0u
    %122:vec3<f32> = if %121 [t: %b13, f: %b14] {  # if_4
      %b13 = block {  # true
        %123:tint_GammaTransferParams = access %params_2, 3u
        %124:tint_GammaTransferParams = access %params_2, 4u
        %125:mat3x3<f32> = access %params_2, 5u
        %126:vec3<f32> = call %tint_GammaCorrection, %110, %123
        %127:vec3<f32> = mul %125, %126
        %128:vec3<f32> = call %tint_GammaCorrection, %127, %124
        exit_if %128  # if_4
      }
      %b14 = block {  # false
        exit_if %110  # if_4
      }
    }
    %129:vec4<f32> = construct %122, %111
    ret %129
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
    auto* var_a = b.Var("texture_a", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var_a->SetBindingPoint(1, 2);
    b.RootBlock()->Append(var_a);

    auto* var_b = b.Var("texture_b", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var_b->SetBindingPoint(2, 2);
    b.RootBlock()->Append(var_b);

    auto* var_c = b.Var("texture_c", ty.ptr(handle, ty.Get<type::ExternalTexture>()));
    var_c->SetBindingPoint(3, 2);
    b.RootBlock()->Append(var_c);

    auto* foo = b.Function("foo", ty.void_());
    auto* coords = b.FunctionParam("coords", ty.vec2<u32>());
    foo->SetParams({coords});
    b.Append(foo->Block(), [&] {
        auto* load_a = b.Load(var_a->Result());
        b.Call(ty.vec2<u32>(), core::Function::kTextureLoad, load_a, coords);
        auto* load_b = b.Load(var_b->Result());
        b.Call(ty.vec2<u32>(), core::Function::kTextureLoad, load_b, coords);
        auto* load_c = b.Load(var_c->Result());
        b.Call(ty.vec2<u32>(), core::Function::kTextureLoad, load_c, coords);
        b.Return(foo);
    });

    auto* src = R"(
%b1 = block {  # root
  %texture_a:ptr<handle, texture_external, read_write> = var @binding_point(1, 2)
  %texture_b:ptr<handle, texture_external, read_write> = var @binding_point(2, 2)
  %texture_c:ptr<handle, texture_external, read_write> = var @binding_point(3, 2)
}

%foo = func(%coords:vec2<u32>):void -> %b2 {
  %b2 = block {
    %6:texture_external = load %texture_a
    %7:vec2<u32> = textureLoad %6, %coords
    %8:texture_external = load %texture_b
    %9:vec2<u32> = textureLoad %8, %coords
    %10:texture_external = load %texture_c
    %11:vec2<u32> = textureLoad %10, %coords
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
}

%b1 = block {  # root
  %texture_a_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 2)
  %texture_a_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(1, 3)
  %texture_a_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(1, 4)
  %texture_b_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(2, 2)
  %texture_b_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(2, 3)
  %texture_b_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(2, 4)
  %texture_c_plane0:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(3, 2)
  %texture_c_plane1:ptr<handle, texture_2d<f32>, read_write> = var @binding_point(3, 3)
  %texture_c_params:ptr<uniform, tint_ExternalTextureParams, read_write> = var @binding_point(3, 4)
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
    %31:u32 = access %params, 0u
    %32:bool = eq %31, 1u
    %33:vec3<f32>, %34:f32 = if %32 [t: %b4, f: %b5] {  # if_1
      %b4 = block {  # true
        %35:vec4<f32> = textureLoad %plane_0, %coords_1, 0u
        %36:vec3<f32> = swizzle %35, xyz
        %37:f32 = access %35, 3u
        exit_if %36, %37  # if_1
      }
      %b5 = block {  # false
        %38:vec4<f32> = textureLoad %plane_0, %coords_1, 0u
        %39:f32 = access %38, 0u
        %40:vec2<u32> = shiftr %coords_1, vec2<u32>(1u)
        %41:vec4<f32> = textureLoad %plane_1, %40, 0u
        %42:vec2<f32> = swizzle %41, xy
        %43:vec4<f32> = construct %39, %42, 1.0f
        %44:vec3<f32> = mul %43, %30
        exit_if %44, 1.0f  # if_1
      }
    }
    %45:bool = eq %29, 0u
    %46:vec3<f32> = if %45 [t: %b6, f: %b7] {  # if_2
      %b6 = block {  # true
        %47:tint_GammaTransferParams = access %params, 3u
        %48:tint_GammaTransferParams = access %params, 4u
        %49:mat3x3<f32> = access %params, 5u
        %50:vec3<f32> = call %tint_GammaCorrection, %33, %47
        %52:vec3<f32> = mul %49, %50
        %53:vec3<f32> = call %tint_GammaCorrection, %52, %48
        exit_if %53  # if_2
      }
      %b7 = block {  # false
        exit_if %33  # if_2
      }
    }
    %54:vec4<f32> = construct %46, %34
    ret %54
  }
}
%tint_GammaCorrection = func(%v:vec3<f32>, %params_1:tint_GammaTransferParams):vec3<f32> -> %b8 {  # %params_1: 'params'
  %b8 = block {
    %57:f32 = access %params_1, 0u
    %58:f32 = access %params_1, 1u
    %59:f32 = access %params_1, 2u
    %60:f32 = access %params_1, 3u
    %61:f32 = access %params_1, 4u
    %62:f32 = access %params_1, 5u
    %63:f32 = access %params_1, 6u
    %64:vec3<f32> = construct %57
    %65:vec3<f32> = construct %61
    %66:vec3<f32> = abs %v
    %67:vec3<f32> = sign %v
    %68:vec3<bool> = lt %66, %65
    %69:vec3<f32> = mul %60, %66
    %70:vec3<f32> = add %69, %63
    %71:vec3<f32> = mul %67, %70
    %72:vec3<f32> = mul %58, %66
    %73:vec3<f32> = add %72, %59
    %74:vec3<f32> = pow %73, %64
    %75:vec3<f32> = add %74, %62
    %76:vec3<f32> = mul %67, %75
    %77:vec3<f32> = select %76, %71, %68
    ret %77
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
}  // namespace tint::ir::transform
