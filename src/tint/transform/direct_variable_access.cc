// Copyright 2022 The Tint Authors.
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

#include "src/tint/transform/direct_variable_access.h"

#include "src/tint/program_builder.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/index_accessor_expression.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/module.h"
#include "src/tint/sem/struct.h"
#include "src/tint/sem/variable.h"
#include "src/tint/utils/reverse.h"
#include "src/tint/utils/scoped_assignment.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::DirectVariableAccess);

using namespace tint::number_suffixes;  // NOLINT

namespace {

/// DynamicIndex is used by DirectVariableAccess::State::AccessIndex to indicate a
/// runtime-expression index
struct DynamicIndex {
    size_t slot;  // The index of the expression in
                  // DirectVariableAccess::State::AccessChain::dynamic_indices
};

/// Inequality operator for DynamicIndex
bool operator!=(const DynamicIndex& a, const DynamicIndex& b) {
    return a.slot != b.slot;
}

}  // namespace

namespace tint::utils {

/// Hasher specialization for DynamicIndex
template <>
struct Hasher<DynamicIndex> {
    /// The hash function for the DynamicIndex
    /// @param d the DynamicIndex to hash
    /// @return the hash for the given DynamicIndex
    size_t operator()(const DynamicIndex& d) const { return utils::Hash(d.slot); }
};

}  // namespace tint::utils

namespace tint::transform {

/// The PIMPL state for the DirectVariableAccess transform
struct DirectVariableAccess::State {
    /// Constructor
    /// @param c the CloneContext
    explicit State(CloneContext& c) : ctx(c) {}

    /// Runs the transform
    void Run() {
        for (auto* decl : utils::Reverse(sem.Module()->DependencyOrderedDeclarations())) {
            if (auto* fn = sem.Get<sem::Function>(decl)) {
                ProcessFunction(fn);
            }
        }

        ctx.ReplaceAll([&](const ast::Function* ast_fn) -> const ast::Function* {
            auto* fn = sem.Get(ast_fn);
            auto* fn_info = fns.Find(fn);
            if (!fn_info) {
                return nullptr;  // Just clone.
            }
            TINT_SCOPED_ASSIGNMENT(current_function, *fn_info);

            const ast::Function* pending_func = nullptr;
            for (auto variant_it : (*fn_info)->variants) {
                if (pending_func) {
                    b.AST().AddFunction(pending_func);
                }
                auto& variant_variant_key = variant_it.key;
                auto& variant = variant_it.value;

                TINT_SCOPED_ASSIGNMENT(current_variant_key, &variant_variant_key);
                TINT_SCOPED_ASSIGNMENT(current_variant, &variant);

                // Build the variant's parameters
                utils::Vector<const ast::Parameter*, 8> params;
                for (auto* param : fn->Parameters()) {
                    if (auto* ptr_access = variant_variant_key.Find(param)) {
                        if (auto* type = DynamicIndexAlias(*ptr_access)) {
                            auto param_name = ctx.Clone(param->Declaration()->symbol);
                            params.Push(b.Param(param_name, type));
                        }
                    } else {
                        params.Push(ctx.Clone(param->Declaration()));
                    }
                }

                // Build the function
                auto* ret_ty = ctx.Clone(fn->Declaration()->return_type);
                auto body = ctx.Clone(fn->Declaration()->body);
                auto attrs = ctx.Clone(fn->Declaration()->attributes);
                auto ret_attrs = ctx.Clone(fn->Declaration()->return_type_attributes);
                pending_func =
                    b.create<ast::Function>(variant.name, std::move(params), ret_ty, body,
                                            std::move(attrs), std::move(ret_attrs));
            }
            return pending_func;
        });

        ctx.Clone();
    }

    /// @returns true if this transform should be run for the given program
    /// @param program the program to inspect
    static bool ShouldRun(const Program* program) {
        (void)program;
        return true;
    }

  private:
    /// AccessIndex describes a single access in an access chain.
    /// The access is one of:
    /// const sem::Variable* - the root variable.
    /// u32                  - a static index on a struct, array index, or matrix column.
    /// DynamicIndex         - a runtime index on an array, matrix column, or vector element.
    using AccessIndex = std::variant<const sem::Variable*, u32, DynamicIndex>;

