// Copyright 2021 The Tint Authors.
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

#include "src/tint/fuzzers/tint_ast_fuzzer/mutations/delete_statement.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "src/tint/ast/block_statement.h"
#include "src/tint/ast/fallthrough_statement.h"
#include "src/tint/ast/for_loop_statement.h"
#include "src/tint/ast/if_statement.h"
#include "src/tint/ast/loop_statement.h"
#include "src/tint/ast/variable_decl_statement.h"
#include "src/tint/fuzzers/tint_ast_fuzzer/jump_tracker.h"
#include "src/tint/fuzzers/tint_ast_fuzzer/util.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/for_loop_statement.h"
#include "src/tint/sem/if_statement.h"
#include "src/tint/sem/loop_statement.h"
#include "src/tint/sem/statement.h"

namespace tint::fuzzers::ast_fuzzer {

MutationDeleteStatement::MutationDeleteStatement(protobufs::MutationDeleteStatement message)
    : message_(std::move(message)) {}

MutationDeleteStatement::MutationDeleteStatement(uint32_t statement_id) {
    message_.set_statement_id(statement_id);
}

bool MutationDeleteStatement::IsApplicable(const tint::Program& program,
                                           const NodeIdMap& node_id_map) const {
    auto* statement_node = tint::As<ast::Statement>(node_id_map.GetNode(message_.statement_id()));

    if (!statement_node) {
        // The statement id is invalid or does not refer to a statement.
        return false;
    }

    const auto* statement_sem_node = tint::As<sem::Statement>(program.Sem().Get(statement_node));

    if (!statement_sem_node) {
        // Semantic information for the statement is not available. This
        // information is required in order to perform the deletion.
        return false;
    }

    // Check whether it is OK to delete this statement.
    if (!CanBeDeleted(*statement_node, program, JumpTracker(program))) {
        return false;
    }

    return true;
}

void MutationDeleteStatement::Apply(const NodeIdMap& node_id_map,
                                    tint::CloneContext* clone_context,
                                    NodeIdMap* /* unused */) const {
    const auto* statement_node =
        tint::As<ast::Statement>(node_id_map.GetNode(message_.statement_id()));
    const auto* statement_sem_node =
        tint::As<sem::Statement>(clone_context->src->Sem().Get(statement_node));

    if (tint::Is<sem::IfStatement>(statement_sem_node->Parent()) &&
        tint::As<ast::IfStatement>(statement_sem_node->Parent()->Declaration())->else_statement ==
            statement_node) {
        clone_context->Replace(statement_node, static_cast<const ast::Statement*>(nullptr));
    } else if (tint::Is<sem::ForLoopStatement>(statement_sem_node->Parent()) &&
               tint::As<ast::ForLoopStatement>(statement_sem_node->Parent()->Declaration())
                       ->initializer == statement_node) {
        clone_context->Replace(statement_node, static_cast<const ast::Statement*>(nullptr));
    } else if (tint::Is<sem::ForLoopStatement>(statement_sem_node->Parent()) &&
               tint::As<ast::ForLoopStatement>(statement_sem_node->Parent()->Declaration())
                       ->continuing == statement_node) {
        clone_context->Replace(statement_node, static_cast<const ast::Statement*>(nullptr));
    } else if (tint::Is<sem::LoopContinuingBlockStatement>(statement_sem_node)) {
        clone_context->Replace(statement_node, static_cast<const ast::Statement*>(nullptr));
    } else if (tint::Is<ast::BlockStatement>(statement_node)) {
        const auto& statement_list =
            statement_sem_node->Parent()->Declaration()->As<ast::BlockStatement>()->statements;
        assert(std::find(statement_list.begin(), statement_list.end(), statement_node) !=
                   statement_list.end() &&
               "Statement not found.");
        clone_context->Remove(statement_list, statement_node);
    } else if (tint::Is<ast::CaseStatement>(statement_node)) {
        const std::vector<const ast::CaseStatement*>* case_statement_list =
            &statement_sem_node->Parent()->Declaration()->As<ast::SwitchStatement>()->body;
        assert(std::find(case_statement_list->begin(), case_statement_list->end(),
                         statement_node) != case_statement_list->end() &&
               "Statement not found.");
        clone_context->Remove(*case_statement_list, statement_node);
    } else {
        const auto& statement_list =
            statement_sem_node->Block()->Declaration()->As<ast::BlockStatement>()->statements;
        assert(std::find(statement_list.begin(), statement_list.end(), statement_node) !=
                   statement_list.end() &&
               "Statement not found.");
        clone_context->Remove(statement_list, statement_node);
    }
}

protobufs::Mutation MutationDeleteStatement::ToMessage() const {
    protobufs::Mutation mutation;
    *mutation.mutable_delete_statement() = message_;
    return mutation;
}

bool MutationDeleteStatement::CanBeDeleted(const ast::Statement& statement_node,
                                           const Program& program,
                                           const JumpTracker& jump_tracker) {
    if (statement_node.Is<ast::VariableDeclStatement>()) {
        // This is conservative. It would be possible to delete variable declarations if they are
        // not used. Further analysis could allow that.
        return false;
    }

    if (jump_tracker.ContainsReturn(statement_node) ||
        jump_tracker.ContainsIntraproceduralDiscard(statement_node)) {
        // This is conservative. It would be possible to delete a return/discard statement as long
        // as there is still a return/discard on every control flow path.
        return false;
    }

    if (jump_tracker.ContainsBreakForInnermostLoop(statement_node)) {
        // This is conservative. Disallowing the removal of breaks ensures that loops cannot become
        // statically infinite. However, a loop might in practice have multiple breaks, some of
        // which can be removed.
        return false;
    }

    if (auto* case_statement = statement_node.As<ast::CaseStatement>()) {
        auto* switch_statement =
            program.Sem().Get(case_statement)->Parent()->Declaration()->As<ast::SwitchStatement>();
        size_t index;
        for (index = 0; index < switch_statement->body.size(); index++) {
            if (switch_statement->body[index] == case_statement) {
                break;
            }
        }
        if (index > 0 && index == switch_statement->body.size() - 1) {
            auto& previous_case_statement_body_statements =
                switch_statement->body[index - 1]->body->statements;
            if (previous_case_statement_body_statements.size() > 0 &&
                previous_case_statement_body_statements
                    [previous_case_statement_body_statements.size() - 1]
                        ->Is<ast::FallthroughStatement>()) {
                return false;
            }
        }
    }

    auto* parent_sem = program.Sem().Get(&statement_node)->Parent();
    if (parent_sem == nullptr) {
        return false;
    }

    auto* parent_stmt = parent_sem->Declaration();
    if (auto* for_loop = parent_stmt->As<ast::ForLoopStatement>()) {
        if (for_loop->body == &statement_node) {
            return false;
        }
    }
    if (auto* if_statement = parent_stmt->As<ast::IfStatement>()) {
        if (if_statement->body == &statement_node) {
            return false;
        }
    }

    return true;
}

}  // namespace tint::fuzzers::ast_fuzzer
