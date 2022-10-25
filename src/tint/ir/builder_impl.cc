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

BuilderImpl::BuilderImpl(const Program* program) : program_(program), ir_(program) {}

BuilderImpl::~BuilderImpl() = default;

/// @returns a block flow node. The  flow node is pushed into the current functions control flow
/// list.
template <>
BlockFlowNode* BuilderImpl::CreateFlowNode<BlockFlowNode>() {
    auto* bb = flow_nodes_.Create<ir::BlockFlowNode>();
    current_flow_block_ = bb;
    current_function_->control_flow.Push(bb);
    return bb;
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

Function* BuilderImpl::CreateFunction(const ast::Function* f) {
    // If there is a current block, set it to branch to the current functions end target.
    if (current_flow_block_ && !current_flow_block_->branch_target) {
        TINT_ASSERT(IR, current_function_);
        current_flow_block_->branch_target = current_function_->end_target;
    }

    current_function_ = functions_.Create<ir::Function>(f);
    current_function_->end_target = CreateFlowNode<ir::BlockFlowNode>();

    // Set the start target after the end so it becomes the current block target
    current_function_->start_target = CreateFlowNode<ir::BlockFlowNode>();
    return current_function_;
}

bool BuilderImpl::EmitFunction(const ast::Function* ast_func) {
    if (ast_func->IsEntryPoint()) {
        ir_.entry_points.Push(functions_.Count());
    }

    auto* ir_func = CreateFunction(ast_func);
    ir_.functions.Push(ir_func);

    if (!EmitStatements(ast_func->body->statements)) {
        return false;
    }

    if (current_flow_block_) {
        current_flow_block_->branch_target = current_function_->end_target;
    }
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
    auto* if_node = CreateFlowNode<ir::IfFlowNode>();
    ast_to_flow_[stmt] = if_node;

    // TODO(dsinclair): set if condition register into if flow node
    if_node->true_target = CreateFlowNode<ir::BlockFlowNode>();
    if (!EmitStatement(stmt->body)) {
        return false;
    }

    if_node->false_target = CreateFlowNode<ir::BlockFlowNode>();
    if (stmt->else_statement && !EmitStatement(stmt->else_statement)) {
        return false;
    }

    // If both branches went somewhere, nothing to do, else setup a merge block to continue
    // executing.
    if (!if_node->true_target->branch_target || !if_node->false_target->branch_target) {
        if_node->merge_target = CreateFlowNode<ir::BlockFlowNode>();

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

    // Curernt block branches out to the end of the function, and is no longer the current block.
    current_flow_block_->branch_target = current_function_->end_target;
    current_flow_block_ = nullptr;
    return true;
}

}  // namespace tint::ir
