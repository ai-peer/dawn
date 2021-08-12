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

#include "src/tint/fuzzers/tint_ast_fuzzer/jump_tracker.h"

#include <cassert>
#include <unordered_set>

#include "src/tint/ast/break_statement.h"
#include "src/tint/ast/discard_statement.h"
#include "src/tint/ast/for_loop_statement.h"
#include "src/tint/ast/loop_statement.h"
#include "src/tint/ast/return_statement.h"
#include "src/tint/ast/switch_statement.h"
#include "src/tint/sem/statement.h"

namespace tint::fuzzers::ast_fuzzer {

JumpTracker::JumpTracker(const Program& program) {
    for (auto* node : program.ASTNodes().Objects()) {
        auto* stmt = node->As<ast::Statement>();
        if (stmt->As<ast::BreakStatement>()) {
            bool found_innermost_loop = false;
            std::unordered_set<const ast::Statement*> candidate_statements;
            const ast::Statement* current = stmt;
            while (true) {
                if (current->Is<ast::ForLoopStatement>() || current->Is<ast::LoopStatement>()) {
                    found_innermost_loop = true;
                    break;
                }
                if (current->Is<ast::SwitchStatement>()) {
                    assert(!found_innermost_loop &&
                           "Should have quit search if a loop was encountered.");
                    break;
                }
                candidate_statements.insert(current);
                current = As<sem::Statement>(program.Sem().Get(current))->Parent()->Declaration();
            }
            if (found_innermost_loop) {
                for (auto* candidate : candidate_statements) {
                    contains_break_for_innermost_loop_.insert(candidate);
                }
            }
        } else if (stmt->As<ast::ReturnStatement>() || stmt->As<ast::DiscardStatement>()) {
            auto& target_set = stmt->As<ast::ReturnStatement>() ? contains_return_
                                                                : contains_intraprocedural_discard_;
            const ast::Statement* current = stmt;
            while (true) {
                target_set.insert(current);
                auto* parent = program.Sem().Get(current)->Parent();
                if (parent == nullptr) {
                    break;
                }
                current = parent->Declaration();
            }
        }
    }
}
}  // namespace tint::fuzzers::ast_fuzzer
