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

#ifndef SRC_TINT_IR_FUNCTION_H_
#define SRC_TINT_IR_FUNCTION_H_

#include "src/tint/ast/function.h"
#include "src/tint/ir/flow_node.h"

// Forward declarations
namespace tint::ir {
class BlockFlowNode;
}  // namespace tint::ir

namespace tint::ir {

/// An IR representation of a function
class Function {
  public:
    /// Constructor
    /// @param func the ast::Function to create from
    explicit Function(const ast::Function* func);
    ~Function();

    /// The ast function this ir function is created from.
    const ast::Function* func;

    /// The list of flow nodes in the function. Not in any particular order.
    utils::Vector<const FlowNode*, 8> control_flow;
    /// The start block is the first block in a function.
    BlockFlowNode* start_target = nullptr;
    /// The end block is the last block in a function. It's always empty. It will be used as the
    /// block if a return is encountered.
    BlockFlowNode* end_target = nullptr;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_FUNCTION_H_
