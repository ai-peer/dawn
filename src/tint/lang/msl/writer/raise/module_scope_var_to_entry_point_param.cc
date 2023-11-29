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

#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/control_instruction.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/core/ir/value.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/pointer.h"
#include "src/tint/lang/core/type/struct.h"
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

// struct S {
//   device float* buffer = nullptr;
//   texture2d<float, access::write> t;
//   threadgroup float* wg_data = nullptr;
//   thread int* private_data = nullptr;
// };
//
// void bar(thread S* s) {
//   s->buffer[0] = 0;
// }
//
// kernel void foo(device float* buffer, texture2d<float, access::write> t [[texture(0)]]) {
//   S s;
//   s.buffer = buffer;
//   s.t = t;
//   bar(&s);
// }

using namespace tint::core::fluent_types;  // NOLINT

using StructMemberDesc = core::type::Manager::StructMemberDesc;

struct State {
    /// The IR module
    core::ir::Module& ir;

    /// The builder
    core::ir::Builder b{ir};

    /// Mapping from a block to the owing function
    Hashmap<core::ir::Block*, core::ir::Function*, 1> blk_to_function{};

    // The symbol for the shared data structure
    Symbol sym = ir.symbols.New("tint_module_vars");

    // Map from a function to the module scoped var parameter
    Hashmap<core::ir::Function*, core::ir::FunctionParam*, 1> function_to_param{};

    // Map from a entrypoint to the new function var parameter
    Hashmap<core::ir::Function*, core::ir::Let*, 1> ep_to_let{};

    // Map from a global to the index in the struct members
    Hashmap<core::ir::Var*, uint32_t, 5> global_to_idx{};

    core::ir::Function* EnclosingFunctionFor(core::ir::Instruction* inst) {
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

    void ExtendCallWithVarsStruct([[maybe_unused]] core::ir::Function* func,
                                  [[maybe_unused]] core::ir::Instruction* inst) {
        // TODO(dsinclair): Pass the module struct through calls to the function
    }

    void ReplaceUsage(core::ir::Value* object,
                      [[maybe_unused]] const core::ir::Usage& usage,
                      core::ir::Var* var) {
        auto* access = b.Access(var->Result(0)->Type(), object,
                                b.Constant(u32(global_to_idx.Get(var).value())));
        usage.instruction->Block()->InsertBefore(usage.instruction, access);
        usage.instruction->SetOperand(usage.operand_index, access->Result(0));
    }

    void Process() {
        Hashmap<core::ir::Function*, Hashset<core::ir::Var*, 5>, 5> function_to_used_vars;
        Hashset<core::ir::Function*, 1> functions_to_process;

        // Record all the function blocks
        for (auto func : ir.functions) {
            blk_to_function.Add(func->Block(), func);
        }

        Vector<core::ir::Var*, 5> globals;

        // Find all the globals
        for (auto* inst : *ir.root_block) {
            if (auto* v = inst->As<core::ir::Var>()) {
                globals.Push(v);
            }
        }

        // Build the module_var struct
        Vector<StructMemberDesc, 8> struct_members{};
        for (auto* v : globals) {
            struct_members.Push(StructMemberDesc{ir.NameOf(v), v->Result(0)->Type(), {}});
            global_to_idx.Add(v, static_cast<uint32_t>(struct_members.Length() - 1));
        }
        auto* strct = ir.Types().Struct(ir.symbols.New("TintModuleVars"), struct_members);

        // Replace usages
        for (auto* v : globals) {
            // Copy the usages list because we'll be modifing it as we go
            auto list = v->Result(0)->Usages();
            for (auto& usage : list) {
                auto* func = EnclosingFunctionFor(usage.instruction);
                TINT_ASSERT(func);

                if (func->Stage() == core::ir::Function::PipelineStage::kUndefined) {
                    // Create the function parameter if needed
                    auto* fp = function_to_param.GetOrCreate(func, [&] {
                        auto* param = b.FunctionParam(strct);
                        func->AddParam(param);
                        return param;
                    });
                    ReplaceUsage(fp, usage, v);
                } else {
                    auto* var = ep_to_let.GetOrCreate(func, [&] {
                        auto* new_var = b.Let(strct);
                        func->Block()->Prepend(new_var);
                        return new_var;
                    });
                    ReplaceUsage(var->Result(0), usage, v);
                }

                function_to_used_vars.GetOrZero(func)->Add(v);
                functions_to_process.Add(func);
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
                auto* enclosing_func = EnclosingFunctionFor(usage.instruction);
                function_worklist.Push(enclosing_func);

                ExtendCallWithVarsStruct(enclosing_func, usage.instruction);

                // Propagate the used variables up to the enclosing function
                auto enclosing_vars = function_to_used_vars.GetOrZero(enclosing_func);
                for (auto* var : *required_vars) {
                    enclosing_vars->Add(var);
                }

                // TODO(dsinclair) : Append new arg to `func` which is a pointer to the module_vars
            }
        }

        for ([[maybe_unused]] const auto* func : ep_list) {
            printf("HANDLE EP struct init\n");
            // Initialize struct
        }

        for (auto* v : globals) {
            v->Destroy();
        }
    }
};

}  // namespace

Result<SuccessType> ModuleScopeVarToEntryPointParam(core::ir::Module& ir) {
    auto result = ValidateAndDumpIfNeeded(ir, "ModuleScopeVarToEntryPointParam transform");
    if (!result) {
        return result.Failure();
    }

    State{ir}.Process();

    return Success;
}

}  // namespace tint::msl::writer::raise
