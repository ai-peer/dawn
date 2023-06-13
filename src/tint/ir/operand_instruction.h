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

#ifndef SRC_TINT_IR_OPERAND_INSTRUCTION_H_
#define SRC_TINT_IR_OPERAND_INSTRUCTION_H_

#include "src/tint/ir/instruction.h"

namespace tint::ir {

/// An instruction in the IR that expects one or more operands.
template <unsigned N>
class OperandInstruction : public utils::Castable<OperandInstruction<N>, Instruction> {
  public:
    /// Destructor
    ~OperandInstruction() override = default;

    /// Set an operand at a given index.
    /// @param index the operand index
    /// @param value the value to use
    void SetOperand(uint32_t index, ir::Value* value) override {
        TINT_ASSERT(IR, index < operands_.Length());

        if (!CheckOperand(index, value)) {
            return;
        }

        if (operands_[index]) {
            operands_[index]->RemoveUsage({this, index});
        }
        operands_[index] = value;
        if (value) {
            value->AddUsage({this, index});
        }
        return;
    }

    /// Writes a list of operands to the operand list for this instruction starting at @p start_idx.
    /// @param start_idx the index to write the first value into
    /// @param values the operand values to append
    void SetOperands(uint32_t start_idx, utils::VectorRef<ir::Value*> values) {
        for (auto* val : values) {
            SetOperand(start_idx, val);
            start_idx += 1;
        }
    }

  protected:
    /// Resizes the operand list to @p size
    /// @param size the size
    void Resize(size_t size) { operands_.Resize(size); }

    /// @param index the index of the operand
    /// @param value the value to be set
    /// @returns true if the @p value is acceptable for operand at @p index
    virtual bool CheckOperand(uint32_t index, ir::Value* value) {
        TINT_ASSERT_OR_RETURN_VALUE(IR, value != nullptr, false);
        return true;

        (void)index;
        (void)value;
    }

    /// The operands to this instruction.
    utils::Vector<ir::Value*, N> operands_;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_OPERAND_INSTRUCTION_H_