    /// A vector of AccessIndex.
    using AccessIndices = utils::Vector<AccessIndex, 8>;

    /// AccessChain describes a chain of access expressions to variable.
    struct AccessChain {
        /// The chain of access indices, starting with the first access on #var.
        AccessIndices indices;
        /// The runtime-evaluated expressions. This vector is indexed by the DynamicIndex::slot.
        utils::Vector<const sem::Expression*, 8> dynamic_indices;
    };

    using FnVariantKey = utils::Hashmap<const sem::Parameter*, AccessIndices, 4>;

    using CallAccessChains = utils::Hashmap<const sem::Expression*, AccessChain, 4>;

    struct FnVariant {
        Symbol name;
        utils::Hashmap<const sem::Call*, Symbol, 4> calls;
    };

    struct FnInfo {
        utils::Hashmap<FnVariantKey, FnVariant, 4> variants;
        CallAccessChains call_access_chains;
    };

    /// The clone context
    CloneContext& ctx;
    /// Alias to the semantic info in ctx.src
    const sem::Info& sem = ctx.src->Sem();
    /// Alias to the symbols in ctx.src
    const SymbolTable& sym = ctx.src->Symbols();
    /// Alias to the ctx.dst program builder
    ProgramBuilder& b = *ctx.dst;

    utils::Hashmap<const sem::Function*, FnInfo*, 8> fns;

    utils::BlockAllocator<FnInfo> fn_info_allocator;

    utils::Hashmap<AccessIndices, Symbol, 8> dynamic_index_aliases;

    const FnInfo* current_function = nullptr;
    const FnVariantKey* current_variant_key = nullptr;
    const FnVariant* current_variant = nullptr;

    /// @returns true
    static bool NeedsTransforming(const sem::Expression* expr) {
        return expr->Type()->UnwrapRef()->Is<sem::Pointer>();
    }

    /// @param fn the function to scan for calls
    void ProcessFunction(const sem::Function* fn) {
        auto* fn_info = FnInfoFor(fn);

        if (fn_info->variants.IsEmpty()) {
            // Function has no variants pre-generated by callers.
            // Create a single variant.
            FnVariant variant;
            variant.name = ctx.Clone(fn->Declaration()->symbol);
            fn_info->variants.Add(FnVariantKey{}, std::move(variant));
        }

        for (auto* call : fn->DirectCalls()) {
            auto* target = call->Target()->As<sem::Function>();
            if (!target) {
                continue;
            }

            // For each argument, check whether the argument is a pointer that needs transforming.
            for (auto arg : call->Arguments()) {
                if (NeedsTransforming(arg)) {
                    fn_info->call_access_chains.Add(arg, AccessChainFor(arg));
                }
            }

            if (fn_info->call_access_chains.IsEmpty()) {
                // Nothing needs changing here.
                continue;
            }

            // Build call target variants
            auto build_target_variant = [&](const FnVariantKey& caller_variant_key,
                                            FnVariant& caller_variant) {
                FnVariantKey target_variant_key;

                for (size_t i = 0; i < call->Arguments().Length(); i++) {
                    const auto* arg = call->Arguments()[i];
                    const auto* param = target->Parameters()[i];
                    const auto& chain = *fn_info->call_access_chains.Find(arg);
                    auto indices = AbsoluteAccessIndices(caller_variant_key, chain);
                    target_variant_key.Add(param, indices);
                }

                auto* target_info = FnInfoFor(target);
                auto& target_variant = target_info->variants.GetOrCreate(target_variant_key, [&] {
                    // TODO: Create better variant name
                    auto name = ctx.src->Symbols().NameFor(target->Declaration()->symbol);
                    return FnVariant{b.Symbols().New(name), {}};
                });
                caller_variant.calls.Add(call, target_variant.name);
            };

            // Caller function has variants.
            // Build the target variant for each variant of the caller.
            for (auto caller_variant_it : fn_info->variants) {
                build_target_variant(caller_variant_it.key, caller_variant_it.value);
            }

            // This call will need to be transformed to call the appropriate variant.
            ctx.Replace(call->Declaration(), [this, call]() { return TransformCall(call); });
        }
    }

