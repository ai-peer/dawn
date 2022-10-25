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

#ifndef SRC_TINT_IR_IF_FLOW_NODE_H_
#define SRC_TINT_IR_IF_FLOW_NODE_H_

#include "src/tint/ir/flow_node.h"

// Forward declarations
namespace tint::ir {
class BlockFlowNode;
}  // namespace tint::ir

namespace tint::ir {

/// A flow node representing an if statment. The node always contains a true and a false block. It
/// may contain a merge block where the true/false blocks will merge too unless they return.
class IfFlowNode : public Castable<IfFlowNode, FlowNode> {
  public:
    /// Constructor
    IfFlowNode();
    ~IfFlowNode() override;

    /// The true branch block
    BlockFlowNode* true_target = nullptr;
    /// The false branch block
    BlockFlowNode* false_target = nullptr;
    /// An optional block where the true/false blocks will branch too if needed.
    BlockFlowNode* merge_target = nullptr;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_IF_FLOW_NODE_H_
