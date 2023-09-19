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

#include "src/tint/lang/core/ir/transform/zero_init_workgroup_memory.h"

#include <map>
#include <utility>

#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/validator.h"

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

namespace tint::core::ir::transform {

namespace {

/// PIMPL state for the transform.
struct State {
    /// The IR module.
    Module* ir = nullptr;

    /// The IR builder.
    Builder b{*ir};

    /// The type manager.
    core::type::Manager& ty{ir->Types()};

    /// A map from variable to an ID used for sorting.
    Hashmap<Var*, uint32_t, 4> var_to_id{};

    /// A map from blocks to their directly referenced workgroup variables.
    Hashmap<Block*, Hashset<Var*, 4>, 64> block_to_direct_vars{};

    /// A map from functions to their transitively referenced workgroup variables.
    Hashmap<Function*, Hashset<Var*, 4>, 4> function_to_transitive_vars{};

    /// An IndexBuilder is a function that produces an index operand for an access instruction from
    /// a linearized index and a bool that indicates if this is the last index in the chain.
    using IndexBuilder = std::function<Value*(Value* linear_index, bool is_last)>;

    /// Store describes a store to a sub-element of a workgroup variable.
    struct Store {
        /// The workgroup variable.
        Var* var = nullptr;
        /// The store type of the element.
        const type::Type* store_type = nullptr;
        /// The list of index operands to get to the element.
        Vector<IndexBuilder, 4> indices;
    };

    /// StoreMap is a map from iteration count to a list of `Store` descriptors.
    using StoreMap = std::map<uint32_t, Vector<Store, 4>>;

    /// Process the module.
    void Process() {
        if (!ir->root_block) {
            return;
        }

        // Loop over module-scope variables, looking for workgroup variables.
        uint32_t next_id = 0;
        for (auto inst : *ir->root_block) {
            if (auto* var = inst->As<Var>()) {
                auto* ptr = var->Result()->Type()->As<core::type::Pointer>();
                if (ptr && ptr->AddressSpace() == core::AddressSpace::kWorkgroup) {
                    // Record the usage of the variable for each block that references it.
                    var->Result()->ForEachUse([&](const Usage& use) {
                        block_to_direct_vars.GetOrZero(use.instruction->Block())->Add(var);
                    });
                    var_to_id.Add(var, next_id++);
                }
            }
        }

        // Process each entry point function.
        for (auto* func : ir->functions) {
            if (func->Stage() == Function::PipelineStage::kCompute) {
                ProcessEntryPoint(func);
            }
        }
    }

    /// Get the set of workgroup variables transitively referenced by @p func.
    /// @param func the function
    /// @returns the set of transitively referenced workgroup variables
    Hashset<Var*, 4> GetReferencedVars(Function* func) {
        return function_to_transitive_vars.GetOrCreate(func, [&] {
            Hashset<Var*, 4> vars;
            GetReferencedVars(func->Block(), vars);
            return vars;
        });
    }

    /// Get the set of workgroup variables transitively referenced by @p block.
    /// @param block the block
    /// @param vars the set of transitively referenced workgroup variables to populate
    void GetReferencedVars(Block* block, Hashset<Var*, 4>& vars) {
        // Add directly referenced vars.
        auto itr = block_to_direct_vars.Find(block);
        if (itr) {
            for (auto* var : *itr) {
                vars.Add(var);
            }
        }

        // Loop over instructions in the block.
        for (auto* inst : *block) {
            tint::Switch(
                inst,
                [&](UserCall* call) {
                    // Get variables referenced by a function called from this block.
                    auto callee_vars = GetReferencedVars(call->Func());
                    for (auto* var : callee_vars) {
                        vars.Add(var);
                    }
                },
                [&](ControlInstruction* ctrl) {
                    // Recurse into control instructions and gather their referenced vars.
                    ctrl->ForeachBlock([&](Block* blk) { GetReferencedVars(blk, vars); });
                });
        }
    }

