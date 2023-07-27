// Copyright 2022 The Tint Authors.
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

#include "src/tint/lang/wgsl/resolver/resolver.h"

#include "gtest/gtest.h"
#include "src/tint/lang/core/builtin/address_space.h"
#include "src/tint/lang/core/builtin/extension.h"
#include "src/tint/lang/core/builtin/texel_format.h"
#include "src/tint/lang/core/type/texture_dimension.h"
#include "src/tint/lang/wgsl/resolver/resolver_test_helper.h"
#include "src/tint/lang/wgsl/sem/index_accessor_expression.h"
#include "src/tint/lang/wgsl/sem/member_accessor_expression.h"
#include "src/tint/lang/wgsl/sem/value_expression.h"
#include "src/tint/utils/containers/vector.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::resolver {
namespace {

struct SideEffectsTest : ResolverTest {
    template <typename T>
    void MakeSideEffectFunc(const char* name) {
        auto global = Sym();
        GlobalVar(global, ty.Of<T>(), builtin::AddressSpace::kPrivate);
        auto local = Sym();
        Func(name, tint::Empty, ty.Of<T>(),
             tint::Vector{
                 Decl(Var(local, ty.Of<T>())),
                 Assign(global, local),
                 Return(global),
             });
    }

    template <typename MAKE_TYPE_FUNC>
    void MakeSideEffectFunc(const char* name, MAKE_TYPE_FUNC make_type) {
        auto global = Sym();
        GlobalVar(global, make_type(), builtin::AddressSpace::kPrivate);
        auto local = Sym();
        Func(name, tint::Empty, make_type(),
             tint::Vector{
                 Decl(Var(local, make_type())),
                 Assign(global, local),
                 Return(global),
             });
    }
};

TEST_F(SideEffectsTest, Phony) {
    auto* expr = Phony();
    auto* body = Assign(expr, 1_i);
    WrapInFunction(body);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Literal) {
    auto* expr = Expr(1_i);
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, VariableUser) {
    auto* var = Decl(Var("a", ty.i32()));
    auto* expr = Expr("a");
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().GetVal(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->UnwrapLoad()->Is<sem::VariableUser>());
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Call_Builtin_NoSE) {
    GlobalVar("a", ty.f32(), builtin::AddressSpace::kPrivate);
    auto* expr = Call("dpdx", "a");
    Func("f", tint::Empty, ty.void_(), tint::Vector{Ignore(expr)},
         tint::Vector{create<ast::StageAttribute>(ast::PipelineStage::kFragment)});

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Call_Builtin_NoSE_WithSEArg) {
    MakeSideEffectFunc<f32>("se");
    auto* expr = Call("dpdx", Call("se"));
    Func("f", tint::Empty, ty.void_(), tint::Vector{Ignore(expr)},
         tint::Vector{create<ast::StageAttribute>(ast::PipelineStage::kFragment)});

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Call_Builtin_SE) {
    GlobalVar("a", ty.atomic(ty.i32()), builtin::AddressSpace::kWorkgroup);
    auto* expr = Call("atomicAdd", AddressOf("a"), 1_i);
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_TRUE(sem->HasSideEffects());
}

namespace builtin_tests {
struct Case {
    const char* name;
    tint::Vector<const char*, 3> args;
    bool has_side_effects;
    bool returns_value;
    ast::PipelineStage pipeline_stage;
};
static Case C(const char* name,
              tint::VectorRef<const char*> args,
              bool has_side_effects,
              bool returns_value,
              ast::PipelineStage stage = ast::PipelineStage::kFragment) {
    Case c;
    c.name = name;
    c.args = std::move(args);
    c.has_side_effects = has_side_effects;
    c.returns_value = returns_value;
    c.pipeline_stage = stage;
    return c;
}
static std::ostream& operator<<(std::ostream& o, const Case& c) {
    o << c.name << "(";
    for (size_t i = 0; i < c.args.Length(); ++i) {
        o << c.args[i];
        if (i + 1 != c.args.Length()) {
            o << ", ";
        }
    }
    o << ")";
    return o;
}

using SideEffectsBuiltinTest = resolver::ResolverTestWithParam<Case>;

TEST_P(SideEffectsBuiltinTest, Test) {
    Enable(tint::builtin::Extension::kChromiumExperimentalDp4A);
    auto& c = GetParam();

    uint32_t next_binding = 0;
    GlobalVar("f", ty.f32(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("i", ty.i32(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("u", ty.u32(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("b", ty.bool_(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("vf", ty.vec3<f32>(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("vf2", ty.vec2<f32>(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("vi2", ty.vec2<i32>(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("vf4", ty.vec4<f32>(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("vb", ty.vec3<bool>(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("m", ty.mat3x3<f32>(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("arr", ty.array<f32, 10>(), tint::builtin::AddressSpace::kPrivate);
    GlobalVar("storage_arr", ty.array<f32>(), tint::builtin::AddressSpace::kStorage, Group(0_a),
              Binding(AInt(next_binding++)));
    GlobalVar("workgroup_arr", ty.array<f32, 4>(), tint::builtin::AddressSpace::kWorkgroup);
    GlobalVar("a", ty.atomic(ty.i32()), tint::builtin::AddressSpace::kStorage,
              tint::builtin::Access::kReadWrite, Group(0_a), Binding(AInt(next_binding++)));
    if (c.pipeline_stage != ast::PipelineStage::kCompute) {
        GlobalVar("t2d", ty.sampled_texture(type::TextureDimension::k2d, ty.f32()), Group(0_a),
                  Binding(AInt(next_binding++)));
        GlobalVar("tdepth2d", ty.depth_texture(type::TextureDimension::k2d), Group(0_a),
                  Binding(AInt(next_binding++)));
        GlobalVar("t2d_arr", ty.sampled_texture(type::TextureDimension::k2dArray, ty.f32()),
                  Group(0_a), Binding(AInt(next_binding++)));
        GlobalVar("t2d_multi", ty.multisampled_texture(type::TextureDimension::k2d, ty.f32()),
                  Group(0_a), Binding(AInt(next_binding++)));
        GlobalVar(
            "tstorage2d",
            ty.storage_texture(type::TextureDimension::k2d, tint::builtin::TexelFormat::kR32Float,
                               tint::builtin::Access::kWrite),
            Group(0_a), Binding(AInt(next_binding++)));
        GlobalVar("s2d", ty.sampler(type::SamplerKind::kSampler), Group(0_a),
                  Binding(AInt(next_binding++)));
        GlobalVar("scomp", ty.sampler(type::SamplerKind::kComparisonSampler), Group(0_a),
                  Binding(AInt(next_binding++)));
    }

    tint::Vector<const ast::Statement*, 4> stmts;
    stmts.Push(Decl(Let("pstorage_arr", AddressOf("storage_arr"))));
    if (c.pipeline_stage == ast::PipelineStage::kCompute) {
        stmts.Push(Decl(Let("pworkgroup_arr", AddressOf("workgroup_arr"))));
    }
    stmts.Push(Decl(Let("pa", AddressOf("a"))));

    tint::Vector<const ast::Expression*, 5> args;
    for (auto& a : c.args) {
        args.Push(Expr(a));
    }
    auto* expr = Call(c.name, args);

    tint::Vector<const ast::Attribute*, 2> attrs;
    attrs.Push(create<ast::StageAttribute>(c.pipeline_stage));
    if (c.pipeline_stage == ast::PipelineStage::kCompute) {
        attrs.Push(WorkgroupSize(Expr(1_u)));
    }

    if (c.returns_value) {
        stmts.Push(Assign(Phony(), expr));
    } else {
        stmts.Push(CallStmt(expr));
    }

    Func("func", tint::Empty, ty.void_(), stmts, attrs);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_EQ(c.has_side_effects, sem->HasSideEffects());
}
INSTANTIATE_TEST_SUITE_P(
    SideEffectsTest_Builtins,
    SideEffectsBuiltinTest,
    testing::ValuesIn(std::vector<Case>{
        // No side-effect builts
        C("abs", tint::Vector{"f"}, false, true),                                               //
        C("acos", tint::Vector{"f"}, false, true),                                              //
        C("acosh", tint::Vector{"f"}, false, true),                                             //
        C("all", tint::Vector{"vb"}, false, true),                                              //
        C("any", tint::Vector{"vb"}, false, true),                                              //
        C("arrayLength", tint::Vector{"pstorage_arr"}, false, true),                            //
        C("asin", tint::Vector{"f"}, false, true),                                              //
        C("asinh", tint::Vector{"f"}, false, true),                                             //
        C("atan", tint::Vector{"f"}, false, true),                                              //
        C("atan2", tint::Vector{"f", "f"}, false, true),                                        //
        C("atanh", tint::Vector{"f"}, false, true),                                             //
        C("atomicLoad", tint::Vector{"pa"}, false, true),                                       //
        C("ceil", tint::Vector{"f"}, false, true),                                              //
        C("clamp", tint::Vector{"f", "f", "f"}, false, true),                                   //
        C("cos", tint::Vector{"f"}, false, true),                                               //
        C("cosh", tint::Vector{"f"}, false, true),                                              //
        C("countLeadingZeros", tint::Vector{"i"}, false, true),                                 //
        C("countOneBits", tint::Vector{"i"}, false, true),                                      //
        C("countTrailingZeros", tint::Vector{"i"}, false, true),                                //
        C("cross", tint::Vector{"vf", "vf"}, false, true),                                      //
        C("degrees", tint::Vector{"f"}, false, true),                                           //
        C("determinant", tint::Vector{"m"}, false, true),                                       //
        C("distance", tint::Vector{"f", "f"}, false, true),                                     //
        C("dot", tint::Vector{"vf", "vf"}, false, true),                                        //
        C("dot4I8Packed", tint::Vector{"u", "u"}, false, true),                                 //
        C("dot4U8Packed", tint::Vector{"u", "u"}, false, true),                                 //
        C("exp", tint::Vector{"f"}, false, true),                                               //
        C("exp2", tint::Vector{"f"}, false, true),                                              //
        C("extractBits", tint::Vector{"i", "u", "u"}, false, true),                             //
        C("faceForward", tint::Vector{"vf", "vf", "vf"}, false, true),                          //
        C("firstLeadingBit", tint::Vector{"u"}, false, true),                                   //
        C("firstTrailingBit", tint::Vector{"u"}, false, true),                                  //
        C("floor", tint::Vector{"f"}, false, true),                                             //
        C("fma", tint::Vector{"f", "f", "f"}, false, true),                                     //
        C("fract", tint::Vector{"vf"}, false, true),                                            //
        C("frexp", tint::Vector{"f"}, false, true),                                             //
        C("insertBits", tint::Vector{"i", "i", "u", "u"}, false, true),                         //
        C("inverseSqrt", tint::Vector{"f"}, false, true),                                       //
        C("ldexp", tint::Vector{"f", "i"}, false, true),                                        //
        C("length", tint::Vector{"vf"}, false, true),                                           //
        C("log", tint::Vector{"f"}, false, true),                                               //
        C("log2", tint::Vector{"f"}, false, true),                                              //
        C("max", tint::Vector{"f", "f"}, false, true),                                          //
        C("min", tint::Vector{"f", "f"}, false, true),                                          //
        C("mix", tint::Vector{"f", "f", "f"}, false, true),                                     //
        C("modf", tint::Vector{"f"}, false, true),                                              //
        C("normalize", tint::Vector{"vf"}, false, true),                                        //
        C("pack2x16float", tint::Vector{"vf2"}, false, true),                                   //
        C("pack2x16snorm", tint::Vector{"vf2"}, false, true),                                   //
        C("pack2x16unorm", tint::Vector{"vf2"}, false, true),                                   //
        C("pack4x8snorm", tint::Vector{"vf4"}, false, true),                                    //
        C("pack4x8unorm", tint::Vector{"vf4"}, false, true),                                    //
        C("pow", tint::Vector{"f", "f"}, false, true),                                          //
        C("radians", tint::Vector{"f"}, false, true),                                           //
        C("reflect", tint::Vector{"vf", "vf"}, false, true),                                    //
        C("refract", tint::Vector{"vf", "vf", "f"}, false, true),                               //
        C("reverseBits", tint::Vector{"u"}, false, true),                                       //
        C("round", tint::Vector{"f"}, false, true),                                             //
        C("select", tint::Vector{"f", "f", "b"}, false, true),                                  //
        C("sign", tint::Vector{"f"}, false, true),                                              //
        C("sin", tint::Vector{"f"}, false, true),                                               //
        C("sinh", tint::Vector{"f"}, false, true),                                              //
        C("smoothstep", tint::Vector{"f", "f", "f"}, false, true),                              //
        C("sqrt", tint::Vector{"f"}, false, true),                                              //
        C("step", tint::Vector{"f", "f"}, false, true),                                         //
        C("tan", tint::Vector{"f"}, false, true),                                               //
        C("tanh", tint::Vector{"f"}, false, true),                                              //
        C("textureDimensions", tint::Vector{"t2d"}, false, true),                               //
        C("textureGather", tint::Vector{"tdepth2d", "s2d", "vf2"}, false, true),                //
        C("textureGatherCompare", tint::Vector{"tdepth2d", "scomp", "vf2", "f"}, false, true),  //
        C("textureLoad", tint::Vector{"t2d", "vi2", "i"}, false, true),                         //
        C("textureNumLayers", tint::Vector{"t2d_arr"}, false, true),                            //
        C("textureNumLevels", tint::Vector{"t2d"}, false, true),                                //
        C("textureNumSamples", tint::Vector{"t2d_multi"}, false, true),                         //
        C("textureSampleCompareLevel",
          tint::Vector{"tdepth2d", "scomp", "vf2", "f"},
          false,
          true),                                                                                //
        C("textureSampleGrad", tint::Vector{"t2d", "s2d", "vf2", "vf2", "vf2"}, false, true),   //
        C("textureSampleLevel", tint::Vector{"t2d", "s2d", "vf2", "f"}, false, true),           //
        C("transpose", tint::Vector{"m"}, false, true),                                         //
        C("trunc", tint::Vector{"f"}, false, true),                                             //
        C("unpack2x16float", tint::Vector{"u"}, false, true),                                   //
        C("unpack2x16snorm", tint::Vector{"u"}, false, true),                                   //
        C("unpack2x16unorm", tint::Vector{"u"}, false, true),                                   //
        C("unpack4x8snorm", tint::Vector{"u"}, false, true),                                    //
        C("unpack4x8unorm", tint::Vector{"u"}, false, true),                                    //
        C("storageBarrier", tint::Empty, false, false, ast::PipelineStage::kCompute),           //
        C("workgroupBarrier", tint::Empty, false, false, ast::PipelineStage::kCompute),         //
        C("textureSample", tint::Vector{"t2d", "s2d", "vf2"}, false, true),                     //
        C("textureSampleBias", tint::Vector{"t2d", "s2d", "vf2", "f"}, false, true),            //
        C("textureSampleCompare", tint::Vector{"tdepth2d", "scomp", "vf2", "f"}, false, true),  //
        C("dpdx", tint::Vector{"f"}, false, true),                                              //
        C("dpdxCoarse", tint::Vector{"f"}, false, true),                                        //
        C("dpdxFine", tint::Vector{"f"}, false, true),                                          //
        C("dpdy", tint::Vector{"f"}, false, true),                                              //
        C("dpdyCoarse", tint::Vector{"f"}, false, true),                                        //
        C("dpdyFine", tint::Vector{"f"}, false, true),                                          //
        C("fwidth", tint::Vector{"f"}, false, true),                                            //
        C("fwidthCoarse", tint::Vector{"f"}, false, true),                                      //
        C("fwidthFine", tint::Vector{"f"}, false, true),                                        //

        // Side-effect builtins
        C("atomicAdd", tint::Vector{"pa", "i"}, true, true),                       //
        C("atomicAnd", tint::Vector{"pa", "i"}, true, true),                       //
        C("atomicCompareExchangeWeak", tint::Vector{"pa", "i", "i"}, true, true),  //
        C("atomicExchange", tint::Vector{"pa", "i"}, true, true),                  //
        C("atomicMax", tint::Vector{"pa", "i"}, true, true),                       //
        C("atomicMin", tint::Vector{"pa", "i"}, true, true),                       //
        C("atomicOr", tint::Vector{"pa", "i"}, true, true),                        //
        C("atomicStore", tint::Vector{"pa", "i"}, true, false),                    //
        C("atomicSub", tint::Vector{"pa", "i"}, true, true),                       //
        C("atomicXor", tint::Vector{"pa", "i"}, true, true),                       //
        C("textureStore", tint::Vector{"tstorage2d", "vi2", "vf4"}, true, false),  //
        C("workgroupUniformLoad",
          tint::Vector{"pworkgroup_arr"},
          true,
          true,
          ast::PipelineStage::kCompute),  //

        // Unimplemented builtins
        // C("quantizeToF16", tint::Vector{"f"}, false), //
        // C("saturate", tint::Vector{"f"}, false), //
    }));

}  // namespace builtin_tests

TEST_F(SideEffectsTest, Call_Function) {
    Func("f", tint::Empty, ty.i32(), tint::Vector{Return(1_i)});
    auto* expr = Call("f");
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Call_TypeConversion_NoSE) {
    auto* var = Decl(Var("a", ty.i32()));
    auto* expr = Call<f32>("a");
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Call_TypeConversion_SE) {
    MakeSideEffectFunc<i32>("se");
    auto* expr = Call<f32>(Call("se"));
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Call_TypeInitializer_NoSE) {
    auto* var = Decl(Var("a", ty.f32()));
    auto* expr = Call<f32>("a");
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Call_TypeInitializer_SE) {
    MakeSideEffectFunc<f32>("se");
    auto* expr = Call<f32>(Call("se"));
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Call>());
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, MemberAccessor_Struct_NoSE) {
    auto* s = Structure("S", tint::Vector{Member("m", ty.i32())});
    auto* var = Decl(Var("a", ty.Of(s)));
    auto* expr = MemberAccessor("a", "m");
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, MemberAccessor_Struct_SE) {
    auto* s = Structure("S", tint::Vector{Member("m", ty.i32())});
    MakeSideEffectFunc("se", [&] { return ty.Of(s); });
    auto* expr = MemberAccessor(Call("se"), "m");
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, MemberAccessor_Vector) {
    auto* var = Decl(Var("a", ty.vec4<f32>()));
    auto* expr = MemberAccessor("a", "x");
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->UnwrapLoad()->Is<sem::MemberAccessorExpression>());
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, MemberAccessor_VectorSwizzleNoSE) {
    auto* var = Decl(Var("a", ty.vec4<f32>()));
    auto* expr = MemberAccessor("a", "xzyw");
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Swizzle>());
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, MemberAccessor_VectorSwizzleSE) {
    MakeSideEffectFunc("se", [&] { return ty.vec4<f32>(); });
    auto* expr = MemberAccessor(Call("se"), "xzyw");
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Is<sem::Swizzle>());
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Binary_NoSE) {
    auto* a = Decl(Var("a", ty.i32()));
    auto* b = Decl(Var("b", ty.i32()));
    auto* expr = Add("a", "b");
    WrapInFunction(a, b, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Binary_LeftSE) {
    MakeSideEffectFunc<i32>("se");
    auto* b = Decl(Var("b", ty.i32()));
    auto* expr = Add(Call("se"), "b");
    WrapInFunction(b, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Binary_RightSE) {
    MakeSideEffectFunc<i32>("se");
    auto* a = Decl(Var("a", ty.i32()));
    auto* expr = Add("a", Call("se"));
    WrapInFunction(a, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Binary_BothSE) {
    MakeSideEffectFunc<i32>("se1");
    MakeSideEffectFunc<i32>("se2");
    auto* expr = Add(Call("se1"), Call("se2"));
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Unary_NoSE) {
    auto* var = Decl(Var("a", ty.bool_()));
    auto* expr = Not("a");
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Unary_SE) {
    MakeSideEffectFunc<bool>("se");
    auto* expr = Not(Call("se"));
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, IndexAccessor_NoSE) {
    auto* var = Decl(Var("a", ty.array<i32, 10>()));
    auto* expr = IndexAccessor("a", 0_i);
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, IndexAccessor_ObjSE) {
    MakeSideEffectFunc("se", [&] { return ty.array<i32, 10>(); });
    auto* expr = IndexAccessor(Call("se"), 0_i);
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, IndexAccessor_IndexSE) {
    MakeSideEffectFunc<i32>("se");
    auto* var = Decl(Var("a", ty.array<i32, 10>()));
    auto* expr = IndexAccessor("a", Call("se"));
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, IndexAccessor_BothSE) {
    MakeSideEffectFunc("se1", [&] { return ty.array<i32, 10>(); });
    MakeSideEffectFunc<i32>("se2");
    auto* expr = IndexAccessor(Call("se1"), Call("se2"));
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Bitcast_NoSE) {
    auto* var = Decl(Var("a", ty.i32()));
    auto* expr = Bitcast<f32>("a");
    WrapInFunction(var, expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_FALSE(sem->HasSideEffects());
}

TEST_F(SideEffectsTest, Bitcast_SE) {
    MakeSideEffectFunc<i32>("se");
    auto* expr = Bitcast<f32>(Call("se"));
    WrapInFunction(expr);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->HasSideEffects());
}

}  // namespace
}  // namespace tint::resolver