    const sem::Parameter* RootParameter(const AccessChain& chain) {
        return std::get<const sem::Variable*>(chain.indices.Front())->As<sem::Parameter>();
    }

    AccessIndices AbsoluteAccessIndices(const FnVariantKey& variant_key, const AccessChain& chain) {
        auto indices = chain.indices;

        auto* root_param = RootParameter(chain);
        if (!root_param) {
            return chain.indices;
        }

        // Access chain starts from a parameter, which will be passed as dynamic indices.
        // Concatenate the parameter indices and the chain's indices.
        indices = *variant_key.Find(root_param);
        for (size_t i = 1; i < chain.indices.Length(); i++) {
            indices.Push(chain.indices[i]);
        }
        return indices;
    }

    const ast::CallExpression* TransformCall(const sem::Call* call) {
        utils::Vector<const ast::Expression*, 8> new_args;
        for (auto* arg : call->Arguments()) {
            auto* chain = current_function->call_access_chains.Find(arg);
            if (!chain) {
                // No access chain means the argument is not a pointer that needs transforming.
                new_args.Push(ctx.Clone(arg->Declaration()));
                continue;
            }
            auto full_indices = AbsoluteAccessIndices(*current_variant_key, *chain);
            if (CountDynamicIndices(full_indices) == 0) {
                // Arguments pointers to entirely static data (no dynamic indices) are omitted.
                continue;
            }
            if (auto* arg_ty = DynamicIndexAlias(full_indices)) {
                utils::Vector<const ast::Expression*, 8> dyn_idx_args;
                if (auto* root_param = RootParameter(*chain)) {
                    // TODO: Handle shadowing!
                    auto root_param_name = ctx.Clone(root_param->Declaration()->symbol);
                    auto& arg_indices = *current_variant_key->Find(root_param);
                    auto num_param_indices = CountDynamicIndices(arg_indices);
                    for (uint32_t i = 0; i < num_param_indices; i++) {
                        dyn_idx_args.Push(b.IndexAccessor(root_param_name, u32(i)));
                    }
                }
                for (auto* dyn_idx : chain->dynamic_indices) {
                    dyn_idx_args.Push(ctx.Clone(dyn_idx->Declaration()));
                }
                new_args.Push(b.Construct(arg_ty, std::move(dyn_idx_args)));
            }
        }
        auto target_variant = current_variant->calls.Find(call);
        return b.Call(*target_variant, std::move(new_args));
    }

    FnInfo* FnInfoFor(const sem::Function* fn) {
        return fns.GetOrCreate(fn, [this] { return fn_info_allocator.Create(); });
    }

