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
            TINT_SCOPED_ASSIGNMENT(current.function, *fn_info);

            const ast::Function* pending_func = nullptr;
            for (auto variant_it : (*fn_info)->SortedVariants()) {
                if (pending_func) {
                    b.AST().AddFunction(pending_func);
                }
                auto& variant_params = variant_it.first;
                auto& variant = variant_it.second;

                TINT_SCOPED_ASSIGNMENT(current.variant_params, &variant_params);
                TINT_SCOPED_ASSIGNMENT(current.variant, &variant);

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
    /// u32                  - a static index on a struct.
    /// DynamicIndex         - a runtime index on an array, matrix column, or vector element.
    using AccessIndex = std::variant<const sem::Variable*, u32, DynamicIndex>;

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

    using CallAccessChains = utils::Hashmap<const sem::Expression*, AccessChain, 4>;

    struct FnVariant {
        Symbol name;
        utils::Hashmap<const sem::Call*, Symbol, 4> calls;
    };

    struct FnInfo {
        utils::Hashmap<FnVariantPtrParams, FnVariant, 4> variants;
        CallAccessChains call_access_chains;
        utils::Hashmap<const sem::Expression*, Symbol, 8> hoisted_exprs;
        utils::Hashmap<const sem::Variable*, Symbol, 8> unshadowed_vars;

        utils::Vector<std::pair<FnVariantPtrParams, FnVariant>, 4> SortedVariants() const {
            utils::Vector<std::pair<FnVariantPtrParams, FnVariant>, 4> out;
            out.Reserve(variants.Count());
            for (auto it : variants) {
                out.Push({it.key, it.value});
            }
            std::sort(out.begin(), out.end(), [&](auto& va, auto& vb) {
                // Assumes that the symbol IDs are sequentially allocated (which they are)
                return va.second.name.value() < vb.second.name.value();
            });
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

    HoistToDeclBefore hoist{ctx};

    struct {
        FnInfo* function = nullptr;
        FnVariant* variant = nullptr;
        const FnVariantPtrParams* variant_params = nullptr;

    } current;

    /// @returns true
    static bool NeedsTransforming(const sem::Expression* expr) {
        return expr->Type()->UnwrapRef()->Is<sem::Pointer>();
    }

    /// @param fn the function to scan for calls
    void ProcessFunction(const sem::Function* fn) {
        auto* fn_info = FnInfoFor(fn);
        TINT_SCOPED_ASSIGNMENT(current.function, fn_info);

        if (fn_info->variants.IsEmpty()) {
            // Function has no variants pre-generated by callers.
            // Create a single variant.
            FnVariant variant;
            variant.name = ctx.Clone(fn->Declaration()->symbol);
            fn_info->variants.Add(FnVariantPtrParams{}, std::move(variant));
        }

        ProcessStatement(fn->Declaration()->body);
    }

    void ProcessStatement(const ast::Statement* stmt) {
        if (!stmt) {
            return;
        }
        Switch(
            stmt,  //
            [&](const ast::BlockStatement* block) {
                for (auto* s : block->statements) {
                    ProcessStatement(s);
                }
            },
            [&](const ast::ReturnStatement* ret) { ProcessExpression(ret->value); },
            [&](const ast::CallStatement* call) { ProcessExpression(call->expr); },
            [&](const ast::VariableDeclStatement* decl) {
                ProcessExpression(decl->variable->constructor);
            },
            [&](const ast::IncrementDecrementStatement*) {},
            [&](const ast::IfStatement* s) {
                ProcessExpression(s->condition);
                ProcessStatement(s->body);
                ProcessStatement(s->else_statement);
            },
            [&](const ast::ForLoopStatement* s) {
                ProcessStatement(s->initializer);
                ProcessExpression(s->condition);
                ProcessStatement(s->continuing);
                ProcessStatement(s->body);
            },
            [&](const ast::WhileStatement* s) {
                ProcessExpression(s->condition);
                ProcessStatement(s->body);
            },
            [&](const ast::VariableDeclStatement* s) {
                ProcessExpression(s->variable->constructor);
            },
            [&](const ast::CompoundAssignmentStatement* s) { ProcessExpression(s->rhs); },
            [&](Default) {
                TINT_ICE(Transform, b.Diagnostics())
                    << "unhandled statement type: " << stmt->TypeInfo().name;
            });
    }

    void ProcessExpression(const ast::Expression* root_expr) {
        if (!root_expr) {
            return;
        }
        ast::TraverseExpressions(root_expr, b.Diagnostics(), [&](const ast::Expression* ast_expr) {
            auto* expr = sem.Get(ast_expr);
            if (auto* call = expr->As<sem::Call>()) {
                return ProcessCall(call);
            }
            if (NeedsTransforming(expr)) {
                // We've found an expression that need reconstructing using an access chain.
                // Build the access chain for this function.
                auto chain = AccessChainFor(expr);

                // Ensure that variables in the access chains are not shadowed.
                for (auto variant_it : current.function->variants) {
                    for (auto param_it : variant_it.key) {
                        Unshadow(param_it.value, expr->Stmt());
                    }
                }

                ctx.Replace(ast_expr, [this, chain] {
                    // Callback for replacing the expression.
                    // This will be called once for each variant of the function.
                    ExprType expr_type = {};

                    size_t start_idx = 0;
                    if (auto* root_param = RootParameter(chain)) {
                        // Chain starts with a pointer parameter.
                        // Replace this with the variant's incoming chain.
                        Symbol param_name;
                        if (auto let = current.function->unshadowed_vars.Find(root_param)) {
                            param_name = *let;
                        } else {
                            param_name = ctx.Clone(root_param->Declaration()->symbol);
                        }
                        for (auto param_access : *current.variant_params->Find(root_param)) {
                            BuildAccessExpr(expr_type, param_access, [&](size_t i) {
                                return b.IndexAccessor(param_name, AInt(i));
                            });
                        }
                        // Skip the parameter access when building the rest of the chain
                        start_idx++;
                    }

                    // For each access in the chain...
                    for (size_t access_idx = start_idx; access_idx < chain.indices.Length();
                         access_idx++) {
                        BuildAccessExpr(expr_type, chain.indices[access_idx],
                                        [&](size_t i) { return chain.dynamic_indices[i](); });
                    }
                    return b.AddressOf(expr_type.expr);
                });
            }
            return ast::TraverseAction::Descend;
        });
    }

    ast::TraverseAction ProcessCall(const sem::Call* call) {
        auto* target = call->Target()->As<sem::Function>();
        if (!target) {
            return ast::TraverseAction::Descend;
        }

        // For each argument, check whether the argument is a pointer that needs transforming.
        for (auto arg : call->Arguments()) {
            if (NeedsTransforming(arg)) {
                current.function->call_access_chains.Add(arg, AccessChainFor(arg));
            } else {
                ProcessExpression(arg->Declaration());
            }
        }

        if (current.function->call_access_chains.IsEmpty()) {
            // Nothing needs changing here.
            return ast::TraverseAction::Skip;
        }

        // Build call target variants
        auto build_target_variant = [&](const FnVariantPtrParams& caller_variant_params,
                                        FnVariant& caller_variant) {
            FnVariantPtrParams target_variant_params;

            for (size_t i = 0; i < call->Arguments().Length(); i++) {
                const auto* arg = call->Arguments()[i];
                const auto* param = target->Parameters()[i];
                if (const auto chain = current.function->call_access_chains.Find(arg)) {
                    auto indices = AbsoluteAccessIndices(caller_variant_params, *chain);
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
                return FnVariant{b.Symbols().New(name), {}};
            });
            caller_variant.calls.Add(call, target_variant.name);
        };

        // Caller function has variants.
        // Build the target variant for each variant of the caller.
        for (auto caller_variant_it : current.function->variants) {
            build_target_variant(caller_variant_it.key, caller_variant_it.value);
        }

        // This call will need to be transformed to call the appropriate variant.
        ctx.Replace(call->Declaration(), [this, call]() { return TransformCall(call); });
        return ast::TraverseAction::Skip;
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

    const ast::CallExpression* TransformCall(const sem::Call* call) {
        utils::Vector<const ast::Expression*, 8> new_args;
        for (auto* arg : call->Arguments()) {
            auto* chain = current.function->call_access_chains.Find(arg);
            if (!chain) {
                // No access chain means the argument is not a pointer that needs transforming.
                new_args.Push(ctx.Clone(arg->Declaration()));
                continue;
            }
            auto full_indices = AbsoluteAccessIndices(*current.variant_params, *chain);
            if (CountDynamicIndices(full_indices) == 0) {
                // Arguments pointers to entirely static data (no dynamic indices) are omitted.
                continue;
            }
            if (auto* arg_ty = DynamicIndexAlias(full_indices)) {
                utils::Vector<const ast::Expression*, 8> dyn_idx_args;
                if (auto* root_param = RootParameter(*chain)) {
                    // TODO: Handle shadowing!
                    Symbol root_param_name;
                    if (auto let = current.function->unshadowed_vars.Find(root_param)) {
                        root_param_name = *let;
                    } else {
                        root_param_name = ctx.Clone(root_param->Declaration()->symbol);
                    }
                    auto& arg_indices = *current.variant_params->Find(root_param);
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
        auto target_variant = current.variant->calls.Find(call);
        return b.Call(*target_variant, std::move(new_args));
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

    void Unshadow(const sem::Variable* variable, const sem::Statement* stmt) {
        if (IsShadowed(variable, stmt)) {
            // Variable is shadowed in the body of the function.
            // Create a pointer alias so this can be safely accessed throughout the
            // function.
            current.function->unshadowed_vars.GetOrCreate(variable, [&] {
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

    bool IsShadowed(const sem::Variable* variable, const sem::Statement* stmt) {
        auto symbol = variable->Declaration()->symbol;
        for (auto p = stmt->Parent(); p != nullptr; p = p->Parent()) {
            if (p->Decls().Find(symbol)) {
                return true;
            }
        }
        return false;
    }

    /// Walks the @p expr, constructing and returning an AccessChain.
    /// @returns an AccessChain.
    AccessChain AccessChainFor(const sem::Expression* expr) {
        // The statement of the incoming expression.
        auto* stmt = expr->Stmt();

        AccessChain access;

        utils::Vector<std::pair<const sem::Expression*, Symbol>, 8> hoists;

        // Walk from the outer-most expression, inwards towards the source variable.
        while (true) {
            enum class Action { kStop, kContinue, kError };
            Action action = Switch(
                expr,  //
                [&](const sem::VariableUser* user) {
                    auto* variable = user->Variable();

                    if (variable->Type()->Is<sem::Pointer>() &&
                        variable->Declaration()->Is<ast::Let>()) {
                        // Found a pointer-let.
                        // Continue traversing from the let initializer.
                        expr = variable->Constructor();
                        return Action::kContinue;
                    }

                    // Global or parameter
                    access.indices.Push(variable);

                    // Ensure that the root variable is accessible from the usage of the final
                    // expression.
                    Unshadow(variable, stmt);

                    // Reached the root variable. Stop traversing.
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
                    access.indices.Push(DynamicIndex{access.dynamic_indices.Length()});
                    auto* idx = a->Index();
                    if (auto* val = idx->ConstantValue()) {
                        access.dynamic_indices.Push(
                            [this, val] { return b.Expr(val->As<AInt>()); });
                    } else {
                        // The index is not a constant value.
                        // In order to ensure that side-effecting expressions are only evaluated
                        // once, and that variables are not shadowed between the index expression
                        // and function calls, hoist the expression to a let.
                        // As access chains can share expressions, we use the hoisted_exprs map to
                        // ensure that we only hoist the expression once.
                        auto hoisted = current.function->hoisted_exprs.GetOrCreate(idx, [&] {
                            auto name = b.Symbols().New();
                            hoists.Push({idx, name});
                            ctx.Replace(idx->Declaration(), [this, name] { return b.Expr(name); });
                            return name;
                        });
                        // The index may be fed to a dynamic index array<u32, N> argument, so the
                        // index expression may need casting to u32.
                        if (idx->UnwrapMaterialize()
                                ->Type()
                                ->UnwrapRef()
                                ->IsAnyOf<sem::U32, sem::AbstractInt>()) {
                            access.dynamic_indices.Push(
                                [this, hoisted] { return b.Expr(hoisted); });
                        } else {
                            access.dynamic_indices.Push(
                                [this, hoisted] { return b.Construct(b.ty.u32(), hoisted); });
                        }
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
                        << "AST: "
                        << (expr && expr->Declaration() ? expr->Declaration()->TypeInfo().name
                                                        : "<null>")
                        << "\n"
                        << "SEM: " << (expr ? expr->TypeInfo().name : "<null>");
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

        for (auto& h : utils::Reverse(hoists)) {
            auto* e = h.first;
            auto n = h.second;
            hoist.InsertBefore(e->Stmt(), [this, e, n] {
                return b.Decl(b.Let(n, ctx.CloneWithoutTransform(e->Declaration())));
            });
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
            b.Alias(symbol, b.ty.array(b.ty.u32(), u32(num_dyn_indices)));
            return symbol;
        });

        return name.IsValid() ? b.ty.type_name(name) : nullptr;
    }

    std::string AccessIndicesName(const AccessIndices& indices) {
        std::stringstream ss;
        const sem::Type* ty = nullptr;
        for (auto& access : indices) {
            if (ty) {
                ss << "_";
            }
            if (auto* const* var = std::get_if<const sem::Variable*>(&access)) {
                ss << ctx.src->Symbols().NameFor((*var)->Declaration()->symbol);
                ty = (*var)->Type()->UnwrapRef()->UnwrapPtr();
            } else if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
                /// The access uses a dynamic (runtime-expression) index.
                ss << "X";
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
            } else {
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
        }
        return ss.str();
    }

    /// Return type of BuildAccessExpr()
    struct ExprType {
        /// The new, post-access expression
        const ast::Expression* expr;
        /// The type of #expr
        const sem::Type* type;
    };

    /// Builds a single access in an access chain, updating the @p expr_type argument.
    /// @param expr_type the current expression and type, updated before returning.
    /// @param access the access to perform on the current expression
    /// @param dynamic_index a function that obtains the i'th dynamic index
    void BuildAccessExpr(ExprType& expr_type,
                         const AccessIndex& access,
                         std::function<const ast::Expression*(size_t)> dynamic_index) {
        if (auto* const* var = std::get_if<const sem::Variable*>(&access)) {
            auto* decl = (*var)->Declaration();
            if (auto let = current.function->unshadowed_vars.Find(*var)) {
                expr_type.expr = b.Deref(*let);
            } else {
                expr_type.expr = b.Expr(ctx.Clone(decl->symbol));
            }
            expr_type.type = (*var)->Type()->UnwrapRef();
            return;
        }

        if (auto* dyn_idx = std::get_if<DynamicIndex>(&access)) {
            /// The access uses a dynamic (runtime-expression) index.
            Switch(
                expr_type.type,  //
                [&](const sem::Array* arr) {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    expr_type.expr = b.IndexAccessor(expr_type.expr, idx);
                    expr_type.type = arr->ElemType();
                },  //
                [&](const sem::Matrix* mat) {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    expr_type.expr = b.IndexAccessor(expr_type.expr, idx);
                    expr_type.type = mat->ColumnType();
                },  //
                [&](const sem::Vector* vec) {
                    auto* idx = dynamic_index(dyn_idx->slot);
                    expr_type.expr = b.IndexAccessor(expr_type.expr, idx);
                    expr_type.type = vec->type();
                },  //
                [&](Default) {
                    TINT_ICE(Transform, b.Diagnostics()) << "unhandled type for access chain: "
                                                         << ctx.src->FriendlyName(expr_type.type);
                });
            return;
        }
        /// The access is a static index.
        auto idx = std::get<u32>(access);
        Switch(
            expr_type.type,  //
            [&](const sem::Struct* str) {
                auto* member = str->Members()[idx];
                expr_type.expr = b.MemberAccessor(expr_type.expr, sym.NameFor(member->Name()));
                expr_type.type = member->Type();
            },  //
            [&](const sem::Array* arr) {
                expr_type.expr = b.IndexAccessor(expr_type.expr, idx);
                expr_type.type = arr->ElemType();
            },  //
            [&](const sem::Matrix* mat) {
                expr_type.expr = b.IndexAccessor(expr_type.expr, idx);
                expr_type.type = mat->ColumnType();
            },  //
            [&](const sem::Vector* vec) {
                expr_type.expr = b.IndexAccessor(expr_type.expr, idx);
                expr_type.type = vec->type();
            },  //
            [&](Default) {
                TINT_ICE(Transform, b.Diagnostics())
                    << "unhandled type for access chain: " << ctx.src->FriendlyName(expr_type.type);
            });
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
