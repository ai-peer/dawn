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

#include <utility>
#include <vector>

#include "src/tint/fuzzers/tint_ast_fuzzer/util.h"
#include "src/tint/program_builder.h"

namespace tint::fuzzers::ast_fuzzer {

MutationDeleteStatement::MutationDeleteStatement(
    protobufs::MutationDeleteStatement message)
    : message_(std::move(message)) {}

MutationDeleteStatement::MutationDeleteStatement(uint32_t statement_id) {
  message_.set_statement_id(statement_id);
}

bool MutationDeleteStatement::IsApplicable(const tint::Program& program,
                                           const NodeIdMap& node_id_map) const {
  auto* statement_node =
      tint::As<ast::Statement>(node_id_map.GetNode(message_.statement_id()));

  if (!statement_node) {
    // The statement id is invalid or does not refer to a statement.
    return false;
  }

  const auto* statement_sem_node =
      tint::As<sem::Statement>(program.Sem().Get(statement_node));

  if (!statement_sem_node) {
    // Semantic information for the statement ast node is not present
    // or the semantic node is not a valid statement type node.
    return false;
  }

  // Check if the statement type is allowed to be deleted.
  if (!CanBeDeleted(*statement_node)) {
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

  const std::vector<const ast::Statement*> statement_list =
      tint::Is<ast::BlockStatement>(statement_node)
          ? statement_sem_node->Parent()
                ->Declaration()
                ->As<ast::BlockStatement>()
                ->statements
          : statement_sem_node->Block()
                ->Declaration()
                ->As<ast::BlockStatement>()
                ->statements;

  assert(!statement_list.empty() &&
         "The block that contains target node must not be empty!");

  // The statement list must contain the target node.
  bool found_node = false;
  for (auto stmt : statement_list)
    if (stmt == statement_node)
      found_node = true;

  assert(found_node && "No node found.");

  // Remove the statement from the cloned copy of the vector that contains
  // the node.
  clone_context->Remove(statement_list, statement_node);
}

protobufs::Mutation MutationDeleteStatement::ToMessage() const {
  protobufs::Mutation mutation;
  *mutation.mutable_delete_statement() = message_;
  return mutation;
}

bool MutationDeleteStatement::CanBeDeleted(
    const ast::Statement& statement_node) {
  if (statement_node.Is<ast::VariableDeclStatement>()) {
    return false;
  }

  if (statement_node.Is<ast::ReturnStatement>()) {
    return false;
  }
  // For now, only variable declaration statement and return
  // statement are not allowed to be deleted.
  // However, one should be aware the complexity of deleting
  // a statement in a for loop or loop statement.
  // There are break statements inside of loop statements
  // that if deleted, will cause infinite loops.
  // Meanwhile, certain assignment statements that can update
  // the condition when deleted might also cause the termination
  // condition to never be met. Those conditions should be
  // considered with follow-ups.
  // One way is to have a breath first search method that
  // iteratively finds the child statements of every level
  // and find the presence of break or assignment statement,
  // and return false to isApplicable if such statement types
  // exists somewhere in the loop statement.

  return true;
}

}  // namespace tint::fuzzers::ast_fuzzer
