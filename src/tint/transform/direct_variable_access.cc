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

#include <algorithm>
#include <string>
#include <utility>

#include "src/tint/ast/traverse_expressions.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/abstract_int.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/index_accessor_expression.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/module.h"
#include "src/tint/sem/statement.h"
#include "src/tint/sem/struct.h"
#include "src/tint/sem/variable.h"
#include "src/tint/transform/utils/hoist_to_decl_before.h"
#include "src/tint/utils/bitset.h"
#include "src/tint/utils/reverse.h"
#include "src/tint/utils/scoped_assignment.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::DirectVariableAccess);

using namespace tint::number_suffixes;  // NOLINT

namespace {

/// DynamicIndex is used by DirectVariableAccess::State::AccessIndex to indicate a
/// runtime-expression index
struct DynamicIndex {
    /// The index of the expression in DirectVariableAccess::State::AccessChain::dynamic_indices
    size_t slot;
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
        // Build all the access chains
        for (auto* node : ctx.src->ASTNodes().Objects()) {
            if (auto* expr = sem.Get<sem::Expression>(node)) {
                AppendAccessChainFor(expr);
            }
        }

        for (auto* decl : utils::Reverse(sem.Module()->DependencyOrderedDeclarations())) {
            if (auto* fn = sem.Get<sem::Function>(decl)) {
                auto* fn_info = FnInfoFor(fn);
                ProcessFunction(fn, fn_info);
            }
        }

        // Ensure that variables in the access chains are not shadowed.
        for (auto chain_it : access_chains) {
            auto* expr = chain_it.key;
            auto* fn_info = FnInfoFor(expr->Stmt()->Function());
            for (auto variant_it : fn_info->variants) {
                for (auto param_it : variant_it.key) {
                    Unshadow(param_it.value, expr->Stmt());
                }
            }
        }

        TransformAccessChains();

        {
            CloneState state;
            clone_state = &state;
            ctx.Clone();
        }
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
    /// Symbol               - a struct member access.
    /// DynamicIndex         - a runtime index on an array, matrix column, or vector element.
    using AccessIndex = std::variant<const sem::Variable*, Symbol, DynamicIndex>;

    /// A vector of AccessIndex.
    using AccessIndices = utils::Vector<AccessIndex, 8>;

    using ExprBuilder = std::function<const ast::Expression*()>;

    /// AccessChain describes a chain of access expressions to variable.
    struct AccessChain {
        /// The chain of access indices, starting with the first access on #var.
        AccessIndices indices;
        /// The runtime-evaluated expressions. This vector is indexed by the DynamicIndex::slot.
        utils::Vector<ExprBuilder, 8> dynamic_indices;
    };

    using FnVariantPtrParams = utils::Hashmap<const sem::Parameter*, AccessIndices, 4>;

    struct FnVariant {
        Symbol name;
        utils::Hashmap<const sem::Call*, Symbol, 4> calls;
        size_t order = 0;
    };

    struct FnInfo {
        utils::Hashmap<FnVariantPtrParams, FnVariant, 8> variants;
        utils::Hashmap<const sem::Expression*, Symbol, 8> hoisted_exprs;
        utils::Hashmap<const sem::Variable*, Symbol, 8> unshadowed_vars;

        utils::Vector<std::pair<const FnVariantPtrParams*, FnVariant*>, 8> SortedVariants() {
            utils::Vector<std::pair<const FnVariantPtrParams*, FnVariant*>, 8> out;
            out.Reserve(variants.Count());
            for (auto it : variants) {
                out.Push({&it.key, &it.value});
            }
            std::sort(out.begin(), out.end(),
                      [&](auto& va, auto& vb) { return va.second->order < vb.second->order; });
            return out;
        }
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

    utils::BlockAllocator<AccessChain> access_chain_allocator;
    utils::Hashmap<const sem::Expression*, AccessChain*, 32> access_chains;

    HoistToDeclBefore hoist{ctx};

