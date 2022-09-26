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
#include "src/tint/utils/reverse.h"
#include "src/tint/utils/scoped_assignment.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::DirectVariableAccess);
TINT_INSTANTIATE_TYPEINFO(tint::transform::DirectVariableAccess::Config);

using namespace tint::number_suffixes;  // NOLINT

namespace {

/// FnPtrParam is used by DirectVariableAccess::State::AccessIndex to indicate a ptr<function, T>
/// parameter
struct FnPtrParam {
    /// The T type `ptr<function, T>` - note that this is the root object type, which may not be the
    /// same as the same as the input program's parameter type.
    const tint::sem::Type* root_type = nullptr;
    /// The parameter in the input program.
    const tint::sem::Parameter* param = nullptr;
};

/// Inequality operator for FnPtrParam
bool operator!=(const FnPtrParam& a, const FnPtrParam& b) {
    return a.root_type != b.root_type || a.param != b.param;
}

/// DynamicIndex is used by DirectVariableAccess::State::AccessIndex to indicate a
/// runtime-expression index
struct DynamicIndex {
    /// The index of the expression in DirectVariableAccess::State::AccessChain::dynamic_indices
    size_t slot = 0;
};

/// Inequality operator for DynamicIndex
bool operator!=(const DynamicIndex& a, const DynamicIndex& b) {
    return a.slot != b.slot;
}

}  // namespace

namespace tint::utils {

/// Hasher specialization for FnPtrParam
template <>
struct Hasher<FnPtrParam> {
    /// The hash function for the FnPtrParam
    /// @param d the FnPtrParam to hash
    /// @return the hash for the given FnPtrParam
    size_t operator()(const FnPtrParam& d) const { return utils::Hash(d.root_type, d.param); }
};

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
    /// @param context the CloneContext
    /// @param options the transform options
    State(CloneContext& context, const Options& options) : ctx(context), opts(options) {}

