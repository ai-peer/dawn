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

#include "src/tint/ir/if_flow_node.h"
#include "src/tint/ir/test_helper.h"

namespace tint::ir {
namespace {

using IRBuilderImplTest = TestHelper;

TEST_F(IRBuilderImplTest, Func) {
    Func("f", utils::Empty, ty.void_(), utils::Empty);
    auto& b = Build();
    ASSERT_NO_FATAL_FAILURE();

    ASSERT_TRUE(b.Build()) << b.error();
    auto m = b.ir();

    ASSERT_EQ(0u, m.entry_points.Length());
    ASSERT_EQ(1u, m.functions.Length());

    auto* f = m.functions[0];
    EXPECT_NE(f->start_target, nullptr);
    EXPECT_NE(f->end_target, nullptr);

    EXPECT_EQ(f->start_target->branch_target, f->end_target);
}

TEST_F(IRBuilderImplTest, EntryPoint) {
    Func("f", utils::Empty, ty.void_(), utils::Empty,
         utils::Vector{Stage(ast::PipelineStage::kFragment)});
    auto& b = Build();
    ASSERT_NO_FATAL_FAILURE();

    ASSERT_TRUE(b.Build()) << b.error();
    auto m = b.ir();

    ASSERT_EQ(1u, m.entry_points.Length());
    EXPECT_EQ(m.functions[0], m.entry_points[0]);
}

TEST_F(IRBuilderImplTest, IfStatement) {
    auto* ast_if = If(true, Block(), Else(Block()));
    WrapInFunction(ast_if);
    auto& b = Build();
    ASSERT_NO_FATAL_FAILURE();

    ASSERT_TRUE(b.Build()) << b.error();

    auto* ir_if = b.FlowNodeForAstNode(ast_if);
    ASSERT_NE(ir_if, nullptr);
    EXPECT_TRUE(ir_if->Is<ir::IfFlowNode>());

    // TODO(dsinclair): check condition

    auto* flow = ir_if->As<ir::IfFlowNode>();
    ASSERT_NE(flow->true_target, nullptr);
    ASSERT_NE(flow->false_target, nullptr);
    ASSERT_NE(flow->merge_target, nullptr);

    EXPECT_EQ(flow->true_target->branch_target, flow->merge_target);
    EXPECT_EQ(flow->false_target->branch_target, flow->merge_target);

    auto m = b.ir();
    ASSERT_EQ(1u, m.functions.Length());
    auto* func = m.functions[0];

    EXPECT_EQ(func->start_target->branch_target, ir_if);
    EXPECT_EQ(flow->merge_target->branch_target, func->end_target);
}

TEST_F(IRBuilderImplTest, IfStatement_TrueReturns) {
    auto* ast_if = If(true, Block(Return()));
    WrapInFunction(ast_if);
    auto& b = Build();
    ASSERT_NO_FATAL_FAILURE();

    ASSERT_TRUE(b.Build()) << b.error();

    auto m = b.ir();
    ASSERT_EQ(1u, m.functions.Length());
    auto* func = m.functions[0];

    auto* ir_if = b.FlowNodeForAstNode(ast_if);
    ASSERT_NE(ir_if, nullptr);
    EXPECT_TRUE(ir_if->Is<ir::IfFlowNode>());

    auto* flow = ir_if->As<ir::IfFlowNode>();
    ASSERT_NE(flow->true_target, nullptr);
    ASSERT_NE(flow->false_target, nullptr);
    ASSERT_NE(flow->merge_target, nullptr);

    EXPECT_EQ(flow->true_target->branch_target, func->end_target);
    EXPECT_EQ(flow->false_target->branch_target, flow->merge_target);
}

TEST_F(IRBuilderImplTest, IfStatement_FalseReturns) {
    auto* ast_if = If(true, Block(), Else(Block(Return())));
    WrapInFunction(ast_if);
    auto& b = Build();
    ASSERT_NO_FATAL_FAILURE();

    ASSERT_TRUE(b.Build()) << b.error();

    auto m = b.ir();
    ASSERT_EQ(1u, m.functions.Length());
    auto* func = m.functions[0];

    auto* ir_if = b.FlowNodeForAstNode(ast_if);
    ASSERT_NE(ir_if, nullptr);
    EXPECT_TRUE(ir_if->Is<ir::IfFlowNode>());

    auto* flow = ir_if->As<ir::IfFlowNode>();
    ASSERT_NE(flow->true_target, nullptr);
    ASSERT_NE(flow->false_target, nullptr);
    ASSERT_NE(flow->merge_target, nullptr);

    EXPECT_EQ(flow->true_target->branch_target, flow->merge_target);
    EXPECT_EQ(flow->false_target->branch_target, func->end_target);
}

TEST_F(IRBuilderImplTest, IfStatement_BothReturn) {
    auto* ast_if = If(true, Block(Return()), Else(Block(Return())));
    WrapInFunction(ast_if);
    auto& b = Build();
    ASSERT_NO_FATAL_FAILURE();

    ASSERT_TRUE(b.Build()) << b.error();

    auto m = b.ir();
    ASSERT_EQ(1u, m.functions.Length());
    auto* func = m.functions[0];

    auto* ir_if = b.FlowNodeForAstNode(ast_if);
    ASSERT_NE(ir_if, nullptr);
    EXPECT_TRUE(ir_if->Is<ir::IfFlowNode>());

    auto* flow = ir_if->As<ir::IfFlowNode>();
    ASSERT_NE(flow->true_target, nullptr);
    ASSERT_NE(flow->false_target, nullptr);
    EXPECT_EQ(flow->merge_target, nullptr);

    EXPECT_EQ(flow->true_target->branch_target, func->end_target);
    EXPECT_EQ(flow->false_target->branch_target, func->end_target);
}

}  // namespace
}  // namespace tint::ir
