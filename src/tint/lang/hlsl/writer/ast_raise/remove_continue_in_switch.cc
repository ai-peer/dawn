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

#include "src/tint/lang/hlsl/writer/ast_raise/remove_continue_in_switch.h"

#include <string>
#include <utility>

#include "src/tint/lang/wgsl/ast/continue_statement.h"
#include "src/tint/lang/wgsl/ast/switch_statement.h"
#include "src/tint/lang/wgsl/ast/transform/get_insertion_point.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/program/program_builder.h"
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/sem/block_statement.h"
#include "src/tint/lang/wgsl/sem/loop_statement.h"
#include "src/tint/lang/wgsl/sem/switch_statement.h"
#include "src/tint/utils/containers/hashmap.h"
#include "src/tint/utils/containers/hashset.h"

TINT_INSTANTIATE_TYPEINFO(tint::hlsl::writer::RemoveContinueInSwitch);

namespace tint::hlsl::writer {

/// PIMPL state for the transform
struct RemoveContinueInSwitch::State {
    /// Constructor
    /// @param program the source program
    explicit State(const Program* program) : src(program) {}

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        // First collect all switch statements within loops that contain a continue statement.
        for (auto* node : src->ASTNodes().Objects()) {
            auto* cont = node->As<ast::ContinueStatement>();
            if (!cont) {
                continue;
            }

            // If first parent is not a switch within a loop, skip
            auto* switch_stmt = GetParentSwitchInLoop(sem, cont);
            if (!switch_stmt) {
                continue;
            }

            const auto& r = switch_info_map.Add(switch_stmt, SwitchInfo{});
            auto* info = r.value;
            if (r) {
                info->switch_stmt = switch_stmt;
                info->loop_block = sem.Get(switch_stmt)->FindFirstParent<sem::LoopBlockStatement>();
                switch_infos.Push(info);
            }
            info->continues.Push(cont);
        }

        if (switch_infos.IsEmpty()) {
            return SkipTransform;
        }

        // For each switch statement:
        // 1. Declare a 'tint_continue' var just before the parent loop, and reset it to false at
        // the top of the loop body.
        // 2. Replace 'continue' with '{ tint_continue = true; break; }'
        // 3. Insert 'if (tint_continue) { break; }' after switch, and all parent switches, except
        // for the parent-most, for which we insert 'if (tint_continue) { continue; }'
        for (auto* info : switch_infos) {
            auto var_name = loop_to_var.GetOrCreate(info->loop_block, [&] {
                // Create and insert 'var tint_continue : bool;' before loop
                auto var_name = b.Symbols().New("tint_continue");
                auto* decl = b.Decl(b.Var(var_name, b.ty.bool_()));
                auto ip = ast::transform::utils::GetInsertionPoint(
                    ctx, info->loop_block->Parent()->Declaration());
                ctx.InsertBefore(ip.first->Declaration()->statements, ip.second, decl);

                // Insert 'tint_continue = false' at top of loop body
                auto assign_false = b.Assign(var_name, false);
                ctx.InsertFront(info->loop_block->Declaration()->statements, assign_false);

                return var_name;
            });

            for (auto& c : info->continues) {
                // Replace 'continue;' with '{ tint_continue = true; break; }'
                ctx.Replace(c, b.Assign(b.Expr(var_name), true));
                auto ip = ast::transform::utils::GetInsertionPoint(ctx, c);
                ctx.InsertAfter(ip.first->Declaration()->statements, ip.second, b.Break());
            }

            // Insert 'if (tint_continue) { break; }' after switch, and all parent switches,
            // except for the parent-most, for which we insert 'if (tint_continue) { continue; }'
            auto* curr_switch = info->switch_stmt;
            while (curr_switch) {
                auto* parent = sem.Get(curr_switch)->Parent()->Declaration();
                auto* next_switch = GetParentSwitchInLoop(sem, parent);

                if (switch_handles_continue.Add(curr_switch)) {
                    const ast::IfStatement* if_stmt = nullptr;
                    if (next_switch) {
                        if_stmt = b.If(b.Expr(var_name), b.Block(b.Break()));
                    } else {
                        if_stmt = b.If(b.Expr(var_name), b.Block(b.Continue()));
                    }
                    auto ip = ast::transform::utils::GetInsertionPoint(ctx, curr_switch);
                    ctx.InsertAfter(ip.first->Declaration()->statements, ip.second, if_stmt);
                }

                curr_switch = next_switch;
            }
        }

        ctx.Clone();
        return resolver::Resolve(b);
    }

  private:
    /// The source program
    const Program* const src;
    /// The target program builder
    ProgramBuilder b;
    /// The clone context
    program::CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};
    /// Alias to src->sem
    const sem::Info& sem = src->Sem();

    // Info for each switch statement within a loop that contains at least one continue statement.
    struct SwitchInfo {
        const ast::SwitchStatement* switch_stmt;
        const sem::LoopBlockStatement* loop_block;
        Vector<const ast::ContinueStatement*, 4> continues;
    };

    // Map of switch statements to per-switch info for switch statements within a loop that contains
    // at least one continue statement.
    Hashmap<const ast::SwitchStatement*, SwitchInfo, 4> switch_info_map;

    // Vector of SwitchInfos added to switch_info_map to ensure ordering for deterministic
    // traversal.
    Vector<SwitchInfo*, 4> switch_infos;

    // Map of loop block statement to the single 'tint_continue' variable used to replace 'continue'
    // control flow.
    Hashmap<const sem::LoopBlockStatement*, Symbol, 4> loop_to_var;

    // Set used to avoid duplicating 'if (tint_continue) { break/continue; }' after each switch
    // within a loop.
    Hashset<const ast::SwitchStatement*, 4> switch_handles_continue;

    // If `stmt` is within a switch statement within a loop, returns a pointer to
    // that switch statement.
    static const ast::SwitchStatement* GetParentSwitchInLoop(const sem::Info& sem,
                                                             const ast::Statement* stmt) {
        // Find whether first parent is a switch or a loop
        auto* sem_stmt = sem.Get(stmt);
        auto* sem_parent =
            sem_stmt->FindFirstParent<sem::SwitchStatement, sem::LoopBlockStatement>();

        if (!sem_parent) {
            return nullptr;
        }
        return sem_parent->Declaration()->As<ast::SwitchStatement>();
    }
};

RemoveContinueInSwitch::RemoveContinueInSwitch() = default;
RemoveContinueInSwitch::~RemoveContinueInSwitch() = default;

ast::transform::Transform::ApplyResult RemoveContinueInSwitch::Apply(
    const Program* src,
    const ast::transform::DataMap&,
    ast::transform::DataMap&) const {
    State state(src);
    return state.Run();
}

}  // namespace tint::hlsl::writer
