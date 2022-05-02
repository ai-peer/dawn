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

#include "src/tint/resolver/uniformity.h"
#include "src/tint/program_builder.h"
#include "src/tint/reader/wgsl/parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace tint::resolver {
namespace {

class UniformityAnalysisTestBase {
  protected:
    /// Parse and resolve a WGSL shader.
    /// @param src the WGSL source code
    /// @param should_pass true if `src` should pass the analysis, otherwise false
    /// @returns true on success, false on failure
    void RunTest(std::string src, bool should_pass) {
        auto file = std::make_unique<Source::File>("test", src);
        auto program = reader::wgsl::Parse(file.get());

        diag::Formatter::Style style;
        style.print_newline_at_end = false;
        error_ = diag::Formatter(style).format(program.Diagnostics());

        bool valid = program.IsValid();
        if (should_pass) {
            EXPECT_TRUE(valid) << error_;
            if (program.Diagnostics().count() == 1u) {
                EXPECT_THAT(program.Diagnostics().str(), ::testing::HasSubstr("unreachable"));
            } else {
                EXPECT_EQ(program.Diagnostics().count(), 0u) << error_;
            }
        } else {
            // TODO(jrprice): expect false when uniformity issues become errors.
            EXPECT_TRUE(valid) << error_;
        }
    }

    /// Build and resolve a program from a ProgramBuilder object.
    /// @param builder the program builder
    /// @returns true on success, false on failure
    bool RunTest(ProgramBuilder&& builder) {
        auto program = Program(std::move(builder));

        diag::Formatter::Style style;
        style.print_newline_at_end = false;
        error_ = diag::Formatter(style).format(program.Diagnostics());

        return program.IsValid();
    }

