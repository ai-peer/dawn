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

#ifndef SRC_TINT_IR_LOOP_FLOW_NODE_H_
#define SRC_TINT_IR_LOOP_FLOW_NODE_H_

#include "src/tint/ast/loop_statement.h"
#include "src/tint/ir/block_flow_node.h"
#include "src/tint/ir/flow_node.h"

namespace tint::ir {

/// Flow node describing a loop.
class LoopFlowNode : public Castable<LoopFlowNode, FlowNode> {
  public:
    /// Constructor
    /// @param stmt the ast::LoopStatement
    explicit LoopFlowNode(const ast::LoopStatement* stmt);
    ~LoopFlowNode() override;

    /// The ast loop statement this ir loop is created from.
    const ast::LoopStatement* source;

    /// The start block is the first block in a loop.
    BlockFlowNode* start_target = nullptr;
    /// The continue target of the block.
    BlockFlowNode* continuing_target = nullptr;
    /// The loop merge target. If the `loop` does a `return` then this block may not actually
    /// end up in the control flow. We need it if the loop does a `break` we know where to break
    /// too.
    BlockFlowNode* merge_target = nullptr;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_LOOP_FLOW_NODE_H_
