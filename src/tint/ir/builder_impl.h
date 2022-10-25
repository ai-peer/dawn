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

#ifndef SRC_TINT_IR_BUILDER_IMPL_H_
#define SRC_TINT_IR_BUILDER_IMPL_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "src/tint/diagnostic/diagnostic.h"
#include "src/tint/ir/block_flow_node.h"
#include "src/tint/ir/flow_node.h"
#include "src/tint/ir/function.h"
#include "src/tint/ir/module.h"
#include "src/tint/utils/block_allocator.h"

// Forward Declarations
namespace tint {
class Program;
}  // namespace tint
namespace tint::ast {
class BlockStatement;
class Function;
class IfStatement;
class ReturnStatement;
class Statement;
}  // namespace tint::ast

namespace tint::ir {

/// Builds an ir::Module from a given ast::Program
class BuilderImpl {
  public:
    /// Constructor
    /// @param program the program to create from
    explicit BuilderImpl(const Program* program);
    /// Destructor
    ~BuilderImpl();

    /// Builds an ir::Module from the given Program
    /// @returns true on success, false otherwise
    bool Build();

    /// @returns the error
    std::string error() const { return diagnostics_.str(); }

    /// @returns the generated ir::Module
    Module ir() { return std::move(ir_); }

    /// Creates a new ir::Function. The function will have start and end blocks created. The
    /// function start block will be set as the current block.
    /// @param f the ast::Function this ir::Function represents.
    /// @returns a function flow node.
    Function* CreateFunction(const ast::Function* f);

    /// @returns a flow node of type T. The flow node is pushed into the current functions control
    /// flow list.
    template <typename T>
    T* CreateFlowNode() {
        auto* node = flow_nodes_.Create<T>();
        current_function_->control_flow.Push(node);

        // The addition of a new flow node terminates the current block node. Set the branch target
        // and clear it out.
        current_flow_block_->branch_target = node;
        current_flow_block_ = nullptr;

        return node;
    }

    /// Emits a function to the IR.
    /// @param func the function to emit
    /// @returns true if successful, false otherwise
    bool EmitFunction(const ast::Function* func);

    /// Emits a set of statements to the IR.
    /// @param stmts the statements to emit
    /// @returns true if successful, false otherwise.
    bool EmitStatements(utils::VectorRef<const ast::Statement*> stmts);

    /// Emits a statement to the IR
    /// @param stmt the statment to emit
    /// @returns true on success, false otherwise.
    bool EmitStatement(const ast::Statement* stmt);

    /// Emits a block statement to the IR.
    /// @param block the block to emit
    /// @returns true if successful, false otherwise.
    bool EmitBlock(const ast::BlockStatement* block);

    /// Emits an if control node to the IR.
    /// @param stmt the if statement
    /// @returns true if successful, false otherwise.
    bool EmitIf(const ast::IfStatement* stmt);

    /// Emits a return node to the IR.
    /// @param stmt the return AST statement
    /// @returns true if successful, false otherwise.
    bool EmitReturn(const ast::ReturnStatement* stmt);

    /// Retrieve the IR Flow node for a given AST node.
    /// @param n the node to lookup
    /// @returns the FlowNode for the given ast::Node or nullptr if it doesn't exist.
    const ir::FlowNode* FlowNodeForAstNode(const ast::Node* n) const {
        if (ast_to_flow_.count(n) == 0) {
            return nullptr;
        }
        return ast_to_flow_.at(n);
    }

  private:
    const Program* program_;

    Module ir_;
    diag::List diagnostics_;

    BlockFlowNode* current_flow_block_ = nullptr;
    Function* current_function_ = nullptr;

    utils::BlockAllocator<Function> functions_;
    utils::BlockAllocator<FlowNode> flow_nodes_;

    /// Map from ast nodes to flow nodes, used to retrieve the flow node for a given AST node.
    /// Used for testing purposes.
    std::unordered_map<const ast::Node*, const FlowNode*> ast_to_flow_;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_BUILDER_H_