    /// Check if a type can be efficiently zeroed with a single store. Returns `false` if there are
    /// any nested arrays or atomics.
    /// @param type the type to inspect
    /// @returns true if a variable with store type @p ty can be efficiently zeroed
    bool CanTriviallyZero(const core::type::Type* type) {
        if (type->IsAnyOf<core::type::Atomic, core::type::Array>()) {
            return false;
        }
        if (auto* str = type->As<core::type::Struct>()) {
            for (auto* member : str->Members()) {
                if (!CanTriviallyZero(member->Type())) {
                    return false;
                }
            }
        }
        return true;
    }

    /// Recursively generate stores for a workgroup variable.
    /// @param var the workgroup variable
    /// @param type the current type
    /// @param iteration_count the iteration count to get to this depth
    /// @param indices the access indices to get to this depth
    /// @param stores the map of stores to populate
    void GenerateStores(Var* var,
                        const type::Type* type,
                        uint32_t iteration_count,
                        Vector<IndexBuilder, 4> indices,
                        StoreMap& stores) {
        // If this type can be trivially zeroed, store to the whole element.
        if (CanTriviallyZero(type)) {
            stores[iteration_count].Push(Store{var, type, indices});
            return;
        }

        tint::Switch(
            type,
            [&](const type::Array* arr) {
                // Add a new index to the list.
                TINT_ASSERT(arr->ConstantCount());
                auto count = arr->ConstantCount().value();
                auto new_indices = indices;
                new_indices.Push([&, iteration_count, count](Value* linear_index, bool is_last) {
                    Value* idx = linear_index;
                    if (iteration_count > 1) {
                        idx = b.Divide(ty.u32(), idx, u32(iteration_count))->Result();
                    }
                    if (!is_last) {
                        idx = b.Modulo(ty.u32(), idx, u32(count))->Result();
                    }
                    return idx;
                });

                // Recurse into the array element type.
                GenerateStores(var, arr->ElemType(), iteration_count * count, new_indices, stores);
            },
            [&](const type::Atomic*) {
                stores[iteration_count].Push(Store{var, type, indices});
            },
            [&](const type::Struct* str) {
                for (auto* member : str->Members()) {
                    // Add the member index to the index list and recurse into its type.
                    auto new_indices = indices;
                    new_indices.Push(
                        [&, member](Value*, bool) { return b.Constant(u32(member->Index())); });
                    GenerateStores(var, member->Type(), iteration_count, new_indices, stores);
                }
            },
            [&](Default) { TINT_UNREACHABLE(); });
    }

    /// Get or inject an entry point builtin for the local invocation index.
    /// @param func the entry point function
    /// @returns the local invocation index builtin
    Value* GetLocalInvocationIndex(Function* func) {
        // Look for an existing local_invocation_index builtin parameter.
        for (auto* param : func->Params()) {
            if (auto* str = param->Type()->As<type::Struct>()) {
                // Check each member for the local invocation index builtin attribute.
                for (auto* member : str->Members()) {
                    if (member->Attributes().builtin && member->Attributes().builtin.value() ==
                                                            BuiltinValue::kLocalInvocationIndex) {
                        auto* access = b.Access(ty.u32(), param, u32(member->Index()));
                        access->InsertBefore(func->Block()->Front());
                        return access->Result();
                    }
                }
            } else {
                // Check if the parameter is the local invocation index.
                if (param->Builtin() &&
                    param->Builtin().value() == FunctionParam::Builtin::kLocalInvocationIndex) {
                    return param;
                }
            }
        }

        // No local invocation index was found, so add one to the parameter list and use that.
        Vector<FunctionParam*, 4> params = func->Params();
        auto* param = b.FunctionParam(ty.u32());
        param->SetBuiltin(FunctionParam::Builtin::kLocalInvocationIndex);
        params.Push(param);
        func->SetParams(params);
        return param;
    }

