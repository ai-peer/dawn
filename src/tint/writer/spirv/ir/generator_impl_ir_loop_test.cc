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

using namespace tint::number_suffixes;  // NOLINT

namespace tint::writer::spirv {
namespace {

TEST_F(SpvGeneratorImplTest, Loop_BreakIf) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* loop = b.CreateLoop();

    loop->Body()->Append(b.Continue(loop));
    loop->Continuing()->Append(b.BreakIf(b.Constant(true), loop));

    func->StartTarget()->Append(loop);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%9 = OpTypeBool
%8 = OpConstantTrue %9
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpLoopMerge %7 %6 None
OpBranch %6
%6 = OpLabel
OpBranchConditional %8 %7 %5
%7 = OpLabel
OpReturn
OpFunctionEnd
)");
}

// Test that we still emit the continuing block with a back-edge, even when it is unreachable.
TEST_F(SpvGeneratorImplTest, Loop_UnconditionalBreakInBody) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* loop = b.CreateLoop();

    loop->Body()->Append(b.ExitLoop(loop));

    func->StartTarget()->Append(loop);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpLoopMerge %7 %6 None
OpBranch %7
%6 = OpLabel
OpBranch %5
%7 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Loop_ConditionalBreakInBody) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* loop = b.CreateLoop();

    auto* cond_break = b.CreateIf(b.Constant(true));
    cond_break->True()->Append(b.ExitLoop(loop));
    cond_break->False()->Append(b.ExitIf(cond_break));

    loop->Body()->Append(cond_break);
    loop->Body()->Append(b.Continue(loop));
    loop->Continuing()->Append(b.NextIteration(loop));

    func->StartTarget()->Append(loop);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%11 = OpTypeBool
%10 = OpConstantTrue %11
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpLoopMerge %7 %6 None
OpSelectionMerge %8 None
OpBranchConditional %10 %9 %8
%9 = OpLabel
OpBranch %7
%8 = OpLabel
OpBranch %6
%6 = OpLabel
OpBranch %5
%7 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Loop_ConditionalContinueInBody) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* loop = b.CreateLoop();

    auto* cond_break = b.CreateIf(b.Constant(true));
    cond_break->True()->Append(b.Continue(loop));
    cond_break->False()->Append(b.ExitIf(cond_break));

    loop->Body()->Append(cond_break);
    loop->Body()->Append(b.ExitLoop(loop));
    loop->Continuing()->Append(b.NextIteration(loop));

    func->StartTarget()->Append(loop);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%11 = OpTypeBool
%10 = OpConstantTrue %11
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpLoopMerge %7 %6 None
OpSelectionMerge %8 None
OpBranchConditional %10 %9 %8
%9 = OpLabel
OpBranch %6
%8 = OpLabel
OpBranch %7
%6 = OpLabel
OpBranch %5
%7 = OpLabel
OpReturn
OpFunctionEnd
)");
}

// Test that we still emit the continuing block with a back-edge, and the merge block, even when
// they are unreachable.
TEST_F(SpvGeneratorImplTest, Loop_UnconditionalReturnInBody) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* loop = b.CreateLoop();

    loop->Body()->Append(b.Return(func));

    func->StartTarget()->Append(loop);

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpLoopMerge %7 %6 None
OpReturn
%6 = OpLabel
OpBranch %5
%7 = OpLabel
OpUnreachable
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Loop_UseResultFromBodyInContinuing) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* loop = b.CreateLoop();

    auto* result = b.Equal(ty.i32(), b.Constant(1_i), b.Constant(2_i));

    loop->Body()->Append(result);
    loop->Continuing()->Append(b.BreakIf(result, loop));

    func->StartTarget()->Append(loop);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%9 = OpTypeInt 32 1
%10 = OpConstant %9 1
%11 = OpConstant %9 2
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpLoopMerge %7 %6 None
%8 = OpIEqual %9 %10 %11
OpUnreachable
%6 = OpLabel
OpBranchConditional %8 %7 %5
%7 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Loop_NestedLoopInBody) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* outer_loop = b.CreateLoop();
    auto* inner_loop = b.CreateLoop();

    inner_loop->Body()->Append(b.ExitLoop(inner_loop));
    inner_loop->Continuing()->Append(b.NextIteration(inner_loop));

    outer_loop->Body()->Append(inner_loop);
    outer_loop->Body()->Append(b.Continue(outer_loop));
    outer_loop->Continuing()->Append(b.BreakIf(b.Constant(true), outer_loop));

    func->StartTarget()->Append(outer_loop);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%12 = OpTypeBool
