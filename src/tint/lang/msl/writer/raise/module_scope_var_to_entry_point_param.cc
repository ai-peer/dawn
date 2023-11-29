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

#include "src/tint/lang/msl/writer/raise/module_scope_var_to_entry_point_param.h"

#include "src/tint/lang/core/ir/control_instruction.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/core/ir/value.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/pointer.h"
#include "src/tint/utils/containers/hashmap.h"
#include "src/tint/utils/containers/vector.h"

namespace tint::msl::writer::raise {
namespace {

// Need to create a struct to pass in `texture`, `sampler`, `buffer` things
// Create an internal struct and assign used module scoped items into that struct as pointers
// Pass the struct around as `thread *S* and use through the buffer
// Each function receives the struct if it needs it, update calls to pass the struct through

// Find module scope non-constant items
// For each item, get Usages
// From usages, find enclosing function
// For each function, find usages
//
// Make <private> structure, all <private> go there
// Storage and Uniform address spaces are redeclared as entry
// point parameters with a pointer type

core::ir::Function* EnclosingFunctionFor(
    const Hashmap<core::ir::Block*, core::ir::Function*, 1>& blk_to_function,
    core::ir::Instruction* inst) {
    auto* blk = inst->Block();
    while (blk) {
        // If there is no parent, then this must be a function block
        if (!blk->Parent()) {
            auto val = blk_to_function.Get(blk);
            return val.has_value() ? val.value() : nullptr;
        }

        blk = blk->Parent()->Block();
    }

    return nullptr;
}

void ExtendCallWithVarsStruct([[maybe_unused]] core::ir::Instruction* func) {}

void Run(core::ir::Module& ir) {
    Hashmap<core::ir::Function*, Hashset<core::ir::Var*, 5>, 5> function_to_used_vars;
    Hashset<core::ir::Function*, 1> functions_to_process;

    Hashmap<core::ir::Block*, core::ir::Function*, 1> blk_to_function;

    // Record all the function blocks
    for (auto func : ir.functions) {
        blk_to_function.Add(func->Block(), func);
    }

    // Find all the globals
    for (auto* inst : *ir.root_block) {
        if (auto* v = inst->As<core::ir::Var>()) {
            for (auto& usage : v->Result(0)->Usages()) {
                auto* func = EnclosingFunctionFor(blk_to_function, usage.instruction);
                TINT_ASSERT(func);

                function_to_used_vars.GetOrZero(func)->Add(v);
                functions_to_process.Add(func);

                // TODO(dsinclair); Replace usage
            }
        }
    }

    auto function_worklist = functions_to_process.Vector();

    Hashset<core::ir::Function*, 5> seen_functions;
    Hashset<core::ir::Function*, 1> ep_list;
    while (!function_worklist.IsEmpty()) {
        auto* func = function_worklist.Pop();
        if (seen_functions.Contains(func)) {
            continue;
        }
        seen_functions.Add(func);

        auto required_vars = function_to_used_vars.GetOrZero(func);

        // Entry points will be handled later after we've collected up all the needed vars.
        if (func->Stage() != core::ir::Function::PipelineStage::kUndefined) {
            ep_list.Add(func);
            continue;
        }

        for (const auto& usage : func->Usages()) {
            auto* enclosing_func = EnclosingFunctionFor(blk_to_function, usage.instruction);
            function_worklist.Push(enclosing_func);

            // TODO(dsinclair): If the enclosing function is an entry point, then this call is
            // probably different as it has to make the pointer, not just pass it ...
            ExtendCallWithVarsStruct(usage.instruction);

            // Propagate the used variables up to the enclosing function
            auto enclosing_vars = function_to_used_vars.GetOrZero(enclosing_func);
            for (auto* var : *required_vars) {
                enclosing_vars->Add(var);
            }

            // TODO(dsinclair) : Append new arg to `func` which is a pointer to the module_vars
        }
    }

    for ([[maybe_unused]] const auto* func : ep_list) {
        // Initialize struct
    }

    //    for (auto* inst : *ir.root_block) {
    //        if (auto* v = inst->As<core::ir::Var>()) {
    //            v->Destroy();
    //        }
    //    }
}

}  // namespace

Result<SuccessType> ModuleScopeVarToEntryPointParam(core::ir::Module& ir) {
    auto result = ValidateAndDumpIfNeeded(ir, "ModuleScopeVarToEntryPointParam transform");
    if (!result) {
        return result.Failure();
    }

    Run(ir);

    return Success;
}

}  // namespace tint::msl::writer::raise
