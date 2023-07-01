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

#include "src/tint/ast/transform/fold_trivial_lets.h"

#include <utility>

#include "src/tint/ast/traverse_expressions.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/value_expression.h"
#include "src/tint/utils/hashmap.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::transform::FoldTrivialLets);

namespace tint::ast::transform {

/// PIMPL state for the transform
struct FoldTrivialLets::State {
    /// The source program
    const Program* const src;
    /// The target program builder
    ProgramBuilder b;
    /// The clone context
    CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};
    /// The semantic info.
    const sem::Info& sem = {ctx.src->Sem()};

    /// Constructor
    /// @param program the source program
    explicit State(const Program* program) : src(program) {}

    /// Process a block.
    /// @param block the block
    void ProcessBlock(const BlockStatement* block) {
        struct PendingLet {
            const VariableDeclStatement* decl = nullptr;
            const Expression* expr = nullptr;
            size_t remaining_uses = 0;
        };
        utils::Hashmap<Symbol, PendingLet, 4> pending_lets;

        auto fold_lets = [&](const Expression* expr) {
            TraverseExpressions(expr, b.Diagnostics(), [&](const IdentifierExpression* ident) {
                auto itr = pending_lets.Find(ident->identifier->symbol);
                if (itr) {
                    auto& pending = (*itr.Get());
                    ctx.Replace(ident, ctx.Clone(pending.expr));
                    if (--pending.remaining_uses == 0) {
                        ctx.Remove(block->statements, pending.decl);
                    }
                }
                return TraverseAction::Descend;
            });
        };

        for (auto* stmt : block->statements) {
            if (auto* decl = stmt->As<VariableDeclStatement>()) {
                if (auto* let = decl->variable->As<Let>()) {
                    if (!sem.GetVal(let->initializer)->HasSideEffects()) {
                        fold_lets(let->initializer);
                        if (let->initializer->Is<IdentifierExpression>()) {
                            pending_lets.Add(
                                let->name->symbol,
                                PendingLet{decl, let->initializer, sem.Get(let)->Users().Length()});
                        } else if (sem.Get(let)->Users().Length() == 1) {
                            pending_lets.Add(let->name->symbol,
                                             PendingLet{decl, let->initializer, 1});
                        }
                        continue;
                    }
                }
            }

            if (auto* assign = stmt->As<AssignmentStatement>()) {
                if (!sem.GetVal(assign->lhs)->HasSideEffects() &&
                    !sem.GetVal(assign->rhs)->HasSideEffects()) {
                    fold_lets(assign->rhs);
                }
            }
            if (auto* ifelse = stmt->As<IfStatement>()) {
                if (!sem.GetVal(ifelse->condition)->HasSideEffects()) {
                    fold_lets(ifelse->condition);
                }
            }
            pending_lets.Clear();
        }
    }

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        for (auto* node : src->ASTNodes().Objects()) {
            if (auto* block = node->As<BlockStatement>()) {
                ProcessBlock(block);
            }
        }

        ctx.Clone();
        return Program(std::move(b));
    }
};

FoldTrivialLets::FoldTrivialLets() = default;

FoldTrivialLets::~FoldTrivialLets() = default;

Transform::ApplyResult FoldTrivialLets::Apply(const Program* src, const DataMap&, DataMap&) const {
    return State(src).Run();
}

}  // namespace tint::ast::transform
