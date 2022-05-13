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

#ifndef SRC_TINT_AST_TRAVERSE_EXPRESSIONS_H_
#define SRC_TINT_AST_TRAVERSE_EXPRESSIONS_H_

#include <unordered_map>
#include <vector>

#include "src/tint/ast/binary_expression.h"
#include "src/tint/ast/bitcast_expression.h"
#include "src/tint/ast/call_expression.h"
#include "src/tint/ast/index_accessor_expression.h"
#include "src/tint/ast/literal_expression.h"
#include "src/tint/ast/member_accessor_expression.h"
#include "src/tint/ast/phony_expression.h"
#include "src/tint/ast/unary_op_expression.h"
#include "src/tint/utils/reverse.h"

namespace tint::ast {

/// The action to perform after calling the TraverseExpressions() callback
/// function.
enum class TraverseAction {
    /// Stop traversal immediately.
    Stop,
    /// Descend into this expression.
    Descend,
    /// Do not descend into this expression.
    Skip,
};

/// The order TraverseExpressions() will traverse expressions
enum class TraverseOrder {
    /// Expressions will be traversed from left to right
    LeftToRight,
    /// Expressions will be traversed from right to left
    RightToLeft,
};

/// TraverseExpressions performs a depth-first traversal of the expression nodes
/// from `root`, calling `callback` for each of the visited expressions that
/// match the predicate parameter type, in pre-ordering (root first).
/// @param root the root expression node
/// @param diags the diagnostics used for error messages
/// @param callback the callback function. Must be of the signature:
///        `TraverseAction(const T* expr)` or `TraverseAction(const T* expr, size_t depth)` where T
///        is an ast::Expression type.
/// @return true on success, false on error
template <TraverseOrder ORDER = TraverseOrder::LeftToRight, typename CALLBACK>
bool TraverseExpressions(const ast::Expression* root, diag::List& diags, CALLBACK&& callback) {
    using EXPR_TYPE = std::remove_pointer_t<traits::ParameterType<CALLBACK, 0>>;
    constexpr static bool HAS_DEPTH_ARG = traits::SignatureOfT<CALLBACK>::parameter_count == 2;

    std::vector<const ast::Expression*> to_visit{root};
    std::unordered_map<const ast::Expression*, size_t> depth;

    if constexpr (HAS_DEPTH_ARG) {
        depth[root] = 0;
    }

    auto set_depth = [&](const ast::Expression* parent, const ast::Expression* expr) {
        (void)parent;
        (void)expr;
        if constexpr (HAS_DEPTH_ARG) {
            depth[expr] = depth[parent] + 1;
        }
    };
    auto push_single = [&](const ast::Expression* parent, const ast::Expression* expr) {
        set_depth(parent, expr);
        to_visit.emplace_back(expr);
    };
    auto push_pair = [&](const ast::Expression* parent, const ast::Expression* left,
                         const ast::Expression* right) {
        set_depth(parent, left);
        set_depth(parent, right);
        if (ORDER == TraverseOrder::LeftToRight) {
            to_visit.emplace_back(right);
            to_visit.emplace_back(left);
        } else {
            to_visit.emplace_back(left);
            to_visit.emplace_back(right);
        }
    };
    auto push_list = [&](const ast::Expression* parent,
                         const std::vector<const ast::Expression*>& exprs) {
        if (ORDER == TraverseOrder::LeftToRight) {
            for (auto* expr : utils::Reverse(exprs)) {
                set_depth(parent, expr);
                to_visit.emplace_back(expr);
            }
        } else {
            for (auto* expr : exprs) {
                set_depth(parent, expr);
                to_visit.emplace_back(expr);
            }
        }
    };

    while (!to_visit.empty()) {
        auto* expr = to_visit.back();
        to_visit.pop_back();

        if (auto* filtered = expr->As<EXPR_TYPE>()) {
            TraverseAction result;
            if constexpr (HAS_DEPTH_ARG) {
                result = callback(filtered, depth[filtered]);
            } else {
                result = callback(filtered);
            }

            switch (result) {
                case TraverseAction::Stop:
                    return true;
                case TraverseAction::Skip:
                    continue;
                case TraverseAction::Descend:
                    break;
            }
        }

        bool ok = Switch(
            expr,
            [&](const IndexAccessorExpression* idx) {
                push_pair(idx, idx->object, idx->index);
                return true;
            },
            [&](const BinaryExpression* bin_op) {
                push_pair(bin_op, bin_op->lhs, bin_op->rhs);
                return true;
            },
            [&](const BitcastExpression* bitcast) {
                push_single(bitcast, bitcast->expr);
                return true;
            },
            [&](const CallExpression* call) {
                // TODO(crbug.com/tint/1257): Resolver breaks if we actually include
                // the function name in the traversal. push_single(call->func);
                push_list(call, call->args);
                return true;
            },
            [&](const MemberAccessorExpression* member) {
                // TODO(crbug.com/tint/1257): Resolver breaks if we actually include
                // the member name in the traversal. push_pair(member->structure,
                // member->member);
                push_single(member, member->structure);
                return true;
            },
            [&](const UnaryOpExpression* unary) {
                push_single(unary, unary->expr);
                return true;
            },
            [&](Default) {
                if (expr->IsAnyOf<LiteralExpression, IdentifierExpression, PhonyExpression>()) {
                    return true;  // Leaf expression
                }
                TINT_ICE(AST, diags) << "unhandled expression type: " << expr->TypeInfo().name;
                return false;
            });
        if (!ok) {
            return false;
        }
    }
    return true;
}

}  // namespace tint::ast

#endif  // SRC_TINT_AST_TRAVERSE_EXPRESSIONS_H_