    /// The error message from the parser or resolver, if any.
    std::string error_;
};

class UniformityAnalysisTest : public UniformityAnalysisTestBase, public ::testing::Test {};

class BasicTest : public UniformityAnalysisTestBase,
                  public ::testing::TestWithParam<std::tuple<int, int>> {
  public:
    /// Enum for the if-statement condition guarding a function call.
    enum Condition {
        // Uniform conditions:
        kTrue,
        kFalse,
        kLiteral,
        kModuleLet,
        kPipelineOverridable,
        kFuncLetUniformRhs,
        kFuncVarUniform,
        kFuncUniformRetVal,
        kUniformBuffer,
        kROStorageBuffer,
        kLastUniformCondition = kROStorageBuffer,
        // MayBeNonUniform conditions:
        kFuncLetNonUniformRhs,
        kFuncVarNonUniform,
        kFuncNonUniformRetVal,
        kRWStorageBuffer,
        // End of range marker:
        kEndOfConditionRange,
    };

    /// Enum for the function call statement.
    enum Function {
        // NoRestrictionFunctions:
        kUserNoRestriction,
        kMin,
        kTextureSampleLevel,
        kLastNoRestrictionFunction = kTextureSampleLevel,
        // RequiredToBeUniform functions:
        kUserRequiredToBeUniform,
        kWorkgroupBarrier,
        kStorageBarrier,
        kTextureSample,
        kTextureSampleBias,
        kTextureSampleCompare,
        kDpdx,
        kDpdxCoarse,
        kDpdxFine,
        kDpdy,
        kDpdyCoarse,
        kDpdyFine,
        kFwidth,
        kFwidthCoarse,
        kFwidthFine,
        // End of range marker:
        kEndOfFunctionRange,
    };

    /// Convert a condition to its string representation.
    static std::string ConditionToStr(Condition c) {
        switch (c) {
            case kTrue:
                return "true";
            case kFalse:
                return "false";
            case kLiteral:
                return "7 == 7";
            case kModuleLet:
                return "module_let == 0";
            case kPipelineOverridable:
                return "pipeline_overridable == 0";
            case kFuncLetUniformRhs:
                return "let_uniform_rhs == 0";
            case kFuncVarUniform:
                return "func_uniform == 0";
            case kFuncUniformRetVal:
                return "func_uniform_retval() == 0";
            case kUniformBuffer:
                return "u == 0";
            case kROStorageBuffer:
                return "ro == 0";
            case kFuncLetNonUniformRhs:
                return "let_nonuniform_rhs == 0";
            case kFuncVarNonUniform:
                return "func_non_uniform == 0";
            case kFuncNonUniformRetVal:
                return "func_nonuniform_retval() == 0";
            case kRWStorageBuffer:
                return "rw == 0";
            case kEndOfConditionRange:
                return "<invalid>";
        }
    }

    /// Convert a function call to its string representation.
    static std::string FunctionToStr(Function f) {
        switch (f) {
            case kUserNoRestriction:
                return "user_no_restriction()";
            case kMin:
                return "min(1, 1)";
            case kTextureSampleLevel:
                return "textureSampleLevel(t, s, vec2(0.5, 0.5), 0.0)";
            case kUserRequiredToBeUniform:
                return "user_required_to_be_uniform()";
            case kWorkgroupBarrier:
                return "workgroupBarrier()";
            case kStorageBarrier:
                return "storageBarrier()";
            case kTextureSample:
                return "textureSample(t, s, vec2(0.5, 0.5))";
            case kTextureSampleBias:
                return "textureSampleBias(t, s, vec2(0.5, 0.5), 2.0)";
            case kTextureSampleCompare:
                return "textureSampleCompare(td, sc, vec2(0.5, 0.5), 0.5)";
            case kDpdx:
                return "dpdx(1.0)";
            case kDpdxCoarse:
                return "dpdxCoarse(1.0)";
            case kDpdxFine:
                return "dpdxFine(1.0)";
            case kDpdy:
                return "dpdy(1.0)";
            case kDpdyCoarse:
                return "dpdyCoarse(1.0)";
            case kDpdyFine:
                return "dpdyFine(1.0)";
            case kFwidth:
                return "fwidth(1.0)";
            case kFwidthCoarse:
                return "fwidthCoarse(1.0)";
            case kFwidthFine:
                return "fwidthFine(1.0)";
            case kEndOfFunctionRange:
                return "<invalid>";
        }
    }

    /// @returns true if `c` is a condition that may be non-uniform.
    static bool MayBeNonUniform(Condition c) { return c > kLastUniformCondition; }

    /// @returns true if `f` is a function call that is required to be uniform.
    static bool RequiredToBeUniform(Function f) { return f > kLastNoRestrictionFunction; }

    /// Convert a test parameter pair of condition+function to a string that can be used as part of
    /// a test name.
    static std::string ParamsToName(::testing::TestParamInfo<ParamType> params) {
        Condition c = static_cast<Condition>(std::get<0>(params.param));
        Function f = static_cast<Function>(std::get<1>(params.param));
        std::string name;
#define CASE(c)     \
    case c:         \
        name += #c; \
        break

        switch (c) {
            CASE(kTrue);
            CASE(kFalse);
            CASE(kLiteral);
            CASE(kModuleLet);
            CASE(kPipelineOverridable);
            CASE(kFuncLetUniformRhs);
            CASE(kFuncVarUniform);
            CASE(kFuncUniformRetVal);
            CASE(kUniformBuffer);
            CASE(kROStorageBuffer);
            CASE(kFuncLetNonUniformRhs);
            CASE(kFuncVarNonUniform);
            CASE(kFuncNonUniformRetVal);
            CASE(kRWStorageBuffer);
            case kEndOfConditionRange:
                break;
        }
        name += "_";
        switch (f) {
            CASE(kUserNoRestriction);
            CASE(kMin);
            CASE(kTextureSampleLevel);
            CASE(kUserRequiredToBeUniform);
            CASE(kWorkgroupBarrier);
            CASE(kStorageBarrier);
            CASE(kTextureSample);
            CASE(kTextureSampleBias);
            CASE(kTextureSampleCompare);
            CASE(kDpdx);
            CASE(kDpdxCoarse);
            CASE(kDpdxFine);
            CASE(kDpdy);
            CASE(kDpdyCoarse);
            CASE(kDpdyFine);
            CASE(kFwidth);
            CASE(kFwidthCoarse);
            CASE(kFwidthFine);
            case kEndOfFunctionRange:
                break;
        }
#undef CASE

        return name;
    }
};

// Test the uniformity constraints for a function call inside a conditional statement.
TEST_P(BasicTest, ConditionalFunctionCall) {
    auto condition = static_cast<Condition>(std::get<0>(GetParam()));
    auto function = static_cast<Function>(std::get<1>(GetParam()));
    auto src = R"(
var<private> p : i32;
var<workgroup> w : i32;
@group(0) @binding(0) var<uniform> u : i32;
@group(0) @binding(0) var<storage, read> ro : i32;
@group(0) @binding(0) var<storage, read_write> rw : i32;

@group(1) @binding(0) var t : texture_2d<f32>;
@group(1) @binding(1) var td : texture_depth_2d;
@group(1) @binding(2) var s : sampler;
@group(1) @binding(3) var sc : sampler_comparison;

let module_let : i32 = 42;
@id(42) override pipeline_overridable : i32;

fn user_no_restriction() {}
fn user_required_to_be_uniform() { workgroupBarrier(); }

fn func_uniform_retval() -> i32 { return u; }
fn func_nonuniform_retval() -> i32 { return rw; }

fn foo() {
  let let_uniform_rhs = 7;
  let let_nonuniform_rhs = rw;

  var func_uniform = 7;
  var func_non_uniform = 7;
  func_non_uniform = rw;

  if ()" + ConditionToStr(condition) +
               R"() {
    )" + FunctionToStr(function) +
               R"(;
  }
}
)";

    bool should_pass = !(MayBeNonUniform(condition) && RequiredToBeUniform(function));
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_THAT(error_, ::testing::StartsWith("test:31:5 warning: "));
        EXPECT_THAT(error_, ::testing::HasSubstr("must only be called from uniform control flow"));
    }
}

INSTANTIATE_TEST_SUITE_P(
    UniformityAnalysisTest,
    BasicTest,
    ::testing::Combine(::testing::Range<int>(0, BasicTest::kEndOfConditionRange),
                       ::testing::Range<int>(0, BasicTest::kEndOfFunctionRange)),
    BasicTest::ParamsToName);

