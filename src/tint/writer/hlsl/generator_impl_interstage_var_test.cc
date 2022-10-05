// Copyright 2021 The Tint Authors.
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
#include "src/tint/ast/id_attribute.h"
#include "src/tint/ast/stage_attribute.h"
#include "src/tint/writer/hlsl/test_helper.h"

using ::testing::HasSubstr;

using namespace tint::number_suffixes;  // NOLINT

namespace tint::writer::hlsl {
namespace {

using HlslGeneratorImplTest_InterstageVarTest = TestHelper;

TEST_F(HlslGeneratorImplTest_InterstageVarTest, VertexOuputEmitPlaceholders) {
    utils::Vector members{
        Member("pos", ty.vec4<f32>(), utils::Vector{Builtin(ast::BuiltinValue::kPosition)}),
        Member("f1", ty.f32(), utils::Vector{Location(1_u)}),
        Member("f3", ty.f32(), utils::Vector{Location(3_u)}),
    };

    Structure("VertexOut", members);

    Func("main", utils::Empty, ty.type_name("VertexOut"),
         utils::Vector{
             Decl(Var("shader_io", ty.type_name("VertexOut"))),
             Return("shader_io"),
         },
         utils::Vector{
             Stage(ast::PipelineStage::kVertex),
         });
    GeneratorImpl& gen = Build();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_EQ(gen.result(), R"(struct VertexOut {
  float4 pos : SV_Position;
  float f1 : TEXCOORD1;
  float f3 : TEXCOORD3;
  float tint_interstage_placeholder_0 : TEXCOORD0;
  float tint_interstage_placeholder_2 : TEXCOORD2;
};

VertexOut main() {
  VertexOut shader_io = (VertexOut)0;
  return shader_io;
}
)");
}

TEST_F(HlslGeneratorImplTest_InterstageVarTest, VertexOuputNoModificationsForCompactLocations) {
    utils::Vector members{
        Member("pos", ty.vec4<f32>(), utils::Vector{Builtin(ast::BuiltinValue::kPosition)}),
        Member("f0", ty.f32(), utils::Vector{Location(0_u)}),
        Member("f1", ty.f32(), utils::Vector{Location(1_u)}),
    };

    Structure("VertexOut", members);

    Func("main", utils::Empty, ty.type_name("VertexOut"),
         utils::Vector{
             Decl(Var("shader_io", ty.type_name("VertexOut"))),
             Return("shader_io"),
         },
         utils::Vector{
             Stage(ast::PipelineStage::kVertex),
         });
    GeneratorImpl& gen = Build();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_EQ(gen.result(), R"(struct VertexOut {
  float4 pos : SV_Position;
  float f0 : TEXCOORD0;
  float f1 : TEXCOORD1;
};

VertexOut main() {
  VertexOut shader_io = (VertexOut)0;
  return shader_io;
}
)");
}

TEST_F(HlslGeneratorImplTest_InterstageVarTest, FragmentInputEmitPlaceholders) {
    utils::Vector members{
        Member("f1", ty.f32(), utils::Vector{Location(1_u)}),
        Member("f3", ty.f32(), utils::Vector{Location(3_u)}),
    };

    Structure("FragmentIn", members);

    Func("main", utils::Vector{Param("shader_io", ty.type_name("FragmentIn"), utils::Empty)},
         ty.void_(),
         utils::Vector{
             Return(),
         },
         utils::Vector{
             Stage(ast::PipelineStage::kFragment),
         });
    GeneratorImpl& gen = Build();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_EQ(gen.result(), R"(struct FragmentIn {
  float f1 : TEXCOORD1;
  float f3 : TEXCOORD3;
  float tint_interstage_placeholder_0 : TEXCOORD0;
  float tint_interstage_placeholder_2 : TEXCOORD2;
};

void main(FragmentIn shader_io) {
  return;
}
)");
}

TEST_F(HlslGeneratorImplTest_InterstageVarTest, FragmentInputNoModificationsForCompactLocations) {
    utils::Vector members{
        Member("f0", ty.f32(), utils::Vector{Location(0_u)}),
        Member("f1", ty.f32(), utils::Vector{Location(1_u)}),
    };

    Structure("FragmentIn", members);

    Func("main", utils::Vector{Param("shader_io", ty.type_name("FragmentIn"), utils::Empty)},
         ty.void_(),
         utils::Vector{
             Return(),
         },
         utils::Vector{
             Stage(ast::PipelineStage::kFragment),
         });
    GeneratorImpl& gen = Build();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_EQ(gen.result(), R"(struct FragmentIn {
  float f0 : TEXCOORD0;
  float f1 : TEXCOORD1;
};

void main(FragmentIn shader_io) {
  return;
}
)");
}

