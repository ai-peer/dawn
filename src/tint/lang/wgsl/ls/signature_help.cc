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

#include "src/tint/lang/core/intrinsic/table.h"
#include "src/tint/lang/wgsl/intrinsic/dialect.h"
#include "src/tint/lang/wgsl/ls/utils.h"
#include "src/tint/lang/wgsl/sem/call.h"
#include "src/tint/utils/rtti/switch.h"
#include "src/tint/utils/text/string_stream.h"

namespace lsp = langsvr::lsp;

namespace tint::wgsl::ls {

namespace {

std::vector<lsp::ParameterInformation> Params(const core::intrinsic::TableData& data,
                                              const core::intrinsic::OverloadInfo& overload) {
    std::vector<lsp::ParameterInformation> params;
    for (size_t i = 0; i < overload.num_parameters; i++) {
        lsp::ParameterInformation param_out;
        auto& param_in = data[overload.parameters + i];
        if (param_in.usage != core::ParameterUsage::kNone) {
            param_out.label = std::string(core::ToString(param_in.usage));
        } else {
            param_out.label = "param-" + std::to_string(i);
        }
        params.push_back(std::move(param_out));
    }
    return params;
}

size_t CalcParamIndex(const Source& call_source, const Source::Location& carat) {
    size_t index = 0;
    int depth = 0;

    auto start = call_source.range.begin;
    auto end = std::min(call_source.range.end, carat);
    auto& lines = call_source.file->content.lines;

    for (auto line = start.line; line <= end.line; line++) {
        auto start_column = line == start.line ? start.column : 0;
        auto end_column = line == end.line ? end.column : 0;
        auto text = lines[line - 1].substr(start_column - 1, end_column - start_column);
        for (char c : text) {
            switch (c) {
                case '(':
                case '[':
                    depth++;
                    break;
                case ')':
                case ']':
                    depth--;
                    break;
                case ',':
                    if (depth == 1) {
                        index++;
                    }
            }
        }
    }
    return index;
}

}  // namespace

langsvr::Result<typename lsp::TextDocumentSignatureHelpRequest::Result>  //
Server::Do(const lsp::TextDocumentSignatureHelpRequest& r) {
    typename lsp::TextDocumentSignatureHelpRequest::Result result = lsp::Null{};

    if (auto file = files_.Get(r.text_document.uri)) {
        auto& program = (*file)->program;
        auto pos = Conv(r.position);
        if (auto call = (*file)->NodeAt<sem::Call>(pos)) {
            lsp::SignatureHelp help;
            help.active_parameter = CalcParamIndex(call->Declaration()->source, pos);
            Switch(call->Target(),  //
                   [&](const sem::BuiltinFn* target) {
                       auto& data = wgsl::intrinsic::Dialect::kData;
                       auto& builtins = data.builtins;
                       auto& intrinsic_info = builtins[static_cast<size_t>(target->Fn())];
                       std::string name{wgsl::str(target->Fn())};

                       for (size_t i = 0; i < intrinsic_info.num_overloads; i++) {
                           auto& overload = data[intrinsic_info.overloads + i];

                           auto params = Params(data, overload);

                           auto type_mgr = core::type::Manager::Wrap(program.Types());
                           auto symbols = SymbolTable::Wrap(program.Symbols());

                           StringStream ss;
                           core::intrinsic::Context ctx{data, type_mgr, symbols};
                           core::intrinsic::PrintOverload(ss, ctx, overload, name);

                           lsp::SignatureInformation sig;
                           sig.parameters = params;
                           sig.label = ss.str();
                           help.signatures.push_back(sig);

                           if (&overload == &target->Overload()) {
                               help.active_signature = i;
                           }
                       }
                   });

            result = help;
        }
    }

    return result;
}

}  // namespace tint::wgsl::ls
