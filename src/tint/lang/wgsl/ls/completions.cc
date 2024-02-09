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

#include "src/tint/lang/wgsl/ls/server.h"

#include "src/tint/lang/wgsl/builtin_fn.h"
#include "src/tint/lang/wgsl/ls/utils.h"
#include "src/tint/lang/wgsl/sem/block_statement.h"
#include "src/tint/utils/rtti/switch.h"

namespace lsp = langsvr::lsp;

namespace tint::wgsl::ls {

langsvr::Result<typename lsp::TextDocumentCompletionRequest::Result>  //
Server::Handle(const lsp::TextDocumentCompletionRequest& r) {
    typename lsp::TextDocumentCompletionRequest::Result result = lsp::Null{};

    if (auto file = files_.Get(r.text_document.uri)) {
        std::vector<lsp::CompletionItem> out;
        Hashset<std::string, 32> seen;

        auto loc = Conv(r.position);
        for (auto* stmt = (*file)->NodeAt<sem::Statement>(loc); stmt; stmt = stmt->Parent()) {
            Switch(stmt,  //
                   [&](const sem::BlockStatement* block) {
                       for (auto it : block->Decls()) {
                           if (seen.Add(it.key->Name())) {
                               lsp::CompletionItem item;
                               item.label = it.key->Name();
                               item.kind = lsp::CompletionItemKind::kVariable;
                               out.push_back(item);
                           }
                       }
                   });
        }

        for (auto& builtin : wgsl::kBuiltinFnStrings) {
            lsp::CompletionItem item;
            item.label = builtin;
            item.kind = lsp::CompletionItemKind::kFunction;
        }

        result = out;
    }

    return result;
}

}  // namespace tint::wgsl::ls
