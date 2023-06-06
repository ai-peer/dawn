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

#include "src/tint/ir/validate.h"

#include <string>
#include <utility>

#include "src/tint/ir/access.h"
#include "src/tint/ir/binary.h"
#include "src/tint/ir/bitcast.h"
#include "src/tint/ir/break_if.h"
#include "src/tint/ir/builtin.h"
#include "src/tint/ir/construct.h"
#include "src/tint/ir/continue.h"
#include "src/tint/ir/convert.h"
#include "src/tint/ir/disassembler.h"
#include "src/tint/ir/discard.h"
#include "src/tint/ir/exit_if.h"
#include "src/tint/ir/exit_loop.h"
#include "src/tint/ir/exit_switch.h"
#include "src/tint/ir/function.h"
#include "src/tint/ir/if.h"
#include "src/tint/ir/load.h"
#include "src/tint/ir/loop.h"
#include "src/tint/ir/next_iteration.h"
#include "src/tint/ir/return.h"
#include "src/tint/ir/store.h"
#include "src/tint/ir/switch.h"
#include "src/tint/ir/swizzle.h"
#include "src/tint/ir/unary.h"
#include "src/tint/ir/user_call.h"
#include "src/tint/ir/var.h"
#include "src/tint/switch.h"
#include "src/tint/type/pointer.h"

namespace tint::ir {
namespace {

class Validator {
  public:
    explicit Validator(Module& mod) : mod_(mod) {}

    ~Validator() {}

    utils::Result<Success, diag::List> IsValid() {
        CheckRootBlock(mod_.root_block);

        for (const auto* func : mod_.functions) {
            CheckFunction(func);
        }

        if (diagnostics_.contains_errors() || !instruction_srcs_needed_.IsEmpty() ||
            !operand_srcs_needed_.IsEmpty()) {
            Disassembler d(mod_, &instruction_srcs_needed_, &operand_srcs_needed_);
            auto dis = d.Disassemble();

            mod_.disassembly_file = std::make_unique<Source::File>("", dis);
            {
                auto keys = instruction_diagnostics_.Keys();
                auto values = instruction_diagnostics_.Values();
                for (uint32_t i = 0; i < keys.Length(); i++) {
                    auto src = d.InstructionSource(keys[i]);
                    src.file = mod_.disassembly_file.get();
                    AddError(values[i], src);
                }
            }
            {
                auto keys = operand_diagnostics_.Keys();
                auto values = operand_diagnostics_.Values();
                for (uint32_t i = 0; i < keys.Length(); i++) {
                    auto src = d.OperandSource(keys[i]);
                    src.file = mod_.disassembly_file.get();
                    AddError(values[i], src);
                }
            }

            diagnostics_.add_note(tint::diag::System::IR, dis, {});

            return std::move(diagnostics_);
        }
        return Success{};
    }

  private:
    Module& mod_;
    diag::List diagnostics_;

    // TODO(dsinclair): Need another hashset for blocks
    utils::Hashset<const Instruction*, 1> instruction_srcs_needed_;
    utils::Hashset<Usage, 1, Usage::Hasher> operand_srcs_needed_;

    // TODO(dsinclair): These two hashmaps, instruction_diagnostics_ and operand_diagnostics_ need
    // to be a single vector so teh diagnostics can be emitted in the correct order. Otherwise,
    // we'll end up with all instruction errors at the top then operand errors below.
    utils::Hashmap<const Instruction*, std::string, 1> instruction_diagnostics_;
    utils::Hashmap<Usage, std::string, 1, Usage::Hasher> operand_diagnostics_;

    void AddError(const std::string& err, Source src) {
        diagnostics_.add_error(tint::diag::System::IR, err, src);
    }
    void AddError(const std::string& err) { diagnostics_.add_error(tint::diag::System::IR, err); }

    std::string Name(const Value* v) { return mod_.NameOf(v).Name(); }

    void CheckRootBlock(const Block* blk) {
        if (!blk) {
            return;
        }

        for (const auto* inst : *blk) {
            auto* var = inst->As<ir::Var>();
            if (!var) {
                instruction_srcs_needed_.Add(inst);
                instruction_diagnostics_.Add(
                    inst, std::string("root block: invalid instruction: ") + inst->TypeInfo().name);

                // TODO(dsinclair): AddNote root_block
                continue;
            }
            if (!var->Type()->Is<type::Pointer>()) {
                instruction_srcs_needed_.Add(inst);
                instruction_diagnostics_.Add(
                    inst, std::string("root block: 'var' ") + Name(var) +
                              "type is not a pointer: " + var->Type()->TypeInfo().name);
                // TODO(dsinclair): AddNote root_block
            }
        }
    }

    void CheckFunction(const Function* func) { CheckBlock(func->StartTarget()); }

    void CheckBlock(const Block* blk) {
        if (!blk->HasBranchTarget()) {
            // TODO(dsinclair): Make this a block error
            AddError("block: does not end in a branch");
        }

        for (const auto* inst : *blk) {
            if (inst->Is<ir::Branch>() && inst != blk->Branch()) {
                instruction_srcs_needed_.Add(inst);
                instruction_diagnostics_.Add(inst,
                                             "block: branch which isn't the final instruction");

                // TODO(dsinclair): AddNote block
                continue;
            }

            CheckInstruction(inst);
        }
    }

    void CheckInstruction(const Instruction* inst) {
        tint::Switch(
            inst,                                          //
            [&](const ir::Access*) {},                     //
            [&](const ir::Binary*) {},                     //
            [&](const ir::Branch* b) { CheckBranch(b); },  //
            [&](const ir::Call* c) { CheckCall(c); },      //
            [&](const ir::Load*) {},                       //
            [&](const ir::Store*) {},                      //
            [&](const ir::Swizzle*) {},                    //
            [&](const ir::Unary*) {},                      //
            [&](const ir::Var*) {},                        //
            [&](Default) {
                AddError(std::string("missing validation of: ") + inst->TypeInfo().name);
            });
    }

    void CheckCall(const ir::Call* call) {
        tint::Switch(
            call,                          //
            [&](const ir::Bitcast*) {},    //
            [&](const ir::Builtin*) {},    //
            [&](const ir::Construct*) {},  //
            [&](const ir::Convert*) {},    //
            [&](const ir::Discard*) {},    //
            [&](const ir::UserCall*) {},   //
            [&](Default) {
                AddError(std::string("missing validation of call: ") + call->TypeInfo().name);
            });
    }

    void CheckBranch(const ir::Branch* b) {
        tint::Switch(
            b,                                 //
            [&](const ir::BreakIf*) {},        //
            [&](const ir::Continue*) {},       //
            [&](const ir::ExitIf*) {},         //
            [&](const ir::ExitLoop*) {},       //
            [&](const ir::ExitSwitch*) {},     //
            [&](const ir::If*) {},             //
            [&](const ir::Loop*) {},           //
            [&](const ir::NextIteration*) {},  //
            [&](const ir::Return* ret) {
                if (ret->Func() == nullptr) {
                    AddError("return: null function");
                }
            },                          //
            [&](const ir::Switch*) {},  //
            [&](Default) {
                AddError(std::string("missing validation of branch: ") + b->TypeInfo().name);
            });
    }
};

}  // namespace

utils::Result<Success, diag::List> Validate(Module& mod) {
    Validator v(mod);
    return v.IsValid();
}

}  // namespace tint::ir
