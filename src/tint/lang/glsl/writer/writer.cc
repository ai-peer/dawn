// Copyright 2021 The Tint Authors.
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

#include "src/tint/lang/glsl/writer/writer.h"

#include <memory>

#include "src/tint/lang/glsl/writer/ast_printer/ast_printer.h"
#include "src/tint/lang/wgsl/ast/transform/binding_remapper.h"
#include "src/tint/lang/wgsl/ast/transform/combine_samplers.h"

namespace tint::glsl::writer {

Result Generate(const Program* program, const Options& options, const std::string& entry_point) {
    Result result;
    if (!program->IsValid()) {
        result.error = "input program is not valid";
        return result;
    }

    // Sanitize the program.
    auto sanitized_result = Sanitize(program, options, entry_point);
    if (!sanitized_result.program.IsValid()) {
        result.success = false;
        result.error = sanitized_result.program.Diagnostics().str();
        return result;
    }

    // Generate the GLSL code.
    auto impl = std::make_unique<ASTPrinter>(&sanitized_result.program, options.version);
    impl->Generate();
    result.success = impl->Diagnostics().empty();
    result.error = impl->Diagnostics().str();
    result.glsl = impl->Result();
    result.needs_internal_uniform_buffer = sanitized_result.needs_internal_uniform_buffer;
    result.bindpoint_to_data = std::move(sanitized_result.bindpoint_to_data);

    // Collect the list of entry points in the sanitized program.
    for (auto* func : sanitized_result.program.AST().Functions()) {
        if (func->IsEntryPoint()) {
            auto name = func->name->symbol.Name();
            result.entry_points.push_back({name, func->PipelineStage()});
        }
    }

    return result;
}

}  // namespace tint::glsl::writer
