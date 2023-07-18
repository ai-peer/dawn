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

#include "src/tint/writer/spirv/ir/test_helper_ir.h"

#include "src/tint/builtin/function.h"
#include "src/tint/type/depth_multisampled_texture.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::writer::spirv {
namespace {

enum TextureType {
    kSampledTexture,
    kMultisampledTexture,
    kDepthTexture,
    kDepthMultisampledTexture,
};

enum SamplerUsage {
    kNoSampler,
    kSampler,
    kComparisonSampler,
};

/// A typed argument or result for a texture builtin.
struct NameAndType {
    /// The name.
    const char* name;
    /// The vector width of the value (1 means scalar).
    uint32_t width;
    /// The element type of the value.
    TestElementType type;
};

/// A parameterized texture builtin function test case.
struct TextureBuiltinTestCase {
    /// The texture type.
    TextureType texture_type;
    /// The dimensionality of the texture.
    type::TextureDimension dim;
    /// The texel type of the texture.
    TestElementType texel_type;
    /// The builtin function arguments.
    utils::Vector<NameAndType, 4> args;
    /// The result type.
    NameAndType result;
    /// The expected SPIR-V instruction string for the texture call.
    std::initializer_list<const char*> instructions;
};

inline utils::StringStream& operator<<(utils::StringStream& out, TextureType type) {
    switch (type) {
        case kSampledTexture:
            out << "SampleTexture";
            break;
        case kMultisampledTexture:
            out << "MultisampleTexture";
            break;
        case kDepthTexture:
            out << "DepthTexture";
            break;
        case kDepthMultisampledTexture:
            out << "DepthMultisampledTexture";
            break;
    }
    return out;
}

std::string PrintCase(testing::TestParamInfo<TextureBuiltinTestCase> cc) {
    utils::StringStream ss;
    ss << cc.param.texture_type << cc.param.dim << "_" << cc.param.texel_type;
    for (const auto& arg : cc.param.args) {
        ss << "_" << arg.name;
    }
    return ss.str();
}

class TextureBuiltinTest : public SpvGeneratorImplTestWithParam<TextureBuiltinTestCase> {
  protected:
    const type::Texture* MakeTextureType(TextureType type,
                                         type::TextureDimension dim,
                                         TestElementType texel_type) {
        switch (type) {
            case kSampledTexture:
                return ty.Get<type::SampledTexture>(dim, MakeScalarType(texel_type));
            case kMultisampledTexture:
                return ty.Get<type::MultisampledTexture>(dim, MakeScalarType(texel_type));
            case kDepthTexture:
                return ty.Get<type::DepthTexture>(dim);
            case kDepthMultisampledTexture:
                return ty.Get<type::DepthMultisampledTexture>(dim);
        }
    }

