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
/// @tparam N the default number of operands
/// @tparam R the default number of result values
template <unsigned N, unsigned R>
class OperandInstruction : public utils::Castable<OperandInstruction<N, R>, Instruction> {
  public:
    /// Destructor
    ~OperandInstruction() override = default;

    /// Set an operand at a given index.
    /// @param index the operand index
    /// @param value the value to use
    void SetOperand(size_t index, ir::Value* value) override {
        TINT_ASSERT(IR, index < operands_.Length());
        if (operands_[index]) {
            operands_[index]->RemoveUsage({this, index});
        }
        operands_[index] = value;
        if (value) {
            value->AddUsage({this, index});
        }
    }

    /// @returns true if the instruction has result values
    bool HasResults() override { return !results_.IsEmpty(); }
    /// @returns true if the instruction has multiple values
    bool HasMultiResults() override { return results_.Length() > 1; }

    /// @returns the first result. Returns `nullptr` if there are no results, or if ther are
    /// multi-results
    Value* Result() override {
        if (!HasResults() || HasMultiResults()) {
            return nullptr;
        }
        return results_[0];
    }

    using Instruction::Result;

    /// @returns the result values for this instruction
    utils::VectorRef<Value*> Results() override { return results_; }

  protected:
    /// Append a new operand to the operand list for this instruction.
    /// @param idx the index the operand should be at
    /// @param value the operand value to append
    void AddOperand(size_t idx, ir::Value* value) {
        TINT_ASSERT(IR, idx == operands_.Length());

        if (value) {
            value->AddUsage({this, static_cast<uint32_t>(operands_.Length())});
        }
        operands_.Push(value);
    }

    /// Append a list of non-null operands to the operand list for this instruction.
    /// @param start_idx the index from whic the values should start
    /// @param values the operand values to append
    void AddOperands(size_t start_idx, utils::VectorRef<ir::Value*> values) {
        size_t idx = start_idx;
        for (auto* val : values) {
            AddOperand(idx, val);
            idx += 1;
        }
    }

    /// Appends a result value to the instruction
    /// @param value the value to append
    void AddResult(Value* value) {
        if (value) {
            value->SetSource(this);
        }
        results_.Push(value);
    }

    /// The operands to this instruction.
    utils::Vector<ir::Value*, N> operands_;
    /// The results of this instruction.
    utils::Vector<ir::Value*, R> results_;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_OPERAND_INSTRUCTION_H_
