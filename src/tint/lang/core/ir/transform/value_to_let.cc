// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/core/ir/transform/value_to_let.h"

#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/validator.h"

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

namespace tint::core::ir::transform {

namespace {

/// PIMPL state for the transform.
struct State {
    /// The IR module.
    Module& ir;

    /// The IR builder.
    Builder b{ir};

    /// The type manager.
    core::type::Manager& ty{ir.Types()};

    /// Process the module.
    void Process() {
        // Process each block.
        for (auto* block : ir.blocks.Objects()) {
            Process(block);
        }
    }

  private:
    void Process(ir::Block* block) {
        // A possibly-inlinable value returned by a sequenced instructions that has not yet been
        // marked-for or ruled-out-for inlining.
        ir::InstructionResult* pending_resolution = nullptr;

        // If pending_resolution is not null, then places pending_resolution into a new let,
        // otherwise does nothing.
        auto put_pending_into_let = [&] {
            if (pending_resolution) {
                PutInLet(pending_resolution);
                pending_resolution = nullptr;
            }
        };

        for (ir::Instruction* inst = block->Front(); inst; inst = inst->next) {
            if (!inst->Alive()) {
                continue;
            }

            // This transform assumes that all multi-result instructions have been replaced
            TINT_ASSERT(inst->Results().Length() < 2);

            // Is this instruction sequenced?
            bool sequenced = inst->Sequenced();

            // Gather the sequenced operands of the instruction
            Vector<ir::InstructionResult*, 8> sequenced_operands;
            for (auto* operand : inst->Operands()) {
                if (auto* res = operand->As<InstructionResult>()) {
                    if (res->Instruction()->Sequenced() || pending_resolution == res) {
                        sequenced_operands.Push(res);
                    }
                }
            }

            // How many of the instruction's operands are sequenced?
            switch (sequenced_operands.Length()) {
                case 0:
                    break;  // No sequenced operands
                case 1: {
                    // Instruction takes only a single sequenced operand.
                    if (pending_resolution == sequenced_operands[0]) {
                        // This instruction's sequenced operand is the pending_resolution value .
                        // This can be inlined.
                        pending_resolution = nullptr;
                        sequenced = true;  // Inherit the 'sequenced' flag from the inlined value
                    } else {
                        // This instruction's only sequenced operand was not the last sequenced
                        // instruction. This cannot be inlined as it would break sequencing order.
                        put_pending_into_let();
                    }
                    break;
                }
                default:
                    // Multiple operands are sequenced. There are no evaluation ordering guarantees,
                    // so we need to put all the pending resolution values into lets.
                    put_pending_into_let();
                    break;
            }

            if (inst->Is<Let>()) {
                continue;  // No point putting a let result in a let
            }
            if (!sequenced) {
                continue;  // Instruction not sequenced
            }

            // We have ourselves a sequenced, non-let instruction.
            // If the pending_resolution could have been inlined, then this would have been done
            // above and pending_resolution would have been set to nullptr. If pending_resolution is
            // not nullptr, then the value needs placing into a let. Do that now.
            put_pending_into_let();

            // Check the usages of the sequenced instruction's result.
            if (auto* result = inst->Result(0)) {
                auto& usages = result->Usages();
                switch (usages.Count()) {
                    case 0: {
                        // No usage
                        break;
                    }
                    case 1: {
                        // Single usage
                        auto* usage = (*usages.begin()).instruction;
                        if (usage->Block() == inst->Block()) {
                            // Usage in same block. Assign to pending_resolution, as we don't
                            // know whether its safe to inline yet.
                            pending_resolution = result;
                        } else {
                            // Usage from another block. Cannot inline.
                            inst = PutInLet(result);
                            break;
                        }
                        break;
                    }
                    default: {
                        // Value has multiple usages. Cannot inline.
                        inst = PutInLet(result);
                        break;
                    }
                }
            }
        }
    }

    /// PutInLet places the value into a new 'let' instruction, immediately after the value's
    /// instruction
    /// @param value the value to place into the 'let'
    /// @return the created 'let' instruction.
    ir::Let* PutInLet(ir::InstructionResult* value) {
        auto* inst = value->Instruction();
        auto* let = b.Let(value->Type());
        value->ReplaceAllUsesWith(let->Result());
        let->SetValue(value);
        let->InsertAfter(inst);
        return let;
    }
};

}  // namespace

Result<SuccessType> ValueToLet(Module& ir) {
    auto result = ValidateAndDumpIfNeeded(ir, "ValueToLet transform");
    if (!result) {
        return result;
    }

    State{ir}.Process();

    return Success;
}

}  // namespace tint::core::ir::transform