    /// The main function for the transform.
    void Run() {
        // Stage 1:
        // Walk all the expressions of the program, starting with the expression leaves.
        // Whenever we find an identifier resolving to a var, pointer parameter or pointer let to
        // another chain, start constructing an access chain. When chains are accessed, these chains
        // are grown and moved up the expression tree. After this stage, we are left with all the
        // expression access chains to variables that we may need to transform.
        for (auto* node : ctx.src->ASTNodes().Objects()) {
            if (auto* expr = sem.Get<sem::Expression>(node)) {
                AppendAccessChain(expr);
            }
        }

        // Stage 2:
        // Walk the functions in dependency order, starting with the entry points.
        // Construct the set of function 'variants' by examining the calls made by each function to
        // their call target. Each variant holds a map of pointer parameter to access chains, and
        // will have the pointer parameters replaced with an array of u32s, used to perform the
        // pointer indexing in the variant.
        // Function call pointer arguments are replaced with an array of these dynamic indices.
        for (auto* decl : utils::Reverse(sem.Module()->DependencyOrderedDeclarations())) {
            if (auto* fn = sem.Get<sem::Function>(decl)) {
                auto* fn_info = FnInfoFor(fn);
                ProcessFunction(fn, fn_info);
                TransformFunction(fn, fn_info);
            }
        }

        // Stage 3:
        // Filter out access chains that do not need transforming.
        // Ensure that identifiers used by the chains are 'unshadowed', and that chain dynamic index
        // expressions are evaluated once at the correct place
        {
            auto chain_exprs = access_chains.Keys();
            chain_exprs.Sort([](const auto& expr_a, const auto& expr_b) {
                return expr_a->Declaration()->node_id.value < expr_b->Declaration()->node_id.value;
            });

            for (auto* expr : chain_exprs) {
                auto* chain = *access_chains.Get(expr);
                if (!chain->used_in_call && !RootParameter(chain->indices)) {
                    // Chain was not used in a function call, and it doesn't originate from a
                    // pointer parameter. This chain does not need transforming. Drop it.
                    access_chains.Remove(expr);
                    continue;
                }

                // Chain is used.

                // We need to be careful that variables referenced by the chain's dynamic indices
                // are not shadowed between the index expression and the use of the chain, and that
                // the chain does not use expressions with side-effects which cannot be repeatedly
                // evaluated. To solve both these problems we can hoist the dynamic index
                // expressions to their own uniquely named lets (if required).
                MaybeHoistDynamicIndices(chain, expr->Stmt());

                auto* fn_info = FnInfoFor(expr->Stmt()->Function());

                // Ensure the chain's variables are accessible from this statement
                Unshadow(chain->indices, expr->Stmt());

                // Ensure the pointer parameters are accessible from this statement
                for (auto variant_it : fn_info->variants) {
                    for (auto param_it : variant_it.key) {
                        Unshadow(param_it.value, expr->Stmt());
                    }
                }
            }
        }

        // Stage 4:
        // Replace all the access chain expressions in all functions with reconstructed expression
        // using the originating global variable, and any dynamic indices passed in to the function
        // variant.
        TransformAccessChains();

        // Stage 5:
        // Actually kick the clone.
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
    /// const sem::Variable* - the root variable (global or parameter).
    /// FnPtrParam          - the root ptr<function, T> parameter.
    /// Symbol               - a struct member access.
    /// DynamicIndex         - a runtime index on an array, matrix column, or vector element.
    using AccessIndex = std::variant<const sem::Variable*, FnPtrParam, Symbol, DynamicIndex>;

    /// A vector of AccessIndex. Describes the static "path" from a root variable to an element
    /// within the variable. Array accessors index expressions are held externally to the
    /// AccessIndices, so AccessIndices will be considered equal even if the array, matrix or vector
    /// index values differ.
    ///
    /// For example, consider the following:
    ///
    /// ```
    ///           struct A {
    ///               x : array<i32, 8>,
    ///               y : u32,
    ///           };
    ///           struct B {
    ///               x : i32,
    ///               y : array<A, 4>
    ///           };
    ///           var<workgroup> C : B;
    /// ```
    ///
    /// The following AccessIndices would describe the following:
    ///
    /// +==============================+===============+=================================+
    /// | AccessIndices                | Type          |  Expression                     |
    /// +==============================+===============+=================================+
    /// | [ Variable 'C', Symbol 'x' ] | i32           |  C.x                            |
    /// +------------------------------+---------------+---------------------------------+
    /// | [ Variable 'C', Symbol 'y' ] | array<A, 4>   |  C.y                            |
    /// +------------------------------+---------------+---------------------------------+
    /// | [ Variable 'C', Symbol 'y',  | A             |  C.y[dyn_idx[0]]                |
    /// |   DynamicIndex ]             |               |                                 |
    /// +------------------------------+---------------+---------------------------------+
    /// | [ Variable 'C', Symbol 'y',  | array<i32, 8> |  C.y[dyn_idx[0]].x              |
    /// |   DynamicIndex, Symbol 'x' ] |               |                                 |
    /// +------------------------------+---------------+---------------------------------+
    /// | [ Variable 'C', Symbol 'y',  | i32           |  C.y[dyn_idx[0]].x[dyn_idx[1]]  |
    /// |   DynamicIndex, Symbol 'x',  |               |                                 |
    /// |   DynamicIndex ]             |               |                                 |
    /// +------------------------------+---------------+---------------------------------+
    /// | [ Variable 'C', Symbol 'y',  | u32           |  C.y[dyn_idx[0]].y              |
    /// |   DynamicIndex, Symbol 'y' ] |               |                                 |
    /// +------------------------------+---------------+---------------------------------+
    ///
    /// Where: `dyn_idx` is the AccessChain::dynamic_indices.
    using AccessIndices = utils::Vector<AccessIndex, 8>;

    /// AccessChain describes a chain of access expressions originating from a module-scope variable
    /// or parameter.
    struct AccessChain {
        /// The chain of access indices. The first index is always to a `const sem::Variable*`.
        AccessIndices indices;
        /// The array accessor index expressions. This vector is indexed by the `DynamicIndex`s in
        /// #indices.
        utils::Vector<const sem::Expression*, 8> dynamic_indices;
        /// If true, then this access chain is used as an argument to call a variant.
        bool used_in_call = false;
    };

    /// FnVariant describes a unique variant of a function, specialized by the AccessIndices of the
    /// pointer arguments - also known as the variant's "signature".
    ///
    /// To help understand what a variant is, consider the following WGSL:
    ///
    /// ```
    /// fn F(a : ptr<storage, u32>, b : u32, c : ptr<storage, u32>) {
    ///    return *a + b + *c;
    /// }
    ///
    /// @group(0) @binding(0) var<storage> S0 : u32;
    /// @group(0) @binding(0) var<storage> S1 : array<u32, 64>;
    ///
    /// fn x() {
    ///    F(&S0, 0, &S0);       // (A)
    ///    F(&S0, 0, &S0);       // (B)
    ///    F(&S1[0], 1, &S0);    // (C)
    ///    F(&S1[5], 2, &S0);    // (D)
    ///    F(&S1[5], 3, &S1[3]); // (E)
    ///    F(&S1[7], 4, &S1[2]); // (F)
    /// }
    /// ```
    ///
    /// Given the calls in x(), function F() will have 3 variants:
    /// (1) F<S0,S0>                   - called by (A) and (B).
    ///                                  Note that only 'uniform', 'storage' and 'workgroup' pointer
    ///                                  parameters are considered for a variant signature, and so
    ///                                  the argument for parameter 'b' is not included in the
    ///                                  signature.
    /// (2) F<S1[dyn_idx],S0>          - called by (C) and (D).
    ///                                  Note that the array index value is external to the
    ///                                  AccessIndices, and so is not part of the variant signature.
    /// (3) F<S1[dyn_idx],S1[dyn_idx]> - called by (E) and (F).
    ///
    /// Each variant of the function will be emitted as a separate function by the transform, and
    /// would look something like:
    ///
    /// ```
    /// // variant F<S0,S0> (1)
    /// fn F_S0_S0(b : u32) {
    ///    return S0 + b + S0;
    /// }
    ///
    /// type S1_X = array<u32, 1>;
    ///
    /// // variant F<S1[dyn_idx],S0> (2)
    /// fn F_S1_X_S0(a : S1_X, b : u32) {
    ///    return S1[a[0]] + b + S0;
    /// }
    ///
    /// // variant F<S1[dyn_idx],S1[dyn_idx]> (3)
    /// fn F_S1_X_S1_X(a : S1_X, b : u32, c : S1_X) {
    ///    return S1[a[0]] + b + S1[c[0]];
    /// }
    ///
    /// @group(0) @binding(0) var<storage> S0 : u32;
    /// @group(0) @binding(0) var<storage> S1 : array<u32, 64>;
    ///
    /// fn x() {
    ///    F_S0_S0(0);                        // (A)
    ///    F(&S0, 0, &S0);                    // (B)
    ///    F_S1_X_S0(S1_X(0), 1);             // (C)
    ///    F_S1_X_S0(S1_X(5), 2);             // (D)
    ///    F_S1_X_S1_X(S1_X(5), 3, S1_X(3));  // (E)
    ///    F_S1_X_S1_X(S1_X(7), 4, S1_X(2));  // (F)
    /// }
    /// ```
    struct FnVariant {
        /// The signature of the variant is a map of each of the function's 'uniform', 'storage' and
        /// 'workgroup' pointer parameters to the caller's AccessIndices.
        using Signature = utils::Hashmap<const sem::Parameter*, AccessIndices, 4>;

        /// The unique name of the variant.
        /// The symbol is in the `ctx.dst` program namespace.
        Symbol name;

        /// A map of direct calls made by this variant to the name of other function variants.
        utils::Hashmap<const sem::Call*, Symbol, 4> calls;

        /// A map of function parameter of type ptr<function, T>, to the variant's base-pointer
        /// parameter name.
        utils::Hashmap<const sem::Parameter*, Symbol, 4> fn_ptr_params;

        /// The declaration order of the variant, in relation to other variants of the same
        /// function. Used to ensure deterministic ordering of the transform, as map iteration is
        /// not deterministic between compilers.
        size_t order = 0;
    };

    /// FnInfo holds information about a function in the input program.
    struct FnInfo {
        /// A map of variant signature to the variant data.
        utils::Hashmap<FnVariant::Signature, FnVariant, 8> variants;
        /// A map of expressions that have been hoisted to a 'let' declaration in the function.
        utils::Hashmap<const sem::Expression*, Symbol, 8> hoisted_exprs;
        /// A map of variables used by the function that have been 'unshadowed' to the new uniquely
        /// named 'let' declared at the top of the function.
        utils::Hashmap<const sem::Variable*, Symbol, 8> unshadowed_vars;

        /// @returns the variants of the function in a deterministically ordered vector.
        utils::Vector<std::pair<const FnVariant::Signature*, FnVariant*>, 8> SortedVariants() {
            utils::Vector<std::pair<const FnVariant::Signature*, FnVariant*>, 8> out;
            out.Reserve(variants.Count());
            for (auto it : variants) {
                out.Push({&it.key, &it.value});
            }
            out.Sort([&](auto& va, auto& vb) { return va.second->order < vb.second->order; });
            return out;
        }
    };

    /// The clone context
    CloneContext& ctx;
    /// The transform options
    const Options& opts;
    /// Alias to the semantic info in ctx.src
    const sem::Info& sem = ctx.src->Sem();
    /// Alias to the symbols in ctx.src
    const SymbolTable& sym = ctx.src->Symbols();
    /// Alias to the ctx.dst program builder
    ProgramBuilder& b = *ctx.dst;
    /// Map of semantic function to the function info
    utils::Hashmap<const sem::Function*, FnInfo*, 8> fns;
    /// Map of AccessIndices to the name of a type alias for the an array<u32, N> used for the
    /// dynamic indices of an access chain, passed down as the transformed type of a variant's
    /// pointer parameter.
    utils::Hashmap<AccessIndices, Symbol, 8> dynamic_index_array_aliases;
    /// Map of semantic expression to AccessChain
    utils::Hashmap<const sem::Expression*, AccessChain*, 32> access_chains;
    /// Allocator for FnInfo
    utils::BlockAllocator<FnInfo> fn_info_allocator;
    /// Allocator for AccessChain
    utils::BlockAllocator<AccessChain> access_chain_allocator;
    /// Helper used for hoisting expressions to lets
    HoistToDeclBefore hoist{ctx};

    /// CloneState holds pointers to the current function, variant and variant's parameters.
    struct CloneState {
        /// The current function being cloned
        FnInfo* current_function = nullptr;
        /// The current function variant being built
        FnVariant* current_variant = nullptr;
        /// The signature of the current function variant being built
        const FnVariant::Signature* current_variant_sig = nullptr;
    };

    /// The clone state.
    /// Only valid during the lifetime of the CloneContext::Clone().
    CloneState* clone_state = nullptr;

    /// AppendAccessChain creates or extends an existing AccessChain for the given expression,
    /// modifying the #access_chains map.
    void AppendAccessChain(const sem::Expression* expr) {
        // take_chain moves the AccessChain from the expression `from` to the expression `expr`.
        // Returns nullptr if `from` did not hold an access chain.
        auto take_chain = [&](const sem::Expression* from) -> AccessChain* {
            if (auto* chain = AccessChainFor(from)) {
                access_chains.Remove(from);
                access_chains.Add(expr, chain);
                return chain;
            }
            return nullptr;
        };

        Switch(
            expr,
            [&](const sem::VariableUser* user) {
                // Expression resolves to a variable.
                auto* variable = user->Variable();

                auto create_new_chain = [&] {
                    auto* chain = access_chain_allocator.Create();
                    chain->indices.Push(variable);
                    access_chains.Add(expr, chain);
                };

                Switch(
                    variable->Declaration(),
                    [&](const ast::Var*) {
                        if (variable->StorageClass() != ast::StorageClass::kHandle) {
                            // Start a new access chain for the non-handle 'var' access
                            create_new_chain();
                        }
                    },
                    [&](const ast::Parameter*) {
                        // Start a new access chain for the parameter access
                        create_new_chain();
                    },
                    [&](const ast::Let*) {
                        if (variable->Type()->Is<sem::Pointer>()) {
                            // variable is a pointer-let.
                            auto* init = sem.Get(variable->Declaration()->constructor);
                            // Note: We do not use take_chain() here, as we need to preserve the
                            // AccessChain on the let's initializer, as the let needs its
                            // initializer updated, and the let may be used multiple times. Instead
                            // we copy the let's AccessChain into a a new AccessChain.
                            if (auto* init_chain = AccessChainFor(init)) {
                                access_chains.Add(expr, access_chain_allocator.Create(*init_chain));
                            }
                        }
                    });
            },
            [&](const sem::StructMemberAccess* a) {
                // Structure member access.
                // Append the Symbol of the member name to the chain, and move the chain to the
                // member access expression.
                if (auto* chain = take_chain(a->Object())) {
                    chain->indices.Push(a->Member()->Name());
                }
            },
            [&](const sem::IndexAccessorExpression* a) {
                // Array, matrix or vector index.
                // Store the index expression into AccessChain::dynamic_indices, append a
                // DynamicIndex to the chain, and move the chain to the index accessor expression.
                if (auto* chain = take_chain(a->Object())) {
                    chain->indices.Push(DynamicIndex{chain->dynamic_indices.Length()});
                    chain->dynamic_indices.Push(a->Index());
                }
            },
            [&](const sem::Expression* e) {
                if (auto* unary = e->Declaration()->As<ast::UnaryOpExpression>()) {
                    // Unary op.
                    // If this is a '&' or '*', simply move the chain to the unary op expression.
                    if (unary->op == ast::UnaryOp::kAddressOf ||
                        unary->op == ast::UnaryOp::kIndirection) {
                        take_chain(sem.Get(unary->expr));
                    }
                }
            });
    }

    /// MaybeHoistDynamicIndices examines the AccessChain::dynamic_indices member of @p chain,
    /// hoisting all expressions to their own uniquely named 'let' if none of the following are
    /// true:
    /// 1. The index expression is a constant value.
    /// 2. The index expression's statement is the same as @p usage.
    /// 3. The index expression is an identifier resolving to a 'let', 'const' or parameter, AND
    ///    that identifier resolves to the same variable at @p usage.
    ///
    /// A dynamic index will only be hoisted once. The hoisting applies to all variants of the
    /// function that holds the dynamic index expression.
    void MaybeHoistDynamicIndices(AccessChain* chain, const sem::Statement* usage) {
        for (auto& idx : chain->dynamic_indices) {
            if (idx->ConstantValue()) {
                // Dynamic index is constant.
                continue;  // Hoisting not required.
            }

            if (idx->Stmt() == usage) {
                // The index expression is owned by the statement of usage.
                continue;  // Hoisting not required
            }

            if (auto* idx_variable_user = idx->UnwrapMaterialize()->As<sem::VariableUser>()) {
                auto* idx_variable = idx_variable_user->Variable();
                if (idx_variable->Declaration()->IsAnyOf<ast::Let, ast::Const, ast::Parameter>()) {
                    if (!IsShadowed(idx_variable, usage)) {
                        // Dynamic index is an immutable variable, and is not shadowed at usage.
                        continue;  // Hoisting not required.
                    }
                }
            }

            // The dynamic index needs to be hoisted (if it hasn't been already).
            auto fn = FnInfoFor(idx->Stmt()->Function());
            fn->hoisted_exprs.GetOrCreate(idx, [=] {
                // Create a name for the new 'let'
                auto name = b.Symbols().New("ptr_index_save");
                // Insert a new 'let' just above the dynamic index statement.
                hoist.InsertBefore(idx->Stmt(), [this, idx, name] {
                    // The ReplaceAll() handler for ast::Expression deals with substituting uses of
                    // the dynamic index expression with the hoisted let identifier. We however need
                    // to generate the original dynamic index expression once to initialize that
                    // let, so we use #clone_real_expression_not_hoist to signal to the ReplaceAll()
                    // handler that we really do want the original expression when constructing the
                    // let.
                    TINT_SCOPED_ASSIGNMENT(clone_real_expression_not_hoist, idx);
                    return b.Decl(b.Let(name, ctx.Clone(idx->Declaration())));
                });
                return name;
            });
        }
    }

    /// BuildDynamicIndex builds the AST expression node for the dynamic index expression used in an
    /// AccessChain. This is similar to just cloning the expression, but BuildDynamicIndex() also:
    /// * Collapses constant value index expressions down to the computed value. This acts as an
    ///   constant folding optimization and reduces noise from the transform.
    /// * Casts the resulting expression to a u32 if @p cast_to_u32 is true, and the expression type
    ///   isn't implicitly usable as a u32. This is to help feed the expression into a
    ///   `array<u32, N>` argument passed to a callee variant function.
    const ast::Expression* BuildDynamicIndex(const sem::Expression* idx, bool cast_to_u32) {
        if (auto* val = idx->ConstantValue()) {
            // Expression evaluated to a constant value. Just emit that constant.
            return b.Expr(val->As<AInt>());
        }

        // Expression is not a constant, clone the expression.
        // Note: If the dynamic index expression was hoisted to a let, then cloning will return an
        // identifier expression to the hoisted let.
        auto* expr = ctx.Clone(idx->Declaration());

        if (cast_to_u32) {
            // The index may be fed to a dynamic index array<u32, N> argument, so the index
            // expression may need casting to u32.
            if (!idx->UnwrapMaterialize()
                     ->Type()
                     ->UnwrapRef()
                     ->IsAnyOf<sem::U32, sem::AbstractInt>()) {
                expr = b.Construct(b.ty.u32(), expr);
            }
        }

        return expr;
    }

    /// ProcessFunction scans the direct calls made by the function @p fn, adding new variants to
    /// the callee functions and transforming the call expression to pass dynamic indices instead of
    /// true pointers.
    /// If the function @p fn has pointer parameters of the 'uniform', 'storage' or 'workgroup'
    /// address space, and the function is not called, then the function is dropped from the output
    /// of the transform, as it cannot be generated.
    /// @note ProcessFunction must be called in dependency order for the program, starting with the
    /// entry points.
    void ProcessFunction(const sem::Function* fn, FnInfo* fn_info) {
        if (fn_info->variants.IsEmpty()) {
            // Function has no variants pre-generated by callers.
            if (HasPointerParameterToUniformStorageWorkgroup(fn)) {
                // Drop the function, as it wasn't called and cannot be generated.
                ctx.Remove(ctx.src->AST().GlobalDeclarations(), fn->Declaration());
                return;
            }

            // Function was not called. Create a single variant with an empty signature.
            FnVariant variant;
            variant.name = ctx.Clone(fn->Declaration()->symbol);
            variant.order = 0;  // Unaltered comes first.
            fn_info->variants.Add(FnVariant::Signature{}, std::move(variant));
        }

        // Process each of the direct calls made by this function.
        for (auto* call : fn->DirectCalls()) {
            ProcessCall(fn_info, call);
        }
    }

    /// ProcessCall creates new variants of the callee function by permuting the call for each of
    /// the variants of @p caller. ProcessCall also registers the clone callback to transform the
    /// call expression to pass dynamic indices instead of true pointers.
    void ProcessCall(FnInfo* caller, const sem::Call* call) {
        auto* target = call->Target()->As<sem::Function>();
        if (!target) {
            // Call target is not a user-declared function.
            return;  // Not interested in this call.
        }

        if (!HasPointerParameter(target)) {
            return;  // Not interested in this call.
        }

        bool call_needs_transforming = false;

        // Build the call target function variant for each variant of the caller.
        for (auto caller_variant_it : caller->SortedVariants()) {
            auto& caller_signature = *caller_variant_it.first;
            auto& caller_variant = *caller_variant_it.second;

            // Build the target variant's signature.
            FnVariant::Signature target_signature;
            for (size_t i = 0; i < call->Arguments().Length(); i++) {
                const auto* arg = call->Arguments()[i];
                const auto* param = target->Parameters()[i];
                const auto* param_ty = param->Type()->As<sem::Pointer>();
                if (!param_ty) {
                    continue;  // Parameter type is not a pointer.
                }

                auto* arg_chain = AccessChainFor(arg);
                if (!arg_chain) {
                    continue;  // Argument does not have an access chain
                }

                if (CallArgumentRequiresTransform(caller_signature, arg_chain)) {
                    // Record that this chain was used in a function call.
                    arg_chain->used_in_call = true;
                    // Construct the absolute AccessIndices by considering the AccessIndices of the
                    // caller's variant and the chain.
                    auto indices = AbsoluteAccessIndices(caller_signature, arg_chain->indices);
                    // If the parameter is a ptr<function, T>, then change the root to be a
                    // FnPtrParam
                    if (param_ty->StorageClass() == ast::StorageClass::kFunction) {
                        auto* root_obj_type = RootType(arg_chain->indices);
                        indices[0] = FnPtrParam{root_obj_type, param};
                    }

                    // Add the parameter's absolute AccessIndices to the target's signature.
                    target_signature.Add(param, std::move(indices));
                }
            }

            // Construct a new FnVariant if this is the first caller of the target signature
            auto* target_info = FnInfoFor(target);
            auto& target_variant = target_info->variants.GetOrCreate(target_signature, [&] {
                if (target_signature.IsEmpty()) {
                    // Call target does not require any argument changes.
                    FnVariant variant;
                    variant.name = ctx.Clone(target->Declaration()->symbol);
                    variant.order = 0;  // Unaltered comes first.
                    return variant;
                }

                // Build an appropriate variant function name.
                // This is derived from the original function name and the pointer parameter
                // chains.
                std::stringstream ss;
                ss << ctx.src->Symbols().NameFor(target->Declaration()->symbol);
                for (auto* param : target->Parameters()) {
                    if (auto* indices = target_signature.Find(param)) {
                        ss << "_" << AccessIndicesName(*indices);
                    }
                }
                // Build the variant.
                FnVariant variant;
                variant.name = b.Symbols().New(ss.str());
                variant.order = target_info->variants.Count() + 1;
                return variant;
            });

            // Record the call made by caller variant to the target variant.
            caller_variant.calls.Add(call, target_variant.name);
            if (!target_signature.IsEmpty()) {
                // The call expression will need transforming for at least one caller variant.
                call_needs_transforming = true;
            }
        }

        if (call_needs_transforming) {
            // Register the clone callback to correctly transform the call expression into the
            // appropriate variant calls.
            TransformCall(call);
        }
    }

    bool CallArgumentRequiresTransform(const FnVariant::Signature& caller_signature,
                                       const AccessChain* arg_chain) const {
        if (auto* fn_ptr = RootFnPtrParam(arg_chain->indices)) {
            // Argument originates from a ptr<function, T> parameter.
            return true;
        }
        if (auto* var = RootVariable(arg_chain->indices)) {
            return Switch(
                var,  //
                [&](const sem::Parameter* param) {
                    // Argument originates from a pointer parameter
                    // If the caller's parameter has an incoming access chain, then the callee
                    // must be a variant.
                    return caller_signature.Contains(param);
                },
                [&](const sem::GlobalVariable* global) {
                    // Argument originates from a module-scope variable
                    if (IsUniformStorageWorkgroup(global->StorageClass())) {
                        // Module-scope variable
                        return true;
                    }

                    if (opts.transform_private &&
                        global->StorageClass() == ast::StorageClass::kPrivate &&
                        arg_chain->indices.Length() > 1) {
                        // Access into a private variable.
                        return true;
                    }

                    return false;
                },
                [&](const sem::LocalVariable* local) {
                    // Argument originates from a function-scope variable
                    if (opts.transform_function &&
                        local->StorageClass() == ast::StorageClass::kFunction &&
                        arg_chain->indices.Length() > 1) {
                        // Access into a function variable.
                        return true;
                    }

                    return false;
                },
                [&](const sem::LocalVariable*) {
                    // Argument originates from a function-scope variable
                    return false;
                });
        }
        TINT_ICE(Transform, b.Diagnostics()) << "unhandled chain root type";
        return false;
    }

    /// @returns the AccessChain for the expression @p expr, or nullptr if the expression does not
    /// hold an access chain.
    AccessChain* AccessChainFor(const sem::Expression* expr) const {
        if (auto* chain = access_chains.Find(expr)) {
            return *chain;
        }
        return nullptr;
    }

    /// @returns the absolute AccessIndices for @p indices, by replacing the originating pointer
    /// parameter with the AccessChain of variant's signature.
    AccessIndices AbsoluteAccessIndices(const FnVariant::Signature& signature,
                                        const AccessIndices& indices) const {
        if (auto* root_param = RootParameter(indices)) {
            // Access chain originates from a parameter, which will be transformed into an array of
            // dynamic indices. Concatenate the signature's AccessIndices for the parameter to
            // the chain's indices, skipping over the chain's initial parameter index.
            auto absolute = *signature.Find(root_param);
            for (size_t access_idx = 1; access_idx < indices.Length(); access_idx++) {
                absolute.Push(indices[access_idx]);
            }
            return absolute;
        }

        // Chain does not originate from a parameter, so is already absolute.
        return indices;
    }

    /// TransformFunction registers the clone callback to transform the function @p fn into the
    /// (potentially multiple) function's variants. TransformFunction will assign the current
    /// function and variant to #clone_state, which can be used by the other clone callbacks.
    void TransformFunction(const sem::Function* fn, FnInfo* fn_info) {
        // Register a custom handler for the specific function
        ctx.Replace(fn->Declaration(), [this, fn, fn_info] {
            // For the duration of this lambda, assign current_function to fn_info.
            TINT_SCOPED_ASSIGNMENT(clone_state->current_function, fn_info);

            // This callback expects a single function returned. As we're generating potentially
            // many variant functions, keep a record of the last created variant, and explicitly add
            // this to the module if it isn't the last. We'll return the last created variant,
            // taking the place of the original function.
            const ast::Function* pending_variant = nullptr;

            // For each variant of fn...
            for (auto variant_it : fn_info->SortedVariants()) {
                if (pending_variant) {
                    b.AST().AddFunction(pending_variant);
                }

                auto& variant_sig = *variant_it.first;
                auto& variant = *variant_it.second;

                // For the duration of this scope, assign the current variant and variant signature.
                TINT_SCOPED_ASSIGNMENT(clone_state->current_variant_sig, &variant_sig);
                TINT_SCOPED_ASSIGNMENT(clone_state->current_variant, &variant);

                // Build the variant's parameters.
                // Pointer parameters in the 'uniform', 'storage' or 'workgroup' address space are
                // either replaced with an array of dynamic indices, or are dropped (if there are no
                // dynamic indices).
                utils::Vector<const ast::Parameter*, 8> params;
                for (auto* param : fn->Parameters()) {
                    if (auto* ptr_access = variant_sig.Find(param)) {
                        // Parameter needs replacing with either zero, one or two parameters:
                        // If the parameter is a ptr<function> then we always create a ptr<function>
                        // parameter to the originating variable. This always comes first.
                        // If the access chain has dynamic indices, then we create a an
                        // array<u32, N> parameter to hold the dynamic indices.

                        auto* param_ty = param->Type()->As<sem::Pointer>();
                        if (param_ty->StorageClass() == ast::StorageClass::kFunction) {
                            // ptr<function> - requires pointer base parameter.
                            auto param_name = b.Symbols().New(
                                ctx.src->Symbols().NameFor(param->Declaration()->symbol) + "_base");
                            auto* root_ty = RootType(*ptr_access)->UnwrapPtr();
                            auto* base_ptr_ty = b.ty.pointer(CreateASTTypeFor(ctx, root_ty),
                                                             ast::StorageClass::kFunction);
                            params.Push(b.Param(param_name, base_ptr_ty));

                            // Remember this base pointer name.
                            clone_state->current_variant->fn_ptr_params.Add(param, param_name);
                        }

                        if (auto* dyn_idx_arr_type = DynamicIndexArrayType(*ptr_access)) {
                            // Variant has dynamic indices for this variant, replace it.
                            auto param_name = ctx.Clone(param->Declaration()->symbol);
                            params.Push(b.Param(param_name, dyn_idx_arr_type));
                        }
                    } else {
                        // Just a regular parameter. Just clone the original parameter.
                        params.Push(ctx.Clone(param->Declaration()));
                    }
                }

                // Build the variant by just cloning every
                auto* ret_ty = ctx.Clone(fn->Declaration()->return_type);
                auto body = ctx.Clone(fn->Declaration()->body);
                auto attrs = ctx.Clone(fn->Declaration()->attributes);
                auto ret_attrs = ctx.Clone(fn->Declaration()->return_type_attributes);
                pending_variant =
                    b.create<ast::Function>(variant.name, std::move(params), ret_ty, body,
                                            std::move(attrs), std::move(ret_attrs));
            }

            return pending_variant;
        });
    }

    /// TransformCall registers the clone callback to transform the call expression @p call to call
    /// the correct target variant, and to replace pointers arguments with an array of dynamic
    /// indices.
    void TransformCall(const sem::Call* call) {
        // Register a custom handler for the specific call expression
        ctx.Replace(call->Declaration(), [this, call]() {
            auto target_variant = clone_state->current_variant->calls.Find(call);
            if (!target_variant) {
                // The current variant does not need to transform this call.
                return ctx.CloneWithoutTransform(call->Declaration());
            }

            // Build the new call expressions's arguments.
            utils::Vector<const ast::Expression*, 8> new_args;
            for (size_t arg_idx = 0; arg_idx < call->Arguments().Length(); arg_idx++) {
                auto* arg = call->Arguments()[arg_idx];
                auto* param = call->Target()->Parameters()[arg_idx];
                auto* param_ty = param->Type()->As<sem::Pointer>();
                if (!param_ty) {
                    continue;  // Parameter is not a pointer
                }

                auto* chain = AccessChainFor(arg);
                if (!chain) {
                    // No access chain means the argument is not a pointer that needs transforming.
                    // Just clone the unaltered argument.
                    new_args.Push(ctx.Clone(arg->Declaration()));
                    continue;
                }

                // Build the absolute AccessIndices for this pointer argument using the current
                // variant of the caller.
                auto full_indices =
                    AbsoluteAccessIndices(*clone_state->current_variant_sig, chain->indices);

                auto* chain_root = RootVariable(chain->indices);

                // If the parameter of type ptr<function, T>, then we need to pass an additional
                // pointer argument to the originating var.
                if (param_ty->StorageClass() == ast::StorageClass::kFunction) {
                    const ast::Expression* root_expr =
                        b.Expr(UnshadowedSymbol(clone_state->current_function, chain_root));
                    if (!chain_root->Type()->Is<sem::Pointer>()) {
                        root_expr = b.AddressOf(root_expr);
                    }
                    new_args.Push(root_expr);
                }

                // Get or create the dynamic indices array.
                if (auto* dyn_idx_arr_ty = DynamicIndexArrayType(full_indices)) {
                    // Build an array of dynamic indices to pass as the replacement for the pointer.
                    utils::Vector<const ast::Expression*, 8> dyn_idx_args;
                    if (auto* chain_root_param = chain_root->As<sem::Parameter>()) {
                        // Access chain originates from a pointer parameter.
                        auto chain_root_name =
                            UnshadowedSymbol(clone_state->current_function, chain_root_param);
                        // This pointer parameter will have been replaced with a array<u32, N>
                        // holding the variant's dynamic indices for the pointer. Unpack these
                        // directly into the array constructor's arguments.
                        auto N = CountDynamicIndices(
                            *clone_state->current_variant_sig->Find(chain_root_param));
                        for (uint32_t i = 0; i < N; i++) {
                            dyn_idx_args.Push(b.IndexAccessor(chain_root_name, u32(i)));
                        }
                    }
                    // Pass the dynamic indices of the access chain into the array constructor.
                    for (auto& dyn_idx : chain->dynamic_indices) {
                        dyn_idx_args.Push(BuildDynamicIndex(dyn_idx, true));
                    }
                    // Construct the dynamic index array, and push as an argument.
                    new_args.Push(b.Construct(dyn_idx_arr_ty, std::move(dyn_idx_args)));
                }
            }

            // Make the call to the target's variant.
            return b.Call(*target_variant, std::move(new_args));
        });
    }

    /// TransformAccessChains registers the clone callback to:
    /// * Transform all expressions that have an AccessChain (which aren't arguments to function
    ///   calls, these are handled by TransformCall()), into the equivalent expression using a
    ///   module-scope variable.
    /// * Replace expressions that have been hoisted to a let, with an identifier expression to that
    ///   let.
    void TransformAccessChains() {
        // Register a custom handler for all non-function call expressions
        ctx.ReplaceAll([this](const ast::Expression* ast_expr) -> const ast::Expression* {
            if (!clone_state->current_variant) {
                // Expression does not belong to a function variant.
                return nullptr;  // Just clone the expression.
            }

            auto* expr = sem.Get<sem::Expression>(ast_expr);
            if (!expr) {
                // No semantic node for the expression.
                return nullptr;  // Just clone the expression.
            }

            // If the expression has been hoisted to a 'let', and we're not generating the let
            // initializer, then replace the expression with an identifier to the hoisted let.
            if (clone_real_expression_not_hoist != expr) {
                if (auto* hoisted = clone_state->current_function->hoisted_exprs.Find(expr)) {
                    return b.Expr(*hoisted);
                }
            }

            auto* chain = AccessChainFor(expr);
            if (!chain) {
                // The expression does not have an AccessChain.
                return nullptr;  // Just clone the expression.
            }

            auto* root_param = RootParameter(chain->indices);
            if (!root_param) {
                // The expression has an access chain, but does not originate with a pointer
                // parameter. We don't need to change anything here.
                return nullptr;  // Just clone the expression.
            }

            auto* incoming_indices = clone_state->current_variant_sig->Find(root_param);
            if (!incoming_indices) {
                // The root parameter of the access chain is not part of the variant's signature.
                return nullptr;  // Just clone the expression.
            }

            // Expression holds an access chain to a pointer parameter in the 'uniform', 'storage'
            // or 'workgroup' address space. Reconstruct the chain from the module-scope originating
            // variable using the function variant's incoming dynamic indices.

            const ast::Expression* chain_expr = nullptr;

            // Chain starts with a pointer parameter.
            // Replace this with the variant's incoming chain. This will bring the expression up to
            // the incoming pointer.
            auto param_name = UnshadowedSymbol(clone_state->current_function, root_param);
            for (auto param_access : *incoming_indices) {
                chain_expr = BuildAccessExpr(chain_expr, param_access, [&](size_t i) {
                    return b.IndexAccessor(param_name, AInt(i));
                });
            }

            // Now build the expression chain within the function.

            // For each access in the chain (excluding the pointer parameter)...
            for (size_t access_idx = 1; access_idx < chain->indices.Length(); access_idx++) {
                auto& access = chain->indices[access_idx];
                chain_expr = BuildAccessExpr(chain_expr, access, [&](size_t i) {
                    return BuildDynamicIndex(chain->dynamic_indices[i], false);
                });
            }

            // BuildAccessExpr() always returns a non-pointer.
            // If the expression we're replacing is a pointer, take the address.
            if (expr->Type()->Is<sem::Pointer>()) {
                chain_expr = b.AddressOf(chain_expr);
            }

            return chain_expr;
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

    /// @returns the cloned symbol of @p variable if not shadowed, otherwise the
    /// symbol of the 'unshadow' var. The symbol is in the `ctx.dst` namespace.
    Symbol UnshadowedSymbol(const FnInfo* fn_info, const sem::Variable* variable) const {
        if (auto let = fn_info->unshadowed_vars.Find(variable)) {
            return *let;
        }
        return ctx.Clone(variable->Declaration()->symbol);
    }

    const sem::Expression* clone_real_expression_not_hoist = nullptr;

    static uint32_t CountDynamicIndices(const AccessIndices& indices) {
        uint32_t count = 0;
        for (auto& idx : indices) {
            if (std::holds_alternative<DynamicIndex>(idx)) {
                count++;
            }
        }
        return count;
    }

    const ast::TypeName* DynamicIndexArrayType(const AccessIndices& full_indices) {
        auto name = dynamic_index_array_aliases.GetOrCreate(full_indices, [&] {
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

        if (RootFnPtrParam(indices)) {
            ss << "F";
        } else {
            auto* root = RootVariable(indices);
            ss << ctx.src->Symbols().NameFor(root->Declaration()->symbol);
        }

        for (size_t i = 1; i < indices.Length(); i++) {
            auto& access = indices[i];

            ss << "_";

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

        if (auto* ptr_param = std::get_if<FnPtrParam>(&access)) {
            /// The access is rooted from a ptr<function, T> parameter.
            if (auto name = clone_state->current_variant->fn_ptr_params.Find(ptr_param->param)) {
                return b.Deref(*name);
            }

            TINT_ICE(Transform, b.Diagnostics())
                << "FnPtrParam not found in variant's fn_ptr_params";
            return nullptr;
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

    /// @returns the originating FnPtrParam of the AccessIndices @p indices.
    const FnPtrParam* RootFnPtrParam(const AccessIndices& indices) const {
        auto& root = indices.Front();
        if (auto* fn_ptr = std::get_if<FnPtrParam>(&root)) {
            return fn_ptr;
        }
        return nullptr;
    }

    /// @returns the originating Variable of the AccessIndices @p indices, or nullptr if the
    /// originating variable is not a variable.
    const sem::Variable* RootVariable(const AccessIndices& indices) const {
        auto& root = indices.Front();
        if (auto* var = std::get_if<const sem::Variable*>(&root)) {
            return *var;
        }
        if (auto* fn_ptr_param = RootFnPtrParam(indices)) {
            return fn_ptr_param->param;
        }
        return nullptr;
    }

    /// @returns the originating parameter of the AccessIndices @p indices, or nullptr if the
    /// originating variable is not a parameter.
    const sem::Parameter* RootParameter(const AccessIndices& indices) const {
        return tint::As<sem::Parameter>(RootVariable(indices));
    }

    /// @returns the originating variable type of the AccessIndices @p indices.
    const sem::Type* RootType(const AccessIndices& indices) const {
        if (auto* fn_ptr = RootFnPtrParam(indices)) {
            return fn_ptr->root_type;
        }
        if (auto* var = RootVariable(indices)) {
            return var->Type();
        }
        return nullptr;
    }

    /// @returns true if the function @p fn has at least one pointer parameter.
    static bool HasPointerParameter(const sem::Function* fn) {
        for (auto* param : fn->Parameters()) {
            if (param->Type()->Is<sem::Pointer>()) {
                return true;
            }
        }
        return false;
    }

    /// @returns true if the function @p fn has at least one pointer parameter in the 'uniform',
    /// 'storage' or 'workgroup' address space.
    static bool HasPointerParameterToUniformStorageWorkgroup(const sem::Function* fn) {
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

DirectVariableAccess::Config::Config(const Options& opt) : options(opt) {}

DirectVariableAccess::Config::~Config() = default;

DirectVariableAccess::DirectVariableAccess() = default;

DirectVariableAccess::~DirectVariableAccess() = default;

bool DirectVariableAccess::ShouldRun(const Program* program, const DataMap&) const {
    return State::ShouldRun(program);
}

void DirectVariableAccess::Run(CloneContext& ctx, const DataMap& inputs, DataMap&) const {
    Options options;
    if (auto* cfg = inputs.Get<Config>()) {
        options = cfg->options;
    }
    State(ctx, options).Run();
}

}  // namespace tint::transform