////////////////////////////////////////////////////////////////////////////////
/// Test specific function and parameter tags that are not tested above.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, SubsequentControlFlowMayBeNonUniform_Pass) {
    // Call a function that causes subsequent control flow to be non-uniform, and then call another
    // function that doesn't require uniformity.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn foo() {
  if (rw == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn bar() {
  if (p == 42) {
    p = 7;
  }
}

fn main() {
  foo();
  bar();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, SubsequentControlFlowMayBeNonUniform_Fail) {
    // Call a function that causes subsequent control flow to be non-uniform, and then call another
    // function that requires uniformity.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn foo() {
  if (rw == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn main() {
  foo();
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:17:3 warning: workgroupBarrier must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ParameterNoRestriction_Pass) {
    // Pass a non-uniform value as an argument, and then try to use the return value for
    // control-flow guarding a barrier.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn foo(i : i32) -> i32 {
  if (i == 0) {
    // This assignment is non-uniform, but shouldn't affect the return value.
    p = 42;
  }
  return 7;
}

fn bar() {
  let x = foo(rw);
  if (x == 7) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniform_Pass) {
    // Pass a uniform value as an argument to a function that uses that parameter for control-flow
    // guarding a barrier.
    auto src = R"(
@group(0) @binding(0) var<storage, read> ro : i32;

fn foo(i : i32) {
  if (i == 0) {
    workgroupBarrier();
  }
}

fn bar() {
  foo(ro);
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniform_Fail) {
    // Pass a non-uniform value as an argument to a function that uses that parameter for
    // control-flow guarding a barrier.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo(i : i32) {
  if (i == 0) {
    workgroupBarrier();
  }
}

fn bar() {
  foo(rw);
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:7 warning: parameter 'i' of foo must be uniform
  foo(rw);
      ^^
)");
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniformForReturnValue_Pass) {
    // Pass a uniform value as an argument to a function that uses that parameter to produce the
    // return value, and then use the return value for control-flow guarding a barrier.
    auto src = R"(
@group(0) @binding(0) var<storage, read> ro : i32;

fn foo(i : i32) -> i32 {
  return 1 + i;
}

fn bar() {
  if (foo(ro) == 7) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniformForReturnValue_Fail) {
    // Pass a non-uniform value as an argument to a function that uses that parameter to produce the
    // return value, and then use the return value for control-flow guarding a barrier.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo(i : i32) -> i32 {
  return 1 + i;
}

fn bar() {
  if (foo(rw) == 7) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniformForSubsequentControlFlow_Pass) {
    // Pass a uniform value as an argument to a function that uses that parameter return early, and
    // then invoke a barrier after calling that function.
    auto src = R"(
@group(0) @binding(0) var<storage, read> ro : i32;

var<private> p : i32;

fn foo(i : i32) {
  if (i == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn bar() {
  foo(ro);
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniformForSubsequentControlFlow_Fail) {
    // Pass a non-uniform value as an argument to a function that uses that parameter return early,
    // and then invoke a barrier after calling that function.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn foo(i : i32) {
  if (i == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn bar() {
  foo(rw);
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:17:3 warning: workgroupBarrier must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Test shader IO attributes.
////////////////////////////////////////////////////////////////////////////////

struct BuiltinEntry {
    std::string name;
    std::string type;
    bool uniform;
    BuiltinEntry(std::string n, std::string t, bool u) : name(n), type(t), uniform(u) {}
};

class ComputeBuiltin : public UniformityAnalysisTestBase,
                       public ::testing::TestWithParam<BuiltinEntry> {};
TEST_P(ComputeBuiltin, AsParam) {
    auto src = R"(
@stage(compute) @workgroup_size(64)
fn main(@builtin()" +
               GetParam().name + R"() b : )" + GetParam().type + R"() {
  if (all(vec3(b) == vec3(0u))) {
    workgroupBarrier();
  }
}
)";

    bool should_pass = GetParam().uniform;
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_EQ(
            error_,
            R"(test:5:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
    }
}

TEST_P(ComputeBuiltin, InStruct) {
    auto src = R"(
struct S {
  @builtin()" + GetParam().name +
               R"() b : )" + GetParam().type + R"(
}

@stage(compute) @workgroup_size(64)
fn main(s : S) {
  if (all(vec3(s.b) == vec3(0u))) {
    workgroupBarrier();
  }
}
)";

    bool should_pass = GetParam().uniform;
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_EQ(
            error_,
            R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
    }
}

INSTANTIATE_TEST_SUITE_P(UniformityAnalysisTest,
                         ComputeBuiltin,
                         ::testing::Values(BuiltinEntry{"local_invocation_id", "vec3<u32>", false},
                                           BuiltinEntry{"local_invocation_index", "u32", false},
                                           BuiltinEntry{"global_invocation_id", "vec3<u32>", false},
                                           BuiltinEntry{"workgroup_id", "vec3<u32>", true},
                                           BuiltinEntry{"num_workgroups", "vec3<u32>", true}),
                         [](const ::testing::TestParamInfo<ComputeBuiltin::ParamType>& info) {
                             return info.param.name;
                         });

TEST_F(UniformityAnalysisTest, ComputeBuiltin_MixedAttributesInStruct) {
    // Mix both non-uniform and uniform shader IO attributes in the same structure. Even accessing
    // just uniform member causes non-uniformity in this case.
    auto src = R"(
struct S {
  @builtin(num_workgroups) num_groups : vec3<u32>,
  @builtin(local_invocation_index) idx : u32,
}

@stage(compute) @workgroup_size(64)
fn main(s : S) {
  if (s.num_groups.x == 0u) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

class FragmentBuiltin : public UniformityAnalysisTestBase,
                        public ::testing::TestWithParam<BuiltinEntry> {};
TEST_P(FragmentBuiltin, AsParam) {
    auto src = R"(
@stage(fragment)
fn main(@builtin()" +
               GetParam().name + R"() b : )" + GetParam().type + R"() {
  if (u32(vec4(b).x) == 0u) {
    dpdx(0.5);
  }
}
)";

    bool should_pass = GetParam().uniform;
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_EQ(error_,
                  R"(test:5:5 warning: dpdx must only be called from uniform control flow
    dpdx(0.5);
    ^^^^
)");
    }
}

TEST_P(FragmentBuiltin, InStruct) {
    auto src = R"(
struct S {
  @builtin()" + GetParam().name +
               R"() b : )" + GetParam().type + R"(
}

@stage(fragment)
fn main(s : S) {
  if (u32(vec4(s.b).x) == 0u) {
    dpdx(0.5);
  }
}
)";

    bool should_pass = GetParam().uniform;
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_EQ(error_,
                  R"(test:9:5 warning: dpdx must only be called from uniform control flow
    dpdx(0.5);
    ^^^^
)");
    }
}

INSTANTIATE_TEST_SUITE_P(UniformityAnalysisTest,
                         FragmentBuiltin,
                         ::testing::Values(BuiltinEntry{"position", "vec4<f32>", false},
                                           BuiltinEntry{"front_facing", "bool", false},
                                           BuiltinEntry{"sample_index", "u32", false},
                                           BuiltinEntry{"sample_mask", "u32", false}),
                         [](const ::testing::TestParamInfo<FragmentBuiltin::ParamType>& info) {
                             return info.param.name;
                         });

TEST_F(UniformityAnalysisTest, FragmentLocation) {
    auto src = R"(
@stage(fragment)
fn main(@location(0) l : f32) {
  if (l == 0.0) {
    dpdx(0.5);
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:5:5 warning: dpdx must only be called from uniform control flow
    dpdx(0.5);
    ^^^^
)");
}

TEST_F(UniformityAnalysisTest, FragmentLocation_InStruct) {
    auto src = R"(
struct S {
  @location(0) l : f32
}

@stage(fragment)
fn main(s : S) {
  if (s.l == 0.0) {
    dpdx(0.5);
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: dpdx must only be called from uniform control flow
    dpdx(0.5);
    ^^^^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Test loop conditions and conditional break statements.
////////////////////////////////////////////////////////////////////////////////

namespace LoopTest {

enum ControlFlowInterrupt {
    kBreak,
    kContinue,
    kReturn,
    kDiscard,
};
enum Condition {
    kNone,
    kUniform,
    kNonUniform,
};

using LoopTestParams = std::tuple<int, int>;

std::string ToStr(ControlFlowInterrupt interrupt) {
    switch (interrupt) {
        case kBreak:
            return "break";
        case kContinue:
            return "continue";
        case kReturn:
            return "return";
        case kDiscard:
            return "discard";
    }
    return "";
}

std::string ToStr(Condition condition) {
    switch (condition) {
        case kNone:
            return "uncondtiional";
        case kUniform:
            return "uniform";
        case kNonUniform:
            return "nonuniform";
    }
    return "";
}

class LoopTest : public UniformityAnalysisTestBase,
                 public ::testing::TestWithParam<LoopTestParams> {
  protected:
    std::string MakeInterrupt(ControlFlowInterrupt interrupt, Condition condition) {
        switch (condition) {
            case kNone:
                return ToStr(interrupt);
            case kUniform:
                return "if (uniform_var == 42) { " + ToStr(interrupt) + "; }";
            case kNonUniform:
                return "if (nonuniform_var == 42) { " + ToStr(interrupt) + "; }";
        }
    }
};

INSTANTIATE_TEST_SUITE_P(UniformityAnalysisTest,
                         LoopTest,
                         ::testing::Combine(::testing::Range<int>(0, kDiscard + 1),
                                            ::testing::Range<int>(0, kNonUniform + 1)),
                         [](const ::testing::TestParamInfo<LoopTestParams>& info) {
                             ControlFlowInterrupt interrupt =
                                 static_cast<ControlFlowInterrupt>(std::get<0>(info.param));
                             auto condition = static_cast<Condition>(std::get<1>(info.param));
                             return ToStr(interrupt) + "_" + ToStr(condition);
                         });

TEST_P(LoopTest, CallInBody_InterruptAfter) {
    // Test control-flow interrupt in a loop after a function call that requires uniform control
    // flow.
    auto interrupt = static_cast<ControlFlowInterrupt>(std::get<0>(GetParam()));
    auto condition = static_cast<Condition>(std::get<1>(GetParam()));
    auto src = R"(
@group(0) @binding(0) var<storage, read> uniform_var : i32;
@group(0) @binding(0) var<storage, read_write> nonuniform_var : i32;

fn foo() {
  loop {
    // Pretend that this isn't an infinite loop, in case the interrupt is a
    // continue statement.
    if (false) {
      break;
    }

    workgroupBarrier();
    )" + MakeInterrupt(interrupt, condition) +
               R"(;
  }
}
)";

    if (condition == kNonUniform) {
        RunTest(src, false);
        EXPECT_EQ(
            error_,
            R"(test:13:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
    } else {
        RunTest(src, true);
    }
}

TEST_P(LoopTest, CallInBody_InterruptBefore) {
    // Test control-flow interrupt in a loop before a function call that requires uniform control
    // flow.
    auto interrupt = static_cast<ControlFlowInterrupt>(std::get<0>(GetParam()));
    auto condition = static_cast<Condition>(std::get<1>(GetParam()));
    auto src = R"(
@group(0) @binding(0) var<storage, read> uniform_var : i32;
@group(0) @binding(0) var<storage, read_write> nonuniform_var : i32;

fn foo() {
  loop {
    // Pretend that this isn't an infinite loop, in case the interrupt is a
    // continue statement.
    if (false) {
      break;
    }

    )" + MakeInterrupt(interrupt, condition) +
               R"(;
    workgroupBarrier();
  }
}
)";

    if (condition == kNonUniform) {
        RunTest(src, false);
        EXPECT_EQ(
            error_,
            R"(test:14:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
    } else {
        RunTest(src, true);
    }
}

TEST_P(LoopTest, CallInContinuing_InterruptInBody) {
    // Test control-flow interrupt in a loop with a function call that requires uniform control flow
    // in the continuing statement.
    auto interrupt = static_cast<ControlFlowInterrupt>(std::get<0>(GetParam()));
    auto condition = static_cast<Condition>(std::get<1>(GetParam()));
    auto src = R"(
@group(0) @binding(0) var<storage, read> uniform_var : i32;
@group(0) @binding(0) var<storage, read_write> nonuniform_var : i32;

fn foo() {
  loop {
    // Pretend that this isn't an infinite loop, in case the interrupt is a
    // continue statement.
    if (false) {
      break;
    }

    )" + MakeInterrupt(interrupt, condition) +
               R"(;
    continuing {
      workgroupBarrier();
    }
  }
}
)";

    if (condition == kNonUniform) {
        RunTest(src, false);
        EXPECT_EQ(
            error_,
            R"(test:15:7 warning: workgroupBarrier must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^
)");
    } else {
        RunTest(src, true);
    }
}

TEST_F(UniformityAnalysisTest, Loop_CallInBody_UniformBreakInContinuing) {
    auto src = R"(
@group(0) @binding(0) var<storage, read> n : i32;

fn foo() {
  var i = 0;
  loop {
    workgroupBarrier();
    continuing {
      i = i + 1;
      if (i == n) {
        break;
      }
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_CallInBody_NonUniformBreakInContinuing) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  var i = 0;
  loop {
    workgroupBarrier();
    continuing {
      i = i + 1;
      if (i == n) {
        break;
      }
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Loop_CallInContinuing_UniformBreakInContinuing) {
    auto src = R"(
@group(0) @binding(0) var<storage, read> n : i32;

fn foo() {
  var i = 0;
  loop {
    continuing {
      workgroupBarrier();
      i = i + 1;
      if (i == n) {
        break;
      }
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_CallInContinuing_NonUniformBreakInContinuing) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  var i = 0;
  loop {
    continuing {
      workgroupBarrier();
      i = i + 1;
      if (i == n) {
        break;
      }
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: workgroupBarrier must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^
)");
}

class LoopDeadCodeTest : public UniformityAnalysisTestBase, public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_SUITE_P(UniformityAnalysisTest,
                         LoopDeadCodeTest,
                         ::testing::Range<int>(0, kDiscard + 1),
                         [](const ::testing::TestParamInfo<LoopDeadCodeTest::ParamType>& info) {
                             return ToStr(static_cast<ControlFlowInterrupt>(info.param));
                         });

TEST_P(LoopDeadCodeTest, AfterInterrupt) {
    // Dead code after a control-flow interrupt in a loop shouldn't cause uniformity errors.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  loop {
    )" + ToStr(static_cast<ControlFlowInterrupt>(GetParam())) +
               R"(;
    if (n == 42) {
      workgroupBarrier();
    }
    continuing {
      // Pretend that this isn't an infinite loop, in case the interrupt is a
      // continue statement.
      if (false) {
        break;
      }
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_NonUniformBreakInBody_Reconverge) {
    // Loops reconverge at exit, so test that we can call workgroupBarrier() after a loop that
    // contains a non-uniform conditional break.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  var i = 0;
  loop {
    if (i == n) {
      break;
    }
    i = i + 1;
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_NonUniformFunctionInBody_Reconverge) {
    // Loops reconverge at exit, so test that we can call workgroupBarrier() after a loop that
    // contains a call to a function that causes non-uniform control flow.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() {
  if (n == 42) {
    return;
  } else {
    return;
  }
}

fn foo() {
  loop {
    bar();
    break;
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_NonUniformFunctionDiscard_NoReconvergence) {
    // Loops should not reconverge after non-uniform discard statements.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() {
  if (n == 42) {
    discard;
  }
}

fn foo() {
  loop {
    bar();
    break;
  }
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:3 warning: workgroupBarrier must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_CallInside_UniformCondition) {
    auto src = R"(
@group(0) @binding(0) var<storage, read> n : i32;

fn foo() {
  for (var i = 0; i < n; i = i + 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ForLoop_CallInside_NonUniformCondition) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  for (var i = 0; i < n; i = i + 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_CallInside_InitializerCausesNonUniformFlow) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() -> i32 {
  if (n == 42) {
    return 1;
  } else {
    return 2;
  }
}

fn foo() {
  for (var i = bar(); i < 10; i = i + 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_CallInside_ContinuingCausesNonUniformFlow) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() -> i32 {
  if (n == 42) {
    return 1;
  } else {
    return 2;
  }
}

fn foo() {
  for (var i = 0; i < 10; i = i + bar()) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_NonUniformCondition_Reconverge) {
    // Loops reconverge at exit, so test that we can call workgroupBarrier() after a loop that has a
    // non-uniform condition.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  for (var i = 0; i < n; i = i + 1) {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

}  // namespace LoopTest

////////////////////////////////////////////////////////////////////////////////
/// If-else statement tests.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, IfElse_UniformCondition_BarrierInTrueBlock) {
    auto src = R"(
@group(0) @binding(0) var<storage, read> uniform_global : i32;

fn foo() {
  if (uniform_global == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_UniformCondition_BarrierInElseBlock) {
    auto src = R"(
@group(0) @binding(0) var<storage, read> uniform_global : i32;

fn foo() {
  if (uniform_global == 42) {
  } else {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_UniformCondition_BarrierInElseIfBlock) {
    auto src = R"(
@group(0) @binding(0) var<storage, read> uniform_global : i32;

fn foo() {
  if (uniform_global == 42) {
  } else if (true) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformCondition_BarrierInTrueBlock) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformCondition_BarrierInElseBlock) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
  } else {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformCondition_BarrierInElseIfBlock) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
  } else if (true) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformCondition_Reconverge) {
    // If statements reconverge at exit, so test that we can call workgroupBarrier() after an if
    // statement with a non-uniform condition.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
  } else {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_ShortCircuitingNonUniformConditionLHS_Reconverge) {
    // If statements reconverge at exit, so test that we can call workgroupBarrier() after an if
    // statement with a non-uniform condition that uses short-circuiting.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42 || false) {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_ShortCircuitingNonUniformConditionRHS_Reconverge) {
    // If statements reconverge at exit, so test that we can call workgroupBarrier() after an if
    // statement with a non-uniform condition that uses short-circuiting.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (true && non_uniform == 42) {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformFunctionCall_Reconverge) {
    // If statements reconverge at exit, so test that we can call workgroupBarrier() after an if
    // statement with a non-uniform condition.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar() {
  if (non_uniform == 42) {
    return;
  } else {
    return;
  }
}

fn foo() {
  if (non_uniform == 42) {
    bar();
  } else {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformDiscard_NoReconverge) {
    // If statements should not reconverge after non-uniform returns.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
    return;
  } else {
  }
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:3 warning: workgroupBarrier must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformReturn_NoReconverge) {
    // If statements should not reconverge after non-uniform discards.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
    discard;
  } else {
  }
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:3 warning: workgroupBarrier must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Switch statement tests.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, Switch_NonUniformCondition_BarrierInCase) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  switch (non_uniform) {
    case 42: {
      workgroupBarrier();
      break;
    }
    default: {
      break;
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:7 warning: workgroupBarrier must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformCondition_BarrierInDefault) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  switch (non_uniform) {
    default: {
      workgroupBarrier();
      break;
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:7 warning: workgroupBarrier must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformCondition_Reconverge) {
    // Switch statements reconverge at exit, so test that we can call workgroupBarrier() after a
    // switch statement that contains a non-uniform conditional break.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  switch (non_uniform) {
    default: {
      break;
    }
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformBreak_Reconverge) {
    // Switch statements reconverge at exit, so test that we can call workgroupBarrier() after a
    // switch statement that contains a non-uniform conditional break.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  switch (42) {
    default: {
      if (non_uniform == 0) {
        break;
      }
      break;
    }
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformFunctionCall_Reconverge) {
    // Switch statements reconverge at exit, so test that we can call workgroupBarrier() after a
    // switch statement that contains a call to a function that causes non-uniform control flow.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() {
  if (n == 42) {
    return;
  } else {
    return;
  }
}

fn foo() {
  switch (42) {
    default: {
      bar();
      break;
    }
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformFunctionDiscard_NoReconvergence) {
    // Switch statements should not reconverge after non-uniform discards.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() {
  if (n == 42) {
    discard;
  }
}

fn foo() {
  switch (42) {
    default: {
      bar();
      break;
    }
  }
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:17:3 warning: workgroupBarrier must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Pointer tests.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  *&v = non_uniform;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughCapturedPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv = &v;
  *pv = non_uniform;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  *&v = 42;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughCapturedPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  let pv = &v;
  *pv = 42;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughCapturedPointer_InNonUniformControlFlow) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv = &v;
  if (non_uniform == 0) {
    *pv = 42;
  }
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadNonUniformThroughPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  if (*&v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadNonUniformThroughCapturedPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  let pv = &v;
  if (*pv == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadUniformThroughPointer) {
    auto src = R"(
fn foo() {
  var v = 42;
  if (*&v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, LoadUniformThroughCapturedPointer) {
    auto src = R"(
fn foo() {
  var v = 42;
  let pv = &v;
  if (*pv == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, StoreNonUniformAfterCapturingPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv = &v;
  v = non_uniform;
  if (*pv == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StoreUniformAfterCapturingPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  let pv = &v;
  v = 42;
  if (*pv == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughLongChainOfPointers) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv1 = &*&v;
  let pv2 = &*&*pv1;
  *&*&*pv2 = non_uniform;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadNonUniformThroughLongChainOfPointers) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  let pv1 = &*&v;
  let pv2 = &*&*pv1;
  if (*&*&*pv2 == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThenNonUniformThroughDifferentPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv1 = &v;
  let pv2 = &v;
  *pv1 = 42;
  *pv2 = non_uniform;
  if (*pv1 == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThenUniformThroughDifferentPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv1 = &v;
  let pv2 = &v;
  *pv1 = non_uniform;
  *pv2 = 42;
  if (*pv1 == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, UnmodifiedPointerParameterNonUniform) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
}

fn foo() {
  var v = non_uniform;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, UnmodifiedPointerParameterUniform) {
    auto src = R"(
fn bar(p : ptr<function, i32>) {
}

fn foo() {
  var v = 42;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointerInFunctionCall) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  *p = non_uniform;
}

fn foo() {
  var v = 0;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointerInFunctionCall) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  *p = 42;
}

fn foo() {
  var v = non_uniform;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointerInFunctionCallViaArg) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>, a : i32) {
  *p = a;
}

fn foo() {
  var v = 0;
  bar(&v, non_uniform);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointerInFunctionCallViaPointerArg) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>, a : ptr<function, i32>) {
  *p = *a;
}

fn foo() {
  var v = 0;
  var a = non_uniform;
  bar(&v, &a);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:13:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointerInFunctionCallViaArg) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>, a : i32) {
  *p = a;
}

fn foo() {
  var v = non_uniform;
  bar(&v, 42);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointerInFunctionCallViaPointerArg) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>, a : ptr<function, i32>) {
  *p = *a;
}

fn foo() {
  var v = non_uniform;
  var a = 42;
  bar(&v, &a);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointerInFunctionCallChain) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn f3(p : ptr<function, i32>, a : ptr<function, i32>) {
  *p = *a;
}

fn f2(p : ptr<function, i32>, a : ptr<function, i32>) {
  f3(p, a);
}

fn f1(p : ptr<function, i32>, a : ptr<function, i32>) {
  f2(p, a);
}

fn foo() {
  var v = 0;
  var a = non_uniform;
  f1(&v, &a);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:21:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointerInFunctionCallChain) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn f3(p : ptr<function, i32>, a : ptr<function, i32>) {
  *p = *a;
}

fn f2(p : ptr<function, i32>, a : ptr<function, i32>) {
  f3(p, a);
}

fn f1(p : ptr<function, i32>, a : ptr<function, i32>) {
  f2(p, a);
}

fn foo() {
  var v = non_uniform;
  var a = 42;
  f1(&v, &a);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, NonUniformPointerParameterBecomesUniform_AfterUse) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *b = *a;
  *a = 0;
}

fn foo() {
  var a = non_uniform;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, NonUniformPointerParameterBecomesUniform_BeforeUse) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *a = 0;
  *b = *a;
}

fn foo() {
  var a = non_uniform;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, UniformPointerParameterBecomesNonUniform_BeforeUse) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *a = non_uniform;
  *b = *a;
}

fn foo() {
  var a = 0;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, UniformPointerParameterBecomesNonUniform_AfterUse) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *b = *a;
  *a = non_uniform;
}

fn foo() {
  var a = 0;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, NonUniformPointerParameterUpdatedInPlace) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  (*p)++;
}

fn foo() {
  var v = non_uniform;
  bar(&v);
  if (v == 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, MultiplePointerParametersBecomeNonUniform) {
    // The analysis traverses the tree for each pointer parameter, and we need to make sure that we
    // reset the "visited" state of nodes in between these traversals to properly capture each of
    // their uniformity states.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *a = non_uniform;
  *b = non_uniform;
}

fn foo() {
  var a = 0;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, MultiplePointerParametersWithEdgesToEachOther) {
    // The analysis traverses the tree for each pointer parameter, and we need to make sure that we
    // reset the "visited" state of nodes in between these traversals to properly capture each of
    // their uniformity states.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>, c : ptr<function, i32>) {
  *a = *a;
  *b = *b;
  *c = *a + *b;
}

fn foo() {
  var a = non_uniform;
  var b = 0;
  var c = 0;
  bar(&a, &b, &c);
  if (c == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:16:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, MaximumNumberOfPointerParameters) {
    // Create a function with the maximum number of parameters, all pointers, to stress the
    // quadratic nature of the analysis.
    ProgramBuilder b;
    auto& ty = b.ty;

    // fn foo(p0 : ptr<function, i32>, p1 : ptr<function, i32>, ...) {
    //   *p1 = *p0;
    //   *p2 = *p1;
    //   ...
    //   *p254 = *p253;
    // }
    ast::VariableList params;
    ast::StatementList foo_body;
    for (int i = 0; i < 255; i++) {
        params.push_back(
            b.Param("p" + std::to_string(i), ty.pointer(ty.i32(), ast::StorageClass::kFunction)));
        if (i > 0) {
            foo_body.push_back(
                b.Assign(b.Deref("p" + std::to_string(i)), b.Deref("p" + std::to_string(i - 1))));
        }
    }
    b.Func("foo", std::move(params), ty.void_(), foo_body);

    // var<private> non_uniform_global : i32;
    // fn main() {
    //   var v0 : i32;
    //   var v1 : i32;
    //   ...
    //   var v254 : i32;
    //   v0 = non_uniform_global;
    //   foo(&v0, &v1, ...,  &v254);
    //   if (v254 == 0) {
    //     workgroupBarrier();
    //   }
    // }
    b.Global("non_uniform_global", ty.i32(), ast::StorageClass::kPrivate);
    ast::StatementList main_body;
    ast::ExpressionList args;
    for (int i = 0; i < 255; i++) {
        auto name = "v" + std::to_string(i);
        main_body.push_back(b.Decl(b.Var(name, ty.i32())));
        args.push_back(b.AddressOf(name));
    }
    main_body.push_back(b.Assign("v0", "non_uniform_global"));
    main_body.push_back(b.CallStmt(b.create<ast::CallExpression>(b.Expr("foo"), args)));
    main_body.push_back(b.If(b.Equal("v254", 0), b.Block(b.CallStmt(b.Call("workgroupBarrier")))));
    b.Func("main", {}, ty.void_(), main_body);

    // TODO(jrprice): Expect false when uniformity issues become errors.
    EXPECT_TRUE(RunTest(std::move(b))) << error_;
    EXPECT_EQ(error_, R"(warning: workgroupBarrier must only be called from uniform control flow)");
}

////////////////////////////////////////////////////////////////////////////////
/// Tests to cover access to aggregate types.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, VectorElement_Uniform) {
    auto src = R"(
@group(0) @binding(0) var<storage, read> v : vec4<i32>;

fn foo() {
  if (v[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, VectorElement_NonUniform) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> v : array<i32>;

fn foo() {
  if (v[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_BecomesNonUniform_BeforeCondition) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  v[2] = rw;
  if (v[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_BecomesNonUniform_AfterCondition) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  if (v[2] == 0) {
    v[2] = rw;
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, VectorElement_DifferentElementBecomesNonUniform) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  v[1] = rw;
  if (v[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_ElementBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element, that element is
    // still considered to be non-uniform.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  v[1] = rw;
  v[1] = 42;
  if (v[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_DifferentElementBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element, the whole vector
    // is still considered to be non-uniform.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  v[1] = rw;
  v[2] = 42;
  if (v[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_NonUniform_AnyBuiltin) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

fn foo() {
  var v : vec4<i32>;
  v[1] = non_uniform_global;
  if (any(v == vec4(42))) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_Uniform) {
    auto src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read> s : S;

fn foo() {
  if (s.b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, StructMember_NonUniform) {
    auto src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> s : S;

fn foo() {
  if (s.b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_BecomesNonUniform_BeforeCondition) {
    auto src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  s.b = rw;
  if (s.b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_BecomesNonUniform_AfterCondition) {
    auto src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  if (s.b == 0) {
    s.b = rw;
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, StructMember_DifferentMemberBecomesNonUniform) {
    auto src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  s.a = rw;
  if (s.b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_MemberBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to a member, that member is
    // still considered to be non-uniform.
    auto src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  s.a = rw;
  s.a = 0;
  if (s.a == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:13:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_DifferentMemberBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to a member, the whole struct
    // is still considered to be non-uniform.
    auto src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  s.a = rw;
  s.b = 0;
  if (s.a == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:13:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_Uniform) {
    auto src = R"(
@group(0) @binding(0) var<storage, read> arr : array<i32>;

fn foo() {
  if (arr[7] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ArrayElement_NonUniform) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> arr : array<i32>;

fn foo() {
  if (arr[7] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_BecomesNonUniform_BeforeCondition) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  arr[2] = rw;
  if (arr[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_BecomesNonUniform_AfterCondition) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  if (arr[2] == 0) {
    arr[2] = rw;
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ArrayElement_DifferentElementBecomesNonUniform) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  arr[1] = rw;
  if (arr[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_DifferentElementBecomesNonUniformThroughPointer) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  let pa = &arr[1];
  *pa = rw;
  if (arr[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_ElementBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element, that element is
    // still considered to be non-uniform.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  arr[1] = rw;
  arr[1] = 42;
  if (arr[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_DifferentElementBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element, the whole array
    // is still considered to be non-uniform.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  arr[1] = rw;
  arr[2] = 42;
  if (arr[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_ElementBecomesUniformThroughPointer) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element through a
    // pointer, the whole array is still considered to be non-uniform.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  let pa = &arr[2];
  arr[1] = rw;
  *pa = 42;
  if (arr[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Miscellaneous statement and expression tests.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, Var_BecomesNonUniform_BeforeCondition) {
    // Use a function-scope variable for control-flow guarding a barrier, and then assign to that
    // variable before checking the condition.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v = 0;
  v = rw;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Var_BecomesNonUniform_AfterCondition) {
    // Use a function-scope variable for control-flow guarding a barrier, and then assign to that
    // variable after checking the condition.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v = 0;
  if (v == 0) {
    v = rw;
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, CompoundAssignment_NonUniformRHS) {
    // Use compound assignment with a non-uniform RHS on a variable.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v = 0;
  v += rw;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, CompoundAssignment_UniformRHS_StillNonUniform) {
    // Use compound assignment with a uniform RHS on a variable that is already non-uniform.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v = rw;
  v += 1;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, PhonyAssignment_LhsCausesNonUniformControlFlow) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> nonuniform_var : i32;

fn bar() -> i32 {
  if (nonuniform_var == 42) {
    return 1;
  } else {
    return 2;
  }
}

fn foo() {
  _ = bar();
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:3 warning: workgroupBarrier must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, DeadCode_AfterReturn) {
    // Dead code after a return statement shouldn't cause uniformity errors.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  return;
  if (non_uniform == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, DeadCode_AfterDiscard) {
    // Dead code after a discard statement shouldn't cause uniformity errors.
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  discard;
  if (non_uniform == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ArrayLength) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> arr : array<f32>;

fn foo() {
  for (var i = 0u; i < arrayLength(&arr); i++) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, WorkgroupAtomics) {
    auto src = R"(
var<workgroup> a : atomic<i32>;

fn foo() {
  if (atomicAdd(&a, 1) == 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: workgroupBarrier must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StorageAtomics) {
    auto src = R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<i32>;

fn foo() {
  if (atomicAdd(&a, 1) == 1) {
    storageBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: storageBarrier must only be called from uniform control flow
    storageBarrier();
    ^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, DisableAnalysisWithExtension) {
    auto src = R"(
enable chromium_disable_uniformity_analysis;

@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  if (rw == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

}  // namespace
}  // namespace tint::resolver
