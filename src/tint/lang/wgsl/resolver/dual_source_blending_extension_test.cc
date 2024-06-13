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

#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/wgsl/resolver/resolver.h"
#include "src/tint/lang/wgsl/resolver/resolver_helper_test.h"

#include "gmock/gmock.h"

using namespace tint::core::number_suffixes;  // NOLINT
using namespace tint::core::fluent_types;     // NOLINT

namespace tint::resolver {
namespace {

using DualSourceBlendingExtensionTest = ResolverTest;

// Using the @blend_src attribute without dual_source_blending enabled should fail.
TEST_F(DualSourceBlendingExtensionTest, UseBlendSrcAttribWithoutExtensionError) {
    Structure("Output", Vector{
                            Member("a", ty.vec4<f32>(),
                                   Vector{Location(0_a), BlendSrc(Source{{12, 34}}, 0_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(
        r()->error(),
        R"(12:34 error: use of '@blend_src' requires enabling extension 'dual_source_blending')");
}

class DualSourceBlendingExtensionTests : public ResolverTest {
  public:
    DualSourceBlendingExtensionTests() { Enable(wgsl::Extension::kDualSourceBlending); }
};

// Using an F32 as a @blend_src value should fail.
TEST_F(DualSourceBlendingExtensionTests, BlendSrcF32Error) {
    Structure("Output", Vector{
                            Member(Source{{12, 34}}, "a", ty.vec4<f32>(),
                                   Vector{Location(0_a), BlendSrc(Source{{12, 34}}, 0_f)}),
                            Member("b", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@blend_src' value must be 'i32' or 'u32'");
}

// Using a floating point number as a @blend_src value should fail.
TEST_F(DualSourceBlendingExtensionTests, BlendSrcFloatValueError) {
    Structure("Output", Vector{
                            Member(Source{{12, 34}}, "a", ty.vec4<f32>(),
                                   Vector{Location(0_a), BlendSrc(Source{{12, 34}}, 1.0_a)}),
                            Member("b", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                        });
    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@blend_src' value must be 'i32' or 'u32'");
}

// Using a number less than zero as a @blend_src value should fail.
TEST_F(DualSourceBlendingExtensionTests, BlendSrcNegativeValue) {
    Structure("Output", Vector{
                            Member(Source{{12, 34}}, "a", ty.vec4<f32>(),
                                   Vector{Location(0_a), BlendSrc(Source{{12, 34}}, -1_a)}),
                            Member("b", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@blend_src' value must be zero or one");
}

// Using a number greater than one as a @blend_src value should fail.
TEST_F(DualSourceBlendingExtensionTests, BlendSrcValueAboveOne) {
    Structure("Output", Vector{
                            Member(Source{{12, 34}}, "a", ty.vec4<f32>(),
                                   Vector{Location(0_a), BlendSrc(Source{{12, 34}}, 2_a)}),
                            Member("b", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@blend_src' value must be zero or one");
}

// Using a @blend_src value at the same location multiple times should fail.
TEST_F(DualSourceBlendingExtensionTests, DuplicateBlendSrces) {
    Structure("Output", Vector{
                            Member("a", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(0_a)}),
                            Member(Source{{12, 34}}, "b", ty.vec4<f32>(),
                                   Vector{Location(Source{{12, 34}}, 0_a), BlendSrc(0_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@location(0) @blend_src(0)' appears multiple times");
}

// Using the @blend_src attribute without a location attribute should fail.
TEST_F(DualSourceBlendingExtensionTests, BlendSrcWithMissingLocationAttribute_Struct) {
    Structure("Output", Vector{
                            Member(Source{{12, 34}}, "a", ty.vec4<f32>(),
                                   Vector{BlendSrc(Source{{12, 34}}, 1_a)}),
                            Member("b", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@blend_src' can only be used with '@location(0)'");
}

// Using a @blend_src attribute on a struct member while there is only one member in the struct
// should fail.
TEST_F(DualSourceBlendingExtensionTests, StructMemberBlendSrcAttribute_OnlyBlendSrc0) {
    Structure("Output", Vector{
                            Member("a", ty.vec4<f32>(),
                                   Vector{Location(0_a), BlendSrc(Source{{12, 34}}, 0_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@blend_src(1)' is missing when '@blend_src' is used");
}

// Using a @blend_src attribute on a struct member while there is only one member in the struct
// should fail.
TEST_F(DualSourceBlendingExtensionTests, StructMemberBlendSrcAttribute_OnlyBlendSrc1) {
    Structure("Output", Vector{
                            Member("a", ty.vec4<f32>(),
                                   Vector{Location(0_a), BlendSrc(Source{{12, 34}}, 1_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@blend_src(0)' is missing when '@blend_src' is used");
}

// Using a @blend_src attribute on a struct member while there is another member that doesn't have
// @blend_src attribute in the struct should fail.
TEST_F(DualSourceBlendingExtensionTests, StructMemberBlendSrcAttribute_LastMemberNoBlendSrc) {
    Structure("Output", Vector{
                            Member("a", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(0_a)}),
                            Member("b", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                            Member(Source{{12, 34}}, "c", ty.vec4<f32>()),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(),
              "12:34 error: '@blend_src' is used on one member while another member doesn't use "
              "'@blend_src'");
}

// Using a @blend_src attribute on a struct member while there is another member that doesn't have
// @blend_src attribute in the struct should fail.
TEST_F(DualSourceBlendingExtensionTests, StructMemberBlendSrcAttribute_FirstMemberNoBlendSrc) {
    Structure("Output", Vector{
                            Member("a", ty.vec4<f32>()),
                            Member(Source{{12, 34}}, "b", ty.vec4<f32>(),
                                   Vector{Location(0_a), BlendSrc(0_a)}),
                            Member("c", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(),
              "12:34 error: '@blend_src' is used on one member while another member doesn't use "
              "'@blend_src'");
}

// Using a @blend_src attribute on a global variable should pass internally with
// `DisabledValidation::kIgnoreAddressSpace`. This is needed internally when using @blend_src with
// the canonicalize_entry_point transform. This test uses an internal attribute to ignore address
// space, which is how it is used with the canonicalize_entry_point transform.
TEST_F(DualSourceBlendingExtensionTests, GlobalVariableBlendSrcAttributeAfterInternalTransform) {
    GlobalVar(
        "var", ty.vec4<f32>(),
        Vector{Location(0_a), BlendSrc(0_a), Disable(ast::DisabledValidation::kIgnoreAddressSpace)},
        core::AddressSpace::kOut);

    EXPECT_TRUE(r()->Resolve()) << r()->error();
}

// Using the @blend_src attribute with a non-zero location should fail.
TEST_F(DualSourceBlendingExtensionTests, BlendSrcWithNonZeroLocation_Struct) {
    Structure("Output", Vector{
                            Member("a", ty.vec4<f32>(),
                                   Vector{Location(1_a), BlendSrc(Source{{12, 34}}, 0_a)}),
                            Member("b", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                        });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), "12:34 error: '@blend_src' can only be used with '@location(0)'");
}

TEST_F(DualSourceBlendingExtensionTests, MixedBlendSrcAndNonBlendSrcOnLocationZero) {
    // struct S {
    //   @location(0) a : vec4<f32>,
    //   @location(0) @blend_src(1) b : vec4<f32>,
    // };
    Structure("S", Vector{
                       Member("a", ty.vec4<f32>(),
                              Vector{
                                  Location(Source{{45, 56}}, 0_a),
                              }),
                       Member(Source{{12, 34}}, "b", ty.vec4<f32>(),
                              Vector{Location(0_a), BlendSrc(Source{{67, 78}}, 1_a)}),
                   });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(),
              "12:34 error: '@blend_src' is used on one member while another member doesn't use "
              "'@blend_src'");
}

TEST_F(DualSourceBlendingExtensionTests, BlendSrcTypes_DifferentWidth) {
    // struct S {
    //   @location(0) @blend_src(0) a : vec4<f32>,
    //   @location(0) @blend_src(1) b : vec2<f32>,
    // };
    // @fragment fn F() -> S { return S(); }
    Structure("S",
              Vector{
                  Member("a", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(0_a)}),
                  Member("b", ty.vec2<f32>(), Vector{Location(0_a), BlendSrc(Source{{1, 2}}, 1_a)}),
              });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(),
              R"(1:2 error: Use of '@blend_src' requires all outputs have same type)");
}

TEST_F(DualSourceBlendingExtensionTests, BlendSrcTypes_DifferentElementType) {
    // struct S {
    //   @location(0) @blend_src(0) a : vec4<f32>,
    //   @location(0) @blend_src(1) b : vec4<i32>,
    // };
    // @fragment fn F() -> S { return S(); }
    Structure("S",
              Vector{
                  Member("a", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(0_a)}),
                  Member("b", ty.vec4<i32>(), Vector{Location(0_a), BlendSrc(Source{{1, 2}}, 1_a)}),
              });

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(),
              R"(1:2 error: Use of '@blend_src' requires all outputs have same type)");
}

TEST_F(DualSourceBlendingExtensionTests, BlendSrcAsFragmentInput) {
    // struct S {
    //   @location(0) @blend_src(0) a : vec4<f32>,
    //   @location(0) @blend_src(1) b : vec4<f32>,
    // };
    // @fragment fn F(s_in : S) -> S { return S(); }
    Structure("S", Vector{
                       Member("a", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(0_a)}),
                       Member("b", ty.vec4<f32>(), Vector{Location(0_a), BlendSrc(1_a)}),
                   });
    Func("F", Vector{Param("s_in", ty("S"))}, ty("S"), Vector{Return(Call("S"))},
         Vector{Stage(ast::PipelineStage::kFragment)});

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), R"(error: '@blend_src' can only be used for fragment shader output
note: while analyzing entry point 'F')");
}

TEST_F(DualSourceBlendingExtensionTest, BlendSrcOnNonStructFragmentOutput) {
    // enable dual_source_blending;
    // @fragment fn F() -> @location(0) @blend_src(0) vec4<f32> {
    //     return vec4<f32>();
    // }
    Enable(wgsl::Extension::kDualSourceBlending);
    Func("F", Empty, ty.vec4<f32>(), Vector{Return(Call("vec4f"))},
         Vector{Stage(ast::PipelineStage::kFragment)},
         Vector{Location(0_a), BlendSrc(Source{{1, 2}}, 0_a)});

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), R"(1:2 error: '@blend_src' is not valid for entry point return types)");
}

}  // namespace
}  // namespace tint::resolver
