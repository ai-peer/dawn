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

#ifndef SRC_TINT_LANG_WGSL_LS_FILE_H_
#define SRC_TINT_LANG_WGSL_LS_FILE_H_

#include <memory>
#include <vector>

#include "src/tint/lang/wgsl/program/program.h"

namespace tint::wgsl::ls {

class File {
  public:
    std::unique_ptr<Source::File> source;
    int64_t version = 0;
    Program program;
    std::vector<const ast::Node*> nodes;

    File(std::unique_ptr<Source::File>&& source_, int64_t version_, Program program_);

    struct TextAndRange {
        std::string text;
        Source::Range range;
    };

    std::vector<Source::Range> References(Source::Location l, bool include_declaration);
    std::optional<TextAndRange> Definition(Source::Location l);

    template <typename T = sem::Node>
    const T* NodeAt(Source::Location loc) const {
        size_t best_len = 0xffffffff;
        const T* best_node = nullptr;
        for (auto* node : nodes) {
            if (node->source.range.begin.line == node->source.range.end.line &&
                node->source.range.begin <= loc && node->source.range.end >= loc) {
                if (auto* sem =
                        As<T, CastFlags::kDontErrorOnImpossibleCast>(program.Sem().Get(node))) {
                    size_t len = node->source.range.end.column - node->source.range.begin.column;
                    if (len < best_len) {
                        best_len = len;
                        best_node = sem;
                    }
                }
            }
        }
        return best_node;
    }
};

}  // namespace tint::wgsl::ls

#endif  // SRC_TINT_LANG_WGSL_LS_FILE_H_
