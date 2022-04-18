// Copyright 2020 The Tint Authors.
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

#ifndef SRC_TINT_RESOLVER_VALIDATOR_H_
#define SRC_TINT_RESOLVER_VALIDATOR_H_

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/tint/ast/pipeline_stage.h"
#include "src/tint/program_builder.h"
#include "src/tint/resolver/sem_helper.h"
#include "src/tint/source.h"

// Forward declarations
namespace tint::ast {
class IndexAccessorExpression;
class BinaryExpression;
class BitcastExpression;
class CallExpression;
class CallStatement;
class CaseStatement;
class ForLoopStatement;
class Function;
class IdentifierExpression;
class LoopStatement;
class MemberAccessorExpression;
class ReturnStatement;
class SwitchStatement;
class UnaryOpExpression;
class Variable;
}  // namespace tint::ast
namespace tint::sem {
class Array;
class Atomic;
class BlockStatement;
class Builtin;
class CaseStatement;
class ElseStatement;
class ForLoopStatement;
class IfStatement;
class LoopStatement;
class Statement;
class SwitchStatement;
class TypeConstructor;
}  // namespace tint::sem

namespace tint::resolver {

class Validator {
 public:
  using ValidTypeStorageLayouts =
      std::set<std::pair<const sem::Type*, ast::StorageClass>>;

  explicit Validator(ProgramBuilder* builder);
  ~Validator();

  /// Adds the given error message to the diagnostics
  void AddError(const std::string& msg, const Source& source) const;

  /// Adds the given warning message to the diagnostics
  void AddWarning(const std::string& msg, const Source& source) const;

  /// Adds the given note message to the diagnostics
  void AddNote(const std::string& msg, const Source& source) const;

  /// @param type the given type
  /// @returns true if the given type is a plain type
  bool IsPlain(const sem::Type* type) const;

  /// @param type the given type
  /// @returns true if the given type is a fixed-footprint type
  bool IsFixedFootprint(const sem::Type* type) const;

  /// @param type the given type
  /// @returns true if the given type is storable
  bool IsStorable(const sem::Type* type) const;

  /// @param type the given type
  /// @returns true if the given type is host-shareable
  bool IsHostShareable(const sem::Type* type) const;

