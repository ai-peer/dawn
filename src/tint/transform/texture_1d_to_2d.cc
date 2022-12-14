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

#include "src/tint/transform/texture_1d_to_2d.h"

#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/statement.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::Texture1DTo2D);

using namespace tint::number_suffixes;  // NOLINT

namespace tint::transform {

/// PIMPL state for the transform
struct Texture1DTo2D::State {
    /// The source program
    const Program* const src;
    /// The target program builder
    ProgramBuilder b;
    /// The clone context
    CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};

    /// Constructor
    /// @param program the source program
    explicit State(const Program* program) : src(program) {}

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        auto& sem = src->Sem();

        ctx.ReplaceAll([&](const ast::Variable* v) -> const ast::Variable* {
            auto* type = sem.Get(v->type);
            if (auto* tex = type->As<type::SampledTexture>()) {
                if (tex->dim() == ast::TextureDimension::k1d) {
                    auto* type2d = ctx.dst->create<ast::SampledTexture>(
                        ast::TextureDimension::k2d, CreateASTTypeFor(ctx, tex->type()));
                    if (v->As<ast::Parameter>()) {
                        return ctx.dst->Param(ctx.Clone(v->symbol), type2d,
                                              ctx.Clone(v->attributes));
                    } else {
                        return ctx.dst->Var(ctx.Clone(v->symbol), type2d, ctx.Clone(v->attributes));
                    }
                }
            } else if (auto* storageTex = type->As<type::StorageTexture>()) {
                if (storageTex->dim() == ast::TextureDimension::k1d) {
                    auto* type2d = ctx.dst->create<ast::StorageTexture>(
                        ast::TextureDimension::k2d, storageTex->texel_format(),
                        CreateASTTypeFor(ctx, storageTex->type()), storageTex->access());
                    if (v->As<ast::Parameter>()) {
                        return ctx.dst->Param(ctx.Clone(v->symbol), type2d,
                                              ctx.Clone(v->attributes));
                    } else {
                        return ctx.dst->Var(ctx.Clone(v->symbol), type2d, ctx.Clone(v->attributes));
                    }
                }
            }
            return nullptr;
        });

        ctx.ReplaceAll([&](const ast::CallExpression* c) -> const ast::Expression* {
            auto* call = sem.Get(c)->UnwrapMaterialize()->As<sem::Call>();
            if (!call) {
                return nullptr;
            }
            auto* builtin = call->Target()->As<sem::Builtin>();
            if (!builtin) {
                return nullptr;
            }
            const auto& signature = builtin->Signature();
            auto texture = signature.Parameter(sem::ParameterUsage::kTexture);
            auto* tex = texture->Type()->UnwrapRef()->As<type::Texture>();
            if (tex->dim() != ast::TextureDimension::k1d) {
                return nullptr;
            }

            if (builtin->Type() == sem::BuiltinType::kTextureDimensions) {
                // If this textureDimensions() call is in a CallStatement, we can leave it
                // unmodified since the return value will be dropped on the floor anyway.
                if (call->Stmt()->Declaration()->Is<ast::CallStatement>()) {
                    return nullptr;
                }
                auto* new_call = ctx.CloneWithoutTransform(c);
                return ctx.dst->MemberAccessor(new_call, "x");
            }

            auto coords_index = signature.IndexOf(sem::ParameterUsage::kCoords);
            if (coords_index == -1) {
                return nullptr;
            }

            utils::Vector<const ast::Expression*, 8> args;
            int index = 0;
            for (auto* arg : c->args) {
                if (index == coords_index) {
                    auto* ctype = call->Arguments()[static_cast<size_t>(coords_index)]->Type();
                    auto* coords = c->args[static_cast<size_t>(coords_index)];

                    const ast::LiteralExpression* half = nullptr;
                    if (ctype->is_integer_scalar()) {
                        half = ctx.dst->Expr(0_a);
                    } else {
                        half = ctx.dst->Expr(0.5_a);
                    }
                    args.Push(
                        ctx.dst->vec(CreateASTTypeFor(ctx, ctype), 2u, ctx.Clone(coords), half));
                } else {
                    args.Push(ctx.Clone(arg));
                }
                index++;
            }
            return ctx.dst->Call(ctx.Clone(c->target.name), args);
        });

        ctx.Clone();
        return Program(std::move(b));
    }
};

Texture1DTo2D::Texture1DTo2D() = default;

Texture1DTo2D::~Texture1DTo2D() = default;

Transform::ApplyResult Texture1DTo2D::Apply(const Program* src, const DataMap&, DataMap&) const {
    return State(src).Run();
}

}  // namespace tint::transform
