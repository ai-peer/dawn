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

#include "src/tint/sem/info.h"

#include "src/tint/sem/expression.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/module.h"
#include "src/tint/sem/statement.h"

namespace tint::sem {

Info::Info() = default;

Info::Info(Info&&) = default;

Info::~Info() = default;

Info& Info::operator=(Info&&) = default;

ast::DiagnosticSeverity Info::DiagnosticSeverity(const ast::Node* ast_node,
                                                 ast::DiagnosticRule rule) const {
    auto* sem = Get(ast_node);
    if (!sem) {
        return ast::DiagnosticSeverity::kUndefined;
    }

    // Get the diagnostic severity modification for a node.
    auto check = [rule](const sem::Node* node) {
        if (node != nullptr) {
            auto& severities = node->DiagnosticSeverities();
            auto itr = severities.find(rule);
            if (itr != severities.end()) {
                return itr->second;
            }
        }
        return ast::DiagnosticSeverity::kUndefined;
    };

    ast::DiagnosticSeverity severity;
    auto* stmt = sem->As<sem::Statement>();
    auto* func = sem->As<sem::Function>();
    if (auto* expr = sem->As<sem::Expression>()) {
        stmt = expr->Stmt();
    }
    if (stmt) {
        // Walk up the statement hierarchy, checking for diagnostic severity modifications.
        while (true) {
            severity = check(stmt);
            if (severity != ast::DiagnosticSeverity::kUndefined) {
                return severity;
            }
            if (!stmt->Parent()) {
                break;
            }
            stmt = stmt->Parent();
        }
        func = stmt->Function();
    }

    // Check for a diagnostic severity modification on the function.
    severity = check(func);
    if (severity != ast::DiagnosticSeverity::kUndefined) {
        return severity;
    }

    // Use the global severity set on the module.
    severity = check(module_);
    TINT_ASSERT(Semantic, severity != ast::DiagnosticSeverity::kUndefined);
    return severity;
}

}  // namespace tint::sem
