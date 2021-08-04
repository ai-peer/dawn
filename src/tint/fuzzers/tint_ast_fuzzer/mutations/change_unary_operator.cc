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

#include "src/tint/fuzzers/tint_ast_fuzzer/mutations/change_unary_operator.h"

#include <utility>

#include "src/tint/fuzzers/tint_ast_fuzzer/util.h"
#include "src/tint/program_builder.h"

namespace tint {
namespace fuzzers {
namespace ast_fuzzer {

MutationChangeUnaryOperator::MutationChangeUnaryOperator(
    protobufs::MutationChangeUnaryOperator message)
    : message_(std::move(message)) {}

MutationChangeUnaryOperator::MutationChangeUnaryOperator(
    uint32_t expression_id,
    ast::UnaryOp new_unary_op) {
  message_.set_expression_id(expression_id);
  message_.set_new_unary_op(static_cast<uint32_t>(new_unary_op));
}

bool MutationChangeUnaryOperator::IsApplicable(
    const tint::Program& program,
    const NodeIdMap& node_id_map) const {
  const auto* unary_expr_ast_node = tint::As<ast::UnaryOpExpression>(
      node_id_map.GetNode(message_.expression_id()));

  if (!unary_expr_ast_node) {
    // The expression id is invalid or it does not refer to a
    // unary expression type.
    return false;
  }

  // Map index to its corresponding binary operator type.
  auto new_unary_operator = static_cast<ast::UnaryOp>(message_.new_unary_op());

  // Get the semantic node of the expression.
  const auto* unary_expr_sem_node =
      tint::As<sem::Expression>(program.Sem().Get(unary_expr_ast_node));

  // Semantic node for the expression is not present.
  assert(unary_expr_sem_node &&
         "Semantic node for unary expression must never be null");

  // Only signed integer or vector of signed integer has more than 1
  // unary operators to change between.
  if (!unary_expr_sem_node->Type()->is_signed_scalar_or_vector()) {
    return false;
  }

  // The new unary operator must not be the same as the original one.
  if (new_unary_operator !=
      GetValidUnaryOpForSignedInt(unary_expr_ast_node->op)) {
    return false;
  }

  return true;
}

void MutationChangeUnaryOperator::Apply(const NodeIdMap& node_id_map,
                                        tint::CloneContext* clone_context,
                                        NodeIdMap* /*unused*/) const {
  const auto* unary_expression_node = tint::As<ast::UnaryOpExpression>(
      node_id_map.GetNode(message_.expression_id()));

  ast::UnaryOpExpression* cloned_replacement_node =
      clone_context->dst->create<ast::UnaryOpExpression>(
          static_cast<ast::UnaryOp>(message_.new_unary_op()),
          clone_context->Clone(unary_expression_node->expr));

  clone_context->Replace(unary_expression_node, cloned_replacement_node);
}

protobufs::Mutation MutationChangeUnaryOperator::ToMessage() const {
  protobufs::Mutation mutation;
  *mutation.mutable_change_unary_operator() = message_;
  return mutation;
}

ast::UnaryOp MutationChangeUnaryOperator::GetValidUnaryOpForSignedInt(
    const ast::UnaryOp& original_op) {
  if (original_op == ast::UnaryOp::kComplement) {
    return ast::UnaryOp::kNegation;
  }
  return ast::UnaryOp::kComplement;
}

}  // namespace ast_fuzzer
}  // namespace fuzzers
}  // namespace tint
