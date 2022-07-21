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

#include "src/tint/transform/substitute_override.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::SubstituteOverride);
TINT_INSTANTIATE_TYPEINFO(tint::transform::SubstituteOverride::Config);

namespace tint::transform {

SubstituteOverride::SubstituteOverride() = default;

SubstituteOverride::~SubstituteOverride() = default;

bool SubstituteOverride::ShouldRun(const Program* program, const DataMap&) const {
    for (auto* node : program->ASTNodes().Objects()) {
        if (node->Is<ast::Override>()) {
            return true;
        }
    }
    return false;
}

void SubstituteOverride::Run(CloneContext& ctx, const DataMap& config, DataMap&) const {
    const auto* data = config.Get<Config>();
    if (!data) {
        ctx.dst->Diagnostics().add_error(diag::System::Transform,
                                         "Missing override substitution data");
        return;
    }

    ctx.ReplaceAll([&](const ast::Override* w) -> const ast::Const* {
        auto ident = w->Identifier(ctx.src->Symbols());

        // No replacement provided, just clone the override node
        auto iter = data->map.find(ident);
        if (iter == data->map.end()) {
            return nullptr;
        }
        auto value = iter->second;

        auto src = ctx.Clone(w->source);
        auto sym = ctx.Clone(w->symbol);
        auto* ty = ctx.Clone(w->type);

        ast::Expression* ctor = nullptr;
        // If a type was provided, use that for the cast expression, otherwise
        // look at the constructor which has to be a scalar and use that type.
        if (ty) {
            Switch(
                ty,
                [&](const ast::Bool*) {
                    ctor = ctx.dst->create<ast::BoolLiteralExpression>(value != 0.0);
                },
                [&](const ast::I32*) {
                    ctor = ctx.dst->create<ast::IntLiteralExpression>(
                        static_cast<int64_t>(value), ast::IntLiteralExpression::Suffix::kI);
                },
                [&](const ast::U32*) {
                    ctor = ctx.dst->create<ast::IntLiteralExpression>(
                        static_cast<int64_t>(value), ast::IntLiteralExpression::Suffix::kU);
                },
                [&](const ast::F32*) {
                    ctor = ctx.dst->create<ast::FloatLiteralExpression>(
                        value, ast::FloatLiteralExpression::Suffix::kF);
                },
                [&](const ast::F16*) {
                    ctor = ctx.dst->create<ast::FloatLiteralExpression>(
                        value, ast::FloatLiteralExpression::Suffix::kH);
                });
        } else {
            Switch(
                w->constructor,
                [&](const ast::FloatLiteralExpression* f) {
                    ctor = ctx.dst->create<ast::FloatLiteralExpression>(value, f->suffix);
                },
                [&](const ast::IntLiteralExpression* i) {
                    ctor = ctx.dst->create<ast::IntLiteralExpression>(static_cast<int64_t>(value),
                                                                      i->suffix);
                },
                [&](const ast::BoolLiteralExpression*) {
                    ctor = ctx.dst->create<ast::BoolLiteralExpression>(value != 0.0);
                });
        }

        if (!ctor) {
            ctx.dst->Diagnostics().add_error(diag::System::Transform,
                                             "Failed to create override expression");
            return nullptr;
        }

        return ctx.dst->Const(src, sym, ty, ctor);
    });

    ctx.Clone();
}

SubstituteOverride::Config::Config() = default;

SubstituteOverride::Config::Config(const Config&) = default;

SubstituteOverride::Config::~Config() = default;

SubstituteOverride::Config& SubstituteOverride::Config::operator=(const Config&) = default;

}  // namespace tint::transform
