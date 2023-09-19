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

#include "src/tint/lang/core/ir/transform/direct_variable_access.h"

#include <utility>

#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/clone_context.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/traverse.h"
#include "src/tint/lang/core/ir/user_call.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/utils/containers/reverse.h"

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

namespace tint::core::ir::transform {
namespace {

struct ModuleScopeVar {
    Var* var = nullptr;

    /// @return a hash value for this object
    uint64_t HashCode() const { return Hash(var); }

    /// Inequality operator
    bool operator!=(const ModuleScopeVar& other) const { return var != other.var; }
};

struct PtrParameter {
    type::Type* type = nullptr;

    /// @return a hash value for this object
    uint64_t HashCode() const { return Hash(type); }

    /// Inequality operator
    bool operator!=(const PtrParameter& other) const { return type != other.type; }
};

using Root = std::variant<ModuleScopeVar, PtrParameter>;

struct ConstantIndex {
    uint32_t value;

    /// @return a hash value for this object
    uint64_t HashCode() const { return value; }

    /// Inequality operator
    bool operator!=(const ConstantIndex& other) const { return value != other.value; }
};

struct DynamicIndex {
    /// @return a hash value for this object
    uint64_t HashCode() const { return 42; }

    /// Inequality operator
    bool operator!=(const DynamicIndex&) const { return false; }
};

using AccessOp = std::variant<ConstantIndex, DynamicIndex>;

struct AccessShape {
    Root root;
    Vector<AccessOp, 8> ops;

    /// @returns the number of DynamicIndex operations in #ops.
    uint32_t NumDynamicIndices() const {
        uint32_t count = 0;
        for (auto& op : ops) {
            if (std::holds_alternative<DynamicIndex>(op)) {
                count++;
            }
        }
        return count;
    }

    /// @return a hash value for this object
    uint64_t HashCode() const { return Hash(root, ops); }

    /// Inequality operator
    bool operator!=(const AccessShape& other) const {
        return root != other.root || ops != other.ops;
    }
};

/// AccessChain describes a chain of access expressions originating from a variable.
struct AccessChain : AccessShape {
    /// The originating pointer
    Value* root_ptr = nullptr;
    /// The array of dynamic indices
    Vector<Value*, 8> dynamic_indices;
};

/// The signature of the variant is a map of each of the function's transformed pointer parameters
/// to the caller's AccessShape.
using VariantSignature = Hashmap<FunctionParam*, AccessShape, 4>;

struct FnInfo {
    Hashset<FunctionParam*, 4> ptr_params;
    Hashmap<VariantSignature, Function*, 4> variants;
};

struct FnVariant {
    VariantSignature signature;
    Function* fn = nullptr;
    FnInfo* info = nullptr;
};

/// PIMPL state for the transform.
struct State {
    /// The IR module.
    Module& ir;

    /// The transform options
    const DirectVariableAccessOptions& options;

    /// The IR b.
    Builder b{ir};

    /// The type manager.
    core::type::Manager& ty{ir.Types()};

    /// The symbol table.
    SymbolTable& sym{ir.symbols};

    /// The functions that need transforming
    Hashmap<ir::Function*, FnInfo*, 8> fns{};

    /// Queue of variants that need building
    Vector<FnVariant*, 8> variants_to_build{};

    /// Allocator for FnInfo
    BlockAllocator<FnInfo> fn_info_allocator{};

    /// Allocator for FnVariant
    BlockAllocator<FnVariant> fn_variant_allocator{};

    /// Process the module.
    void Process() {
        BuildFnInfos();

        BuildRootFns();

        while (!variants_to_build.IsEmpty()) {
            BuildFnVariant(variants_to_build.Pop());
        }
    }

    /// Populates fns with all the functions that have pointer parameters which need transforming.
    void BuildFnInfos() {
        for (auto* fn : ir.functions) {
            FnInfo* info = nullptr;
            for (auto* param : fn->Params()) {
                if (auto* ptr = param->Type()->As<type::Pointer>()) {
                    if (PtrParamInfoFor(ptr->AddressSpace()).transform) {
                        if (!info) {
                            info = fn_info_allocator.Create();
                            fns.Add(fn, info);
                        }
                        info->ptr_params.Add(param);
                    }
                }
            }
        }
    }

    void BuildRootFns() {
        for (auto* fn : ir.functions) {
            if (!fns.Contains(fn)) {
                TransformCalls(fn);
            }
        }
    }

    void BuildFnVariant(FnVariant* variant) {
        BuildFnVariantParams(variant);
        TransformCalls(variant->fn);
    }

    void TransformCalls(Function* fn) {
        Traverse(fn->Block(), [&](UserCall* call) {
            Vector<Value*, 8> args;      // New arguments to the call
            VariantSignature signature;  // Signature of the callee variant

            auto* target = call->Target();
            auto* target_info = *fns.Get(target);

            for (size_t i = 0, n = call->Args().Length(); i < n; i++) {
                auto* arg = call->Args()[i];
                auto* param = target->Params()[i];
                if (target_info->ptr_params.Contains(param)) {
                    b.InsertBefore(call, [&] {
                        auto chain = AccessChainFor(arg);
                        signature.Add(param, chain);
                        if (std::holds_alternative<PtrParameter>(chain.root)) {
                            args.Push(chain.root_ptr);
                        }
                        if (size_t array_len = chain.dynamic_indices.Length(); array_len > 0) {
                            auto* array = ty.array(ty.u32(), static_cast<uint32_t>(array_len));
                            auto* indices = b.Construct(array, std::move(chain.dynamic_indices));
                            args.Push(indices->Result());
                        }
                    });
                } else {
                    args.Push(arg);
                }
            }

            auto* new_target = target_info->variants.GetOrCreate(signature, [&] {
                auto* variant_fn = CloneContext{ir}.Clone(target);
                auto* variant = fn_variant_allocator.Create(FnVariant{/* signature */ signature,
                                                                      /* fn */ variant_fn,
                                                                      /* info */ target_info});
                variants_to_build.Push(variant);
                return variant_fn;
            });

            call->SetTarget(new_target);
        });
    }