    void Run(enum builtin::Function function, SamplerUsage sampler) {
        auto params = GetParam();

        auto* result_ty = MakeScalarType(params.result.type);
        if (params.result.width > 1) {
            result_ty = ty.vec(result_ty, params.result.width);
        }

        utils::Vector<ir::FunctionParam*, 4> func_params;

        auto* t = b.FunctionParam(
            "t", MakeTextureType(params.texture_type, params.dim, params.texel_type));
        func_params.Push(t);
        ir::FunctionParam* s = nullptr;
        if (sampler == kSampler) {
            s = b.FunctionParam("s", ty.sampler());
            func_params.Push(s);
        } else if (sampler == kComparisonSampler) {
            s = b.FunctionParam("s", ty.comparison_sampler());
            func_params.Push(s);
        }

        auto* func = b.Function("foo", result_ty);
        func->SetParams(std::move(func_params));

        b.With(func->Block(), [&] {
            utils::Vector<ir::Value*, 4> args = {t};
            if (s) {
                args.Push(s);
            }

            uint32_t arg_value = 1;
            for (const auto& arg : params.args) {
                auto* value = MakeScalarValue(arg.type, arg_value++);
                if (arg.width > 1) {
                    value = b.Constant(mod.constant_values.Splat(ty.vec(value->Type(), arg.width),
                                                                 value->Value(), arg.width));
                }
                args.Push(value);
                mod.SetName(value, arg.name);
            }
            auto* result = b.Call(result_ty, function, std::move(args));
            b.Return(func, result);
            mod.SetName(result, "result");
        });

        ASSERT_TRUE(Generate()) << Error() << output_;
        for (auto& inst : params.instructions) {
            EXPECT_INST(inst);
        }
    }
};

////////////////////////////////////////////////////////////////
//// textureSample
////////////////////////////////////////////////////////////////
using TextureSample = TextureBuiltinTest;
TEST_P(TextureSample, Emit) {
    Run(builtin::Function::kTextureSample, kSampler);
}
INSTANTIATE_TEST_SUITE_P(
    SpvGeneratorImplTest,
    TextureSample,
    testing::Values(
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k1d,
            /* texel type */ kF32,
            {{"coord", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coord None",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords None",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"offset", 2, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords ConstOffset %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%16 = OpCompositeConstruct %v3float %coords %12",
                "OpImageSampleImplicitLod %v4float %10 %16 None",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"offset", 2, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%16 = OpCompositeConstruct %v3float %coords %12",
                "OpImageSampleImplicitLod %v4float %10 %16 ConstOffset %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k3d,
            /* texel type */ kF32,
            {{"coords", 3, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords None",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k3d,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"offset", 3, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords ConstOffset %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::kCube,
            /* texel type */ kF32,
            {{"coords", 3, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords None",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::kCubeArray,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"array_idx", 1, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v4float %coords %12",
                "OpImageSampleImplicitLod %v4float %10 %15 None",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleImplicitLod %v4float %9 %coords None",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"offset", 2, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleImplicitLod %v4float %9 %coords ConstOffset %offset",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::kCube,
            /* texel type */ kF32,
            {{"coords", 3, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleImplicitLod %v4float %9 %coords None",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v3float %coords %11",
                "OpImageSampleImplicitLod %v4float %9 %15 None",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"offset", 2, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v3float %coords %11",
                "OpImageSampleImplicitLod %v4float %9 %15 ConstOffset %offset",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::kCubeArray,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"array_idx", 1, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v4float %coords %11",
                "OpImageSampleImplicitLod %v4float %9 %15 None",
            },
        }),
    PrintCase);

////////////////////////////////////////////////////////////////
//// textureSampleBias
////////////////////////////////////////////////////////////////
using TextureSampleBias = TextureBuiltinTest;
TEST_P(TextureSampleBias, Emit) {
    Run(builtin::Function::kTextureSampleBias, kSampler);
}
INSTANTIATE_TEST_SUITE_P(
    SpvGeneratorImplTest,
    TextureSampleBias,
    testing::Values(
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"bias", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords Bias %bias",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"bias", 1, kF32}, {"offset", 2, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords Bias|ConstOffset %bias %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"bias", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%16 = OpCompositeConstruct %v3float %coords %12",
                "OpImageSampleImplicitLod %v4float %10 %16 Bias %bias",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"bias", 1, kF32}, {"offset", 2, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%16 = OpCompositeConstruct %v3float %coords %12",
                "OpImageSampleImplicitLod %v4float %10 %16 Bias|ConstOffset %bias %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k3d,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"bias", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords Bias %bias",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k3d,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"bias", 1, kF32}, {"offset", 3, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords Bias|ConstOffset %bias %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::kCube,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"bias", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleImplicitLod %v4float %10 %coords Bias %bias",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::kCubeArray,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"array_idx", 1, kI32}, {"bias", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v4float %coords %12",
                "OpImageSampleImplicitLod %v4float %10 %15 Bias %bias",
            },
        }),
    PrintCase);

////////////////////////////////////////////////////////////////
//// textureSampleGrad
////////////////////////////////////////////////////////////////
using TextureSampleGrad = TextureBuiltinTest;
TEST_P(TextureSampleGrad, Emit) {
    Run(builtin::Function::kTextureSampleGrad, kSampler);
}
INSTANTIATE_TEST_SUITE_P(
    SpvGeneratorImplTest,
    TextureSampleGrad,
    testing::Values(
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"ddx", 2, kF32}, {"ddy", 2, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Grad %ddx %ddy",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"ddx", 2, kF32}, {"ddy", 2, kF32}, {"offset", 2, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Grad|ConstOffset %ddx %ddy %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"ddx", 2, kF32}, {"ddy", 2, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%16 = OpCompositeConstruct %v3float %coords %12",
                "OpImageSampleExplicitLod %v4float %10 %16 Grad %ddx %ddy",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32},
             {"array_idx", 1, kI32},
             {"ddx", 2, kF32},
             {"ddy", 2, kF32},
             {"offset", 2, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%16 = OpCompositeConstruct %v3float %coords %12",
                "OpImageSampleExplicitLod %v4float %10 %16 Grad|ConstOffset %ddx %ddy %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k3d,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"ddx", 3, kF32}, {"ddy", 3, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Grad %ddx %ddy",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k3d,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"ddx", 3, kF32}, {"ddy", 3, kF32}, {"offset", 3, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Grad|ConstOffset %ddx %ddy %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::kCube,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"ddx", 3, kF32}, {"ddy", 3, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Grad %ddx %ddy",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::kCubeArray,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"array_idx", 1, kI32}, {"ddx", 3, kF32}, {"ddy", 3, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v4float %coords %12",
                "OpImageSampleExplicitLod %v4float %10 %15 Grad %ddx %ddy",
            },
        }),
    PrintCase);

////////////////////////////////////////////////////////////////
//// textureSampleLevel
////////////////////////////////////////////////////////////////
using TextureSampleLevel = TextureBuiltinTest;
TEST_P(TextureSampleLevel, Emit) {
    Run(builtin::Function::kTextureSampleLevel, kSampler);
}
INSTANTIATE_TEST_SUITE_P(
    SpvGeneratorImplTest,
    TextureSampleLevel,
    testing::Values(
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"lod", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Lod %lod",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"lod", 1, kF32}, {"offset", 2, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Lod|ConstOffset %lod %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"lod", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%16 = OpCompositeConstruct %v3float %coords %12",
                "OpImageSampleExplicitLod %v4float %10 %16 Lod %lod",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"lod", 1, kF32}, {"offset", 2, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%16 = OpCompositeConstruct %v3float %coords %12",
                "OpImageSampleExplicitLod %v4float %10 %16 Lod|ConstOffset %lod %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k3d,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"lod", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Lod %lod",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::k3d,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"lod", 1, kF32}, {"offset", 3, kI32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Lod|ConstOffset %lod %offset",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::kCube,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"lod", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "OpImageSampleExplicitLod %v4float %10 %coords Lod %lod",
            },
        },
        TextureBuiltinTestCase{
            kSampledTexture,
            type::TextureDimension::kCubeArray,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"array_idx", 1, kI32}, {"lod", 1, kF32}},
            {"result", 4, kF32},
            {
                "%10 = OpSampledImage %11 %t %s",
                "%12 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v4float %coords %12",
                "OpImageSampleExplicitLod %v4float %10 %15 Lod %lod",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"lod", 1, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %lod",
                "OpImageSampleExplicitLod %v4float %9 %coords Lod %11",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"lod", 1, kI32}, {"offset", 2, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %lod",
                "OpImageSampleExplicitLod %v4float %9 %coords Lod|ConstOffset %11 %offset",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"lod", 1, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v3float %coords %11",
                "%19 = OpConvertSToF %float %lod",
                "OpImageSampleExplicitLod %v4float %9 %15 Lod %19",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"lod", 1, kI32}, {"offset", 2, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v3float %coords %11",
                "%19 = OpConvertSToF %float %lod",
                "OpImageSampleExplicitLod %v4float %9 %15 Lod|ConstOffset %19 %offset",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::kCube,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"lod", 1, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %lod",
                "OpImageSampleExplicitLod %v4float %9 %coords Lod %11",
                "%result = OpCompositeExtract %float",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::kCubeArray,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"array_idx", 1, kI32}, {"lod", 1, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v4float %coords %11",
                "%19 = OpConvertSToF %float %lod",
                "OpImageSampleExplicitLod %v4float %9 %15 Lod %19",
            },
        }),
    PrintCase);

