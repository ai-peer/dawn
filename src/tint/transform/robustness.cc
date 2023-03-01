// Copyright 2020 The Tint Authors.
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

#include "src/tint/transform/robustness.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/block_statement.h"
#include "src/tint/sem/builtin.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/index_accessor_expression.h"
#include "src/tint/sem/load.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/statement.h"
#include "src/tint/sem/value_expression.h"
#include "src/tint/transform/utils/hoist_to_decl_before.h"
#include "src/tint/type/reference.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::Robustness);
TINT_INSTANTIATE_TYPEINFO(tint::transform::Robustness::Config);

using namespace tint::number_suffixes;  // NOLINT

namespace tint::transform {

/// PIMPL state for the transform
struct Robustness::State {
    /// Constructor
    /// @param p the source program
    /// @param c the transform config
    State(const Program* p, Config&& c) : src(p), cfg(std::move(c)) {}

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        for (auto* node : ctx.src->ASTNodes().Objects()) {
            Switch(
                node,  //
                [&](const ast::IndexAccessorExpression* e) {
                    switch (cfg.handle_action) {
                        case Action::kPredicate:
                            PredicateIndexAccessor(e);
                            break;
                        case Action::kClamp:
                            ClampIndexAccessor(e);
                            break;
                        case Action::kIgnore:
                            break;
                    }
                },
                [&](const ast::AccessorExpression* e) {
                    if (auto pred = predicates.Get(e->object)) {
                        predicates.Add(e, *pred);
                    }
                },
                [&](const ast::UnaryOpExpression* e) {
                    if (auto pred = predicates.Get(e->expr)) {
                        predicates.Add(e, *pred);
                    }
                },
                [&](const ast::AssignmentStatement* s) {
                    if (auto pred = predicates.Get(s->lhs)) {
                        // Assignment expression is predicated
                        ctx.Replace(
                            s, [=] { return b.If(*pred, b.Block(ctx.CloneWithoutTransform(s))); });
                    }
                },
                [&](const ast::CompoundAssignmentStatement* s) {
                    if (auto pred = predicates.Get(s->lhs)) {
                        // Assignment expression is predicated
                        ctx.Replace(
                            s, [=] { return b.If(*pred, b.Block(ctx.CloneWithoutTransform(s))); });
                    }
                },
                [&](const ast::CallExpression* e) {
                    if (auto* call = sem.Get<sem::Call>(e)) {
                        if (auto* builtin = call->Target()->As<sem::Builtin>()) {
                            if (IsTextureBuiltinThatRequiresRobustness(builtin->Type())) {
                                switch (cfg.handle_action) {
                                    case Action::kPredicate:
                                        PredicateTextureBuiltin(call, builtin);
                                        break;
                                    case Action::kClamp:
                                        ClampTextureBuiltin(call, builtin);
                                        break;
                                    case Action::kIgnore:
                                        break;
                                }
                            }
                        }
                    }
                });

            if (auto* expr = node->As<ast::Expression>()) {
                if (auto pred = predicates.Get(expr)) {
                    // Expression is predicated
                    auto* sem_expr = sem.GetVal(expr);
                    if (!sem_expr->Type()->Is<type::Reference>()) {
                        // Expression is not of a reference type
                        auto pred_load = b.Symbols().New("predicated_load");
                        hoist.InsertBefore(sem_expr->Stmt(), [=] {
                            auto ty = CreateASTTypeFor(ctx, sem_expr->Type());
                            return b.Decl(b.Var(pred_load, ty));
                        });
                        hoist.InsertBefore(sem_expr->Stmt(), [=] {
                            return b.If(*pred, b.Block(b.Assign(pred_load,
                                                                ctx.CloneWithoutTransform(expr))));
                        });
                        ctx.Replace(expr, [=] { return b.Expr(pred_load); });
                    }
                }
            }
        }

        ctx.Clone();
        return Program(std::move(b));
    }

