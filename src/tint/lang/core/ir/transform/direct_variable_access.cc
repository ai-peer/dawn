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

// An access chain root, originating from a module-scope var.
struct RootModuleScopeVar {
    Var* var = nullptr;

    /// @return a hash value for this object
    uint64_t HashCode() const { return Hash(var); }

    /// Inequality operator
    bool operator!=(const RootModuleScopeVar& other) const { return var != other.var; }
};

// An access chain root, originating from another pointer parameter or function-scope var, passed as
// a pointer parameter.
struct RootPtrParameter {
    const type::Type* type = nullptr;

    /// @return a hash value for this object
    uint64_t HashCode() const { return Hash(type); }

    /// Inequality operator
    bool operator!=(const RootPtrParameter& other) const { return type != other.type; }
};

using Root = std::variant<RootModuleScopeVar, RootPtrParameter>;

struct MemberAccess {
    const type::StructMember* member;

    /// @return a hash member for this object
    uint64_t HashCode() const { return Hash(member); }

    /// Inequality operator
    bool operator!=(const MemberAccess& other) const { return member != other.member; }
};

struct IndexAccess {
    /// @return a hash value for this object
    uint64_t HashCode() const { return 42; }

    /// Inequality operator
    bool operator!=(const IndexAccess&) const { return false; }
};

using AccessOp = std::variant<MemberAccess, IndexAccess>;

struct AccessShape {
    Root root;
    Vector<AccessOp, 8> ops;