////////////////////////////////////////////////////////////////
//// textureSampleCompare
////////////////////////////////////////////////////////////////
using TextureSampleCompare = TextureBuiltinTest;
TEST_P(TextureSampleCompare, Emit) {
    Run(builtin::Function::kTextureSampleCompare, kComparisonSampler);
}
INSTANTIATE_TEST_SUITE_P(
    SpvGeneratorImplTest,
    TextureSampleCompare,
    testing::Values(
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"depth", 1, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleDrefImplicitLod %float %9 %coords %depth",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"depth", 1, kF32}, {"offset", 2, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleDrefImplicitLod %float %9 %coords %depth ConstOffset %offset",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"depth", 1, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v3float %coords %11",
                "OpImageSampleDrefImplicitLod %float %9 %15 %depth",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"depth", 1, kF32}, {"offset", 2, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v3float %coords %11",
                "OpImageSampleDrefImplicitLod %float %9 %15 %depth ConstOffset %offset",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::kCube,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"depth", 1, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleDrefImplicitLod %float %9 %coords %depth",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::kCubeArray,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"array_idx", 1, kI32}, {"depth", 1, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v4float %coords %11",
                "OpImageSampleDrefImplicitLod %float %9 %15 %depth",
            },
        }),
    PrintCase);

////////////////////////////////////////////////////////////////
//// textureSampleCompareLevel
////////////////////////////////////////////////////////////////
using TextureSampleCompareLevel = TextureBuiltinTest;
TEST_P(TextureSampleCompareLevel, Emit) {
    Run(builtin::Function::kTextureSampleCompareLevel, kComparisonSampler);
}
INSTANTIATE_TEST_SUITE_P(
    SpvGeneratorImplTest,
    TextureSampleCompareLevel,
    testing::Values(
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"depth_l0", 1, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleDrefExplicitLod %float %9 %coords %depth_l0 Lod %float_0",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2d,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"depth_l0", 1, kF32}, {"offset", 2, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleDrefExplicitLod %float %9 %coords %depth_l0 Lod|ConstOffset %float_0 "
                "%offset",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32}, {"array_idx", 1, kI32}, {"depth_l0", 1, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v3float %coords %11",
                "OpImageSampleDrefExplicitLod %float %9 %15 %depth_l0 Lod %float_0",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::k2dArray,
            /* texel type */ kF32,
            {{"coords", 2, kF32},
             {"array_idx", 1, kI32},
             {"depth_l0", 1, kF32},
             {"offset", 2, kI32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v3float %coords %11",
                "OpImageSampleDrefExplicitLod %float %9 %15 %depth_l0 Lod|ConstOffset %float_0 "
                "%offset",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::kCube,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"depth_l0", 1, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "OpImageSampleDrefExplicitLod %float %9 %coords %depth_l0 Lod %float_0",
            },
        },
        TextureBuiltinTestCase{
            kDepthTexture,
            type::TextureDimension::kCubeArray,
            /* texel type */ kF32,
            {{"coords", 3, kF32}, {"array_idx", 1, kI32}, {"depth_l0", 1, kF32}},
            {"result", 1, kF32},
            {
                "%9 = OpSampledImage %10 %t %s",
                "%11 = OpConvertSToF %float %array_idx",
                "%15 = OpCompositeConstruct %v4float %coords %11",
                "OpImageSampleDrefExplicitLod %float %9 %15 %depth_l0 Lod %float_0",
            },
        }),
    PrintCase);

