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

#ifndef SRC_TINT_IR_CONTROL_INSTRUCTION_H_
#define SRC_TINT_IR_CONTROL_INSTRUCTION_H_

#include "src/tint/ir/branch.h"
#include "src/tint/ir/operand_instruction.h"
#include "src/tint/type/tuple.h"

namespace tint::ir {

/// Base class of instructions that perform branches to two or more blocks, owned by the
/// ControlInstruction.
class ControlInstruction : public utils::Castable<ControlInstruction, OperandInstruction<1>> {
  public:
    /// Destructor
    ~ControlInstruction() override;

    /// @return All the exit branches for the flow control instruction
    utils::Slice<Branch const* const> Exits() const { return exits_.Slice(); }

    /// Adds the exit to the flow control instruction
    /// @param exit the exit branch
    void AddExit(Branch* exit) { exits_.Push(exit); }

    /// @returns the result type of the if
    const type::Tuple* Type() const override { return result_type_; }

    /// Sets the result type of the if
    /// @param type the new result type of the if
    void SetType(const type::Tuple* type) { result_type_ = type; }

  protected:
    /// The flow control exits
    utils::Vector<Branch*, 2> exits_;

    /// The result type
    const type::Tuple* result_type_ = nullptr;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_CONTROL_INSTRUCTION_H_