TEST_F(HlslGeneratorImplTest_InterstageVarTest, VertexInputUnchanged) {
    Structure("VertexIn", utils::Vector{
                              Member("f1", ty.f32(), utils::Vector{Location(1_u)}),
                              Member("f3", ty.f32(), utils::Vector{Location(3_u)}),
                          });
    Structure("VertexOut", utils::Vector{
                               Member("pos", ty.vec4<f32>(),
                                      utils::Vector{Builtin(ast::BuiltinValue::kPosition)}),
                               Member("f0", ty.f32(), utils::Vector{Location(0_u)}),
                               Member("f1", ty.f32(), utils::Vector{Location(1_u)}),
                           });

    Func("main", utils::Vector{Param("shader_io", ty.type_name("VertexIn"), utils::Empty)},
         ty.type_name("VertexOut"),
         utils::Vector{
             Decl(Var("out", ty.type_name("VertexOut"))),
             Return("out"),
         },
         utils::Vector{
             Stage(ast::PipelineStage::kVertex),
         });
    GeneratorImpl& gen = Build();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_EQ(gen.result(), R"(struct VertexIn {
  float f1 : TEXCOORD1;
  float f3 : TEXCOORD3;
};
struct VertexOut {
  float4 pos : SV_Position;
  float f0 : TEXCOORD0;
  float f1 : TEXCOORD1;
};

VertexOut main(VertexIn shader_io) {
  VertexOut out = (VertexOut)0;
  return out;
}
)");
}

TEST_F(HlslGeneratorImplTest_InterstageVarTest, FragmentOutputUnchanged) {
    Structure("FragmentIn", utils::Vector{
                                Member("f0", ty.f32(), utils::Vector{Location(0_u)}),
                                Member("f1", ty.f32(), utils::Vector{Location(1_u)}),
                            });
    Structure("FragmentOut", utils::Vector{
                                 Member("f1", ty.f32(), utils::Vector{Location(1_u)}),
                                 Member("f3", ty.f32(), utils::Vector{Location(3_u)}),
                             });

    Func("main", utils::Vector{Param("shader_io", ty.type_name("FragmentIn"), utils::Empty)},
         ty.type_name("FragmentOut"),
         utils::Vector{
             Decl(Var("out", ty.type_name("FragmentOut"))),
             Return("out"),
         },
         utils::Vector{
             Stage(ast::PipelineStage::kFragment),
         });
    GeneratorImpl& gen = Build();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_EQ(gen.result(), R"(struct FragmentIn {
  float f0 : TEXCOORD0;
  float f1 : TEXCOORD1;
};
struct FragmentOut {
  float f1 : SV_Target1;
  float f3 : SV_Target3;
};

FragmentOut main(FragmentIn shader_io) {
  FragmentOut out = (FragmentOut)0;
  return out;
}
)");
}

// TEST_F(HlslGeneratorImplTest_InterstageVarTest, OutsideOfStruct) {

//     utils::Vector members{
//         Member("f1", ty.f32(), utils::Vector{ Location(1_u) }),
//         Member("f3", ty.f32(), utils::Vector{ Location(3_u) }),
//     };

//     Structure("in_struct", members);

//     Func(
//         "main",
//         // utils::Empty,
//         utils::Vector{
//             Param("shader_io", ty.type_name("in_struct"), utils::Empty),
//             Param("f0", ty.f32(), utils::Vector{ Location(0_u) }),
//         },
//         ty.void_(),
//         utils::Vector{
//             // Assign(Phony(), 1.2_f),
//             Return(),
//         },
//          utils::Vector{
//              Stage(ast::PipelineStage::kFragment),
//          });
//     GeneratorImpl& gen = Build();

//     ASSERT_TRUE(gen.Generate()) << gen.error();
//     EXPECT_EQ(gen.result(), R"()");
// }

// TEST_F(HlslGeneratorImplTest_InterstageVarTest, Params) {

//     Func(
//         "main",
//         // utils::Empty,
//         utils::Vector{
//             Param("f1", ty.f32(), utils::Vector{ Location(1_u) }),
//             Param("f3", ty.f32(), utils::Vector{ Location(3_u) }),
//         },
//         ty.void_(),
//         utils::Vector{
//             // Assign(Phony(), 1.2_f),
//             Return(),
//         },
//          utils::Vector{
//              Stage(ast::PipelineStage::kFragment),
//          });
//     GeneratorImpl& gen = Build();

//     ASSERT_TRUE(gen.Generate()) << gen.error();
//     EXPECT_EQ(gen.result(), R"()");
// }

// struct attributes + param

}  // namespace
}  // namespace tint::writer::hlsl
