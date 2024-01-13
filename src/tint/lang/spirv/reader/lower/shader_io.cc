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

#include "src/tint/lang/spirv/reader/lower/shader_io.h"

#include <utility>

#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/transform/common/referenced_module_vars.h"
#include "src/tint/lang/core/ir/validator.h"

namespace tint::spirv::reader::lower {

namespace {

/// PIMPL state for the transform.
struct State {
    /// The IR module.
    core::ir::Module& ir;

    /// The IR builder.
    core::ir::Builder b{ir};

    /// The type manager.
    core::type::Manager& ty{ir.Types()};

    /// A map from block to its containing function.
    Hashmap<core::ir::Block*, core::ir::Function*, 64> block_to_function_{};

    /// A map from each function to a map from input variable to parameter.
    Hashmap<core::ir::Function*, Hashmap<core::ir::Var*, core::ir::FunctionParam*, 4>, 8>
        function_parameter_map{};

    /// The mapping from functions to their transitively referenced output variables.
    core::ir::ReferencedModuleVars referenced_output_vars_{
        ir, [](const core::ir::Var* var) {
            auto* view = var->Result(0)->Type()->As<core::type::MemoryView>();
            return view && view->AddressSpace() == core::AddressSpace::kOut;
        }};

    /// Process the module.
    void Process() {
        // Process outputs first, as that may introduce new functions that input variables need to
        // be propagated through.
        ProcessOutputs();
        ProcessInputs();
    }

    /// Process output variables.
    void ProcessOutputs() {
        // Gather the list of output variables and update their address spaces.
        Vector<core::ir::Var*, 4> outputs;
        for (auto* global : *ir.root_block) {
            if (auto* var = global->As<core::ir::Var>()) {
                auto addrspace = var->Result(0)->Type()->As<core::type::Pointer>()->AddressSpace();
                if (addrspace == core::AddressSpace::kOut) {
                    // Change the address space of the variable to private and update its uses.
                    ReplaceOutputPointerAddressSpace(var->Result(0));
                    outputs.Push(var);
                }
            }
        }

        // Update entry point functions to return their outputs, using a wrapper function.
        // Use a worklist as `ProcessEntryPointOutputs()` will add new functions.
        Vector<core::ir::Function*, 4> entry_points;
        for (auto& func : ir.functions) {
            if (func->Stage() != core::ir::Function::PipelineStage::kUndefined) {
                entry_points.Push(func);
            }
        }
        for (auto& ep : entry_points) {
            ProcessEntryPointOutputs(ep);
        }

        // Remove attributes from all of the original module-scope output variables.
        for (auto* var : outputs) {
            var->SetAttributes({});
        }
    }

    /// Process input variables.
    void ProcessInputs() {
        // Seed the block-to-function map with the function entry blocks.
        for (auto& func : ir.functions) {
            block_to_function_.Add(func->Block(), func);
        }

        // Gather the list of all input variables.
        Vector<core::ir::Var*, 4> inputs;
        for (auto* global : *ir.root_block) {
            if (auto* var = global->As<core::ir::Var>()) {
                auto addrspace = var->Result(0)->Type()->As<core::type::Pointer>()->AddressSpace();
                if (addrspace == core::AddressSpace::kIn) {
                    inputs.Push(var);
                }
            }
        }

        // Replace all module-scope input variables with function parameters.
        for (auto* var : inputs) {
            ReplaceInputPointerUses(var, var->Result(0));
            var->Destroy();
        }

        // Copy the input variable attributes to each entry point's function parameters.
        for (auto& func : ir.functions) {
            if (func->Stage() != core::ir::Function::PipelineStage::kUndefined) {
                AddEntryPointParameterAttributes(func);
            }
        }
    }

