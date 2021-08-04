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

#include "src/tint/fuzzers/tint_ast_fuzzer/mutation_finders/change_unary_operators.h"

#include <memory>

#include "src/tint/fuzzers/tint_ast_fuzzer/mutations/change_unary_operator.h"
#include "src/tint/fuzzers/tint_ast_fuzzer/util.h"
#include "src/tint/sem/expression.h"
#include "src/tint/sem/statement.h"
#include "src/tint/sem/variable.h"

namespace tint::fuzzers::ast_fuzzer {

MutationList MutationFinderChangeUnaryOperators::FindMutations(
    const tint::Program& program,
    NodeIdMap* node_id_map,
    ProbabilityContext* /*unused*/) const {
  MutationList result;

  // Iterate through all ast nodes and for each valid unary op expression node,
  // try to replace the node's unary operator.
  for (const auto* node : program.ASTNodes().Objects()) {
    const auto* ast_unary_expr = tint::As<ast::UnaryOpExpression>(node);

    // Transformation applies only when the node represents a valid unary
    // expression.
    if (!ast_unary_expr) {
      continue;
    }

    // Get the expression from unary op expression node.
    const auto* expression = ast_unary_expr->expr;

    assert(expression &&
           "Expression node for unary expression must never be null");

    // Get the semantic node for the expression.
    const auto* expr_sem_node =
        tint::As<sem::Expression>(program.Sem().Get(expression));

    // Only signed integer or vector of signed integer can
    // be mutated.
    if (!expr_sem_node->Type()->is_signed_scalar_or_vector()) {
      continue;
    }

    result.push_back(std::make_unique<MutationChangeUnaryOperator>(
        node_id_map->GetId(ast_unary_expr),
        MutationChangeUnaryOperator::GetValidUnaryOpForSignedInt(
            ast_unary_expr->op)));
  }

  return result;
}

uint32_t MutationFinderChangeUnaryOperators::GetChanceOfApplyingMutation(
    ProbabilityContext* probability_context) const {
  return probability_context->GetChanceOfChangingUnaryOperators();
}

}  // namespace tint::fuzzers::ast_fuzzer
