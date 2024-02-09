// Copyright 2024 The Dawn & Tint Authors
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

#include "src/tint/lang/core/constant/scalar.h"
#include "src/tint/lang/core/constant/splat.h"
#include "src/tint/lang/wgsl/ast/const.h"
#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/ast/let.h"
#include "src/tint/lang/wgsl/ast/override.h"
#include "src/tint/lang/wgsl/ast/var.h"
#include "src/tint/lang/wgsl/ls/server.h"
#include "src/tint/lang/wgsl/ls/utils.h"
#include "src/tint/lang/wgsl/sem/member_accessor_expression.h"
#include "src/tint/lang/wgsl/sem/struct.h"
#include "src/tint/lang/wgsl/sem/type_expression.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/utils/rtti/switch.h"

using namespace tint::core::fluent_types;  // NOLINT

namespace lsp = langsvr::lsp;

namespace tint::wgsl::ls {

namespace {

lsp::MarkedStringWithLanguage WGSL(std::string wgsl) {
    lsp::MarkedStringWithLanguage str;
    str.language = "wgsl";
    str.value = wgsl;
    return str;
}

lsp::MarkedStringWithLanguage Plain(std::string wgsl) {
    lsp::MarkedStringWithLanguage str;
    str.language = "";
    str.value = wgsl;
    return str;
}

void PrintConstant(const core::constant::Value* val, StringStream& ss) {
    Switch(
        val,  //
        [&](const core::constant::Scalar<AInt>* s) { ss << s->value; },
        [&](const core::constant::Scalar<AFloat>* s) { ss << s->value; },
        [&](const core::constant::Scalar<bool>* s) { ss << s->value; },
        [&](const core::constant::Scalar<f16>* s) { ss << s->value << "h"; },
        [&](const core::constant::Scalar<f32>* s) { ss << s->value << "f"; },
        [&](const core::constant::Scalar<i32>* s) { ss << s->value << "i"; },
        [&](const core::constant::Scalar<u32>* s) { ss << s->value << "u"; },
        [&](const core::constant::Splat* s) {
            ss << s->Type()->FriendlyName() << "(";
            PrintConstant(s->el, ss);
            ss << ")";
        },
        [&](const core::constant::Composite* s) {
            ss << s->Type()->FriendlyName() << "(";
            for (size_t i = 0, n = s->elements.Length(); i < n; i++) {
                if (i > 0) {
                    ss << ", ";
                }
                PrintConstant(s->elements[i], ss);
            }
            ss << ")";
        });
}

void VariableDecl(const sem::Variable* v, std::vector<lsp::MarkedString>& out) {
    StringStream ss;
    Switch(
        v->Declaration(),                           //
        [&](const ast::Var*) { ss << "var"; },      //
        [&](const ast::Let*) { ss << "let"; },      //
        [&](const ast::Const*) { ss << "const"; },  //
        [&](const ast::Override*) { ss << "override"; });
    ss << " " << v->Declaration()->name->symbol.NameView();

    if (auto* val = v->ConstantValue()) {
        ss << " = ";
        PrintConstant(val, ss);
    } else {
        ss << " : " << v->Type()->FriendlyName();
    }

    out.push_back(WGSL(ss.str()));
}

}  // namespace

langsvr::Result<typename lsp::TextDocumentHoverRequest::Result>  //
Server::Handle(const lsp::TextDocumentHoverRequest& r) {
    typename lsp::TextDocumentHoverRequest::Result result = lsp::Null{};

    if (auto file = files_.Get(r.text_document.uri)) {
        if (auto* node = (*file)->NodeAt<CastableBase>(Conv(r.position))) {
            std::vector<lsp::MarkedString> strings;
            langsvr::Optional<lsp::Range> range;

            Switch(
                Unwrap(node),  //
                [&](const sem::VariableUser* user) { VariableDecl(user->Variable(), strings); },
                [&](const sem::Variable* v) { VariableDecl(v, strings); },
                [&](const sem::TypeExpression* expr) {
                    strings.push_back(WGSL(expr->Type()->FriendlyName()));
                },
                [&](const sem::StructMemberAccess* access) {
                    if (auto* member = access->Member()->As<sem::StructMember>()) {
                        StringStream ss;
                        ss << member->Declaration()->name->symbol.NameView() << " : "
                           << member->Type()->FriendlyName();
                        strings.push_back(WGSL(ss.str()));
                    }
                },
                [&](const sem::ValueExpression* expr) {
                    if (auto* val = expr->ConstantValue()) {
                        StringStream ss;
                        ss << "value: ";
                        PrintConstant(val, ss);
                        strings.push_back(Plain(ss.str()));
                        range = Conv(expr->Declaration()->source.range);
                    }
                });

            lsp::Hover hover;
            hover.contents = strings;
            hover.range = range;
            result = hover;
        }
    }

    return result;
}

}  // namespace tint::wgsl::ls
