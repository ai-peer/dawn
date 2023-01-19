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

#include "src/tint/transform/unshadow_core_definitions.h"

#include <string>
#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/builtin_type.h"
#include "src/tint/sem/function.h"
#include "src/tint/type/short_name.h"
#include "src/tint/utils/hashset.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::UnshadowCoreDefinitions);

namespace tint::transform {

namespace {

bool IsCoreDefinition(std::string_view name) {
    return sem::ParseBuiltinType(name) != sem::BuiltinType::kNone ||
           type::ParseShortName(name) != type::ShortName::kUndefined;
}

}  // namespace

UnshadowCoreDefinitions::UnshadowCoreDefinitions() = default;

UnshadowCoreDefinitions::~UnshadowCoreDefinitions() = default;

Transform::ApplyResult UnshadowCoreDefinitions::Apply(const Program* src,
                                                      const DataMap&,
                                                      DataMap&) const {
    ProgramBuilder b;
    CloneContext ctx(&b, src, /* auto_clone_symbols */ true);

    bool made_changes = false;
    utils::Hashmap<Symbol, Symbol, 8> renamed_types;
    for (auto* decl : src->ASTNodes().Objects()) {
        Switch(
            decl,  //
            [&](const ast::Alias* alias) {
                if (auto name = src->Symbols().NameFor(alias->name); IsCoreDefinition(name)) {
                    auto new_name = b.Symbols().New(name);
                    renamed_types.Add(alias->name, new_name);
                    ctx.Replace(alias, [&ctx, alias, new_name] {
                        return ctx.dst->create<ast::Alias>(new_name, ctx.Clone(alias->type));
                    });
                    made_changes = true;
                }
            },
            [&](const ast::Struct* str) {
                if (auto name = src->Symbols().NameFor(str->name); IsCoreDefinition(name)) {
                    auto new_name = b.Symbols().New(name);
                    renamed_types.Add(str->name, new_name);
                    ctx.Replace(str, [&ctx, str, new_name] {
                        auto members = ctx.Clone(str->members);
                        auto attrs = ctx.Clone(str->attributes);
                        return ctx.dst->create<ast::Struct>(new_name, std::move(members),
                                                            std::move(attrs));
                    });
                    made_changes = true;
                }
            },
            [&](const ast::Variable* variable) {
                if (auto name = src->Symbols().NameFor(variable->symbol); IsCoreDefinition(name)) {
                    auto new_name = b.Symbols().New(name);
                    ctx.Replace(variable, [&ctx, variable, new_name] {
                        auto* type = ctx.Clone(variable->type);
                        auto* initializer = ctx.Clone(variable->initializer);
                        auto attributes = ctx.Clone(variable->attributes);
                        return Switch(
                            variable,  //
                            [&](const ast::Var* var) {
                                return ctx.dst->Var(new_name, type, initializer,
                                                    var->declared_address_space,
                                                    std::move(attributes));
                            },
                            [&](const ast::Let*) {
                                return ctx.dst->Let(new_name, type, initializer,
                                                    std::move(attributes));
                            },
                            [&](const ast::Override*) {
                                return ctx.dst->Override(new_name, type, initializer,
                                                         std::move(attributes));
                            },
                            [&](const ast::Const*) {
                                return ctx.dst->Const(new_name, type, initializer,
                                                      std::move(attributes));
                            },
                            [&](const ast::Parameter*) {
                                return ctx.dst->Param(new_name, type, std::move(attributes));
                            });
                    });
                    for (auto* user : src->Sem().Get(variable)->Users()) {
                        ctx.Replace(user->Declaration(),
                                    [&ctx, new_name] { return ctx.dst->Expr(new_name); });
                    }
                    made_changes = true;
                }
            },
            [&](const ast::Function* fn) {
                if (auto name = src->Symbols().NameFor(fn->symbol); IsCoreDefinition(name)) {
                    auto new_name = b.Symbols().New(name);
                    ctx.Replace(fn, [&ctx, fn, new_name] {
                        auto params = ctx.Clone(fn->params);
                        auto* body = ctx.Clone(fn->body);
                        auto* ret_ty = ctx.Clone(fn->return_type);
                        auto attrs = ctx.Clone(fn->attributes);
                        auto ret_ty_attrs = ctx.Clone(fn->return_type_attributes);
                        return ctx.dst->create<ast::Function>(new_name, std::move(params), ret_ty,
                                                              body, std::move(attrs),
                                                              std::move(ret_ty_attrs));
                    });
                    for (auto* callsite : src->Sem().Get(fn)->CallSites()) {
                        auto* call = callsite->Declaration();
                        ctx.Replace(call, [&ctx, call, new_name] {
                            auto args = ctx.Clone(call->args);
                            return ctx.dst->Call(new_name, std::move(args));
                        });
                    }
                    made_changes = true;
                }
            });
    }

    if (!made_changes) {
        return SkipTransform;
    }

    if (!renamed_types.IsEmpty()) {
        ctx.ReplaceAll([&](const ast::TypeName* type_name) -> const ast::TypeName* {
            if (auto new_name = renamed_types.Get(type_name->name)) {
                return ctx.dst->ty.type_name(*new_name);
            }
            return nullptr;  // Just clone
        });
        ctx.ReplaceAll([&](const ast::CallExpression* expr) -> const ast::CallExpression* {
            if (auto* ident = expr->target.name) {
                if (auto new_name = renamed_types.Get(ident->symbol)) {
                    return ctx.dst->Call(*new_name);
                }
            }
            return nullptr;  // Just clone
        });
    }

    ctx.Clone();
    return Program(std::move(b));
}

}  // namespace tint::transform
