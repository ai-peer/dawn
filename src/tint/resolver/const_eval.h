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

#ifndef SRC_TINT_RESOLVER_CONST_EVAL_H_
#define SRC_TINT_RESOLVER_CONST_EVAL_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "src/tint/utils/result.h"

// Forward declarations
namespace tint {
class ProgramBuilder;
class Source;
}  // namespace tint
namespace tint::ast {
class LiteralExpression;
}  // namespace tint::ast
namespace tint::sem {
class Constant;
class Expression;
class StructMember;
class Type;
}  // namespace tint::sem

namespace tint::resolver {

/// ConstEval performs shader creation-time (constant expression) expression evaluation.
/// Methods are called from the resolver, either directly or via member-function pointers indexed by
/// the IntrinsicTable. All child-expression nodes are guaranteed to have been already resolved
/// before calling a method to evaluate an expression's value.
class ConstEval {
  public:
    /// Typedef for a constant evaluation function
    using Function = const sem::Constant* (ConstEval::*)(const sem::Type* result_ty,
                                                         sem::Expression const* const* args,
                                                         size_t num_args);

    /// The result type of a method that may raise a diagnostic error and the caller should abort
    /// resolving. Can be one of three distinct values:
    /// * A non-null sem::Constant pointer. Returned when a expression resolves to a creation time
    ///   value.
    /// * A null sem::Constant pointer. Returned when a expression cannot resolve to a creation time
    ///   value, but is otherwise legal.
    /// * `utils::Failure`. Returned when there was a resolver error. In this situation the method
    ///   will have already reported a diagnostic error message, and the caller should abort
    ///   resolving.
    using ConstantResult = utils::Result<const sem::Constant*>;

    /// Constructor
    /// @param b the program builder
    explicit ConstEval(ProgramBuilder& b);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Constant value evaluation methods, to be called directly from Resolver
    ////////////////////////////////////////////////////////////////////////////////////////////////

    /// @param ty the target type
    /// @param expr the input expression
    /// @return the bit-cast of the given expression to the given type, or null if the value cannot
    ///         be calculated
    const sem::Constant* Bitcast(const sem::Type* ty, const sem::Expression* expr);

    /// @param ty the target type
    /// @param args the input arguments
    /// @return the resulting type constructor or conversion, or null if the value cannot be
    ///         calculated
    const sem::Constant* CtorOrConv(const sem::Type* ty,
                                    const std::vector<const sem::Expression*>& args);

    /// @param obj the object being indexed
    /// @param idx the index expression
    /// @return the result of the index, or null if the value cannot be calculated
    const sem::Constant* Index(const sem::Expression* obj, const sem::Expression* idx);

    /// @param ty the result type
    /// @param lit the literal AST node
    /// @return the constant value of the literal
    const sem::Constant* Literal(const sem::Type* ty, const ast::LiteralExpression* lit);

    /// @param obj the object being accessed
    /// @param member the member
    /// @return the result of the member access, or null if the value cannot be calculated
    const sem::Constant* MemberAccess(const sem::Expression* obj, const sem::StructMember* member);

    /// @param ty the result type
    /// @param vector the vector being swizzled
    /// @param indices the swizzle indices
    /// @return the result of the swizzle, or null if the value cannot be calculated
    const sem::Constant* Swizzle(const sem::Type* ty,
                                 const sem::Expression* vector,
                                 const std::vector<uint32_t>& indices);

    /// Convert the `value` to `target_type`
    /// @param ty the result type
    /// @param value the value being converted
    /// @param source the source location of the conversion
    /// @return the converted value, or null if the value cannot be calculated
    ConstantResult Convert(const sem::Type* ty, const sem::Constant* value, const Source& source);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Constant value evaluation methods, to be referenced by the intrinsic table
    ////////////////////////////////////////////////////////////////////////////////////////////////

    /// Type conversion
    /// @param ty the result type
    /// @param args the input arguments
    /// @param num_args the number of input arguments
    /// @return the converted value, or null if the value cannot be calculated
    const sem::Constant* conv(const sem::Type* ty,
                              sem::Expression const* const* args,
                              size_t num_args);

    /// Type construction
    /// @param ty the result type
    /// @param args the input arguments
    /// @param num_args the number of input arguments
    /// @return the constructed value, or null if the value cannot be calculated
    const sem::Constant* ctor(const sem::Type* ty,
                              sem::Expression const* const* args,
                              size_t num_args);

    /// Zero value type constructor
    /// @param ty the result type
    /// @param args unused (no arguments provided)
    /// @param num_args unused (no arguments provided)
    /// @return the constructed value, or null if the value cannot be calculated
    const sem::Constant* zero(const sem::Type* ty,
                              sem::Expression const* const* args,
                              size_t num_args);

    /// Identity value type constructor
    /// @param ty the result type
    /// @param args single input argument
    /// @param num_args must be 1
    /// @return the constructed value, or null if the value cannot be calculated
    const sem::Constant* identity(const sem::Type* ty,
                                  sem::Expression const* const* args,
                                  size_t num_args);

  private:
    /// Adds the given error message to the diagnostics
    void AddError(const std::string& msg, const Source& source) const;

    /// Adds the given warning message to the diagnostics
    void AddWarning(const std::string& msg, const Source& source) const;

    ProgramBuilder& builder;
};

}  // namespace tint::resolver

#endif  // SRC_TINT_RESOLVER_CONST_EVAL_H_