    /// Walks the @p expr, constructing and returning an AccessChain.
    /// @returns an AccessChain.
    AccessChain AccessChainFor(const sem::Expression* expr) const {
        AccessChain access;

        // Walk from the outer-most expression, inwards towards the source variable.
        while (true) {
            enum class Action { kStop, kContinue, kError };
            Action action = Switch(
                expr,  //
                [&](const sem::VariableUser* user) {
                    if (user->Variable()->Type()->Is<sem::Pointer>()) {
                        // Found a pointer. This must be a pointer-let.
                        // Continue traversing from the let initializer.
                        expr = user->Variable()->Constructor();
                        return Action::kContinue;
                    }
                    // Walked all the way to the source variable. We're done traversing.
                    access.indices.Push(user->Variable());
                    return Action::kStop;
                },
                [&](const sem::StructMemberAccess* a) {
                    // Structure member accesses are always statically indexed
                    access.indices.Push(u32(a->Member()->Index()));
                    expr = a->Object();
                    return Action::kContinue;
                },
                [&](const sem::IndexAccessorExpression* a) {
                    // Array, matrix or vector index.
                    if (auto* val = a->Index()->ConstantValue()) {
                        access.indices.Push(val->As<u32>());
                    } else {
                        access.indices.Push(DynamicIndex{access.dynamic_indices.Length()});
                        access.dynamic_indices.Push(a->Index());
                    }
                    expr = a->Object();
                    return Action::kContinue;
                },
                [&](const sem::Expression* e) {
                    // Walk past indirection and address-of unary ops.
                    return Switch(e->Declaration(),  //
                                  [&](const ast::UnaryOpExpression* u) {
                                      switch (u->op) {
                                          case ast::UnaryOp::kAddressOf:
                                          case ast::UnaryOp::kIndirection:
                                              expr = sem.Get(u->expr);
                                              return Action::kContinue;
                                          default:
                                              TINT_ICE(Transform, b.Diagnostics())
                                                  << "unhandled unary op for access chain: "
                                                  << u->op;
                                              return Action::kError;
                                      }
                                  });
                },
                [&](Default) {
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unhandled expression type for access chain\n"
                        << "AST: " << expr->Declaration()->TypeInfo().name << "\n"
                        << "SEM: " << expr->TypeInfo().name;
                    return Action::kError;
                });

            switch (action) {
                case Action::kContinue:
                    continue;
                case Action::kStop:
                    break;
                case Action::kError:
                    return {};
            }

            break;
        }

        // As the access walked from RHS to LHS, the last index operation applies to the source
        // variable. We want this the other way around, so reverse the arrays and fix indicies.
        std::reverse(access.indices.begin(), access.indices.end());
        std::reverse(access.dynamic_indices.begin(), access.dynamic_indices.end());
        for (auto& index : access.indices) {
            if (auto* dyn_idx = std::get_if<DynamicIndex>(&index)) {
                dyn_idx->slot = access.dynamic_indices.Length() - dyn_idx->slot - 1;
            }
        }

        return access;
    }

    static uint32_t CountDynamicIndices(const AccessIndices& indices) {
        uint32_t count = 0;
        for (auto& idx : indices) {
            if (std::holds_alternative<DynamicIndex>(idx)) {
                count++;
            }
        }
        return count;
    }

    const ast::TypeName* DynamicIndexAlias(const AccessIndices& full_indices) {
        auto name = dynamic_index_aliases.GetOrCreate(full_indices, [&] {
            // Count the number of dynamic indices
            uint32_t num_dyn_indices = CountDynamicIndices(full_indices);
            if (num_dyn_indices == 0) {
                return Symbol{};
            }
            auto symbol = b.Symbols().New(AccessIndicesName(full_indices));
            b.Alias(symbol, b.ty.array<u32>(num_dyn_indices));
            return symbol;
        });

        return name.IsValid() ? b.ty.type_name(name) : nullptr;
    }

    std::string AccessIndicesName(const AccessIndices& indices) {
        std::stringstream ss;
        const sem::Type* ty = nullptr;
        for (auto& access : indices) {
            if (auto* const* var = std::get_if<const sem::Variable*>(&access)) {
                ss << ctx.src->Symbols().NameFor((*var)->Declaration()->symbol);
                ty = (*var)->Type()->UnwrapRef();
                continue;
            }
            if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
                /// The access uses a dynamic (runtime-expression) index.
                ss << "p" + std::to_string(dyn_idx->slot);
                ty = Switch(
                    ty,  //
                    [&](const sem::Array* arr) { return arr->ElemType(); },
                    [&](const sem::Matrix* mat) { return mat->ColumnType(); },
                    [&](const sem::Vector* vec) { return vec->type(); },
                    [&](Default) {
                        TINT_ICE(Transform, b.Diagnostics())
                            << "unhandled type for access chain: " << ctx.src->FriendlyName(ty);
                        return ty;
                    });
                continue;
            }
            /// The access is a static index.
            auto idx = std::get<u32>(access);
            ty = Switch(
                ty,  //
                [&](const sem::Struct* str) {
                    auto* member = str->Members()[idx];
                    ss << sym.NameFor(member->Name());
                    return member->Type();
                },  //
                [&](const sem::Array* arr) {
                    ss << std::to_string(idx);
                    return arr->ElemType();
                },  //
                [&](const sem::Matrix* mat) {
                    ss << std::to_string(idx);
                    return mat->ColumnType();
                },  //
                [&](const sem::Vector* vec) {
                    ss << std::to_string(idx);
                    return vec->type();
                },  //
                [&](Default) {
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unhandled type for access chain: " << ctx.src->FriendlyName(ty);
                    return ty;
                });
        }
        return ss.str();
    }

