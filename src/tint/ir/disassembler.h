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

#ifndef SRC_TINT_IR_DISASSEMBLER_H_
#define SRC_TINT_IR_DISASSEMBLER_H_

#include <string>

#include "src/tint/ir/binary.h"
#include "src/tint/ir/block.h"
#include "src/tint/ir/call.h"
#include "src/tint/ir/if.h"
#include "src/tint/ir/loop.h"
#include "src/tint/ir/module.h"
#include "src/tint/ir/switch.h"
#include "src/tint/ir/unary.h"
#include "src/tint/utils/hashmap.h"
#include "src/tint/utils/hashset.h"
#include "src/tint/utils/string_stream.h"

namespace tint::ir {

/// Helper class to disassemble the IR
class Disassembler {
  public:
    /// Constructor
    /// @param mod the module
    explicit Disassembler(const Module& mod,
                          utils::Hashset<const Instruction*, 1>* instruction_requests = nullptr,
                          utils::Hashset<Usage, 1, Usage::Hasher>* operand_requests = nullptr);
    ~Disassembler();

    /// Returns the module as a string
    /// @returns the string representation of the module
    std::string Disassemble();

    /// Writes the block instructions to the stream
    /// @param b the block containing the instructions
    void EmitBlockInstructions(const Block* b);

    /// @returns the string representation
    std::string AsString() const { return out_.str(); }

    /// @param inst the instruction to retrieve
    /// @returns the source for the instruction
    Source InstructionSource(const Instruction* inst) {
        auto res = instruction_to_src_.Get(inst);
        if (res) {
            return *res;
        }
        return Source{};
    }

    /// @param usage the usage to retrieve
    /// @returns the source for the usage
    Source OperandSource(Usage usage) {
        auto res = operand_to_src_.Get(usage);
        if (res) {
            return *res;
        }
        return Source{};
    }

  private:
    utils::StringStream& Indent();

    size_t IdOf(const Block* blk);
    std::string_view IdOf(const Value* node);

    void Walk(const Block* blk);
    void WalkInternal(const Block* blk);
    void EmitFunction(const Function* func);
    void EmitParamAttributes(const FunctionParam* p);
    void EmitReturnAttributes(const Function* func);
    void EmitBindingPoint(BindingPoint p);
    void EmitLocation(Location loc);
    Source EmitInstruction(const Instruction* inst);
    void EmitValueWithType(const Value* val);
    void EmitValue(const Value* val);
    void EmitValueList(tint::utils::Slice<const tint::ir::Value*> values);
    void EmitArgs(const Call* call);
    Source EmitBinary(const Binary* b);
    Source EmitUnary(const Unary* b);
    Source EmitBranch(const Branch* b);
    Source EmitSwitch(const Switch* s);
    Source EmitLoop(const Loop* l);
    Source EmitIf(const If* i);
    void EmitLine();

    Source::Location MakeCurrentLocation();

    const Module& mod_;
    utils::StringStream out_;
    utils::Hashset<const Block*, 32> visited_;
    utils::Hashmap<const Block*, size_t, 32> block_ids_;
    utils::Hashmap<const Value*, std::string, 32> value_ids_;
    uint32_t indent_size_ = 0;
    bool in_function_ = false;

    uint32_t current_output_line_ = 1;
    uint32_t current_output_start_pos_ = 0;

    utils::Hashset<const Instruction*, 1>* instruction_requests_;
    utils::Hashset<Usage, 1, Usage::Hasher>* operand_requests_;

    // These maps are only populated with instructions and usages which are requested
    // when calling `Disassemble`
    utils::Hashmap<const Instruction*, Source, 0> instruction_to_src_;
    utils::Hashmap<Usage, Source, 0, Usage::Hasher> operand_to_src_;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_DISASSEMBLER_H_
