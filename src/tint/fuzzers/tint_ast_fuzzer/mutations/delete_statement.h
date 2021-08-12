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

#ifndef SRC_TINT_FUZZERS_TINT_AST_FUZZER_MUTATIONS_DELETE_STATEMENT_H_
#define SRC_TINT_FUZZERS_TINT_AST_FUZZER_MUTATIONS_DELETE_STATEMENT_H_

#include "src/tint/fuzzers/tint_ast_fuzzer/mutation.h"
#include "src/tint/sem/variable.h"

namespace tint::fuzzers::ast_fuzzer {

/// @see MutationDeleteStatement::Apply
class MutationDeleteStatement : public Mutation {
 public:
  /// @brief Constructs an instance of this mutation from a protobuf message.
  /// @param message - protobuf message
  explicit MutationDeleteStatement(protobufs::MutationDeleteStatement message);

  /// @brief Constructor.
  /// @param statement_id - the id of the `ast::Statement` instance to delete.
  explicit MutationDeleteStatement(uint32_t statement_id);

  /// @copybrief Mutation::IsApplicable
  ///
  /// The mutation is applicable iff:
  /// - `statement_id` is an id of an `ast::Statement`, that references
  ///   a valid statement.
  /// - `statement_id` does not refer to a variable declaration, since the
  ///   declared variables will be inaccessible if the statement is deleted.
  /// - `statement_id` is not a return statement.
  /// - `statement_id` does not contain a top-level break statement, since
  ///   removing such a statement may lead to a loop that can be statically
  ///   observed to be non-terminating, which is not valid.
  ///
  /// @copydetails Mutation::IsApplicable
  bool IsApplicable(const tint::Program& program,
                    const NodeIdMap& node_id_map) const override;

  /// @copybrief Mutation::Apply
  ///
  /// Delete the statement referenced by `statement_id`.
  ///
  /// @copydetails Mutation::Apply
  void Apply(const NodeIdMap& node_id_map,
             tint::CloneContext* clone_context,
             NodeIdMap* new_node_id_map) const override;

  protobufs::Mutation ToMessage() const override;

  /// Return if a given node is not a variable statement or a
  /// break statement or a compound statement that contains
  /// break statement.
  /// @param statement_node - an `ast::Statement` instance.
  /// @return true if the given node is a valid and allowed
  /// statement type.
  /// @return false otherwise.
  static bool CanBeDeleted(const ast::Statement& statement_node);

 private:
  protobufs::MutationDeleteStatement message_;
};

}  // namespace tint::fuzzers::ast_fuzzer

#endif  // SRC_TINT_FUZZERS_TINT_AST_FUZZER_MUTATIONS_DELETE_STATEMENT_H_
