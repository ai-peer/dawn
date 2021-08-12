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

#include "src/tint/fuzzers/tint_ast_fuzzer/mutation_finders/delete_statements.h"

#include <memory>

#include "src/tint/fuzzers/tint_ast_fuzzer/mutations/delete_statement.h"
#include "src/tint/fuzzers/tint_ast_fuzzer/util.h"
#include "src/tint/sem/expression.h"
#include "src/tint/sem/statement.h"
#include "src/tint/sem/variable.h"

namespace tint::fuzzers::ast_fuzzer {

MutationList MutationFinderDeleteStatements::FindMutations(
    const tint::Program& program,
    NodeIdMap* node_id_map,
    ProbabilityContext* /*unused*/) const {
  MutationList result;

  // Iterate through all ast nodes and for each valid statement node
  // with allowed type, try to delete it.
  for (auto* node : program.ASTNodes().Objects()) {
    auto* statement_node = tint::As<ast::Statement>(node);

    // Transformation applies only when the node represents a
    // valid statement.
    if (!statement_node) {
      continue;
    }

    const auto* statement_sem_node =
        tint::As<sem::Statement>(program.Sem().Get(statement_node));

    // Transformation applies only when the sem node of the node
    // is present and of a valid type.
    if (!statement_sem_node) {
      continue;
    }

    // Transformation only applies to a subset of statement types
    // that can be safely deleted.
    if (!MutationDeleteStatement::CanBeDeleted(*statement_node)) {
      continue;
    }

    result.push_back(std::make_unique<MutationDeleteStatement>(
        node_id_map->GetId(statement_node)));
  }

  return result;
}

uint32_t MutationFinderDeleteStatements::GetChanceOfApplyingMutation(
    ProbabilityContext* probability_context) const {
  return probability_context->GetChanceOfDeletingStatements();
}

}  // tint::fuzzers::ast_fuzzer
