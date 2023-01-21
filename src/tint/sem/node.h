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

#ifndef SRC_TINT_SEM_NODE_H_
#define SRC_TINT_SEM_NODE_H_

#include "src/tint/castable.h"

#include "src/tint/ast/diagnostic_control.h"

namespace tint::sem {

/// Node is the base class for all semantic nodes
class Node : public Castable<Node> {
  public:
    /// Constructor
    Node();

    /// Copy constructor
    Node(const Node&);

    /// Destructor
    ~Node() override;

    /// Modifies the severity of a specific diagnostic rule for this node and its children.
    /// @param rule the diagnostic rule
    /// @param severity the new diagnostic severity
    void SetDiagnosticSeverity(ast::DiagnosticRule rule, ast::DiagnosticSeverity severity) {
        diagnostic_severities_[rule] = severity;
    }

    /// @returns the diagnostic severity modifications applied to this node and its children
    const ast::DiagnosticRuleSeverities& DiagnosticSeverities() const {
        return diagnostic_severities_;
    }

  private:
    ast::DiagnosticRuleSeverities diagnostic_severities_;
};

}  // namespace tint::sem

#endif  // SRC_TINT_SEM_NODE_H_
