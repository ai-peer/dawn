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

#include "gmock/gmock.h"

#include "src/tint/lang/core/texel_format.h"
#include "src/tint/lang/core/type/depth_multisampled_texture.h"
#include "src/tint/lang/core/type/depth_texture.h"
#include "src/tint/lang/core/type/f32.h"
#include "src/tint/lang/core/type/multisampled_texture.h"
#include "src/tint/lang/core/type/sampled_texture.h"
#include "src/tint/lang/core/type/storage_texture.h"
#include "src/tint/lang/core/type/texture_dimension.h"
#include "src/tint/lang/core/type/type.h"
#include "src/tint/lang/glsl/writer/printer/helper_test.h"

namespace tint::glsl::writer {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

TEST_F(GlslPrinterTest, TypeEmitArray) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.array<bool, 4>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  bool a[4] = {};
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitArrayOfArray) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.array(ty.array<bool, 4>(), 5_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  bool ary[5][4] = {};
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitArrayOfArrayOfArray) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate,
                          ty.array(ty.array(ty.array<bool, 4>(), 5_u), 6_u)));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  bool ary[6][5][4] = {};
}
)");
}

// TODO(dsinclair): How to write, struct?
TEST_F(GlslPrinterTest, DISABLED_TypeEmitArray_WithoutName) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.array<bool, 4>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  bool[4]
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitBool) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.bool_()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
    bool a = false;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitF32) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.f32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
    float a = 0.0;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitF16) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.f16()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
    float16_t a = 0.0;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitI32) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.i32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
    int a = 0;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitMatrix_F32) {
    auto* func = b.Function("foo", ty.mat2x3<f32>());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.bool_()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
    mat2x3 a;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitMatrix_F16) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.mat2x3<f16>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
    f16mat2x3 a;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitStruct) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.i32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
struct S {
  int a;
  float b;
};

void foo() {
    S a;
}
)");
}

// TODO(dsinclair): When does remapper run ...
TEST_F(GlslPrinterTest, DISABLED_TypeEmitStruct_NameCollision) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("double"), ty.i32()},
                                                  {mod.symbols.Register("float"), ty.f32()},
                                              });

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
struct S {
  int tint_symbol;
  float tint_symbol_1;
};

void foo() {
    S a;
}
)");
}

TEST_F(GlslPrinterTest, EmitType_Struct_Dedup) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.i32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Var("b", ty.ptr(core::AddressSpace::kPrivate, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(struct S {
  int a;
  float b;
};

void foo() {
  S a;
  S b;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitU32) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.u32()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  uint a;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitVector_F32) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.vec3<f32>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  vec3 a;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitVector_F16) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kPrivate, ty.vec3<f16>()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  f16vec3 a;
}
)");
}

TEST_F(GlslPrinterTest, TypeEmitVoid) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] { b.Return(func); });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
}
)");
}

TEST_F(GlslPrinterTest, EmitSampler) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, ty.sampler()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  sampler a;
}
)");
}

TEST_F(GlslPrinterTest, EmitSamplerComparison) {
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, ty.comparison_sampler()));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  sampler a;
}
)");
}

struct GlslDepthTextureData {
    core::type::TextureDimension dim;
    std::string result;
};
[[maybe_unused]] inline std::ostream& operator<<(std::ostream& out, GlslDepthTextureData data) {
    StringStream s;
    s << data.dim;
    out << s.str();
    return out;
}
using GlslPrinterDepthTexturesTest = GlslPrinterTestWithParam<GlslDepthTextureData>;
TEST_P(GlslPrinterDepthTexturesTest, Emit) {
    auto params = GetParam();

    auto* t = ty.Get<core::type::DepthTexture>(params.dim);
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, t));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  )" + params.result +
                           R"( a;
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslPrinterTest,
    GlslPrinterDepthTexturesTest,
    testing::Values(
        GlslDepthTextureData{core::type::TextureDimension::k2d, "sampler2DShadow"},
        GlslDepthTextureData{core::type::TextureDimension::k2dArray, "sampler2DArrayShadow"},
        GlslDepthTextureData{core::type::TextureDimension::kCube, "samplerCubeShadow"},
        GlslDepthTextureData{core::type::TextureDimension::kCubeArray, "samplerCubeArrayShadow"}));

TEST_F(GlslPrinterTest, EmiType_DepthMultisampledTexture) {
    auto* t = ty.Get<core::type::DepthMultisampledTexture>(core::type::TextureDimension::k2d);
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, t));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  sampler2DMS a;
}
)");
}