    /// Process an entry point function to zero-initialize the workgroup variables that it uses.
    /// @param func the entry point function
    void ProcessEntryPoint(Function* func) {
        // Get list of transitively referenced workgroup variables.
        auto vars = GetReferencedVars(func);
        if (vars.IsEmpty()) {
            return;
        }

        // Sort the variables to get deterministic output in tests.
        auto sorted_vars = vars.Vector();
        sorted_vars.Sort([&](Var* first, Var* second) {
            return *var_to_id.Get(first) < *var_to_id.Get(second);
        });

        // Build list of zeroing statements.
        StoreMap stores;
        for (auto* var : sorted_vars) {
            GenerateStores(var, var->Result()->Type()->UnwrapPtr(), 1, {}, stores);
        }

        // Capture the first instruction of the function.
        // All new instructions will be inserted before this.
        auto* function_start = func->Block()->Front();

        // Helper to build the store instructions for a given element store from a linearized index.
        auto build = [&](const Store& store, Value* linear_index) {
            auto* to = store.var->Result();
            if (!store.indices.IsEmpty()) {
                Vector<Value*, 4> indices;
                for (auto& idx : store.indices) {
                    // TODO: true when last
                    // TODO: flip so innermost is adjacent instead
                    indices.Push(idx(linear_index, false));
                }
                to = b.Access(ty.ptr(workgroup, store.store_type), to, indices)->Result();
            }

            if (auto* atomic = store.store_type->As<type::Atomic>()) {
                auto* zero = b.Constant(ir->constant_values.ZeroValue(atomic->Type()));
                b.Call(ty.void_(), core::Function::kAtomicStore, to, zero);
            } else {
                auto* zero = b.Constant(ir->constant_values.ZeroValue(store.store_type));
                b.Store(to, zero);
            }
        };

        // Get the local invocation index and the linearized workgroup size.
        auto* local_index = GetLocalInvocationIndex(func);
        auto wgsizes = func->WorkgroupSize().value();
        auto wgsize = wgsizes[0] * wgsizes[1] * wgsizes[2];

        // Insert instructions to zero-initialize every variable.
        b.InsertBefore(function_start, [&] {
            for (auto& entry : stores) {
                if (entry.first == 1u) {
                    // Make the first invocation in the group perform all of the non-arrayed stores.
                    auto* ifelse = b.If(b.Equal(ty.bool_(), local_index, 0_u));
                    b.Append(ifelse->True(), [&] {
                        for (auto& store : entry.second) {
                            build(store, nullptr);
                        }
                        b.ExitIf(ifelse);
                    });
                } else {
                    // Generate a loop for each unique iteration count, that will store to every
                    // arrayed element that has that iteration count.
                    // The loop is equivalent to:
                    //   for (var idx = local_index; idx < linear_iteration_count; idx += wgsize) {
                    //     <store to element at `idx`>
                    //   }
                    auto* loop = b.Loop();
                    auto* index = b.BlockParam(ty.u32());
                    loop->Body()->SetParams({index});
                    b.Append(loop->Initializer(), [&] {  //
                        b.NextIteration(loop, local_index);
                    });
                    b.Append(loop->Body(), [&] {
                        auto* gt_max = b.GreaterThan(ty.bool_(), local_index, u32(entry.first));
                        auto* ifelse = b.If(gt_max);
                        b.Append(ifelse->True(), [&] {  //
                            b.ExitLoop(loop);
                        });
                        for (auto& store : entry.second) {
                            build(store, index);
                        }
                        b.Continue(loop);
                    });
                    b.Append(loop->Continuing(), [&] {  //
                        // Increment the loop index by linearized workgroup size.
                        b.NextIteration(loop, b.Add(ty.u32(), index, u32(wgsize)));
                    });
                }
            }
            b.Call(ty.void_(), core::Function::kWorkgroupBarrier);
        });
    }
};

}  // namespace

Result<SuccessType, std::string> ZeroInitWorkgroupMemory(Module* ir) {
    auto result = ValidateAndDumpIfNeeded(*ir, "ZeroInitWorkgroupMemory transform");
    if (!result) {
        return result;
    }

    State{ir}.Process();

    return Success;
}

}  // namespace tint::core::ir::transform
