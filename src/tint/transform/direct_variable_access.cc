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

    /// The main function for the transform.
    void Run() {
        // Stage 1:
        // Walk all the expressions of the program, starting with the expression leaves.
        // Whenever we find a expression originating with a global or parameter in the `uniform`,
        // `storage` or `workgroup` address space, then start constructing an access chain. These
        // chains are grown and moved up the expression tree until they are used. After this stage,
        // we are left with all the root expression access chains to the pointer variables.
        for (auto* node : ctx.src->ASTNodes().Objects()) {
            if (auto* expr = sem.Get<sem::Expression>(node)) {
                AppendAccessChain(expr);
            }
        }

        // Stage 2:
        // Walk the functions in dependency order, starting with the entry points.
        // Construct the set of function 'variants' by examining the calls made by each function to
        // their call target. Each variant holds a unique set of global variable access chains, and
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
        // Ensure that all globals and parameters are not shadowed at the each access chain
        // statement. If the variable is shadowed, create a `let` at the top of the function.
        for (auto chain_it : access_chains) {
            auto [expr, chain] = chain_it;
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
    /// Symbol               - a struct member access.
    /// DynamicIndex         - a runtime index on an array, matrix column, or vector element.
    using AccessIndex = std::variant<const sem::Variable*, Symbol, DynamicIndex>;

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
    /// Map of semantic function to the function info
    utils::Hashmap<const sem::Function*, FnInfo*, 8> fns;
    /// Map of AccessIndices to the name of a type alias for the dynamic index parameter types.
    utils::Hashmap<AccessIndices, Symbol, 8> dynamic_index_aliases;
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
                auto* ty_as_ptr = variable->Type()->As<sem::Pointer>();
                if (ty_as_ptr && variable->Declaration()->Is<ast::Let>()) {
                    // variable is a pointer-let.
                    auto* init = sem.Get(variable->Declaration()->constructor);
                    // Note: We do not use take_chain() here, as we need to preserve the AccessChain
                    // on the let's initializer, as the let needs its initializer updated, and the
                    // let may be used multiple times. Instead we copy the let's AccessChain into a
                    // a new AccessChain.
                    if (auto* init_chain = AccessChainFor(init)) {
                        access_chains.Add(expr, access_chain_allocator.Create(*init_chain));
                        // As we are using the shared AccessChain of another statement, we need to
                        // be careful that variables referenced by the dynamic indices are not
                        // shadowed between the let and this use, and that the chain does not use
                        // expressions with side-effects which cannot be repeatedly evaluated. To
                        // solve both these problems we can hoist the dynamic index expressions to
                        // their own uniquely named lets (if required).
                        MaybeHoistDynamicIndices(init_chain, user->Stmt());
                    }
                } else {
                    if (IsUniformStorageWorkgroup(variable->StorageClass()) ||
                        (ty_as_ptr && IsUniformStorageWorkgroup(ty_as_ptr->StorageClass()))) {
                        // variable is either a module-scope var or a pointer-parameter of
                        // 'uniform', 'storage' or 'workgroup' address space.
                        // Construct a new AccessChain to this variable.
                        auto* chain = access_chain_allocator.Create();
                        chain->indices.Push(variable);
                        access_chains.Add(expr, chain);
                    }
                }
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
    /// 2. The index expression is an identifier resolving to a 'let', 'const' or parameter, AND
    ///    that identifier resolves to the same variable at @p usage.
    ///
    /// A dynamic index will only be hoisted once. The hoisting applies to all variants of the
    /// function that holds the dynamic index expression.
    void MaybeHoistDynamicIndices(AccessChain* chain, const sem::Statement* usage) {
        for (auto& idx : chain->dynamic_indices) {
            if (idx->ConstantValue()) {
                // Dynamic index is constant. Hoisting not required.
                continue;
            }

            if (auto* idx_variable_user = idx->UnwrapMaterialize()->As<sem::VariableUser>()) {
                auto* idx_variable = idx_variable_user->Variable();
                if (idx_variable->Declaration()->IsAnyOf<ast::Let, ast::Const, ast::Parameter>()) {
                    if (!IsShadowed(idx_variable, usage)) {
                        // Dynamic index is an immutable variable, and is not shadowed at usage.
                        // Hoisting not required.
                        continue;
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
            // Not interested in this call.
            return;
        }

        if (!HasPointerParameterToUniformStorageWorkgroup(target)) {
            // Target function doesn't have any 'uniform', 'storage' or 'workgroup' pointer
            // parameters.
            return;
        }

        // Build the call target function variant for each variant of the caller.
        for (auto caller_variant_it : caller->SortedVariants()) {
            auto& caller_signature = *caller_variant_it.first;
            auto& caller_variant = *caller_variant_it.second;

            // Build the target variant's signature.
            FnVariant::Signature target_signature;
            for (size_t i = 0; i < call->Arguments().Length(); i++) {
                const auto* arg = call->Arguments()[i];
                const auto* param = target->Parameters()[i];
                if (IsPointerToUniformStorageWorkgroup(param->Type())) {
                    // Grab the access chain for the pointer argument. This chain either starts with
                    // a module-scope variable, or a pointer parameter of the caller.
                    auto& chain = *AccessChainFor(arg);
                    // Construct the absolute AccessIndices by considering the AccessIndices of the
                    // caller's variant and the chain.
                    auto indices = AbsoluteAccessIndices(caller_signature, chain.indices);
                    // Add the parameter's absolute AccessIndices to the target's signature.
                    target_signature.Add(param, indices);
                }
            }

            // Construct a new FnVariant if this is the first caller of the target signature
            auto* target_info = FnInfoFor(target);
            auto& target_variant = target_info->variants.GetOrCreate(target_signature, [&] {
                // Build an appropriate variant function name.
                // This is derived from the original function name and the pointer parameter chains.
                auto name = ctx.src->Symbols().NameFor(target->Declaration()->symbol);
                for (auto* param : target->Parameters()) {
                    if (auto* indices = target_signature.Find(param)) {
                        name += "_" + AccessIndicesName(*indices);
                    }
                }
                // Build the variant.
                FnVariant variant;
                variant.name = b.Symbols().New(name);
                variant.order = target_info->variants.Count();
                return variant;
            });

            // Record the call made by caller variant to the target variant.
            caller_variant.calls.Add(call, target_variant.name);
        }

        // Register the clone callback to correctly transform the call expression into the
        // appropriate variant call.
        TransformCall(call);
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
            for (size_t i = 1; i < indices.Length(); i++) {
                absolute.Push(indices[i]);
            }
            return absolute;
        }

        // Chain does not originate from a parameter, so is already absolute.
        return indices;
    }

    /// TransformCall registers the clone callback to transform the call expression @p call to call
    /// the correct target variant, and to replace pointers arguments with an array of dynamic
    /// indices.
    void TransformCall(const sem::Call* call) {
        // Register a custom handler for the specific call statement
        ctx.Replace(call->Declaration(), [this, call]() {
            // Build the new call expressions's arguments.
            utils::Vector<const ast::Expression*, 8> new_args;
            for (auto* arg : call->Arguments()) {
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

                // Get or create the type alias for the array of dynamic indices (array<u32, N>)
                auto* arg_ty = DynamicIndexAlias(full_indices);
                if (!arg_ty) {
                    // Pointer has no dynamic indices in the AccessChain.
                    // In this situation the function omits this parameter, so skip the argument.
                    continue;
                }

                // Build the array of dynamic indices to pass as the replacement for the pointer.
                utils::Vector<const ast::Expression*, 8> dyn_idx_args;
                if (auto* root_param = RootParameter(chain->indices)) {
                    // Chain originates from a pointer parameter.
                    // This parameter will have been replaced with an array of u32 indices.
                    auto param_name = UnshadowedSymbol(clone_state->current_function, root_param);
                    auto& arg_indices = *clone_state->current_variant_sig->Find(root_param);
                    auto num_param_indices = CountDynamicIndices(arg_indices);
                    for (uint32_t i = 0; i < num_param_indices; i++) {
                        dyn_idx_args.Push(b.IndexAccessor(param_name, u32(i)));
                    }
                }
                for (auto& dyn_idx : chain->dynamic_indices) {
                    dyn_idx_args.Push(BuildDynamicIndex(dyn_idx, true));
                }
                new_args.Push(b.Construct(arg_ty, std::move(dyn_idx_args)));
            }
            auto target_variant = clone_state->current_variant->calls.Find(call);
            return b.Call(*target_variant, std::move(new_args));
        });
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

                TINT_SCOPED_ASSIGNMENT(clone_state->current_variant_sig, &variant_params);
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

            if (clone_real_expression_not_hoist != expr) {
                if (auto* hoisted = clone_state->current_function->hoisted_exprs.Find(expr)) {
                    return b.Expr(*hoisted);
                }
            }

            auto* chain = AccessChainFor(expr);
            if (!chain) {
                return ctx.CloneWithoutTransform(ast_expr);
            }

            // Reconstruct the chain using a module-scope variable and the function variant's
            // incoming dynamic indices.

            // Callback for replacing the expression.
            // This will be called once for each variant of the function.
            const ast::Expression* chain_expr = nullptr;

            size_t start_idx = 0;
            if (auto* root_param = RootParameter(chain->indices)) {
                // Chain starts with a pointer parameter.
                // Replace this with the variant's incoming chain.
                auto param_name = UnshadowedSymbol(clone_state->current_function, root_param);
                for (auto param_access : *clone_state->current_variant_sig->Find(root_param)) {
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
                chain_expr = BuildAccessExpr(chain_expr, access, [&](size_t i) {
                    return BuildDynamicIndex(chain->dynamic_indices[i], false);
                });
            }

            // End result of the access chain is always a non-pointer. If the expression we're
            // replacing is a pointer, take the address.
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

    /// @returns an symbol of the module-scope or parameter variable if not shadowed, otherwise the
    /// symbol of the 'unshadow' var.
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

    /// @returns the originating parameter of the AccessIndices @p indices, or nullptr if the
    /// originating variable is not a parameter.
    const sem::Parameter* RootParameter(const AccessIndices& indices) const {
        return std::get<const sem::Variable*>(indices.Front())->As<sem::Parameter>();
    }

    /// @returns true if the function @p fn has at least one parameter in the 'uniform', 'storage'
    /// or 'workgroup' address space.
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

DirectVariableAccess::DirectVariableAccess() = default;

DirectVariableAccess::~DirectVariableAccess() = default;

bool DirectVariableAccess::ShouldRun(const Program* program, const DataMap&) const {
    return State::ShouldRun(program);
}

void DirectVariableAccess::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    State(ctx).Run();
}

}  // namespace tint::transform
