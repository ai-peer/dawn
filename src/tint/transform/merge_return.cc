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

#include "src/tint/transform/merge_return.h"

#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/statement.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::MergeReturn);

using namespace tint::number_suffixes;  // NOLINT

namespace tint::transform {

MergeReturn::MergeReturn() = default;

MergeReturn::~MergeReturn() = default;

bool MergeReturn::ShouldRun(const Program* program, const DataMap&) const {
    for (auto* func : program->AST().Functions()) {
        // TODO(jrprice): We could use the uniformity analysis to determine the transform is needed.
        if (!func->IsEntryPoint()) {
            return true;
        }
    }
    return false;
}

namespace {

/// Internal class used to during the transform.
class State {
  private:
    /// The clone context.
    CloneContext& ctx;

    /// The program builder.
    ProgramBuilder& b;

    /// The function.
    const ast::Function* function;

    /// The symbol for the return flag variable.
    Symbol flag;

    /// The symbol for the return value variable.
    Symbol retval;

    /// Loop/switch nesting depth.
    uint32_t loop_switch_depth = 0;

  public:
    /// Constructor
    /// @param context the clone context
    explicit State(CloneContext& context, const ast::Function* func)
        : ctx(context), b(*ctx.dst), function(func) {}

    /// Process a statement (recursively).
    void ProcessStatement(const ast::Statement* stmt) {
        // Helper to check if a statement has the behavior `behavior`.
        auto has_behavior = [&](const ast::Statement* stmt, sem::Behavior behavior) {
            return ctx.src->Sem().Get(stmt)->Behaviors().Contains(behavior);
        };

        if (stmt == nullptr) {
            return;
        }
        if (!has_behavior(stmt, sem::Behavior::kReturn)) {
            return;
        }

        Switch(
            stmt,
            [&](const ast::BlockStatement* block) {
                // We will rebuild the contents of the block statement.
                // We may introduce conditionals around statements that follow a statement with the
                // `Return` behavior, so build a stack of statement lists that represent the new
                // (potentially nested) conditional blocks.
                utils::Vector<utils::Vector<const ast::Statement*, 8>, 8> new_stmts({{}});

                // Insert the return flag and return value variables at the top of the function.
                if (block == function->body) {
                    // Avoid transforming functions that already have a single exit point.
                    // TODO(jrprice): Alternatively, use the uniformity analysis to decide which
                    // functions need to be transformed.
                    for (auto* stmt : block->statements) {
                        // Find the first statement that has contains the Return behavior.
                        if (has_behavior(stmt, sem::Behavior::kReturn)) {
                            // If this statement is itself a return, it will be the only exit point,
                            // so no need to apply the transform to the function.
                            if (stmt->Is<ast::ReturnStatement>()) {
                                return;
                            } else {
                                // Apply the transform in all other cases.
                                break;
                            }
                        }
                    }

                    flag = b.Symbols().New("tint_return_flag");
                    new_stmts[0].Push(b.Decl(b.Var(flag, b.ty.bool_())));

                    if (!function->return_type->Is<ast::Void>()) {
                        retval = b.Symbols().New("tint_return_value");
                        new_stmts[0].Push(b.Decl(b.Var(retval, ctx.Clone(function->return_type))));
                    }
                }

                for (auto* stmt : block->statements) {
                    // Process the statement and add it to the current block.
                    ProcessStatement(stmt);
                    new_stmts.Back().Push(ctx.Clone(stmt));

                    // Check if the statement is or contains a return statement.
                    // We need to make sure any statements that follow this one do not get executed
                    // if the return flag has been set.
                    if (has_behavior(stmt, sem::Behavior::kReturn)) {
                        if (loop_switch_depth > 0) {
                            // We're in a loop/switch, and so we would have inserted a `break`.
                            // If we've just come out of a loop/switch statement, we need to `break`
                            // again.
                            if (stmt->IsAnyOf<ast::LoopStatement, ast::ForLoopStatement,
                                              ast::SwitchStatement>()) {
                                // If the loop only has the 'Return' behavior, we can just
                                // unconditionally break. Otherwise check the return flag.
                                if (has_behavior(stmt, sem::Behavior::kNext)) {
                                    new_stmts.Back().Push(
                                        b.If(b.Expr(flag), b.Block(utils::Vector{b.Break()})));
                                } else {
                                    new_stmts.Back().Push(b.Break());
                                }
                            }
                        } else {
                            // Create a new list for any subsequent statements, which we will wrap
                            // in a conditional block.
                            new_stmts.Push({});
                        }
                    }
                }

                // Descend the stack of new block statements, wrapping them in conditionals.
                while (new_stmts.Length() > 1) {
                    const ast::IfStatement* i = nullptr;
                    if (new_stmts.Back().Length() > 0) {
                        i = b.If(b.Not(b.Expr(flag)), b.Block(new_stmts.Back()));
                    }
                    new_stmts.Pop();
                    if (i) {
                        new_stmts.Back().Push(i);
                    }
                }

                // Insert the final return statement at the end of the function body.
                if (block == function->body && retval.IsValid()) {
                    new_stmts[0].Push(b.Return(b.Expr(retval)));
                }

                ctx.Replace(block, b.Block(new_stmts[0]));
            },
            [&](const ast::CaseStatement* c) { ProcessStatement(c->body); },
            [&](const ast::ForLoopStatement* f) {
                loop_switch_depth++;
                ProcessStatement(f->body);
                loop_switch_depth--;
            },
            [&](const ast::IfStatement* i) {
                ProcessStatement(i->body);
                ProcessStatement(i->else_statement);
            },
            [&](const ast::LoopStatement* l) {
                loop_switch_depth++;
                ProcessStatement(l->body);
                loop_switch_depth--;
            },
            [&](const ast::ReturnStatement* r) {
                utils::Vector<const ast::Statement*, 8> stmts;
                // Set the return flag to signal that we have hit a return.
                stmts.Push(b.Assign(b.Expr(flag), true));
                if (r->value) {
                    // Set the return value if necessary.
                    stmts.Push(b.Assign(b.Expr(retval), ctx.Clone(r->value)));
                }
                if (loop_switch_depth > 0) {
                    // If we are in a loop or switch statement, break out of it.
                    stmts.Push(b.Break());
                }
                ctx.Replace(r, b.Block(stmts));
            },
            [&](const ast::SwitchStatement* s) {
                loop_switch_depth++;
                for (auto* c : s->body) {
                    ProcessStatement(c);
                }
                loop_switch_depth--;
            },
            [&](Default) { TINT_ICE(Transform, b.Diagnostics()) << "unhandled statement type"; });
    }
};

}  // namespace

void MergeReturn::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    for (auto* func : ctx.src->AST().Functions()) {
        if (func->IsEntryPoint()) {
            continue;
        }

        State state(ctx, func);
        state.ProcessStatement(func->body);
    }

    ctx.Clone();
}

}  // namespace tint::transform
