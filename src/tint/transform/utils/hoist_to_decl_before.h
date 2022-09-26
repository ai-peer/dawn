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

#ifndef SRC_TINT_TRANSFORM_UTILS_HOIST_TO_DECL_BEFORE_H_
#define SRC_TINT_TRANSFORM_UTILS_HOIST_TO_DECL_BEFORE_H_

#include <functional>
#include <memory>

#include "src/tint/sem/expression.h"
#include "src/tint/transform/transform.h"

namespace tint::transform {

/// Utility class that can be used to hoist expressions before other
/// expressions, possibly converting 'for-loop's to 'loop's and 'else-if's to
// 'else {if}'s.
class HoistToDeclBefore {
  public:
    /// Constructor
    /// @param ctx the clone context
    explicit HoistToDeclBefore(CloneContext& ctx);

    /// Destructor
    ~HoistToDeclBefore();

    using StmtBuilder = std::function<const ast::Statement*()>;

    /// Hoists @p expr to a `let` or `var` with optional `decl_name`, inserting it
    /// before @p before_expr.
    /// @param before_expr expression to insert `expr` before
    /// @param expr expression to hoist
    /// @param as_let hoist to `let` if true, otherwise to `var`
    /// @param decl_name optional name to use for the variable/constant name
    /// @return true on success
    bool Add(const sem::Expression* before_expr,
             const ast::Expression* expr,
             bool as_let,
             const char* decl_name = "");

    /// Inserts `stmt` before `before_stmt`, possibly converting 'for-loop's to
    /// 'loop's if necessary.
    /// @param before_stmt statement to insert `stmt` before
    /// @param stmt statement to insert
    /// @return true on success
    bool InsertBefore(const sem::Statement* before_stmt, const ast::Statement* stmt);

    /// Inserts the returned statement of @p builder before @p before_stmt, possibly converting
    /// 'for-loop's to 'loop's if necessary.
    /// @param before_stmt the preceding statement that the statement of @p builder will be inserted
    /// before
    /// @param builder the statement builder used to create the new statement
    /// @return true on success
    bool InsertBefore(const sem::Statement* before_stmt, const StmtBuilder& builder);

    /// Use to signal that we plan on hoisting a decl before `before_expr`. This
    /// will convert 'for-loop's to 'loop's and 'else-if's to 'else {if}'s if
    /// needed.
    /// @param before_expr expression we would hoist a decl before
    /// @return true on success
    bool Prepare(const sem::Expression* before_expr);

  private:
    class State;
    std::unique_ptr<State> state_;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_UTILS_HOIST_TO_DECL_BEFORE_H_
