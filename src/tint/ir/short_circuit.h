// Copyright 2023 The Tint Authors.
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

#ifndef SRC_TINT_IR_SHORT_CIRCUIT_H_
#define SRC_TINT_IR_SHORT_CIRCUIT_H_

#include "src/tint/ir/branch.h"
#include "src/tint/ir/flow_node.h"
#include "src/tint/ir/if.h"
#include "src/tint/ir/value.h"

// Forward declarations
namespace tint::ir {
class Block;
}  // namespace tint::ir

namespace tint::ir {

/// A flow node representing a short-circuit  statement.
class ShortCircuit : public utils::Castable<ShortCircuit, FlowNode> {
  public:
    /// The type of operation this short circuit represents
    enum class Type {
        /// A `&&`
        kAnd,
        /// A `||`
        kOr,
    };

    /// Constructor
    /// @param ty the type of short-circuit
    explicit ShortCircuit(ShortCircuit::Type ty);
    ~ShortCircuit() override;

    /// The lhs block
    Branch lhs = {};
    /// If node for the conditional RHS
    If* rhs = nullptr;
    /// The merge block
    Branch merge = {};
    /// The type of short circuit
    Type type;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_SHORT_CIRCUIT_H_
