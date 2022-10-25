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

#include "src/tint/ir/builder_impl.h"

#include "src/tint/ast/alias.h"
#include "src/tint/ast/block_statement.h"
#include "src/tint/ast/function.h"
#include "src/tint/ast/if_statement.h"
#include "src/tint/ast/return_statement.h"
#include "src/tint/ast/statement.h"
#include "src/tint/ast/static_assert.h"
#include "src/tint/ir/if_flow_node.h"
#include "src/tint/ir/module.h"
#include "src/tint/program.h"
#include "src/tint/sem/module.h"

namespace tint::ir {
namespace {

class FlowStackScope {
  public:
    FlowStackScope(BuilderImpl* impl, FlowNode* node) : impl_(impl) {
        impl_->flow_stack.Push(node);
    }

    ~FlowStackScope() { impl_->flow_stack.Pop(); }

  private:
    BuilderImpl* impl_;
};

}  // namespace

BuilderImpl::BuilderImpl(const Program* program) : program_(program), ir_(program) {}

BuilderImpl::~BuilderImpl() = default;

BlockFlowNode* BuilderImpl::CreateBlock() {
    return ir_.flow_nodes.Create<ir::BlockFlowNode>();
}

FunctionFlowNode* BuilderImpl::CreateFunction(const ast::Function* ast_func) {
    current_function_ = ir_.flow_nodes.Create<ir::FunctionFlowNode>(ast_func);
    current_function_->end_target = CreateBlock();
    current_function_->start_target = CreateBlock();

    // The flow stack should have been emptied when the previous function finshed building.
    TINT_ASSERT(IR, flow_stack.IsEmpty());

    return current_function_;
}

IfFlowNode* BuilderImpl::CreateIf(const ast::IfStatement* stmt) {
    auto* ir_if = ir_.flow_nodes.Create<ir::IfFlowNode>(stmt);
    ir_if->false_target = CreateBlock();
    ir_if->true_target = CreateBlock();
    return ir_if;
}

bool BuilderImpl::Build() {
    auto* sem = program_->Sem().Module();

    for (auto* decl : sem->DependencyOrderedDeclarations()) {
        bool ok = Switch(
            decl,  //
            // [&](const ast::Struct* str) {
            //   return false;
            // },
            [&](const ast::Alias*) {
                // Folded away and doesn't appear in the IR.
                return true;
            },
            // [&](const ast::Const*) {
            //   return false;
            // },
            // [&](const ast::Override*) {
            //   return false;
            // },
            [&](const ast::Function* func) { return EmitFunction(func); },
            // [&](const ast::Enable*) {
            //   return false;
            // },
            [&](const ast::StaticAssert*) {
                // Evaluated by the resolver, drop from the IR.
                return true;
            },
            [&](Default) {
                TINT_ICE(IR, diagnostics_) << "unhandled type: " << decl->TypeInfo().name;
                return false;
            });
        if (!ok) {
            return false;
        }
    }

    return true;
}

bool BuilderImpl::EmitFunction(const ast::Function* ast_func) {
    auto* ir_func = CreateFunction(ast_func);
    ir_.functions.Push(ir_func);

    if (ast_func->IsEntryPoint()) {
        ir_.entry_points.Push(ir_func);
    }

    {
        FlowStackScope scope(this, ir_func);

        current_flow_block_ = ir_func->start_target;
        if (!EmitStatements(ast_func->body->statements)) {
            return false;
        }

        // If the branch target has already been set then a `return` was called. Only set in the
        // case where `return` wasn't called.
        if (current_flow_block_ && !current_flow_block_->branch_target) {
            current_flow_block_->branch_target = current_function_->end_target;
        }
    }

    TINT_ASSERT(IR, flow_stack.IsEmpty());
    current_flow_block_ = nullptr;
    current_function_ = nullptr;

    return true;
}

bool BuilderImpl::EmitStatements(utils::VectorRef<const ast::Statement*> stmts) {
    for (auto* s : stmts) {
        if (!EmitStatement(s)) {
            return false;
        }
    }
    return true;
}

bool BuilderImpl::EmitStatement(const ast::Statement* stmt) {
    return Switch(
        stmt,
        //        [&](const ast::AssignmentStatement* a) {
        //        },
        [&](const ast::BlockStatement* b) { return EmitBlock(b); },
        //        [&](const ast::BreakStatement* b) {
        //        },
        //        [&](const ast::BreakIfStatement* b) {
        //        },
        //        [&](const ast::CallStatement* c) {
        //        },
        //        [&](const ast::ContinueStatement* c) {
        //        },
        //        [&](const ast::DiscardStatement* d) {
        //        },
        //        [&](const ast::FallthroughStatement*) {
        //
        //        },
        [&](const ast::IfStatement* i) { return EmitIf(i); },
        //        [&](const ast::LoopStatement* l) {
        //        },
        //        [&](const ast::ForLoopStatement* l) {
        //        },
        //        [&](const ast::WhileStatement* l) {
        //        },
        [&](const ast::ReturnStatement* r) { return EmitReturn(r); },
        //        [&](const ast::SwitchStatement* s) {
        //        },
        //        [&](const ast::VariableDeclStatement* v) {
        //        },
        [&](const ast::StaticAssert*) {
            return true;  // Not emitted
        },
        [&](Default) {
            TINT_ICE(IR, diagnostics_)
                << "unknown statement type: " << std::string(stmt->TypeInfo().name);
            return false;
        });
}

bool BuilderImpl::EmitBlock(const ast::BlockStatement* block) {
    // Note, this doesn't need to emit a BlockFlowNode as the current block flow node should be
    // sufficient as the blocks all get flattened. Each flow control node will inject the basic
    // blocks it requires.
    return EmitStatements(block->statements);
}

bool BuilderImpl::EmitIf(const ast::IfStatement* stmt) {
    TINT_ASSERT(IR, current_flow_block_);
    TINT_ASSERT(IR, !current_flow_block_->branch_target);

    auto* if_node = CreateIf(stmt);

    // TODO(dsinclair): Emit the condition expression into the current block

    // Branch the current block to this if node
    current_flow_block_->branch_target = if_node;
    current_flow_block_ = nullptr;

    ast_to_flow_[stmt] = if_node;

    {
        FlowStackScope scope(this, if_node);

        // TODO(dsinclair): set if condition register into if flow node

        current_flow_block_ = if_node->true_target;
        if (!EmitStatement(stmt->body)) {
            return false;
        }

        current_flow_block_ = if_node->false_target;
        if (stmt->else_statement && !EmitStatement(stmt->else_statement)) {
            return false;
        }
    }
    current_flow_block_ = nullptr;

    // If both branches went somewhere, nothing to do, else setup a merge block to continue
    // executing.
    if (!if_node->true_target->branch_target || !if_node->false_target->branch_target) {
        if_node->merge_target = CreateBlock();
        current_flow_block_ = if_node->merge_target;

        // If the true branch did not execute control flow, then go to the merge target
        if (!if_node->true_target->branch_target) {
            if_node->true_target->branch_target = if_node->merge_target;
        }
        // If the false branch did not execute control flow, then go to the merge target
        if (!if_node->false_target->branch_target) {
            if_node->false_target->branch_target = if_node->merge_target;
        }
    }
    return true;
}

bool BuilderImpl::EmitReturn(const ast::ReturnStatement*) {
    TINT_ASSERT(IR, current_flow_block_);
    TINT_ASSERT(IR, current_function_);

    // TODO(dsinclair): Emit the return value ....

    current_flow_block_->branch_target = current_function_->end_target;
    return true;
}

}  // namespace tint::ir
