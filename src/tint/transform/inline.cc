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

#include "src/tint/transform/inline.h"

#include "src/tint/program_builder.h"
#include "src/tint/sem/block_statement.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/module.h"
#include "src/tint/sem/statement.h"
#include "src/tint/transform/utils/hoist_to_decl_before.h"
#include "src/tint/utils/transform.h"
#include "src/tint/utils/vector.h"

using namespace tint::number_suffixes;  // NOLINT

TINT_INSTANTIATE_TYPEINFO(tint::transform::Inline);

namespace tint::transform {

struct Inline::State {
    CloneContext& ctx;
    const sem::Info& sem = ctx.src->Sem();
    ProgramBuilder& b = *ctx.dst;

    void Run() {
        for (auto* decl : ctx.src->Sem().Module()->DependencyOrderedDeclarations()) {
            if (auto* fn_decl = decl->As<ast::Function>()) {
                const bool should_inline = true;
                auto* fn = sem.Get(fn_decl);
                if (should_inline) {
                    for (auto* call_site : fn->CallSites()) {
                        const auto* block = call_site->Stmt()->Parent()->As<sem::BlockStatement>();
                        const auto& block_stmts = block->Declaration()->statements;
                        const auto* expr_stmt = call_site->Stmt();

                        auto inlined = Inline(fn);
                        for (auto* inlined_stmt : inlined.body) {
                            ctx.InsertBefore(block_stmts, expr_stmt->Declaration(), inlined_stmt);
                        }
                        if (auto* call_stmt = expr_stmt->Declaration()->As<ast::CallStatement>()) {
                            if (call_stmt->expr == call_site->Declaration()) {
                                ctx.Remove(block_stmts, expr_stmt->Declaration());
                            }
                        }
                    }
                }
            }
        }
    }

    struct Inlined {
        const utils::Vector<const ast::Statement*, 2> body;
        const Symbol return_value;
    };

    Inlined Inline(const sem::Function* fn) const {
        auto stmts = utils::Transform(fn->Declaration()->body->statements,
                                      [&](auto* stmt) { return ctx.Clone(stmt); });
        return {{b.Block(stmts)}, Symbol{}};
    }
};

Inline::Inline() = default;

Inline::~Inline() = default;

void Inline::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    State{ctx}.Run();

    ctx.Clone();
}

}  // namespace tint::transform
