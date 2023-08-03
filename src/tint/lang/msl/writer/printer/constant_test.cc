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

#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/matrix.h"
#include "src/tint/lang/msl/writer/printer/helper_test.h"
#include "src/tint/utils/text/string.h"

namespace tint::msl::writer {
namespace {

using namespace tint::number_suffixes;  // NOLINT

TEST_F(MslPrinterTest, Constant_Bool_True) {
    auto* c = b.Constant(true);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.bool_()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread bool a = true;
)");
}

TEST_F(MslPrinterTest, Constant_Bool_False) {
    auto* c = b.Constant(false);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.bool_()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread bool a = false;
)");
}

TEST_F(MslPrinterTest, Constant_i32) {
    auto* c = b.Constant(-12345_i);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.i32()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread int a = -12345;
)");
}

TEST_F(MslPrinterTest, Constant_u32) {
    auto* c = b.Constant(12345_u);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.u32()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread uint a = 12345u;
)");
}

TEST_F(MslPrinterTest, Constant_F32) {
    auto* c = b.Constant(f32((1 << 30) - 4));
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.f32()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float a = 1073741824.0f;
)");
}

TEST_F(MslPrinterTest, Constant_F16) {
    auto* c = b.Constant(f16((1 << 15) - 8));
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.f16()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread half a = 32752.0h;
)");
}

TEST_F(MslPrinterTest, Constant_Vector_Splat) {
    auto* c = b.Splat(ty.vec3<f32>(), 1.5_f, 3);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.vec3<f32>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float3 a = float3(1.5f);
)");
}

TEST_F(MslPrinterTest, Constant_Vector_Composite) {
    auto* c = b.Composite(ty.vec3<f32>(), 1.5_f, 1.0_f, 1.5_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.vec3<f32>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float3 a = float3(1.5f, 1.0f, 1.5f);
)");
}

TEST_F(MslPrinterTest, Constant_Vector_Composite_AnyZero) {
    auto* c = b.Composite(ty.vec3<f32>(), 1.0_f, 0.0_f, 1.5_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.vec3<f32>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float3 a = float3(1.0f, 0.0f, 1.5f);
)");
}

TEST_F(MslPrinterTest, Constant_Vector_Composite_AllZero) {
    auto* c = b.Composite(ty.vec3<f32>(), 0.0_f, 0.0_f, 0.0_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.vec3<f32>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float3 a = float3(0.0f);
)");
}

TEST_F(MslPrinterTest, Constant_Matrix_Splat) {
    auto* c = b.Splat(ty.mat3x2<f32>(), 1.5_f, 3);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.mat3x2<f32>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float3x2 a = float3x2(1.5f, 1.5f, 1.5f);
)");
}

TEST_F(MslPrinterTest, Constant_Matrix_Composite) {
    auto* c = b.Composite(ty.mat3x2<f32>(),                           //
                          b.Composite(ty.vec2<f32>(), 1.5_f, 1.0_f),  //
                          b.Composite(ty.vec2<f32>(), 1.5_f, 2.0_f),  //
                          b.Composite(ty.vec2<f32>(), 2.5_f, 3.5_f));
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.mat3x2<f32>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float3x2 a = float3x2(float2(1.5f, 1.0f), float2(1.5f, 2.0f), float2(2.5f, 3.5f));
)");
}

TEST_F(MslPrinterTest, Constant_Matrix_Composite_AnyZero) {
    auto* c = b.Composite(ty.mat2x2<f32>(),                           //
                          b.Composite(ty.vec2<f32>(), 1.0_f, 0.0_f),  //
                          b.Composite(ty.vec2<f32>(), 1.5_f, 2.5_f));
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.mat2x2<f32>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float2x2 a = float2x2(float2(1.0f, 0.0f), float2(1.5f, 2.5f));
)");
}

TEST_F(MslPrinterTest, Constant_Matrix_Composite_AllZero) {
    auto* c = b.Composite(ty.mat3x2<f32>(),                           //
                          b.Composite(ty.vec2<f32>(), 0.0_f, 0.0_f),  //
                          b.Composite(ty.vec2<f32>(), 0.0_f, 0.0_f),  //
                          b.Composite(ty.vec2<f32>(), 0.0_f, 0.0_f));
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.mat3x2<f32>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(
thread float3x2 a = float3x2(float2(0.0f), float2(0.0f), float2(0.0f));
)");
}

TEST_F(MslPrinterTest, Constant_Array_Splat) {
    auto* c = b.Splat(ty.array<f32, 3>(), 1.5_f, 3);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.array<f32, 3>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + MetalArray() + R"(
thread tint_array<float, 3> a = tint_array<float, 3>{1.5f, 1.5f, 1.5f};
)");
}

TEST_F(MslPrinterTest, Constant_Array_Composite) {
    auto* c = b.Composite(ty.array<f32, 3>(), 1.5_f, 1.0_f, 2.0_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.array<f32, 3>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + MetalArray() + R"(
thread tint_array<float, 3> a = tint_array<float, 3>{1.5f, 1.0f, 2.0f};
)");
}

TEST_F(MslPrinterTest, Constant_Array_Composite_AnyZero) {
    auto* c = b.Composite(ty.array<f32, 2>(), 1.0_f, 0.0_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.array<f32, 2>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + MetalArray() + R"(
thread tint_array<float, 2> a = tint_array<float, 2>{1.0f, 0.0f};
)");
}

TEST_F(MslPrinterTest, Constant_Array_Composite_AllZero) {
    auto* c = b.Composite(ty.array<f32, 3>(), 0.0_f, 0.0_f, 0.0_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, ty.array<f32, 3>()));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + MetalArray() + R"(
thread tint_array<float, 3> a = tint_array<float, 3>{};
)");
}

TEST_F(MslPrinterTest, Constant_Struct_Splat) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.f32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* c = b.Splat(s, 1.5_f, 2);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, s));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(struct S {
  float a;
  float b;
};

thread S a = S{.a=1.5f, .b=1.5f};
)");
}

TEST_F(MslPrinterTest, Constant_Struct_Composite) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.f32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* c = b.Composite(s, 1.5_f, 1.0_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, s));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(struct S {
  float a;
  float b;
};

thread S a = S{.a=1.5f, .b=1.0f};
)");
}

TEST_F(MslPrinterTest, Constant_Struct_Composite_AnyZero) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.f32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* c = b.Composite(s, 1.0_f, 0.0_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, s));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(struct S {
  float a;
  float b;
};

thread S a = S{.a=1.0f, .b=0.0f};
)");
}

TEST_F(MslPrinterTest, Constant_Struct_Composite_AllZero) {
    auto* s = ty.Struct(mod.symbols.New("S"), {
                                                  {mod.symbols.Register("a"), ty.f32()},
                                                  {mod.symbols.Register("b"), ty.f32()},
                                              });
    auto* c = b.Composite(s, 0.0_f, 0.0_f);
    b.Append(b.RootBlock(), [&] {
        auto* v = b.Var("a", ty.ptr(builtin::AddressSpace::kPrivate, s));
        v->SetInitializer(c);
    });

    ASSERT_TRUE(Generate()) << err_ << output_;
    EXPECT_EQ(output_, MetalHeader() + R"(struct S {
  float a;
  float b;
};

thread S a = S{};
)");
}

}  // namespace
}  // namespace tint::msl::writer
