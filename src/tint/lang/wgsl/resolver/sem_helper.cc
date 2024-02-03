// Copyright 2022 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/wgsl/resolver/sem_helper.h"

#include "src/tint/lang/wgsl/resolver/incomplete_type.h"
#include "src/tint/lang/wgsl/resolver/unresolved_identifier.h"
#include "src/tint/lang/wgsl/sem/builtin_enum_expression.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/function_expression.h"
#include "src/tint/lang/wgsl/sem/type_expression.h"
#include "src/tint/lang/wgsl/sem/value_expression.h"
#include "src/tint/utils/rtti/switch.h"
#include "src/tint/utils/text/text_styles.h"

namespace tint::resolver {

SemHelper::SemHelper(ProgramBuilder* builder) : builder_(builder) {}

SemHelper::~SemHelper() = default;

std::string SemHelper::TypeNameOf(const core::type::Type* ty) const {
    return RawTypeNameOf(ty->UnwrapRef());
}

std::string SemHelper::RawTypeNameOf(const core::type::Type* ty) const {
    return ty->FriendlyName();
}

core::type::Type* SemHelper::TypeOf(const ast::Expression* expr) const {
    auto* sem = GetVal(expr);
    return sem ? const_cast<core::type::Type*>(sem->Type()) : nullptr;
}

sem::TypeExpression* SemHelper::AsTypeExpression(sem::Expression* expr) const {
    if (TINT_UNLIKELY(!expr)) {
        return nullptr;
    }

    auto* ty_expr = expr->As<sem::TypeExpression>();
    if (TINT_UNLIKELY(!ty_expr)) {
        ErrorUnexpectedExprKind(expr, "type");
        return nullptr;
    }

    auto* type = ty_expr->Type();
    if (auto* incomplete = type->As<IncompleteType>(); TINT_UNLIKELY(incomplete)) {
        AddError(StyledText{} << "expected '<' for '" << incomplete->builtin << "'",
                 expr->Declaration()->source.End());
        return nullptr;
    }

    return ty_expr;
}

StyledText SemHelper::Describe(const sem::Expression* expr) const {
    StyledText text;
    TINT_DEFER(text << TextStyle{});

    Switch(
        expr,  //
        [&](const sem::VariableUser* var_expr) {
            auto* variable = var_expr->Variable()->Declaration();
            auto name = variable->name->symbol.Name();
            auto* kind = Switch(
                variable,                                            //
                [&](const ast::Var*) { return "var"; },              //
                [&](const ast::Let*) { return "let"; },              //
                [&](const ast::Const*) { return "const"; },          //
                [&](const ast::Parameter*) { return "parameter"; },  //
                [&](const ast::Override*) { return "override"; },    //
                [&](Default) { return "variable"; });
            text << TINT_TS_CODE_VARIABLE(kind << " " << name);
        },
        [&](const sem::ValueExpression* val_expr) {
            text << "value of type " << TINT_TS_CODE_TYPE(val_expr->Type()->FriendlyName());
        },
        [&](const sem::TypeExpression* ty_expr) {
            text << "type " << TINT_TS_CODE_TYPE(ty_expr->Type()->FriendlyName());
        },
        [&](const sem::FunctionExpression* fn_expr) {
            auto* fn = fn_expr->Function()->Declaration();
            text << "function " << TINT_TS_CODE_FN(fn->name->symbol.Name());
        },
        [&](const sem::BuiltinEnumExpression<wgsl::BuiltinFn>* fn) {
            text << "builtin function " << TINT_TS_CODE_FN(fn->Value());
        },
        [&](const sem::BuiltinEnumExpression<core::Access>* access) {
            text << "access " << TINT_TS_CODE(access->Value());
        },
        [&](const sem::BuiltinEnumExpression<core::AddressSpace>* addr) {
            text << "address space " << TINT_TS_CODE(addr->Value());
        },
        [&](const sem::BuiltinEnumExpression<core::BuiltinValue>* builtin) {
            text << "builtin value " << TINT_TS_CODE(builtin->Value());
        },
        [&](const sem::BuiltinEnumExpression<core::InterpolationSampling>* fmt) {
            text << "interpolation sampling " << TINT_TS_CODE(fmt->Value());
        },
        [&](const sem::BuiltinEnumExpression<core::InterpolationType>* fmt) {
            text << "interpolation type " << TINT_TS_CODE(fmt->Value());
        },
        [&](const sem::BuiltinEnumExpression<core::TexelFormat>* fmt) {
            text << "texel format " << TINT_TS_CODE(fmt->Value());
        },
        [&](const UnresolvedIdentifier* ui) {
            auto name = ui->Identifier()->identifier->symbol.Name();
            text << "unresolved identifier " << TINT_TS_CODE(name);
        },  //
        TINT_ICE_ON_NO_MATCH);
    return text;
}

void SemHelper::ErrorUnexpectedExprKind(
    const sem::Expression* expr,
    std::string_view wanted,
    tint::Slice<const std::string_view> suggestions /* = Empty */) const {
    if (auto* ui = expr->As<UnresolvedIdentifier>()) {
        auto* ident = ui->Identifier();
        auto name = ident->identifier->symbol.Name();
        AddError(StyledText{} << "unresolved " << wanted << " " << TINT_TS_CODE(name),
                 ident->source);
        if (!suggestions.IsEmpty()) {
            // Filter out suggestions that have a leading underscore.
            Vector<std::string_view, 8> filtered;
            for (auto str : suggestions) {
                if (str[0] != '_') {
                    filtered.Push(str);
                }
            }
            StyledText msg;
            tint::SuggestAlternatives(name, filtered.Slice(), msg);
            AddNote(msg, ident->source);
        }
        return;
    }

    AddError(StyledText{} << "cannot use " << Describe(expr) << " as " << wanted,
             expr->Declaration()->source);
    NoteDeclarationSource(expr->Declaration());
}

void SemHelper::ErrorExpectedValueExpr(const sem::Expression* expr) const {
    ErrorUnexpectedExprKind(expr, "value");
    if (auto* ident = expr->Declaration()->As<ast::IdentifierExpression>()) {
        if (expr->IsAnyOf<sem::FunctionExpression, sem::TypeExpression,
                          sem::BuiltinEnumExpression<wgsl::BuiltinFn>>()) {
            AddNote(StyledText{} << "are you missing '()'?", ident->source.End());
        }
    }
}

void SemHelper::NoteDeclarationSource(const ast::Node* node) const {
    if (!node) {
        return;
    }

    Switch(
        Get(node),  //
        [&](const sem::VariableUser* var_expr) { node = var_expr->Variable()->Declaration(); },
        [&](const sem::TypeExpression* ty_expr) {
            Switch(ty_expr->Type(),  //
                   [&](const sem::Struct* s) { node = s->Declaration(); });
        },
        [&](const sem::FunctionExpression* fn_expr) { node = fn_expr->Function()->Declaration(); });

    Switch(
        node,
        [&](const ast::Struct* n) {
            AddNote(StyledText{} << TINT_TS_CODE_TYPE("struct " << n->name->symbol.Name())
                                 << " declared here",
                    n->source);
        },
        [&](const ast::Alias* n) {
            AddNote(StyledText{} << TINT_TS_CODE_TYPE("alias " << n->name->symbol.Name())
                                 << " declared here",
                    n->source);
        },
        [&](const ast::Var* n) {
            AddNote(StyledText{} << TINT_TS_CODE_VARIABLE("var " << n->name->symbol.Name())
                                 << " declared here",
                    n->source);
        },
        [&](const ast::Let* n) {
            AddNote(StyledText{} << TINT_TS_CODE_VARIABLE("let " << n->name->symbol.Name())
                                 << " declared here",
                    n->source);
        },
        [&](const ast::Override* n) {
            AddNote(StyledText{} << TINT_TS_CODE_VARIABLE("override " << n->name->symbol.Name())
                                 << " declared here",
                    n->source);
        },
        [&](const ast::Const* n) {
            AddNote(StyledText{} << TINT_TS_CODE_VARIABLE("const " << n->name->symbol.Name())
                                 << " declared here",
                    n->source);
        },
        [&](const ast::Parameter* n) {
            AddNote(StyledText{} << "parameter " << TINT_TS_CODE_VARIABLE(n->name->symbol.Name())
                                 << " declared here",
                    n->source);
        },
        [&](const ast::Function* n) {
            AddNote(StyledText{} << "function " << TINT_TS_CODE_FN(n->name->symbol.Name())
                                 << " declared here",
                    n->source);
        });
}

void SemHelper::AddError(const StyledText& msg, const Source& source) const {
    builder_->Diagnostics().AddError(diag::System::Resolver, msg, source);
}

void SemHelper::AddWarning(const StyledText& msg, const Source& source) const {
    builder_->Diagnostics().AddWarning(diag::System::Resolver, msg, source);
}

void SemHelper::AddNote(const StyledText& msg, const Source& source) const {
    builder_->Diagnostics().AddNote(diag::System::Resolver, msg, source);
}
}  // namespace tint::resolver
