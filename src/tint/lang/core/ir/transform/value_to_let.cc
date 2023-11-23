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

enum class Kind { kLoad, kStore };
using Kinds = EnumSet<Kind>;

Kinds Classify(ir::Instruction* inst) {
    return tint::Switch<Kinds>(
        inst,                                                         //
        [&](const ir::Load*) { return Kind::kLoad; },                 //
        [&](const ir::LoadVectorElement*) { return Kind::kLoad; },    //
        [&](const ir::Store*) { return Kind::kStore; },               //
        [&](const ir::StoreVectorElement*) { return Kind::kStore; },  //
        [&](const ir::Call*) {
            return Kinds{Kind::kLoad, Kind::kStore};
        },
        [&](Default) { return Kinds{}; });
}

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
        Hashset<ir::InstructionResult*, 32> pending_resolution;
        Kind pending_kind = Kind::kLoad;

        auto put_pending_in_lets = [&] {
            for (auto* pending : pending_resolution) {
                PutInLet(pending);
            }
            pending_resolution.Clear();
        };

        for (ir::Instruction* inst = block->Front(); inst; inst = inst->next) {
            if (!inst->Alive()) {
                continue;
            }

            // This transform assumes that all multi-result instructions have been replaced
            TINT_ASSERT(inst->Results().Length() < 2);

            // Is this instruction sequenced?
            auto kinds = Classify(inst);

            if (kinds.Contains(Kind::kLoad)) {
                if (pending_kind != Kind::kLoad) {
                    put_pending_in_lets();
                    pending_kind = Kind::kLoad;
                }

                if (auto* result = inst->Result(0)) {
                    auto& usages = result->Usages();
                    switch (usages.Count()) {
                        case 0:  // No usage
                            break;
                        case 1: {  // Single usage
                            auto* usage = (*usages.begin()).instruction;
                            if (usage->Block() == inst->Block()) {
                                // Usage in same block. Assign to pending_resolution, as we don't
                                // know whether its safe to inline yet.
                                pending_resolution.Add(result);
                            } else {
                                // Usage from another block. Cannot inline.
                                inst = PutInLet(result);
                            }
                            break;
                        }
                        default:  // Value has multiple usages. Cannot inline.
                            inst = PutInLet(result);
                            break;
                    }
                }
            }

            if (kinds.Contains(Kind::kStore)) {
                // A store may change the values of the pending_resolution instructions. Move all of
                // these to lets.
                if (pending_kind != Kind::kStore) {
                    put_pending_in_lets();
                    pending_kind = Kind::kStore;
                }
            }

            for (auto* operand : inst->Operands()) {
                if (auto* result = operand->As<InstructionResult>()) {
                    pending_resolution.Remove(result);
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
        if (auto name = b.ir.NameOf(value); name.IsValid()) {
            b.ir.SetName(let->Result(), name);
            b.ir.ClearName(value);
        }
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