    /// Replace an output pointer address space to make it `private`.
    /// @param value the output variable
    void ReplaceOutputPointerAddressSpace(core::ir::InstructionResult* value) {
        // Change the address space to `private`.
        auto* old_ptr_type = value->Type();
        auto* new_ptr_type = ty.ptr(core::AddressSpace::kPrivate, old_ptr_type->UnwrapPtr());
        value->SetType(new_ptr_type);

        // Update all uses of the module-scope variable.
        value->ForEachUse([&](core::ir::Usage use) {
            Switch(
                use.instruction,  //
                [&](core::ir::Access* a) { ReplaceOutputPointerAddressSpace(a->Result(0)); },
                [&](core::ir::Load*) { /* nothing to do */ },
                [&](core::ir::LoadVectorElement*) { /* nothing to do */ },
                [&](core::ir::Store*) { /* nothing to do */ },
                [&](core::ir::StoreVectorElement*) { /* nothing to do */ },
                //
                TINT_ICE_ON_NO_MATCH);
        });
    }

    /// Process the outputs of an entry point function, adding a wrapper function to forward outputs
    /// through the return value.
    /// @param ep the entry point
    void ProcessEntryPointOutputs(core::ir::Function* ep) {
        const auto& referenced_outputs = referenced_output_vars_.TransitiveReferences(ep);
        if (referenced_outputs.IsEmpty()) {
            return;
        }

        // Add a wrapper function to return either a single value or a struct.
        auto* wrapper = b.Function(ty.void_(), ep->Stage());
        if (auto name = ir.NameOf(ep)) {
            ir.SetName(ep, name.Name() + "_inner");
            ir.SetName(wrapper, name);
        }

        // Call the original entry point and make it a regular function.
        ep->SetStage(core::ir::Function::PipelineStage::kUndefined);
        b.Append(wrapper->Block(), [&] {  //
            b.Call(ep);
        });

        if (referenced_outputs.Length() == 1) {
            // Return the output from the wrapper function.
            auto* output = referenced_outputs[0];
            wrapper->SetReturnType(output->Result(0)->Type()->UnwrapPtr());
            b.Append(wrapper->Block(), [&] {  //
                b.Return(wrapper, b.Load(output));
            });

            // Copy the output attributes to the function return.
            const auto& attributes = output->Attributes();
            wrapper->SetReturnInvariant(attributes.invariant);
            if (attributes.builtin) {
                wrapper->SetReturnBuiltin(attributes.builtin.value());
            } else if (attributes.location) {
                core::ir::Location loc;
                loc.value = attributes.location.value();
                if (attributes.interpolation) {
                    loc.interpolation = attributes.interpolation;
                }
                wrapper->SetReturnLocation(std::move(loc));
            }
        } else {
            // Create a struct to hold all of the output values.
            Vector<core::type::Manager::StructMemberDesc, 4> members;
            for (auto* output : referenced_outputs) {
                // Copy the output attributes to the struct member.
                const auto& var_attributes = output->Attributes();
                core::type::StructMemberAttributes member_attributes;
                member_attributes.invariant = var_attributes.invariant;
                if (var_attributes.builtin) {
                    member_attributes.builtin = var_attributes.builtin.value();
                } else if (var_attributes.location) {
                    member_attributes.location = var_attributes.location.value();
                    if (var_attributes.interpolation) {
                        member_attributes.interpolation = var_attributes.interpolation;
                    }
                }

                auto name = ir.NameOf(output);
                if (!name) {
                    name = ir.symbols.New();
                }
                auto* type = output->Result(0)->Type()->UnwrapPtr();
                members.Push(core::type::Manager::StructMemberDesc{name, type,
                                                                   std::move(member_attributes)});
            }
            auto* str = ty.Struct(ir.symbols.New(), std::move(members));
            wrapper->SetReturnType(str);

            // Collect the output values and return them from the wrapper function.
            b.Append(wrapper->Block(), [&] {
                Vector<core::ir::Value*, 4> args;
                for (auto* output : referenced_outputs) {
                    args.Push(b.Load(output)->Result(0));
                }
                b.Return(wrapper, b.Construct(str, std::move(args)));
            });
        }
    }