    /// Return type of BuildAccessExpr()
    struct ExprTypeName {
        /// The new, post-access expression
        const ast::Expression* expr;
        /// The type of #expr
        const sem::Type* type;
        /// A name segment which can be used to build sensible names for helper functions
        std::string name;
    };

    /// Builds a single access in an access chain.
    /// @param lhs the expression to index using @p access
    /// @param ty the type of the expression @p lhs
    /// @param chain the access index to perform on @p lhs
    /// @param dynamic_index a function that obtains the i'th dynamic index
    /// @returns a ExprTypeName which holds the new expression, new type and a name segment which
    ///          can be used for creating helper function names.
    ExprTypeName BuildAccessExpr(const ast::Expression* lhs,
                                 const sem::Type* ty,
                                 const AccessChain& chain,
                                 size_t index,
                                 std::function<const ast::Expression*(size_t)> dynamic_index) {
        auto& access = chain.indices[index];

        if (auto* const* var = std::get_if<const sem::Variable*>(&access)) {
            auto* decl = (*var)->Declaration();
            const auto* expr = b.Expr(ctx.Clone(decl->symbol));
            const auto name = ctx.src->Symbols().NameFor(decl->symbol);
            ty = (*var)->Type()->UnwrapRef();
            return {expr, ty, name};
        }

        if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
            /// The access uses a dynamic (runtime-expression) index.
            auto name = "p" + std::to_string(dyn_idx->slot);
            return Switch(
                ty,  //
                [&](const sem::Array* arr) -> ExprTypeName {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    auto* expr = b.IndexAccessor(lhs, idx);
                    return {expr, arr->ElemType(), name};
                },  //
                [&](const sem::Matrix* mat) -> ExprTypeName {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    auto* expr = b.IndexAccessor(lhs, idx);
                    return {expr, mat->ColumnType(), name};
                },  //
                [&](const sem::Vector* vec) -> ExprTypeName {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    auto* expr = b.IndexAccessor(lhs, idx);
                    return {expr, vec->type(), name};
                },  //
                [&](Default) -> ExprTypeName {
                    TINT_ICE(Transform, b.Diagnostics())
                        << "unhandled type for access chain: " << ctx.src->FriendlyName(ty);
                    return {};
                });
        }
        /// The access is a static index.
        auto idx = std::get<u32>(access);
        return Switch(
            ty,  //
            [&](const sem::Struct* str) -> ExprTypeName {
                auto* member = str->Members()[idx];
                auto member_name = sym.NameFor(member->Name());
                auto* expr = b.MemberAccessor(lhs, member_name);
                ty = member->Type();
                return {expr, ty, member_name};
            },  //
            [&](const sem::Array* arr) -> ExprTypeName {
                auto* expr = b.IndexAccessor(lhs, idx);
                return {expr, arr->ElemType(), std::to_string(idx)};
            },  //
            [&](const sem::Matrix* mat) -> ExprTypeName {
                auto* expr = b.IndexAccessor(lhs, idx);
                return {expr, mat->ColumnType(), std::to_string(idx)};
            },  //
            [&](const sem::Vector* vec) -> ExprTypeName {
                auto* expr = b.IndexAccessor(lhs, idx);
                return {expr, vec->type(), std::to_string(idx)};
            },  //
            [&](Default) -> ExprTypeName {
                TINT_ICE(Transform, b.Diagnostics())
                    << "unhandled type for access chain: " << ctx.src->FriendlyName(ty);
                return {};
            });
    }
};

DirectVariableAccess::DirectVariableAccess() = default;

DirectVariableAccess::~DirectVariableAccess() = default;

bool DirectVariableAccess::ShouldRun(const Program* program, const DataMap&) const {
    return State::ShouldRun(program);
}

void DirectVariableAccess::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    State(ctx).Run();
}

}  // namespace tint::transform