%11 = OpConstantTrue %12
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpLoopMerge %7 %6 None
OpBranch %8
%8 = OpLabel
OpLoopMerge %10 %9 None
OpBranch %10
%9 = OpLabel
OpBranch %8
%10 = OpLabel
OpBranch %6
%6 = OpLabel
OpBranchConditional %11 %7 %5
%7 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Loop_NestedLoopInContinuing) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* outer_loop = b.CreateLoop();
    auto* inner_loop = b.CreateLoop();

    inner_loop->Body()->Append(b.Continue(inner_loop));
    inner_loop->Continuing()->Append(b.BreakIf(b.Constant(true), inner_loop));

    outer_loop->Body()->Append(b.Continue(outer_loop));
    outer_loop->Continuing()->Append(inner_loop);
    outer_loop->Continuing()->Append(b.BreakIf(b.Constant(true), outer_loop));

    func->StartTarget()->Append(outer_loop);
    func->StartTarget()->Append(b.Return(func));

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%12 = OpTypeBool
%11 = OpConstantTrue %12
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpLoopMerge %7 %6 None
OpBranch %6
%6 = OpLabel
OpBranch %8
%8 = OpLabel
OpLoopMerge %10 %9 None
OpBranch %9
%9 = OpLabel
OpBranchConditional %11 %10 %8
%10 = OpLabel
OpBranchConditional %11 %7 %5
%7 = OpLabel
OpReturn
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Loop_Phi_SingleValue) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* l = b.CreateLoop();

    l->Initializer()->Append(b.NextIteration(l, utils::Vector{b.Constant(1_i)}));

    auto* loop_param = b.BlockParam(ty.i32());
    l->Body()->SetParams(utils::Vector{loop_param});
    auto* inc = b.Add(ty.i32(), loop_param, b.Constant(1_i));
    l->Body()->Append(inc);
    l->Body()->Append(b.Continue(l, utils::Vector{inc}));

    auto* cont_param = b.BlockParam(ty.i32());
    l->Continuing()->SetParams(utils::Vector{cont_param});
    auto* cmp = b.GreaterThan(ty.bool_(), cont_param, b.Constant(5_i));
    l->Continuing()->Append(cmp);
    l->Continuing()->Append(b.BreakIf(cmp, l, utils::Vector{cont_param}));

    func->StartTarget()->Append(l);

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%9 = OpTypeInt 32 1
%11 = OpConstant %9 1
%15 = OpTypeBool
%16 = OpConstant %9 5
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpBranch %6
%6 = OpLabel
OpLoopMerge %8 %7 None
%10 = OpPhi %9 %11 %5 %12 %7
%13 = OpIAdd %9 %10 %11
OpBranch %7
%7 = OpLabel
%12 = OpPhi %9 %13 %6
%14 = OpSGreaterThan %15 %12 %16
OpBranchConditional %14 %8 %6
%8 = OpLabel
OpUnreachable
OpFunctionEnd
)");
}

TEST_F(SpvGeneratorImplTest, Loop_Phi_MultipleValue) {
    auto* func = b.CreateFunction("foo", ty.void_());

    auto* l = b.CreateLoop();

    l->Initializer()->Append(b.NextIteration(l, utils::Vector{b.Constant(1_i), b.Constant(false)}));

    auto* loop_param_a = b.BlockParam(ty.i32());
    auto* loop_param_b = b.BlockParam(ty.bool_());
    l->Body()->SetParams(utils::Vector{loop_param_a, loop_param_b});
    auto* inc = b.Add(ty.i32(), loop_param_a, b.Constant(1_i));
    l->Body()->Append(inc);
    l->Body()->Append(b.Continue(l, utils::Vector{inc, loop_param_b}));

    auto* cont_param_a = b.BlockParam(ty.i32());
    auto* cont_param_b = b.BlockParam(ty.bool_());
    l->Continuing()->SetParams(utils::Vector{cont_param_a, cont_param_b});
    auto* cmp = b.GreaterThan(ty.bool_(), cont_param_a, b.Constant(5_i));
    l->Continuing()->Append(cmp);
    auto* not_b = b.Not(ty.bool_(), cont_param_b);
    l->Continuing()->Append(not_b);
    l->Continuing()->Append(b.BreakIf(cmp, l, utils::Vector{cont_param_a, not_b}));

    func->StartTarget()->Append(l);

    ASSERT_TRUE(IRIsValid()) << Error();

    generator_.EmitFunction(func);
    EXPECT_EQ(DumpModule(generator_.Module()), R"(OpName %1 "foo"
%2 = OpTypeVoid
%3 = OpTypeFunction %2
%9 = OpTypeInt 32 1
%11 = OpConstant %9 1
%13 = OpTypeBool
%15 = OpConstantFalse %13
%20 = OpConstant %9 5
%1 = OpFunction %2 None %3
%4 = OpLabel
OpBranch %5
%5 = OpLabel
OpBranch %6
%6 = OpLabel
OpLoopMerge %8 %7 None
%10 = OpPhi %9 %11 %5 %12 %7
%14 = OpPhi %13 %15 %5 %16 %7
%17 = OpIAdd %9 %10 %11
OpBranch %7
%7 = OpLabel
%12 = OpPhi %9 %17 %6
%18 = OpPhi %13 %14 %6
%19 = OpSGreaterThan %13 %12 %20
%16 = OpLogicalEqual %13 %18 %15
OpBranchConditional %19 %8 %6
%8 = OpLabel
OpUnreachable
OpFunctionEnd
)");
}

}  // namespace
}  // namespace tint::writer::spirv
