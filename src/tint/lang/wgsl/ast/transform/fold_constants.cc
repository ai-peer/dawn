// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/wgsl/ast/transform/fold_constants.h"

#include <utility>

#include "src/tint/lang/core/constant/composite.h"
#include "src/tint/lang/core/constant/scalar.h"
#include "src/tint/lang/core/constant/splat.h"
#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/core/type/abstract_float.h"
#include "src/tint/lang/core/type/abstract_int.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/index_accessor_expression.h"
#include "src/tint/lang/wgsl/sem/module.h"
#include "src/tint/lang/wgsl/sem/statement.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/utils/rtti/switch.h"

using namespace tint::core::fluent_types;  // NOLINT

TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::FoldConstants);

namespace tint::ast::transform {
namespace {

struct State {
    enum class Splat {
        kAllowed,
        kDisallowed,
    };

    void Replace(const ast::Expression* expr) {
        if (!expr) {
            return;
        }
        if (expr->Is<LiteralExpression>()) {
            return;
        }

        auto* with = Replacement(expr);
        if (!with) {
            return;
        }

        ctx.Replace(expr, with);
    }

    const ast::Expression* Replacement(const ast::Expression* expr) {
        auto& sem = ctx.src->Sem();
        auto* ve = sem.Get<sem::ValueExpression>(expr);
        // No sem node found
        if (!ve) {
            return nullptr;
        }

        auto* cv = ve->ConstantValue();
        // No constant value for this expression
        if (!cv) {
            return nullptr;
        }

        return Constant(cv);
    }

    const ast::Expression* Constant(const core::constant::Value* c) {
        if (c->Type()->HoldsAbstract()) {
            return nullptr;
        }

        auto composite = [&](Splat splat) -> const ast::Expression* {
            auto ty = FoldConstants::CreateASTTypeFor(ctx, c->Type());
            if (c->AllZero()) {
                return b.Call(ty);
            }
            if (splat == Splat::kAllowed && c->Is<core::constant::Splat>()) {
                return b.Call(ty, Constant(c->Index(0)));
            }

            Vector<const ast::Expression*, 8> els;
            for (size_t i = 0, n = c->NumElements(); i < n; i++) {
                els.Push(Constant(c->Index(i)));
            }
            return b.Call(ty, std::move(els));
        };

        return tint::Switch(
            c->Type(),  //
            [&](const core::type::I32*) { return b.Expr(c->ValueAs<i32>()); },
            [&](const core::type::U32*) { return b.Expr(c->ValueAs<u32>()); },
            [&](const core::type::F32*) { return b.Expr(c->ValueAs<f32>()); },
            [&](const core::type::F16*) { return b.Expr(c->ValueAs<f16>()); },
            [&](const core::type::Bool*) { return b.Expr(c->ValueAs<bool>()); },
            [&](const core::type::Array*) { return composite(Splat::kDisallowed); },
            [&](const core::type::Vector*) { return composite(Splat::kAllowed); },
            [&](const core::type::Matrix*) { return composite(Splat::kDisallowed); },
            [&](const core::type::Struct*) { return composite(Splat::kDisallowed); },
            TINT_ICE_ON_NO_MATCH);
    }

    void HandleStatement(const ast::Statement* stmt) {
        // In the case of things like the for-initializer we call `HandleStatement` without checking
        // the value which may not exist. Handle `nullptr` statements.
        if (!stmt) {
            return;
        }

        tint::Switch(
            stmt,  //
            [&](const ast::AssignmentStatement* a) { Replace(a->rhs); },
            [&](const ast::BlockStatement* blk) { HandleBlock(blk); },
            [&](const ast::BreakIfStatement* brk) { Replace(brk->condition); },
            [&](const ast::CallStatement* c) { Replace(c->expr); },
            [&](const ast::CompoundAssignmentStatement* c) { Replace(c->rhs); },
            [&](const ast::IfStatement* i) {
                Replace(i->condition);
                if (i->else_statement) {
                    HandleStatement(i->else_statement);
                }
                HandleBlock(i->body);
            },
            [&](const ast::LoopStatement* l) {
                HandleBlock(l->body);
                HandleBlock(l->continuing);
            },
            [&](const ast::ForLoopStatement* l) {
                HandleStatement(l->initializer);
                Replace(l->condition);
                HandleStatement(l->continuing);
                HandleBlock(l->body);
            },
            [&](const ast::WhileStatement* l) {
                Replace(l->condition);
                HandleBlock(l->body);
            },
            [&](const ast::ReturnStatement* r) { Replace(r->value); },
            [&](const ast::SwitchStatement* s) {
                Replace(s->condition);

                for (auto* it : s->body) {
                    for (auto* sel : it->selectors) {
                        Replace(sel->expr);
                    }
                    HandleBlock(it->body);
                }
            },
            [&](const ast::VariableDeclStatement* v) { Replace(v->variable->initializer); },
            [&](const ast::ConstAssert* c) { Replace(c->condition); },  //
            [&](const ast::IncrementDecrementStatement*) {},            //
            [&](const ast::ContinueStatement*) {},                      //
            [&](const ast::BreakStatement*) {},                         //
            [&](const ast::DiscardStatement*) {},                       //
            TINT_ICE_ON_NO_MATCH);
    }

    void HandleBlock(const ast::BlockStatement* block) {
        // Loop continuing blocks can be a `nullptr`.
        if (!block) {
            return;
        }

        for (auto* s : block->statements) {
            HandleStatement(s);

            if (auto* sem = src.Sem().Get(s);
                sem && !sem->Behaviors().Contains(sem::Behavior::kNext)) {
                break;  // Unreachable statement.
            }
        }
    }

    Transform::ApplyResult Run() {
        auto* sem = src.Sem().Module();
        for (auto* decl : sem->DependencyOrderedDeclarations()) {
            tint::Switch(
                decl,  //
                [&](const ast::Variable* var) { Replace(var->initializer); },
                [&](const ast::Function* func) { HandleBlock(func->body); },  //
                [&](const ast::Struct*) {},                                   //
                [&](const ast::Alias*) {},                                    //
                [&](const ast::Enable*) {},                                   //
                [&](const ast::ConstAssert*) {},                              //
                [&](const ast::DiagnosticDirective*) {},                      //
                [&](const ast::Requires*) {},                                 //
                TINT_ICE_ON_NO_MATCH);
        }

        ctx.Clone();

        return resolver::Resolve(b);
    }

    const Program& src;
    ProgramBuilder b;
    program::CloneContext ctx{&b, &src, /* auto_clone_symbols */ true};
};

}  // namespace

FoldConstants::FoldConstants() = default;

FoldConstants::~FoldConstants() = default;

Transform::ApplyResult FoldConstants::Apply(const Program& src, const DataMap&, DataMap&) const {
    State s{src, {}};
    return s.Run();
}

}  // namespace tint::ast::transform