    /// Replace a use of an input pointer value.
    /// @param var the originating input variable
    /// @param value the input pointer value
    void ReplaceInputPointerUses(core::ir::Var* var, core::ir::Value* value) {
        Vector<core::ir::Instruction*, 8> to_destroy;
        value->ForEachUse([&](core::ir::Usage use) {
            // Get (or create) the function parameter that will replace the variable.
            auto* func = ContainingFunction(use.instruction);
            auto* param = GetParameter(func, var);

            Switch(
                use.instruction,
                [&](core::ir::Load* l) {
                    // Fold the load away and replace its uses with the new parameter.
                    l->Result(0)->ReplaceAllUsesWith(param);
                    to_destroy.Push(l);
                },
                [&](core::ir::LoadVectorElement* lve) {
                    // Replace the vector element load with an access instruction.
                    auto* access = b.Access(lve->Result(0)->Type(), param, lve->Index());
                    access->InsertBefore(lve);
                    lve->Result(0)->ReplaceAllUsesWith(access->Result(0));
                    to_destroy.Push(lve);
                },
                [&](core::ir::Access* a) {
                    // Fold the access away and replace its uses.
                    ReplaceInputPointerUses(var, a->Result(0));
                    to_destroy.Push(a);
                },
                TINT_ICE_ON_NO_MATCH);
        });

        // Clean up orphaned instructions.
        for (auto* inst : to_destroy) {
            inst->Destroy();
        }
    }

    /// Get the function that contains an instruction.
    /// @param inst the instruction
    /// @returns the function
    core::ir::Function* ContainingFunction(core::ir::Instruction* inst) {
        return block_to_function_.GetOrCreate(inst->Block(), [&] {  //
            return ContainingFunction(inst->Block()->Parent());
        });
    }

    /// Get or create a function parameter to replace a module-scope variable.
    /// @param func the function
    /// @param var the module-scope variable
    /// @returns the function parameter
    core::ir::FunctionParam* GetParameter(core::ir::Function* func, core::ir::Var* var) {
        return function_parameter_map.GetOrZero(func)->GetOrCreate(var, [&] {
            // Create a new function parameter.
            auto* param = b.FunctionParam(var->Result(0)->Type()->UnwrapPtr());
            if (auto name = ir.NameOf(var)) {
                ir.SetName(param, name);
            }

            // Append the parameter to the parameter list.
            Vector<core::ir::FunctionParam*, 4> params = func->Params();
            params.Push(param);
            func->SetParams(std::move(params));

            // Update the callsites of this function.
            func->ForEachUse([&](core::ir::Usage use) {
                Switch(
                    use.instruction,
                    [&](core::ir::UserCall* call) {
                        // Recurse into the calling function.
                        auto* caller = ContainingFunction(call);
                        call->AppendArg(GetParameter(caller, var));
                    },
                    [&](core::ir::Return*) {
                        // Nothing to do.
                    },
                    TINT_ICE_ON_NO_MATCH);
            });

            return param;
        });
    }

    /// Add attributes to the parameters of an entry point function.
    /// @param ep the entry point
    void AddEntryPointParameterAttributes(core::ir::Function* ep) {
        for (auto var_to_param : *function_parameter_map.GetOrZero(ep)) {
            const auto& attributes = var_to_param.key->Attributes();
            var_to_param.value->SetInvariant(attributes.invariant);
            if (attributes.builtin) {
                var_to_param.value->SetBuiltin(attributes.builtin.value());
            } else if (attributes.location) {
                core::ir::Location loc;
                loc.value = attributes.location.value();
                if (attributes.interpolation) {
                    loc.interpolation = attributes.interpolation;
                }
                var_to_param.value->SetLocation(std::move(loc));
            }
        }
    }
};

}  // namespace

Result<SuccessType> ShaderIO(core::ir::Module& ir) {
    auto result = ValidateAndDumpIfNeeded(ir, "ShaderIO transform");
    if (result != Success) {
        return result.Failure();
    }

    State{ir}.Process();

    return Success;
}

}  // namespace tint::spirv::reader::lower
