// Copyright 2024 The Dawn & Tint Authors
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

#include "src/tint/lang/hlsl/writer/raise/promote_initializers.h"

#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/validator.h"

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

namespace tint::hlsl::writer::raise {

namespace {

/// PIMPL state for the transform.
struct State {
    /// The IR module.
    core::ir::Module& ir;

    /// The IR builder.
    core::ir::Builder b{ir};

    /// The type manager.
    core::type::Manager& ty{ir.Types()};

    /// Process the module.
    void Process() {
        for (auto* block : ir.blocks.Objects()) {
            // In the root block, all structs need to be split out, no nested structs
            bool is_root_block = block == ir.root_block;

            Process(block, is_root_block);
        }
    }

    struct ValueInfo {
        core::ir::Instruction* inst;
        core::ir::Value* val;
    };

    void Process(core::ir::Block* block, bool is_root_block) {
        Vector<ValueInfo, 4> worklist;
        Hashset<core::ir::Value*, 4> seen;

        for (auto* inst : *block) {
            if (inst->Is<core::ir::Let>()) {
                continue;
            }
            if (inst->Is<core::ir::Var>()) {
                if (is_root_block) {
                    // split root var if needed ...
                }
                continue;
            }

            for (auto* operand : inst->Operands()) {
                if (!operand || seen.Contains(operand)) {
                    continue;
                }

                if (auto* res = As<core::ir::InstructionResult>(operand)) {
                    if (!res->Type()->IsAnyOf<core::type::Struct, core::type::Array>()) {
                        continue;
                    }
                    seen.Add(res);
                    worklist.Push({inst, res});
                } else if (auto* val = As<core::ir::Constant>(operand)) {
                    if (!val->Type()->IsAnyOf<core::type::Struct, core::type::Array>()) {
                        continue;
                    }
                    seen.Add(val);
                    worklist.Push({inst, val});
                }
            }
        }

        for (auto& item : worklist) {
            if (auto* res = As<core::ir::InstructionResult>(item.val)) {
                PutInLet(res);
            } else if (auto* val = As<core::ir::Constant>(item.val)) {
                PutInLet(item.inst, val);
            }
        }
    }

    core::ir::Let* MkLet(core::ir::Value* value) {
        auto* let = b.Let(value->Type());
        value->ReplaceAllUsesWith(let->Result(0));
        let->SetValue(value);

        auto name = b.ir.NameOf(value);
        if (name.IsValid()) {
            b.ir.SetName(let->Result(0), name);
            b.ir.ClearName(value);
        }
        return let;
    }

    void PutInLet(core::ir::Instruction* inst, core::ir::Value* value) {
        auto* let = MkLet(value);
        let->InsertBefore(inst);
    }

    [[maybe_unused]] void PutInLet(core::ir::InstructionResult* value) {
        auto* inst = value->Instruction();
        auto* let = MkLet(value);
        let->InsertAfter(inst);
    }
};

}  // namespace

Result<SuccessType> PromoteInitializers(core::ir::Module& ir) {
    auto result = ValidateAndDumpIfNeeded(ir, "PromoteInitializers transform");
    if (result != Success) {
        return result;
    }

    State{ir}.Process();

    return Success;
}

}  // namespace tint::hlsl::writer::raise
