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

#include "src/tint/lang/wgsl/ls/server.h"

#include "src/tint/lang/wgsl/ast/identifier.h"
#include "src/tint/lang/wgsl/ast/identifier_expression.h"
#include "src/tint/lang/wgsl/ls/utils.h"
#include "src/tint/lang/wgsl/sem/function.h"
#include "src/tint/lang/wgsl/sem/function_expression.h"
#include "src/tint/lang/wgsl/sem/member_accessor_expression.h"
#include "src/tint/lang/wgsl/sem/struct.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/utils/rtti/switch.h"

namespace lsp = langsvr::lsp;

namespace tint::wgsl::ls {

langsvr::Result<typename lsp::TextDocumentPrepareRenameRequest::Result>  //
Server::Do(const lsp::TextDocumentPrepareRenameRequest& r) {
    typename lsp::TextDocumentPrepareRenameRequest::Result result = lsp::Null{};

    if (auto file = files_.Get(r.text_document.uri)) {
        if (auto def = (*file)->Definition(Conv(r.position))) {
            lsp::PrepareRenamePlaceholder out;
            out.range = Conv(def->range);
            out.placeholder = def->text;
            result = lsp::PrepareRenameResult{out};
        }
    }

    return result;
}

langsvr::Result<typename lsp::TextDocumentRenameRequest::Result>  //
Server::Do(const lsp::TextDocumentRenameRequest& r) {
    typename lsp::TextDocumentRenameRequest::Result result = lsp::Null{};

    if (auto file = files_.Get(r.text_document.uri)) {
        std::vector<lsp::TextEdit> changes;
        for (auto& ref : (*file)->References(Conv(r.position), /* include_declaration */ true)) {
            lsp::TextEdit edit;
            edit.range = Conv(ref);
            edit.new_text = r.new_name;
            changes.emplace_back(std::move(edit));
        }

        lsp::WorkspaceEdit edit;
        (*edit.changes)[r.text_document.uri] = std::move(changes);
        result = std::move(edit);
    }

    return result;
}

}  // namespace tint::wgsl::ls
