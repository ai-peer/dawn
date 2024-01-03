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

#include "src/tint/lang/wgsl/writer/raise/ptr_to_ref.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/let.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/lang/core/type/pointer.h"
#include "src/tint/lang/core/type/reference.h"
#include "src/tint/lang/wgsl/ir/unary.h"

namespace tint::wgsl::writer::raise {

Result<SuccessType> PtrToRef(core::ir::Module& mod) {
    core::ir::Builder b{mod};

    Vector<core::ir::Var*, 32> vars;
    for (auto* inst : mod.instructions.Objects()) {
        if (auto* var = inst->As<core::ir::Var>()) {
            vars.Push(var);
        }
    }

    for (auto* var : vars) {
        auto* var_val = var->Result(0);
        auto* ptr_ty = var_val->Type()->As<core::type::Pointer>();
        auto* ref_ty = mod.Types().Get<core::type::Reference>(
            ptr_ty->AddressSpace(), ptr_ty->StoreType(), ptr_ty->Access());
        var_val->SetType(ref_ty);

        core::ir::InstructionResult* ptr_val = nullptr;
        auto ptr = [&] {
            if (ptr_val) {
                return ptr_val;
            }
            ptr_val = b.InstructionResult(ptr_ty);
            auto* unary = mod.instructions.Create<wgsl::ir::Unary>(
                ptr_val, core::UnaryOp::kAddressOf, var_val);
            unary->InsertAfter(var);
            return ptr_val;
        };
        for (auto& use : var_val->Usages().Vector()) {
            Switch(
                use.instruction,
                [&](core::ir::Let*) { use.instruction->SetOperand(use.operand_index, ptr()); },
                [&](core::ir::Call*) { use.instruction->SetOperand(use.operand_index, ptr()); });
        }
    }

    return Success;
}

}  // namespace tint::wgsl::writer::raise