  // AST and Type validation methods
  // Each return true on success, false on failure.
  bool ValidatePipelineStages(
      const std::vector<sem::Function*>& entry_points) const;
  bool ValidateAlias(const ast::Alias*) const;
  bool ValidateArray(const sem::Array* arr, const Source& source) const;
  bool ValidateArrayStrideAttribute(const ast::StrideAttribute* attr,
                                    uint32_t el_size,
                                    uint32_t el_align,
                                    const Source& source) const;
  bool ValidateAtomic(const ast::Atomic* a, const sem::Atomic* s) const;
  bool ValidateAtomicVariable(
      const sem::Variable* var,
      std::unordered_map<const sem::Type*, const Source&> atomic_composite_info)
      const;
  bool ValidateAssignment(const ast::Statement* a,
                          const sem::Type* rhs_ty) const;
  bool ValidateBitcast(const ast::BitcastExpression* cast,
                       const sem::Type* to) const;
  bool ValidateBreakStatement(const sem::Statement* stmt) const;
  bool ValidateBuiltinAttribute(const ast::BuiltinAttribute* attr,
                                const sem::Type* storage_type,
                                ast::PipelineStage stage,
                                const bool is_input) const;
  bool ValidateContinueStatement(const sem::Statement* stmt) const;
  bool ValidateDiscardStatement(const sem::Statement* stmt) const;
  bool ValidateElseStatement(const sem::ElseStatement* stmt) const;
  bool ValidateEntryPoint(const sem::Function* func,
                          ast::PipelineStage stage) const;
  bool ValidateForLoopStatement(const sem::ForLoopStatement* stmt) const;
  bool ValidateFallthroughStatement(const sem::Statement* stmt) const;
  bool ValidateFunction(const sem::Function* func,
                        ast::PipelineStage stage) const;
  bool ValidateFunctionCall(const sem::Call* call) const;
  bool ValidateGlobalVariable(
      const sem::Variable* var,
      std::unordered_map<uint32_t, const sem::Variable*> constant_ids,
      std::unordered_map<const sem::Type*, const Source&> atomic_composite_info)
      const;
  bool ValidateIfStatement(const sem::IfStatement* stmt) const;
  bool ValidateIncrementDecrementStatement(
      const ast::IncrementDecrementStatement* stmt) const;
  bool ValidateInterpolateAttribute(const ast::InterpolateAttribute* attr,
                                    const sem::Type* storage_type) const;
  bool ValidateBuiltinCall(const sem::Call* call) const;
  bool ValidateLocationAttribute(const ast::LocationAttribute* location,
                                 const sem::Type* type,
                                 std::unordered_set<uint32_t>& locations,
                                 ast::PipelineStage stage,
                                 const Source& source,
                                 const bool is_input = false) const;
  bool ValidateLoopStatement(const sem::LoopStatement* stmt) const;
  bool ValidateMatrix(const sem::Matrix* ty, const Source& source) const;
  bool ValidateFunctionParameter(const ast::Function* func,
                                 const sem::Variable* var) const;
  bool ValidateReturn(const ast::ReturnStatement* ret,
                      const sem::Type* func_type,
                      const sem::Type* ret_type) const;
  bool ValidateStatements(const ast::StatementList& stmts) const;
  bool ValidateStorageTexture(const ast::StorageTexture* t) const;
  bool ValidateStructure(const sem::Struct* str,
                         ast::PipelineStage stage) const;
  bool ValidateStructureConstructorOrCast(const ast::CallExpression* ctor,
                                          const sem::Struct* struct_type) const;
  bool ValidateSwitch(const ast::SwitchStatement* s);
  bool ValidateVariable(const sem::Variable* var) const;
  bool ValidateVariableConstructorOrCast(const ast::Variable* var,
                                         ast::StorageClass storage_class,
                                         const sem::Type* storage_type,
                                         const sem::Type* rhs_type) const;
  bool ValidateVector(const sem::Vector* ty, const Source& source) const;
  bool ValidateVectorConstructorOrCast(const ast::CallExpression* ctor,
                                       const sem::Vector* vec_type) const;
  bool ValidateMatrixConstructorOrCast(const ast::CallExpression* ctor,
                                       const sem::Matrix* matrix_type) const;
  bool ValidateScalarConstructorOrCast(const ast::CallExpression* ctor,
                                       const sem::Type* type) const;
  bool ValidateArrayConstructorOrCast(const ast::CallExpression* ctor,
                                      const sem::Array* arr_type) const;
  bool ValidateTextureBuiltinFunction(const sem::Call* call) const;
  bool ValidateNoDuplicateAttributes(
      const ast::AttributeList& attributes) const;
  bool ValidateStorageClassLayout(const sem::Type* type,
                                  ast::StorageClass sc,
                                  Source source,
                                  ValidTypeStorageLayouts& layouts) const;
  bool ValidateStorageClassLayout(const sem::Variable* var,
                                  ValidTypeStorageLayouts& layouts) const;

  /// @returns true if the attribute list contains a
  /// ast::DisableValidationAttribute with the validation mode equal to
  /// `validation`
  bool IsValidationDisabled(const ast::AttributeList& attributes,
                            ast::DisabledValidation validation) const;

  /// @returns true if the attribute list does not contains a
  /// ast::DisableValidationAttribute with the validation mode equal to
  /// `validation`
  bool IsValidationEnabled(const ast::AttributeList& attributes,
                           ast::DisabledValidation validation) const;

 private:
  /// Searches the current statement and up through parents of the current
  /// statement looking for a loop or for-loop continuing statement.
  /// @returns the closest continuing statement to the current statement that
  /// (transitively) owns the current statement.
  /// @param stop_at_loop if true then the function will return nullptr if a
  /// loop or for-loop was found before the continuing.
  const ast::Statement* ClosestContinuing(bool stop_at_loop) const;

  /// Returns a human-readable string representation of the vector type name
  /// with the given parameters.
  /// @param size the vector dimension
  /// @param element_type scalar vector sub-element type
  /// @return pretty string representation
  std::string VectorPretty(uint32_t size, const sem::Type* element_type) const;

  SymbolTable& symbols_;
  diag::List& diagnostics_;
  SemHelper sem_helper_;
};

}  // namespace tint::resolver

#endif  // SRC_TINT_RESOLVER_VALIDATOR_H_