    /// CloneState holds pointers to the current function, variant and variant's parameters.
    struct CloneState {
        FnInfo* current_function = nullptr;
        FnVariant* current_variant = nullptr;
        const FnVariantPtrParams* current_variant_params = nullptr;
    };
    /// Only valid during cloning
    CloneState* clone_state = nullptr;

    /// @param fn the function to scan for calls
    void ProcessFunction(const sem::Function* fn, FnInfo* fn_info) {
        if (fn_info->variants.IsEmpty()) {
            // Function has no variants pre-generated by callers.
            if (NeedsVariantToGenerate(fn)) {
                // Drop the function, as it wasn't called and cannot be generated.
                ctx.Remove(ctx.src->AST().GlobalDeclarations(), fn->Declaration());
                return;
            }

            // Create a single variant.
            FnVariant variant;
            variant.name = ctx.Clone(fn->Declaration()->symbol);
            fn_info->variants.Add(FnVariantPtrParams{}, std::move(variant));
        }

        for (auto* call : fn->DirectCalls()) {
            ProcessCall(fn_info, call);
        }

        TransformFunction(fn, fn_info);
    }

    void TransformFunction(const sem::Function* fn, FnInfo* fn_info) {
        ctx.Replace(fn->Declaration(), [this, fn, fn_info] {
            TINT_SCOPED_ASSIGNMENT(clone_state->current_function, fn_info);

            const ast::Function* pending_func = nullptr;
            for (auto variant_it : fn_info->SortedVariants()) {
                if (pending_func) {
                    b.AST().AddFunction(pending_func);
                }
                auto& variant_params = *variant_it.first;
                auto& variant = *variant_it.second;

                TINT_SCOPED_ASSIGNMENT(clone_state->current_variant_params, &variant_params);
                TINT_SCOPED_ASSIGNMENT(clone_state->current_variant, &variant);

                // Build the variant's parameters
                utils::Vector<const ast::Parameter*, 8> params;
                for (auto* param : fn->Parameters()) {
                    if (auto* ptr_access = variant_params.Find(param)) {
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
    }

    void TransformAccessChains() {
        ctx.ReplaceAll([this](const ast::Expression* ast_expr) -> const ast::Expression* {
            if (!clone_state->current_variant) {
                return ctx.CloneWithoutTransform(ast_expr);
            }

            auto* expr = sem.Get<sem::Expression>(ast_expr);
            if (!expr) {
                return ctx.CloneWithoutTransform(ast_expr);
            }

            auto* chain = AccessChainFor(expr);
            if (!chain) {
                return ctx.CloneWithoutTransform(ast_expr);
            }

            if (clone_real_expression_not_hoist != expr) {
                if (auto* hoisted = clone_state->current_function->hoisted_exprs.Find(expr)) {
                    return b.Expr(*hoisted);
                }
            }

            // Reconstruct the chain using a global variable and the function variant's
            // incoming dynamic indices.

            // Callback for replacing the expression.
            // This will be called once for each variant of the function.
            const ast::Expression* chain_expr = nullptr;

            size_t start_idx = 0;
            if (auto* root_param = RootParameter(*chain)) {
                // Chain starts with a pointer parameter.
                // Replace this with the variant's incoming chain.
                Symbol param_name;
                if (auto let = clone_state->current_function->unshadowed_vars.Find(root_param)) {
                    param_name = *let;
                } else {
                    param_name = ctx.Clone(root_param->Declaration()->symbol);
                }

                for (auto param_access : *clone_state->current_variant_params->Find(root_param)) {
                    chain_expr = BuildAccessExpr(chain_expr, param_access, [&](size_t i) {
                        return b.IndexAccessor(param_name, AInt(i));
                    });
                }
                start_idx = 1;
            }

            // For each access in the chain...
            for (size_t access_idx = start_idx; access_idx < chain->indices.Length();
                 access_idx++) {
                auto& access = chain->indices[access_idx];
                chain_expr = BuildAccessExpr(chain_expr, access,
                                             [&](size_t i) { return chain->dynamic_indices[i](); });
            }

            // End result of the access chain is always a non-pointer. If the expression we're
            // replacing is a pointer, take the address.
            if (expr->Type()->Is<sem::Pointer>()) {
                chain_expr = b.AddressOf(chain_expr);
            }

            return chain_expr;
        });
    }

    /// @returns true if the function needs at least one caller in order to be generated.
    static bool NeedsVariantToGenerate(const sem::Function* fn) {
        for (auto* param : fn->Parameters()) {
            if (IsPointerToUniformStorageWorkgroup(param->Type())) {
                return true;
            }
        }
        return false;
    }

    /// @returns true
    static bool IsPointerToUniformStorageWorkgroup(const sem::Type* ty) {
        if (auto* ptr = ty->As<sem::Pointer>()) {
            if (IsUniformStorageWorkgroup(ptr->StorageClass())) {
                return true;
            }
        }
        return false;
    }

    /// @returns true if the StorageClass is `uniform`, `storage` or `workgroup`.
    static bool IsUniformStorageWorkgroup(ast::StorageClass sc) {
        switch (sc) {
            case ast::StorageClass::kUniform:
            case ast::StorageClass::kStorage:
            case ast::StorageClass::kWorkgroup:
                return true;
            default:
                return false;
        }
    }

    void ProcessCall(FnInfo* caller_info, const sem::Call* call) {
        auto* target = call->Target()->As<sem::Function>();
        if (!target) {
            // Call target is not a user-declared function.
            // Not interested in this.
            return;
        }

        if (!NeedsVariantToGenerate(target)) {
            return;
        }

        // Build call target variants
        auto build_target_variant = [&](const FnVariantPtrParams& caller_variant_params,
                                        FnVariant& caller_variant) {
            FnVariantPtrParams target_variant_params;

            for (size_t i = 0; i < call->Arguments().Length(); i++) {
                const auto* arg = call->Arguments()[i];
                const auto* param = target->Parameters()[i];
                if (IsPointerToUniformStorageWorkgroup(param->Type())) {
                    auto& chain = *AccessChainFor(arg);
                    auto indices = AbsoluteAccessIndices(caller_variant_params, chain);
                    target_variant_params.Add(param, indices);
                }
            }

            auto* target_info = FnInfoFor(target);
            auto& target_variant = target_info->variants.GetOrCreate(target_variant_params, [&] {
                // Build a function variant name.
                // This is derived from the original function name, appended with the pointer
                // parameter chains.
                auto name = ctx.src->Symbols().NameFor(target->Declaration()->symbol);
                for (auto* param : target->Parameters()) {
                    if (auto* indices = target_variant_params.Find(param)) {
                        name += "_" + AccessIndicesName(*indices);
                    }
                }
                FnVariant variant;
                variant.name = b.Symbols().New(name);
                variant.order = target_info->variants.Count();
                return variant;
            });
            caller_variant.calls.Add(call, target_variant.name);
        };

        // Caller function has variants.
        // Build the target variant for each variant of the caller.
        for (auto caller_variant_it : caller_info->SortedVariants()) {
            build_target_variant(*caller_variant_it.first, *caller_variant_it.second);
        }

        // This call will need to be transformed to call the appropriate variant.
        TransformCall(call);
    }

    const sem::Parameter* RootParameter(const AccessChain& chain) {
        return std::get<const sem::Variable*>(chain.indices.Front())->As<sem::Parameter>();
    }

    AccessIndices AbsoluteAccessIndices(const FnVariantPtrParams& variant_key,
                                        const AccessChain& chain) {
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

    void TransformCall(const sem::Call* call) {
        ctx.Replace(call->Declaration(), [this, call]() {
            utils::Vector<const ast::Expression*, 8> new_args;
            for (auto* arg : call->Arguments()) {
                auto* chain = AccessChainFor(arg);
                if (!chain) {
                    // No access chain means the argument is not a pointer that needs transforming.
                    new_args.Push(ctx.Clone(arg->Declaration()));
                    continue;
                }
                auto full_indices =
                    AbsoluteAccessIndices(*clone_state->current_variant_params, *chain);
                if (CountDynamicIndices(full_indices) == 0) {
                    // Arguments pointers to entirely static data (no dynamic indices) are omitted.
                    continue;
                }
                if (auto* arg_ty = DynamicIndexAlias(full_indices)) {
                    utils::Vector<const ast::Expression*, 8> dyn_idx_args;
                    if (auto* root_param = RootParameter(*chain)) {
                        Symbol root_param_name;
                        if (auto let =
                                clone_state->current_function->unshadowed_vars.Find(root_param)) {
                            root_param_name = *let;
                        } else {
                            root_param_name = ctx.Clone(root_param->Declaration()->symbol);
                        }
                        auto& arg_indices = *clone_state->current_variant_params->Find(root_param);
                        auto num_param_indices = CountDynamicIndices(arg_indices);
                        for (uint32_t i = 0; i < num_param_indices; i++) {
                            dyn_idx_args.Push(b.IndexAccessor(root_param_name, u32(i)));
                        }
                    }
                    for (auto& dyn_idx : chain->dynamic_indices) {
                        dyn_idx_args.Push(dyn_idx());
                    }
                    new_args.Push(b.Construct(arg_ty, std::move(dyn_idx_args)));
                }
            }
            auto target_variant = clone_state->current_variant->calls.Find(call);
            return b.Call(*target_variant, std::move(new_args));
        });
    }

    FnInfo* FnInfoFor(const sem::Function* fn) {
        return fns.GetOrCreate(fn, [this] { return fn_info_allocator.Create(); });
    }

    void Unshadow(const AccessIndices& indices, const sem::Statement* stmt) {
        for (auto& access : indices) {
            if (auto const* const* variable = std::get_if<const sem::Variable*>(&access)) {
                Unshadow(*variable, stmt);
            }
        }
    }

    /// If @p variable is shadowed by another variable from the scope of the statement @p stmt, then
    /// Unshadow() creates a `let` pointer at the start of the function to 'unshadow' the parameter
    /// or module-scope variable @p variable.
    /// @param variable the variable to unshadow. Must be global or parameter.
    /// @param stmt the statement used to check whether the un-shadowing is necessary.
    /// @note @p variable is only unshadowed once per function.
    void Unshadow(const sem::Variable* variable, const sem::Statement* stmt) {
        if (IsShadowed(variable, stmt)) {
            // Variable is shadowed in the body of the function.
            // Create a pointer alias so this can be safely accessed throughout the
            // function.
            auto* fn_info = FnInfoFor(stmt->Function());
            fn_info->unshadowed_vars.GetOrCreate(variable, [&] {
                auto variable_sym = variable->Declaration()->symbol;
                auto name = b.Symbols().New(sym.NameFor(variable_sym));
                const ast::Expression* init = b.Expr(ctx.Clone(variable_sym));
                if (!variable->Type()->Is<sem::Pointer>()) {
                    init = b.AddressOf(init);
                }
                auto* let = b.Let(name, init);
                ctx.InsertFront(stmt->Function()->Declaration()->body->statements, b.Decl(let));
                return name;
            });
        }
    }

    /// @returns true if the variable @p variable is shadowed from the scope of statement @p at.
    bool IsShadowed(const sem::Variable* variable, const sem::Statement* at) {
        auto symbol = variable->Declaration()->symbol;
        for (auto p = at->Parent(); p != nullptr; p = p->Parent()) {
            if (auto* decl = p->Decls().Find(symbol)) {
                return decl->variable != variable;
            }
        }
        return false;
    }

    AccessChain* AccessChainFor(const sem::Expression* expr) {
        if (auto* chain = access_chains.Find(expr)) {
            return *chain;
        }
        return nullptr;
    }

    const sem::Expression* clone_real_expression_not_hoist = nullptr;

    void AppendAccessChainFor(const sem::Expression* expr) {
        auto take_chain = [&](const sem::Expression* e) -> AccessChain* {
            if (auto* chain = AccessChainFor(e)) {
                access_chains.Remove(e);
                return chain;
            }
            return nullptr;
        };

        auto* new_chain = Switch(
            expr,

            [&](const sem::VariableUser* user) -> AccessChain* {
                auto* variable = user->Variable();
                auto* ty_as_ptr = variable->Type()->As<sem::Pointer>();
                if (ty_as_ptr && variable->Declaration()->Is<ast::Let>()) {
                    // Expression is a pointer-let.
                    if (auto* init = sem.Get(variable->Declaration()->constructor)) {
                        if (auto* init_chain = AccessChainFor(init)) {
                            // Start this chain from the initializer of the pointer let
                            auto* chain = access_chain_allocator.Create(*init_chain);
                            access_chains.Add(expr, chain);
                            return chain;
                        }
                    }
                } else {
                    // Global or parameter
                    if (IsUniformStorageWorkgroup(variable->StorageClass()) ||
                        (ty_as_ptr && IsUniformStorageWorkgroup(ty_as_ptr->StorageClass()))) {
                        auto* chain = access_chain_allocator.Create();
                        access_chains.Add(expr, chain);
                        chain->indices.Push(variable);
                        return chain;
                    }
                }

                return nullptr;
            },

            [&](const sem::StructMemberAccess* a) -> AccessChain* {
                if (auto* chain = take_chain(a->Object())) {
                    // Structure member accesses are always statically indexed
                    chain->indices.Push(a->Member()->Name());
                    return chain;
                }
                return nullptr;
            },

            [&](const sem::IndexAccessorExpression* a) -> AccessChain* {
                // Array, matrix or vector index.
                auto* chain = take_chain(a->Object());
                if (!chain) {
                    return nullptr;
                }

                auto* idx = a->Index();
                if (auto* val = idx->ConstantValue()) {
                    // Simple case: The index is a constant value.
                    chain->indices.Push(DynamicIndex{chain->dynamic_indices.Length()});
                    chain->dynamic_indices.Push([this, val] { return b.Expr(val->As<AInt>()); });
                    return chain;
                }

                // The index is not a constant value.
                // In order to ensure that side-effecting expressions are only evaluated once, that
                // expressions do not change between index and usage, and that variables are not
                // shadowed between the index expression and function calls, hoist the expression to
                // a let. As access chains can share expressions, we use the hoisted_exprs map to
                // ensure that we only hoist the expression once.
                bool needs_hoisting = true;
                // There's one exception to this rule: If the index expression is a `let` or
                // parameter and that variable is not shadowed between use as the index and `stmt`,
                // then we can omit the hoisting.
                if (auto* idx_variable_user = idx->UnwrapMaterialize()->As<sem::VariableUser>()) {
                    auto* idx_variable = idx_variable_user->Variable();
                    if (idx_variable->Declaration()->IsAnyOf<ast::Let, ast::Parameter>()) {
                        needs_hoisting = IsShadowed(idx_variable, expr->Stmt());
                    }
                }
                std::function<const ast::Expression*()> idx_expr;
                if (needs_hoisting) {
                    auto fn = FnInfoFor(expr->Stmt()->Function());
                    auto hoisted = fn->hoisted_exprs.GetOrCreate(idx, [&] {
                        auto name = b.Symbols().New();
                        hoist.InsertBefore(expr->Stmt(), [this, idx, name] {
                            TINT_SCOPED_ASSIGNMENT(clone_real_expression_not_hoist, idx);
                            return b.Decl(b.Let(name, ctx.Clone(idx->Declaration())));
                        });
                        return name;
                    });
                    idx_expr = [this, hoisted] { return b.Expr(hoisted); };
                } else {
                    idx_expr = [this, idx] { return ctx.Clone(idx->Declaration()); };
                }

                // The index may be fed to a dynamic index array<u32, N> argument, so the
                // index expression may need casting to u32.
                if (!idx->UnwrapMaterialize()
                         ->Type()
                         ->UnwrapRef()
                         ->IsAnyOf<sem::U32, sem::AbstractInt>()) {
                    idx_expr = [this, idx_expr] { return b.Construct(b.ty.u32(), idx_expr()); };
                }

                chain->indices.Push(DynamicIndex{chain->dynamic_indices.Length()});
                chain->dynamic_indices.Push(idx_expr);
                return chain;
            },

            [&](const sem::Expression* e) {
                // Walk past indirection and address-of unary ops.
                return Switch(e->Declaration(),  //
                              [&](const ast::UnaryOpExpression* u) -> AccessChain* {
                                  switch (u->op) {
                                      case ast::UnaryOp::kAddressOf:
                                      case ast::UnaryOp::kIndirection:
                                          return take_chain(sem.Get(u->expr));
                                      default:
                                          return nullptr;
                                  }
                              });
            });

        if (new_chain) {
            // Ensure the chain's variables are accessible from this statement
            Unshadow(new_chain->indices, expr->Stmt());

            access_chains.Add(expr, new_chain);
        }
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
            b.Alias(symbol, b.ty.array(b.ty.u32(), u32(num_dyn_indices)));
            return symbol;
        });

        return name.IsValid() ? b.ty.type_name(name) : nullptr;
    }

    std::string AccessIndicesName(const AccessIndices& indices) {
        std::stringstream ss;
        bool first = true;
        for (auto& access : indices) {
            if (!first) {
                ss << "_";
            }
            first = false;

            if (auto* const* var = std::get_if<const sem::Variable*>(&access)) {
                ss << ctx.src->Symbols().NameFor((*var)->Declaration()->symbol);
                continue;
            }

            if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
                /// The access uses a dynamic (runtime-expression) index.
                ss << "X";
                continue;
            }

            if (auto* member = std::get_if<Symbol>(&access)) {
                ss << sym.NameFor(*member);
                continue;
            }

            TINT_ICE(Transform, b.Diagnostics()) << "unhandled variant for access chain";
            break;
        }
        return ss.str();
    }

    /// Builds a single access in an access chain, returning the new expression.
    /// The returned expression will always be of a reference type.
    /// @param expr the input expression
    /// @param access the access to perform on the current expression
    /// @param dynamic_index a function that obtains the i'th dynamic index
    const ast::Expression* BuildAccessExpr(
        const ast::Expression* expr,
        const AccessIndex& access,
        std::function<const ast::Expression*(size_t)> dynamic_index) {
        if (auto* const* variable = std::get_if<const sem::Variable*>(&access)) {
            auto* decl = (*variable)->Declaration();
            if (auto let = clone_state->current_function->unshadowed_vars.Find(*variable)) {
                // Unshadowed lets are always pointers.
                return b.Deref(*let);
            }

            expr = b.Expr(ctx.Clone(decl->symbol));
            if (auto* ptr = (*variable)->Type()->As<sem::Pointer>()) {
                expr = b.Deref(expr);
            }
            return expr;
        }

        if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
            /// The access uses a dynamic (runtime-expression) index.
            auto* idx = dynamic_index(dyn_idx->slot);
            return b.IndexAccessor(expr, idx);
        }

        if (auto* member = std::get_if<Symbol>(&access)) {
            /// The access is a member access.
            return b.MemberAccessor(expr, ctx.Clone(*member));
        }

        TINT_ICE(Transform, b.Diagnostics()) << "unhandled variant type for access chain";
        return nullptr;
    }

#if 0
    void DebugPrint(const AccessIndices& indices) const {
        bool first = true;
        for (auto& access : indices) {
            if (!first) {
                std::cout << ".";
            }
            first = false;
            if (auto* const* var = std::get_if<const sem::Variable*>(&access)) {
                std::cout << sym.NameFor((*var)->Declaration()->symbol);
                continue;
            }
            if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
                std::cout << "dyn<" << dyn_idx->slot << ">";
                continue;
            }
            std::cout << std::get<u32>(access);
        }
        std::cout << std::endl;
    }
#endif
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