enum class TextureDataType : uint8_t { F32, U32, I32 };
struct GlslSampledTextureData {
    core::type::TextureDimension dim;
    TextureDataType datatype;
    std::string result;
};
[[maybe_unused]] inline std::ostream& operator<<(std::ostream& out, GlslSampledTextureData data) {
    StringStream str;
    str << data.dim;
    out << str.str();
    return out;
}
using GlslPrinterSampledTexturesTest = GlslPrinterTestWithParam<GlslSampledTextureData>;
TEST_P(GlslPrinterSampledTexturesTest, Emit) {
    auto params = GetParam();

    const core::type::Type* datatype;
    switch (params.datatype) {
        case TextureDataType::F32:
            datatype = ty.f32();
            break;
        case TextureDataType::U32:
            datatype = ty.u32();
            break;
        case TextureDataType::I32:
            datatype = ty.i32();
            break;
    }

    auto* t = ty.Get<core::type::SampledTexture>(params.dim, datatype);
    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, t));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  )" + params.result +
                           R"( a;
}
)");
}
INSTANTIATE_TEST_SUITE_P(GlslPrinterTest,
                         GlslPrinterSampledTexturesTest,
                         testing::Values(
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k1d,
                                 TextureDataType::F32,
                                 "sampler1D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k2d,
                                 TextureDataType::F32,
                                 "sampler2D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k2dArray,
                                 TextureDataType::F32,
                                 "sampler2DArray",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k3d,
                                 TextureDataType::F32,
                                 "sampler3D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::kCube,
                                 TextureDataType::F32,
                                 "samplerCube",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::kCubeArray,
                                 TextureDataType::F32,
                                 "samplerCubeArray",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k1d,
                                 TextureDataType::U32,
                                 "usampler1D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k2d,
                                 TextureDataType::U32,
                                 "usampler2D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k2dArray,
                                 TextureDataType::U32,
                                 "usampler2DArray",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k3d,
                                 TextureDataType::U32,
                                 "usampler3D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::kCube,
                                 TextureDataType::U32,
                                 "usamplerCube",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::kCubeArray,
                                 TextureDataType::U32,
                                 "usamplerCubeArray",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k1d,
                                 TextureDataType::I32,
                                 "isampler1D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k2d,
                                 TextureDataType::I32,
                                 "isampler2D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k2dArray,
                                 TextureDataType::I32,
                                 "isampler2DArray",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::k3d,
                                 TextureDataType::I32,
                                 "isampler3D",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::kCube,
                                 TextureDataType::I32,
                                 "isamplerCube",
                             },
                             GlslSampledTextureData{
                                 core::type::TextureDimension::kCubeArray,
                                 TextureDataType::I32,
                                 "isamplerCubeArray",
                             }));

TEST_F(GlslPrinterTest, EmitMultisampledTexture) {
    auto* ms = ty.Get<core::type::MultisampledTexture>(core::type::TextureDimension::k2d, ty.f32());

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, ms));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  highp sampler2DMS a;
}
)");
}

struct GlslStorageTextureData {
    core::type::TextureDimension dim;
    core::TexelFormat imgfmt;
    std::string result;
};
[[maybe_unused]] inline std::ostream& operator<<(std::ostream& out, GlslStorageTextureData data) {
    StringStream str;
    str << data.dim;
    return out << str.str();
}
using GlslPrinterStorageTexturesTest = GlslPrinterTestWithParam<GlslStorageTextureData>;
TEST_P(GlslPrinterStorageTexturesTest, Emit) {
    auto params = GetParam();

    auto* f32 = const_cast<core::type::F32*>(ty.f32());
    auto s = ty.Get<core::type::StorageTexture>(params.dim, core::TexelFormat::kR32Float,
                                                core::Access::kWrite, f32);

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        b.Var("a", ty.ptr(core::AddressSpace::kWorkgroup, s));
        b.Return(func);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, GlslHeader() + R"(
void foo() {
  )" + params.result + R"( a;
}
)");
}
INSTANTIATE_TEST_SUITE_P(
    GlslPrinterTest,
    GlslPrinterStorageTexturesTest,
    testing::Values(GlslStorageTextureData{core::type::TextureDimension::k1d,
                                           core::TexelFormat::kRgba8Unorm, "image1D"},
                    GlslStorageTextureData{core::type::TextureDimension::k2d,
                                           core::TexelFormat::kRgba16Float, "image2D"},
                    GlslStorageTextureData{core::type::TextureDimension::k2dArray,
                                           core::TexelFormat::kR32Float, "image2DArray"},
                    GlslStorageTextureData{core::type::TextureDimension::k3d,
                                           core::TexelFormat::kRg32Float, "image3D"},
                    GlslStorageTextureData{core::type::TextureDimension::k1d,
                                           core::TexelFormat::kRgba32Float, "image1D"},
                    GlslStorageTextureData{core::type::TextureDimension::k2d,
                                           core::TexelFormat::kRgba16Uint, "image2D"},
                    GlslStorageTextureData{core::type::TextureDimension::k2dArray,
                                           core::TexelFormat::kR32Uint, "image2DArray"},
                    GlslStorageTextureData{core::type::TextureDimension::k3d,
                                           core::TexelFormat::kRg32Uint, "image3D"},
                    GlslStorageTextureData{core::type::TextureDimension::k1d,
                                           core::TexelFormat::kRgba32Uint, "image1D"},
                    GlslStorageTextureData{core::type::TextureDimension::k2d,
                                           core::TexelFormat::kRgba16Sint, "image2D"},
                    GlslStorageTextureData{core::type::TextureDimension::k2dArray,
                                           core::TexelFormat::kR32Sint, "image2DArray"},
                    GlslStorageTextureData{core::type::TextureDimension::k3d,
                                           core::TexelFormat::kRg32Sint, "image3D"},
                    GlslStorageTextureData{core::type::TextureDimension::k1d,
                                           core::TexelFormat::kRgba32Sint, "image1D"}));

}  // namespace
}  // namespace tint::glsl::writer
