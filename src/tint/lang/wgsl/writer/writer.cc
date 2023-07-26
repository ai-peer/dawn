// Copyright 2020 The Tint Authors.
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

#include "src/tint/lang/wgsl/writer/writer.h"

#include <memory>
#include <utility>

#include "src/tint/lang/wgsl/program/program.h"
#include "src/tint/lang/wgsl/writer/ast_printer/ast_printer.h"

#if TINT_BUILD_SYNTAX_TREE_WRITER
#include "src/tint/lang/wgsl/writer/syntax_tree_printer/syntax_tree_printer.h"
#endif  // TINT_BUILD_SYNTAX_TREE_WRITER

namespace tint::wgsl::writer {

Result Generate(const Program* program, const Options& options) {
    (void)options;

    Result result;
    if (!program->IsValid()) {
        result.error = "input program is not valid";
        return result;
    }

#if TINT_BUILD_SYNTAX_TREE_WRITER
    if (options.use_syntax_tree_writer) {
        // Generate the WGSL code.
        auto impl = std::make_unique<SyntaxTreePrinter>(program);
        impl->Generate();
        result.success = impl->Diagnostics().empty();
        result.error = impl->Diagnostics().str();
        result.wgsl = impl->Result();
    } else  // NOLINT(readability/braces)
#endif
    {
        // Generate the WGSL code.
        auto impl = std::make_unique<ASTPrinter>(program);
        impl->Generate();
        result.success = impl->Diagnostics().empty();
        result.error = impl->Diagnostics().str();
        result.wgsl = impl->Result();
    }

    return result;
}

}  // namespace tint::wgsl::writer
