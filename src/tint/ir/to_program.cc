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

#include "src/tint/ir/to_program.h"

#include "src/tint/ir/block.h"
#include "src/tint/ir/call.h"
#include "src/tint/ir/constant.h"
#include "src/tint/ir/if.h"
#include "src/tint/ir/instruction.h"
#include "src/tint/ir/module.h"
#include "src/tint/ir/user_call.h"
#include "src/tint/program_builder.h"
#include "src/tint/switch.h"
#include "src/tint/utils/transform.h"
#include "src/tint/utils/vector.h"

namespace tint::ir {

namespace {

struct State {
    const Module& mod;
    ProgramBuilder b{};

    Program Run() {
        for (auto* fn : mod.functions) {
            Fn(fn);
        }
        return Program{std::move(b)};
    }

    void Fn(const Function* fn) {
        auto name = Sym(fn->name);
        // TODO(crbug.com/tint/1915): Properly implement this when we've fleshed out Function
        utils::Vector<const ast::Parameter*, 1> params{};
        ast::Type ret_ty;
        auto* body = Block(fn->start_target);
        utils::Vector<const ast::Attribute*, 1> attrs{};
        utils::Vector<const ast::Attribute*, 1> ret_attrs{};
        b.Func(name, std::move(params), ret_ty, body, std::move(attrs), std::move(ret_attrs));
    }

    const ast::BlockStatement* Block(const ir::Block* block) {
        utils::Vector<const ast::Statement*, decltype(ir::Block::instructions)::static_length>
            stmts;
        for (auto* inst : block->instructions) {
            auto* stmt = Stmt(inst);
            if (!stmt) {
                return nullptr;
            }
            stmts.Push(stmt);
        }
        return b.Block(std::move(stmts));
    }

    const ast::Statement* FlowNode(const ir::FlowNode* node) {
        return Switch(
            node,  //
            [&](const ir::If* i) {
                auto* cond = Expr(i->condition);
                auto* t = Block(i->true_);
                if (auto* f = Block(i->false_)) {
                    return b.If(cond, t, b.Else(f));
                }
                return b.If(cond, t);
            },
            [&](Default) {
                TINT_UNIMPLEMENTED(IR, b.Diagnostics())
                    << "unhandled case in Switch(): " << node->TypeInfo().name;
                return nullptr;
            });
    }

    const ast::BlockStatement* Block(const ir::Branch& branch) {
        auto* stmt = FlowNode(branch.target);
        if (!stmt) {
            return nullptr;
        }
        if (auto* block = stmt->As<ast::BlockStatement>()) {
            return block;
        }
        return b.Block(stmt);
    }

    const ast::Statement* Stmt(const ir::Instruction* inst) {
        return Switch(
            inst,  //
            [&](const ir::Call* c) { return CallStmt(c); },
            [&](Default) {
                TINT_UNIMPLEMENTED(IR, b.Diagnostics())
                    << "unhandled case in Switch(): " << inst->TypeInfo().name;
                return nullptr;
            });
    }

    const ast::CallStatement* CallStmt(const ir::Call* call) {
        auto* expr = Call(call);
        if (!expr) {
            return nullptr;
        }
        return b.CallStmt(expr);
    }

    const ast::CallExpression* Call(const ir::Call* call) {
        auto args = utils::Transform(call->args, [&](const ir::Value* arg) { return Expr(arg); });
        if (args.Any(nullptr)) {
            return nullptr;
        }
        return Switch(
            call,  //
            [&](const ir::UserCall* c) { return b.Call(Sym(c->name), std::move(args)); },
            [&](Default) {
                TINT_UNIMPLEMENTED(IR, b.Diagnostics())
                    << "unhandled case in Switch(): " << call->TypeInfo().name;
                return nullptr;
            });
    }

    const ast::Expression* Expr(const ir::Value* val) {
        return Switch(
            val,  //
            [&](const ir::Constant* c) { return ConstExpr(c); },
            [&](Default) {
                TINT_UNIMPLEMENTED(IR, b.Diagnostics())
                    << "unhandled case in Switch(): " << val->TypeInfo().name;
                return nullptr;
            });
    }

    const ast::Expression* ConstExpr(const ir::Constant* c) {
        return Switch(
            c->Type(),  //
            [&](const type::I32*) { return b.Expr(c->value->ValueAs<i32>()); },
            [&](const type::U32*) { return b.Expr(c->value->ValueAs<u32>()); },
            [&](const type::F32*) { return b.Expr(c->value->ValueAs<f32>()); },
            [&](const type::F16*) { return b.Expr(c->value->ValueAs<f16>()); },
            [&](const type::Bool*) { return b.Expr(c->value->ValueAs<bool>()); },
            [&](Default) {
                TINT_UNIMPLEMENTED(IR, b.Diagnostics())
                    << "unhandled case in Switch(): " << c->TypeInfo().name;
                return nullptr;
            });
    }

    Symbol Sym(const Symbol& s) { return b.Symbols().Register(s.NameView()); }

    // void Err(std::string_view str) { b.Diagnostics().add_error(diag::System::IR, str); }
};

}  // namespace

Program ToProgram(const Module& i) {
    return State{i}.Run();
}

}  // namespace tint::ir