  private:
    /// The source program
    const Program* const src;
    /// The transform's config
    Config cfg;
    /// The target program builder
    ProgramBuilder b{};
    /// The clone context
    CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};
    /// Helper for hoisting declarations
    HoistToDeclBefore hoist{ctx};
    /// Alias to the source program's semantic info
    const sem::Info& sem = ctx.src->Sem();
    /// Map of expression to predicate condition
    utils::Hashmap<const ast::Expression*, Symbol, 32> predicates{};

    const ast::Expression* DynamicLimitFor(const ast::IndexAccessorExpression* expr) {
        auto* expr_sem = sem.Get(expr)->Unwrap()->As<sem::IndexAccessorExpression>();
        auto* obj_type = expr_sem->Object()->Type();
        return Switch(
            obj_type->UnwrapRef(),  //
            [&](const type::Vector* vec) -> const ast::Expression* {
                if (expr_sem->Index()->ConstantValue() || expr_sem->Index()->Is<sem::Swizzle>()) {
                    // Index and size is constant.
                    // Validation will have rejected any OOB accesses.
                    return nullptr;
                }
                return b.Expr(u32(vec->Width() - 1u));
            },
            [&](const type::Matrix* mat) -> const ast::Expression* {
                if (expr_sem->Index()->ConstantValue()) {
                    // Index and size is constant.
                    // Validation will have rejected any OOB accesses.
                    return nullptr;
                }
                return b.Expr(u32(mat->columns() - 1u));
            },
            [&](const type::Array* arr) -> const ast::Expression* {
                if (arr->Count()->Is<type::RuntimeArrayCount>()) {
                    // Size is unknown until runtime.
                    // Must clamp, even if the index is constant.

                    auto* arr_ptr = b.AddressOf(ctx.Clone(expr->object));
                    return b.Sub(b.Call(sem::BuiltinType::kArrayLength, arr_ptr), 1_u);
                }
                if (auto count = arr->ConstantCount()) {
                    if (expr_sem->Index()->ConstantValue()) {
                        // Index and size is constant.
                        // Validation will have rejected any OOB accesses.
                        return nullptr;
                    }
                    return b.Expr(u32(count.value() - 1u));
                }
                // Note: Don't be tempted to use the array override variable as an expression here,
                // the name might be shadowed!
                b.Diagnostics().add_error(diag::System::Transform,
                                          type::Array::kErrExpectedConstantCount);
                return nullptr;
            },
            [&](Default) -> const ast::Expression* {
                TINT_ICE(Transform, b.Diagnostics())
                    << "unhandled object type in robustness of array index: "
                    << src->FriendlyName(obj_type->UnwrapRef());
                return nullptr;
            });
    }

    void PredicateIndexAccessor(const ast::IndexAccessorExpression* expr) {
        auto* max = DynamicLimitFor(expr);
        if (!max) {
            // robustness is not required
            // Just propagate predicate from object
            if (auto pred = predicates.Get(expr->object)) {
                predicates.Add(expr, *pred);
            }
            return;
        }

        auto* sem_expr = sem.Get(expr);
        auto* stmt = sem_expr->Stmt();
        auto obj_pred = *predicates.GetOrZero(expr->object);

        auto idx_let = b.Symbols().New("index");
        auto pred = b.Symbols().New("predicate");

        hoist.InsertBefore(
            stmt, [=] { return b.Decl(b.Let(idx_let, ctx.CloneWithoutTransform(expr->index))); });

        ctx.Replace(expr->index, [=] { return b.Expr(idx_let); });

        hoist.InsertBefore(stmt, [=] {
            auto* cond = b.LessThan(b.Call<u32>(b.Expr(idx_let)), max);
            if (obj_pred.IsValid()) {
                cond = b.And(b.Expr(obj_pred), cond);
            }
            return b.Decl(b.Let(pred, cond));
        });

        predicates.Add(expr, pred);
    }

    /// Apply bounds clamping to array, vector and matrix indexing
    /// @param expr the array, vector or matrix index expression
    void ClampIndexAccessor(const ast::IndexAccessorExpression* expr) {
        auto* max = DynamicLimitFor(expr);
        if (!max) {
            return;  // robustness is not required
        }

        auto* expr_sem = sem.Get(expr)->Unwrap()->As<sem::IndexAccessorExpression>();
        ctx.Replace(expr, [=] {
            auto idx = ctx.Clone(expr->index);
            if (expr_sem->Index()->Type()->is_signed_integer_scalar()) {
                idx = b.Call<u32>(idx);  // u32(idx)
            }
            auto* clamped_idx = b.Call(sem::BuiltinType::kMin, idx, max);
            auto idx_src = ctx.Clone(expr->source);
            auto* idx_obj = ctx.Clone(expr->object);
            return b.IndexAccessor(idx_src, idx_obj, clamped_idx);
        });
    }

    void PredicateTextureBuiltin(const sem::Call* call, const sem::Builtin* builtin) {
        auto* expr = call->Declaration();
        auto* stmt = call->Stmt();

        // Indices of the mandatory texture and coords parameters, and the optional
        // array and level parameters.
        auto& signature = builtin->Signature();
        auto texture_arg_idx = signature.IndexOf(sem::ParameterUsage::kTexture);
        auto coords_arg_idx = signature.IndexOf(sem::ParameterUsage::kCoords);
        auto array_arg_idx = signature.IndexOf(sem::ParameterUsage::kArrayIndex);
        auto level_arg_idx = signature.IndexOf(sem::ParameterUsage::kLevel);

        auto* texture_arg = expr->args[static_cast<size_t>(texture_arg_idx)];
        auto* coords_arg = expr->args[static_cast<size_t>(coords_arg_idx)];
        auto* coords_ty = builtin->Parameters()[static_cast<size_t>(coords_arg_idx)]->Type();

        // If the level is provided, then we need to clamp this. As the level is used by
        // textureDimensions() and the texture[Load|Store]() calls, we need to clamp both usages.
        Symbol level, level_clamped;
        if (level_arg_idx >= 0) {
            level = b.Symbols().New("level");
            const auto* arg = expr->args[static_cast<size_t>(level_arg_idx)];
            hoist.InsertBefore(stmt, [=] {
                return b.Decl(b.Let(level, CastToUnsigned(ctx.CloneWithoutTransform(arg), 1u)));
            });

            level_clamped = b.Symbols().New("level_clamped");
            hoist.InsertBefore(stmt, [=] {
                const auto* num_levels =
                    b.Call(sem::BuiltinType::kTextureNumLevels, ctx.Clone(texture_arg));
                const auto* max = b.Sub(num_levels, 1_a);
                return b.Decl(
                    b.Let(level_clamped, b.Call(sem::BuiltinType::kMin, b.Expr(level), max)));
            });
            ctx.Replace(arg, [=] { return b.Expr(level); });
        }

        const ast::Expression* predicate = nullptr;

        {  //  all(coords < textureDimensions(texture))
            auto coords = b.Symbols().New("coords");
            auto* dimensions =
                level_clamped.IsValid()
                    ? b.Call(sem::BuiltinType::kTextureDimensions, ctx.Clone(texture_arg),
                             level_clamped)
                    : b.Call(sem::BuiltinType::kTextureDimensions, ctx.Clone(texture_arg));
            hoist.InsertBefore(stmt, [=] {  //
                return b.Decl(b.Let(coords, ctx.CloneWithoutTransform(coords_arg)));
            });
            ctx.Replace(coords_arg, [=] { return b.Expr(coords); });
            auto* c = CastToUnsigned(b.Expr(coords), WidthOf(coords_ty));
            predicate = b.Call(sem::BuiltinType::kAll, b.LessThan(c, dimensions));
        }

        if (level_arg_idx >= 0) {  //  level_idx < textureNumLevels(texture)
            auto* num_levels = b.Call(sem::BuiltinType::kTextureNumLevels, ctx.Clone(texture_arg));
            predicate = b.And(predicate, b.LessThan(level, num_levels));
        }

        if (array_arg_idx >= 0) {  //  array_idx < textureNumLayers(texture)
            auto* arg = expr->args[static_cast<size_t>(array_arg_idx)];
            auto* num_layers = b.Call(sem::BuiltinType::kTextureNumLayers, ctx.Clone(texture_arg));
            auto array_idx = b.Symbols().New("array_idx");
            hoist.InsertBefore(stmt, [=] {  //
                return b.Decl(b.Let(array_idx, ctx.CloneWithoutTransform(arg)));
            });
            ctx.Replace(arg, [=] { return b.Expr(array_idx); });
            auto* c = CastToUnsigned(b.Expr(array_idx), 1u);
            predicate = b.And(predicate, b.LessThan(c, num_layers));
        }

        if (builtin->Type() == sem::BuiltinType::kTextureStore) {
            hoist.Replace(stmt, [=] {
                return b.If(predicate, b.Block(ctx.CloneWithoutTransform(stmt->Declaration())));
            });
        } else {
            auto value = b.Symbols().New("texture_load");
            hoist.InsertBefore(
                stmt, [=] { return b.Decl(b.Var(value, CreateASTTypeFor(ctx, call->Type()))); });
            hoist.InsertBefore(stmt, [=] {
                return b.If(predicate, b.Block(b.Assign(value, ctx.CloneWithoutTransform(expr))));
            });
            ctx.Replace(expr, b.Expr(value));
        }
    }

    /// Apply bounds clamping to the coordinates, array index and level arguments of the
    /// `textureLoad()` and `textureStore()` builtins.
    void ClampTextureBuiltin(const sem::Call* call, const sem::Builtin* builtin) {
        auto* expr = call->Declaration();
        auto* stmt = call->Stmt();

        // Indices of the mandatory texture and coords parameters, and the optional
        // array and level parameters.
        auto& signature = builtin->Signature();
        auto texture_arg_idx = signature.IndexOf(sem::ParameterUsage::kTexture);
        auto coords_arg_idx = signature.IndexOf(sem::ParameterUsage::kCoords);
        auto array_arg_idx = signature.IndexOf(sem::ParameterUsage::kArrayIndex);
        auto level_arg_idx = signature.IndexOf(sem::ParameterUsage::kLevel);

        auto* texture_arg = expr->args[static_cast<size_t>(texture_arg_idx)];
        auto* coords_arg = expr->args[static_cast<size_t>(coords_arg_idx)];
        auto* coords_ty = builtin->Parameters()[static_cast<size_t>(coords_arg_idx)]->Type();

        // If the level is provided, then we need to clamp this. As the level is used by
        // textureDimensions() and the texture[Load|Store]() calls, we need to clamp both usages.
        Symbol level;
        if (level_arg_idx >= 0) {
            const auto* arg = expr->args[static_cast<size_t>(level_arg_idx)];
            level = b.Symbols().New("level");
            hoist.InsertBefore(stmt, [=] {
                const auto* num_levels =
                    b.Call(sem::BuiltinType::kTextureNumLevels, ctx.Clone(texture_arg));
                const auto* max = b.Sub(num_levels, 1_a);
                return b.Decl(
                    b.Let(level, b.Call(sem::BuiltinType::kMin,
                                        b.Call<u32>(ctx.CloneWithoutTransform(arg)), max)));
            });
            ctx.Replace(arg, b.Expr(level));
        }

        // Clamp the coordinates argument
        {
            const auto* target_ty = coords_ty;
            const auto width = WidthOf(target_ty);
            const auto* dimensions =
                level.IsValid()
                    ? b.Call(sem::BuiltinType::kTextureDimensions, ctx.Clone(texture_arg), level)
                    : b.Call(sem::BuiltinType::kTextureDimensions, ctx.Clone(texture_arg));

            // dimensions is u32 or vecN<u32>
            const auto* unsigned_max = b.Sub(dimensions, ScalarOrVec(b.Expr(1_a), width));
            if (target_ty->is_signed_integer_scalar_or_vector()) {
                const auto* zero = ScalarOrVec(b.Expr(0_a), width);
                const auto* signed_max = CastToSigned(unsigned_max, width);
                ctx.Replace(coords_arg, b.Call(sem::BuiltinType::kClamp, ctx.Clone(coords_arg),
                                               zero, signed_max));
            } else {
                ctx.Replace(coords_arg,
                            b.Call(sem::BuiltinType::kMin, ctx.Clone(coords_arg), unsigned_max));
            }
        }

        // Clamp the array_index argument, if provided
        if (array_arg_idx >= 0) {
            auto* target_ty = builtin->Parameters()[static_cast<size_t>(array_arg_idx)]->Type();
            auto* arg = expr->args[static_cast<size_t>(array_arg_idx)];
            auto* num_layers = b.Call(sem::BuiltinType::kTextureNumLayers, ctx.Clone(texture_arg));

            const auto* unsigned_max = b.Sub(num_layers, 1_a);
            if (target_ty->is_signed_integer_scalar()) {
                const auto* signed_max = CastToSigned(unsigned_max, 1u);
                ctx.Replace(arg, b.Call(sem::BuiltinType::kClamp, ctx.Clone(arg), 0_a, signed_max));
            } else {
                ctx.Replace(arg, b.Call(sem::BuiltinType::kMin, ctx.Clone(arg), unsigned_max));
            }
        }
    }

    /// @return true if the given action is enabled for any address space
    bool HasAction(Action action) const {
        return action == cfg.function_action ||       //
               action == cfg.handle_action ||         //
               action == cfg.private_action ||        //
               action == cfg.push_constant_action ||  //
               action == cfg.storage_action ||        //
               action == cfg.uniform_action ||        //
               action == cfg.workgroup_action;
    }

    Action ActionFor(const sem::ValueExpression* expr) {
        return Switch(
            expr->Type(),  //
            [&](const type::Reference* t) { return ActionFor(t->AddressSpace()); },
            [&](Default) { return cfg.function_action; });
    }

    /// @return the robustness action for an OOB access in the address space @p address_space
    Action ActionFor(builtin::AddressSpace address_space) {
        switch (address_space) {
            case builtin::AddressSpace::kFunction:
                return cfg.function_action;
            case builtin::AddressSpace::kHandle:
                return cfg.handle_action;
            case builtin::AddressSpace::kPrivate:
                return cfg.private_action;
            case builtin::AddressSpace::kPushConstant:
                return cfg.push_constant_action;
            case builtin::AddressSpace::kStorage:
                return cfg.storage_action;
            case builtin::AddressSpace::kUniform:
                return cfg.uniform_action;
            case builtin::AddressSpace::kWorkgroup:
                return cfg.workgroup_action;
            default:
                break;
        }
        TINT_UNREACHABLE(Transform, b.Diagnostics()) << "unhandled address space" << address_space;
        return Action::kDefault;
    }

    /// @param type builtin type
    /// @returns true if the given builtin is a texture function that requires robustness checks.
    bool IsTextureBuiltinThatRequiresRobustness(sem::BuiltinType type) const {
        return type == sem::BuiltinType::kTextureLoad || type == sem::BuiltinType::kTextureStore;
    }

    static uint32_t WidthOf(const type::Type* ty) {
        if (auto* vec = ty->As<type::Vector>()) {
            return vec->Width();
        }
        return 1u;
    }

    ast::Type ScalarOrVecTy(ast::Type scalar, uint32_t width) const {
        if (width > 1) {
            return b.ty.vec(scalar, width);
        }
        return scalar;
    }

    const ast::Expression* ScalarOrVec(const ast::Expression* scalar, uint32_t width) {
        if (width > 1) {
            return b.Call(b.ty.vec<Infer>(width), scalar);
        }
        return scalar;
    }

    const ast::CallExpression* CastToSigned(const ast::Expression* val, uint32_t width) {
        return b.Call(ScalarOrVecTy(b.ty.i32(), width), val);
    }

    const ast::CallExpression* CastToUnsigned(const ast::Expression* val, uint32_t width) {
        return b.Call(ScalarOrVecTy(b.ty.u32(), width), val);
    }
};

Robustness::Config::Config() = default;
Robustness::Config::Config(const Config&) = default;
Robustness::Config::~Config() = default;
Robustness::Config& Robustness::Config::operator=(const Config&) = default;

Robustness::Robustness() = default;
Robustness::~Robustness() = default;

Transform::ApplyResult Robustness::Apply(const Program* src,
                                         const DataMap& inputs,
                                         DataMap&) const {
    Config cfg;
    if (auto* cfg_data = inputs.Get<Config>()) {
        cfg = *cfg_data;
    }

    return State{src, std::move(cfg)}.Run();
}

}  // namespace tint::transform
