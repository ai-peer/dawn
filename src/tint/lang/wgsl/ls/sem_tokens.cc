// Copyright 2023 The Dawn & Tint Authors
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

#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/ast/identifier_expression.h"
#include "src/tint/lang/wgsl/ast/member_accessor_expression.h"
#include "src/tint/lang/wgsl/ls/sem_token.h"
#include "src/tint/lang/wgsl/ls/server.h"
#include "src/tint/lang/wgsl/ls/utils.h"
#include "src/tint/lang/wgsl/sem/function_expression.h"
#include "src/tint/lang/wgsl/sem/type_expression.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/utils/rtti/switch.h"

namespace lsp = langsvr::lsp;

namespace tint::wgsl::ls {

namespace {

struct Token {
    size_t kind = 0;
    size_t line = 0;
    size_t column = 0;
    size_t length = 0;
};

Token TokenFromRange(const tint::Source::Range& range, size_t kind) {
    Token tok;
    tok.line = range.begin.line;
    tok.column = range.begin.column;
    tok.length = range.end.column - tok.column;
    tok.kind = kind;
    return tok;
}

std::optional<size_t> TokenKindFor(const sem::Expression* expr) {
    return Switch<std::optional<size_t>>(
        Unwrap(expr),  //
        [](const sem::TypeExpression*) { return SemToken::kType; },
        [](const sem::VariableUser*) { return SemToken::kVariable; },
        [](const sem::FunctionExpression*) { return SemToken::kFunction; },
        [](const sem::BuiltinEnumExpressionBase*) { return SemToken::kEnumMember; },
        [](tint::Default) { return std::nullopt; });
}

std::vector<Token> Tokens(File& file) {
    std::vector<Token> tokens;
    auto& sem = file.program.Sem();
    for (auto* node : file.nodes) {
        Switch(
            node,  //
            [&](const ast::IdentifierExpression* expr) {
                if (auto kind = TokenKindFor(sem.Get(expr))) {
                    tokens.push_back(TokenFromRange(expr->identifier->source.range, *kind));
                }
            },
            [&](const ast::Variable* var) {
                tokens.push_back(TokenFromRange(var->name->source.range, SemToken::kVariable));
            },
            [&](const ast::Function* fn) {
                tokens.push_back(TokenFromRange(fn->name->source.range, SemToken::kFunction));
            },
            [&](const ast::MemberAccessorExpression* a) {
                tokens.push_back(TokenFromRange(a->member->source.range, SemToken::kMember));
            });
    }
    return tokens;
}

}  // namespace

langsvr::Result<typename lsp::TextDocumentSemanticTokensFullRequest::Result>  //
Server::Do(const lsp::TextDocumentSemanticTokensFullRequest& r) {
    typename lsp::TextDocumentSemanticTokensFullRequest::Result result;

    if (auto file = files_.Get(r.text_document.uri)) {
        lsp::SemanticTokens tokens;
        // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_semanticTokens
        Token last;
        last.line = 1;
        last.column = 1;

        for (auto tok : Tokens(**file)) {
            if (last.line != tok.line) {
                last.column = 1;
            }
            tokens.data.push_back(tok.line - last.line);
            tokens.data.push_back(tok.column - last.column);
            tokens.data.push_back(tok.length);
            tokens.data.push_back(tok.kind);
            tokens.data.push_back(0);  // modifiers
            last = tok;
        }

        result = tokens;
    }

    return result;
}

}  // namespace tint::wgsl::ls