    AccessChain AccessChainFor(Value* value) {
        AccessChain chain;
        while (value) {
            value = tint::Switch(
                value,  //
                [&](InstructionResult* res) {
                    auto* inst = res->Source();
                    return tint::Switch(
                        inst,
                        [&](Access* access) {
                            for (auto idx : access->Indices()) {
                                if (auto* c = idx->As<Constant>()) {
                                    chain.ops.Push(ConstantIndex{c->Value()->ValueAs<uint32_t>()});
                                } else {
                                    chain.ops.Push(DynamicIndex{});
                                    chain.dynamic_indices.Push(idx);
                                }
                            }
                            return access->Object();
                        },
                        [&](Var* var) {
                            chain.root_ptr = var->Result();
                            return nullptr;
                        },
                        [&](Let* let) { return let->Value(); },
                        [&](Default) {
                            TINT_ICE() << "unhandled instruction type: "
                                       << (inst ? inst->TypeInfo().name : "<null>");
                            return nullptr;
                        });
                },
                [&](FunctionParam* param) {
                    chain.root_ptr = param;
                    return nullptr;
                },
                [&](Default) {
                    TINT_ICE() << "unhandled value type: "
                               << (value ? value->TypeInfo().name : "<null>");
                    return nullptr;
                });
        }
        chain.ops.Reverse();
        chain.dynamic_indices.Reverse();
        return chain;
    }

    void BuildFnVariantParams(FnVariant* variant) {
        b.Prepend(variant->fn->Block(), [&] {
            Vector<ir::FunctionParam*, 8> params;
            for (auto* param : variant->fn->Params()) {  // For each param...
                if (!variant->info->ptr_params.Contains(param)) {
                    // Parameter is not transformed.
                    params.Push(param);
                    continue;
                }

                // Pointer parameter that needs transforming
                auto shape = variant->signature.Get(param);

                Value* root_ptr = nullptr;
                if (auto* ptr_param = std::get_if<PtrParameter>(&shape->root)) {
                    // Root pointer is passed as a parameter
                    auto* root_ptr_param = b.FunctionParam(ptr_param->type);
                    params.Push(root_ptr_param);
                    root_ptr = root_ptr_param;
                } else if (auto* global = std::get_if<ModuleScopeVar>(&shape->root)) {
                    // Root pointer is a module-scope var
                    root_ptr = global->var->Result();
                } else {
                    TINT_ICE() << "unhandled AccessShape root variant";
                }

                ir::FunctionParam* dyn_indices = nullptr;
                if (uint32_t n = shape->NumDynamicIndices(); n > 0) {
                    // Dynamic indices are passed as an array of u32
                    dyn_indices = b.FunctionParam(ty.array(ty.u32(), n));
                    params.Push(dyn_indices);
                }

                // Build an access chain to the pointer
                Vector<ir::Value*, 8> chain;
                uint32_t dynamic_index_index = 0;
                for (auto& op : shape->ops) {
                    if (auto* idx = std::get_if<ConstantIndex>(&op)) {
                        chain.Push(b.Constant(u32(idx->value)));
                    } else {
                        uint32_t dynamic_index = dynamic_index_index++;
                        auto* access = b.Access(ty.u32(), dyn_indices, u32(dynamic_index));
                        chain.Push(access->Result());
                    }
                }

                auto* access = b.Access(param->Type(), root_ptr, std::move(chain));
                param->ReplaceAllUsesWith(access->Result());
                param->Destroy();
            }

            // Replace the function's parameter
            variant->fn->SetParams(std::move(params));
        });
    }

    struct PtrParamInfo {
        bool transform = false;
        bool pass_root_ptr = false;
    };

    PtrParamInfo PtrParamInfoFor(core::AddressSpace space) const {
        PtrParamInfo info;
        switch (space) {
            case core::AddressSpace::kStorage:
            case core::AddressSpace::kUniform:
            case core::AddressSpace::kWorkgroup:
                info.transform = true;
                info.pass_root_ptr = false;
                break;
            case core::AddressSpace::kFunction:
                if (options.transform_function) {
                    info.transform = true;
                    info.pass_root_ptr = true;
                }
                break;
            case core::AddressSpace::kPrivate:
                if (options.transform_private) {
                    info.transform = true;
                    info.pass_root_ptr = false;
                }
                break;
            default:
                break;
        }
        return info;
    }
};

}  // namespace

Result<SuccessType, std::string> DirectVariableAccess(Module* ir,
                                                      const DirectVariableAccessOptions& options) {
    auto result = ValidateAndDumpIfNeeded(*ir, "DirectVariableAccess transform");
    if (!result) {
        return result;
    }

    State{*ir, options}.Process();

    return Success;
}

}  // namespace tint::core::ir::transform