////////////////////////////////////////////////////////////////
//// textureLoad
////////////////////////////////////////////////////////////////
using TextureLoad = TextureBuiltinTest;
TEST_P(TextureLoad, Emit) {
    Run(builtin::Function::kTextureLoad, kNoSampler);
}
INSTANTIATE_TEST_SUITE_P(SpvGeneratorImplTest,
                         TextureLoad,
                         testing::Values(
                             TextureBuiltinTestCase{
                                 kSampledTexture,
                                 type::TextureDimension::k1d,
                                 /* texel type */ kF32,
                                 {{"coord", 1, kI32}, {"lod", 1, kI32}},
                                 {"result", 4, kF32},
                                 {
                                     "OpImageFetch %v4float %t %coord Lod %lod",
                                 },
                             },
                             TextureBuiltinTestCase{
                                 kSampledTexture,
                                 type::TextureDimension::k2d,
                                 /* texel type */ kF32,
                                 {{"coords", 2, kI32}, {"lod", 1, kI32}},
                                 {"result", 4, kF32},
                                 {
                                     "OpImageFetch %v4float %t %coords Lod %lod",
                                 },
                             },
                             TextureBuiltinTestCase{
                                 kSampledTexture,
                                 type::TextureDimension::k2dArray,
                                 /* texel type */ kF32,
                                 {{"coords", 2, kI32}, {"array_idx", 1, kI32}, {"lod", 1, kI32}},
                                 {"result", 4, kF32},
                                 {
                                     "%10 = OpCompositeConstruct %v3int %coords %array_idx",
                                     "OpImageFetch %v4float %t %10 Lod %lod",
                                 },
                             },
                             TextureBuiltinTestCase{
                                 kSampledTexture,
                                 type::TextureDimension::k3d,
                                 /* texel type */ kF32,
                                 {{"coords", 3, kI32}, {"lod", 1, kI32}},
                                 {"result", 4, kF32},
                                 {
                                     "OpImageFetch %v4float %t %coords Lod %lod",
                                 },
                             },
                             TextureBuiltinTestCase{
                                 kMultisampledTexture,
                                 type::TextureDimension::k2d,
                                 /* texel type */ kF32,
                                 {{"coords", 2, kI32}, {"sample_idx", 1, kI32}},
                                 {"result", 4, kF32},
                                 {
                                     "OpImageFetch %v4float %t %coords Sample %sample_idx",
                                 },
                             },
                             TextureBuiltinTestCase{
                                 kDepthTexture,
                                 type::TextureDimension::k2d,
                                 /* texel type */ kF32,
                                 {{"coords", 2, kI32}, {"lod", 1, kI32}},
                                 {"result", 1, kF32},
                                 {
                                     "OpImageFetch %v4float %t %coords Lod %lod",
                                     "%result = OpCompositeExtract %float",
                                 },
                             },
                             TextureBuiltinTestCase{
                                 kDepthTexture,
                                 type::TextureDimension::k2dArray,
                                 /* texel type */ kF32,
                                 {{"coords", 2, kI32}, {"array_idx", 1, kI32}, {"lod", 1, kI32}},
                                 {"result", 1, kF32},
                                 {
                                     "%9 = OpCompositeConstruct %v3int %coords %array_idx",
                                     "OpImageFetch %v4float %t %9 Lod %lod",
                                     "%result = OpCompositeExtract %float",
                                 },
                             },
                             TextureBuiltinTestCase{
                                 kDepthMultisampledTexture,
                                 type::TextureDimension::k2d,
                                 /* texel type */ kF32,
                                 {{"coords", 3, kI32}, {"sample_idx", 1, kI32}},
                                 {"result", 1, kF32},
                                 {
                                     "OpImageFetch %v4float %t %coords Sample %sample_idx",
                                     "%result = OpCompositeExtract %float",
                                 },
                             },

                             // Test some textures with integer texel types.
                             TextureBuiltinTestCase{
                                 kSampledTexture,
                                 type::TextureDimension::k2d,
                                 /* texel type */ kI32,
                                 {{"coords", 2, kI32}, {"lod", 1, kI32}},
                                 {"result", 4, kI32},
                                 {
                                     "OpImageFetch %v4int %t %coords Lod %lod",
                                 },
                             },
                             TextureBuiltinTestCase{
                                 kSampledTexture,
                                 type::TextureDimension::k2d,
                                 /* texel type */ kU32,
                                 {{"coords", 2, kI32}, {"lod", 1, kI32}},
                                 {"result", 4, kU32},
                                 {
                                     "OpImageFetch %v4uint %t %coords Lod %lod",
                                 },
                             }),
                         PrintCase);

}  // namespace
}  // namespace tint::writer::spirv
