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

#ifndef SRC_TINT_IR_CASE_H_
#define SRC_TINT_IR_CASE_H_

#include "src/tint/ir/block.h"
#include "src/tint/ir/flow_node.h"

// Forward declarations
namespace tint::ast {
class CaseSelector;
}  // namespace tint::ast

namespace tint::ir {

/// Flow node representing a case statement
class Case : public Castable<Case, FlowNode> {
  public:
    /// Constructor
    /// @param s the originating ast case selectors
    explicit Case(const utils::VectorRef<const ast::CaseSelector*> s);
    ~Case() override;

    /// The case selector for this node
    const utils::VectorRef<const ast::CaseSelector*> selectors;

    /// The start block for the case block.
    Block* start_target;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_CASE_H_