    /// @returns the number of IndexAccess operations in #ops.
    uint32_t NumIndexAccesses() const {
        uint32_t count = 0;
        for (auto& op : ops) {
            if (std::holds_alternative<IndexAccess>(op)) {
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
    Vector<Value*, 8> indices;
};

/// The signature of the variant is a map of the index of the function's pointer parameter to the
/// caller's AccessShape.
using VariantSignature = Hashmap<size_t, AccessShape, 4>;

struct FnInfo {
    Hashmap<VariantSignature, Function*, 4> variants_by_sig;
    Vector<Function*, 4> ordered_variants;
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
    Hashmap<ir::Function*, FnInfo*, 8> fns_to_transform{};

    /// Queue of variants that need building
    Vector<FnVariant*, 8> variants_to_build{};

    /// Allocator for FnInfo
    BlockAllocator<FnInfo> fn_info_allocator{};

    /// Allocator for FnVariant
    BlockAllocator<FnVariant> fn_variant_allocator{};

    /// Process the module.
    void Process() {
        auto input_fns = ir.functions;

        GatherFnsThatNeedTransforming();

        BuildRootFns();

        BuildFnVariants();

        ir.functions.Clear();
        for (auto* fn : input_fns) {
            if (auto info = fns_to_transform.Get(fn)) {
                for (auto variant : (*info)->ordered_variants) {
                    ir.functions.Push(variant);
                }
            } else {
                ir.functions.Push(fn);
            }
        }
    }

    /// Populates fns_to_transform with all the functions that have pointer parameters which need
    /// transforming.
    void GatherFnsThatNeedTransforming() {
        for (auto* fn : ir.functions) {
            for (auto* param : fn->Params()) {
                if (ParamNeedsTransforming(param)) {
                    fns_to_transform.Add(fn, fn_info_allocator.Create());
                    break;
                }
            }
        }
    }

    void BuildRootFns() {
        for (auto* fn : ir.functions) {
            if (!fns_to_transform.Contains(fn)) {
                TransformCalls(fn);
            }
        }
    }

    void TransformCalls(Function* fn) {
        printf("TransformCalls(%s)\n", ir.NameOf(fn).Name().c_str());
        Traverse(fn->Block(), [&](UserCall* call) {
            Vector<Value*, 8> new_args;  // New arguments to the call
            VariantSignature signature;  // Signature of the callee variant

            auto* target = call->Target();
            auto target_info = fns_to_transform.Get(target);
            if (!target_info) {
                return;  // Not a call to a function that has pointer parameters.
            }

            Vector<Value*, 8> replaced_args;
            for (size_t i = 0, n = call->Args().Length(); i < n; i++) {
                auto* arg = call->Args()[i];
                auto* param = target->Params()[i];
                if (ParamNeedsTransforming(param)) {
                    b.InsertBefore(call, [&] {
                        auto chain = AccessChainFor(arg);
                        signature.Add(i, chain);
                        if (std::holds_alternative<RootPtrParameter>(chain.root)) {
                            new_args.Push(chain.root_ptr);
                        }
                        if (size_t array_len = chain.indices.Length(); array_len > 0) {
                            auto* array = ty.array(ty.u32(), static_cast<uint32_t>(array_len));
                            auto* indices = b.Construct(array, std::move(chain.indices));
                            new_args.Push(indices->Result());
                        }
                    });
                    replaced_args.Push(arg);
                } else {
                    new_args.Push(arg);
                }
            }

            call->SetArgs(std::move(new_args));
            for (auto* old_arg : replaced_args) {
                DeleteDeadInstructions(old_arg);
            }

            auto* new_target = (*target_info)->variants_by_sig.GetOrCreate(signature, [&] {
                auto* variant_fn = CloneContext{ir}.Clone(target);
                (*target_info)->ordered_variants.Push(variant_fn);

                if (auto fn_name = ir.NameOf(variant_fn); fn_name.IsValid()) {
                    StringStream variant_name;
                    variant_name << fn_name.NameView();
                    auto params = signature.Keys().Sort();
                    for (auto param_idx : params) {
                        variant_name << "_" << AccessShapeName(*signature.Get(param_idx));
                    }
                    ir.SetName(variant_fn, variant_name.str());
                }

                auto* variant = fn_variant_allocator.Create(FnVariant{/* signature */ signature,
                                                                      /* fn */ variant_fn,
                                                                      /* info */ *target_info});
                variants_to_build.Push(variant);
                return variant_fn;
            });

            call->SetTarget(new_target);
        });
    }

    void BuildFnVariants() {
        while (!variants_to_build.IsEmpty()) {
            auto* variant = variants_to_build.Pop();
            BuildFnVariantParams(variant);
            TransformCalls(variant->fn);
        }
    }

    AccessChain AccessChainFor(Value* value) {
        AccessChain chain;
        while (value) {
            TINT_ASSERT(value->Alive());
            value = tint::Switch(
                value,  //
                [&](InstructionResult* res) {
                    auto* inst = res->Source();
                    return tint::Switch(
                        inst,
                        [&](Access* access) {
                            Vector<AccessOp, 8> ops;
                            Vector<Value*, 8> indices;

                            auto* obj_ty = access->Object()->Type()->UnwrapPtr();
                            for (auto idx : access->Indices()) {
                                if (auto* str = obj_ty->As<type::Struct>()) {
                                    TINT_ASSERT(idx->Is<Constant>());
                                    auto i = idx->As<Constant>()->Value()->ValueAs<uint32_t>();
                                    auto* member = str->Members()[i];
                                    ops.Push(MemberAccess{member});
                                    obj_ty = member->Type();
                                    continue;
                                }

                                // Ensure index is u32
                                if (!idx->Type()->Is<type::U32>()) {
                                    idx = b.Convert(ty.u32(), idx)->Result();
                                }

                                ops.Push(IndexAccess{});
                                indices.Push(idx);
                                obj_ty = obj_ty->Elements().type;
                            }

                            for (auto& op : Reverse(ops)) {
                                chain.ops.Push(op);
                            }
                            for (auto& idx : Reverse(indices)) {
                                chain.indices.Push(idx);
                            }

                            TINT_ASSERT(obj_ty == access->Result()->Type()->UnwrapPtr());
                            return access->Object();
                        },
                        [&](Var* var) {
                            if (var->Block() == ir.root_block) {
                                chain.root = RootModuleScopeVar{var};
                            } else {
                                chain.root = RootPtrParameter{var->Result()->Type()};
                            }
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
                    chain.root = RootPtrParameter{param->Type()};
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
        chain.indices.Reverse();

        return chain;
    }

    void BuildFnVariantParams(FnVariant* variant) {
        b.InsertBefore(variant->fn->Block()->Front(), [&] {
            const auto old_params = variant->fn->Params();
            Vector<ir::FunctionParam*, 8> new_params;
            for (size_t param_idx = 0; param_idx < old_params.Length(); param_idx++) {
                auto* old_param = old_params[param_idx];
                if (!ParamNeedsTransforming(old_param)) {
                    // Parameter is not transformed.
                    new_params.Push(old_param);
                    continue;
                }

                // Pointer parameter that needs transforming
                auto shape = variant->signature.Get(param_idx);

                Value* root_ptr = nullptr;
                FunctionParam* root_ptr_param = nullptr;
                if (auto* ptr_param = std::get_if<RootPtrParameter>(&shape->root)) {
                    // Root pointer is passed as a parameter
                    root_ptr_param = b.FunctionParam(ptr_param->type);
                    new_params.Push(root_ptr_param);
                    root_ptr = root_ptr_param;
                } else if (auto* global = std::get_if<RootModuleScopeVar>(&shape->root)) {
                    // Root pointer is a module-scope var
                    root_ptr = global->var->Result();
                } else {
                    TINT_ICE() << "unhandled AccessShape root variant";
                }

                ir::FunctionParam* indices_param = nullptr;
                if (uint32_t n = shape->NumIndexAccesses(); n > 0) {
                    // Indices are passed as an array of u32
                    indices_param = b.FunctionParam(ty.array(ty.u32(), n));
                    new_params.Push(indices_param);
                }

                // Build an access chain to the pointer
                uint32_t index_index = 0;
                auto chain = Transform(shape->ops, [&](const AccessOp& op) -> Value* {
                    if (auto* m = std::get_if<MemberAccess>(&op)) {
                        return b.Constant(u32(m->member->Index()));
                    }
                    auto* access = b.Access(ty.u32(), indices_param, u32(index_index++));
                    return access->Result();
                });

                if (auto param_name = ir.NameOf(old_param); param_name.IsValid()) {
                    // Propagate old parameter name to the new parameters
                    if (root_ptr_param) {
                        ir.SetName(root_ptr_param, param_name.Name() + "_root");
                    }
                    if (indices_param) {
                        ir.SetName(indices_param, param_name.Name() + "_indices");
                    }
                }

                auto* access = b.Access(old_param->Type(), root_ptr, std::move(chain));
                old_param->ReplaceAllUsesWith(access->Result());
                old_param->Destroy();
            }

            // Replace the function's parameters
            variant->fn->SetParams(std::move(new_params));
        });
    }

    bool ParamNeedsTransforming(FunctionParam* param) const {
        if (auto* ptr = param->Type()->As<type::Pointer>()) {
            return PtrParamInfoFor(ptr->AddressSpace()).transform;
        }
        return false;
    }

    void DeleteDeadInstructions(ir::Value* value) {
        while (value && value->Usages().Count() == 0) {
            auto* inst_res = value->As<InstructionResult>();
            if (!inst_res) {
                return;
            }
            value = tint::Switch(
                inst_res->Source(),  //
                [&](Access* access) {
                    TINT_DEFER(access->Destroy());
                    return access->Object();
                },
                [&](Let* let) {
                    TINT_DEFER(let->Destroy());
                    return let->Value();
                });
        }
    }

    /// @returns a name describing the given shape
    std::string AccessShapeName(const AccessShape& shape) {
        StringStream ss;

        if (auto* global = std::get_if<RootModuleScopeVar>(&shape.root)) {
            ss << ir.NameOf(global->var).NameView();
        } else {
            ss << "P";
        }

        for (auto& op : shape.ops) {
            ss << "_";

            if (std::holds_alternative<IndexAccess>(op)) {
                /// The op uses an index taken from an array parameter.
                ss << "X";
                continue;
            }

            if (auto* access = std::get_if<MemberAccess>(&op); TINT_LIKELY(access)) {
                ss << access->member->Name().NameView();
                continue;
            }

            TINT_ICE() << "unhandled variant for access chain";
            break;
        }
        return ss.str();
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

Result<SuccessType, diag::List> DirectVariableAccess(Module* ir,
                                                     const DirectVariableAccessOptions& options) {
    auto result = ValidateAndDumpIfNeeded(*ir, "DirectVariableAccess transform");
    if (!result) {
        return result;
    }

    State{*ir, options}.Process();

    return Success;
}

}  // namespace tint::core::ir::transform
