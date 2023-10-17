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
#include <utility>

#include "src/tint/lang/glsl/writer/ast_printer/ast_printer.h"
#include "src/tint/lang/glsl/writer/printer/printer.h"
#include "src/tint/lang/glsl/writer/raise/raise.h"

#if TINT_BUILD_WGSL_READER
#include "src/tint/lang/wgsl/reader/lower/lower.h"
#include "src/tint/lang/wgsl/reader/program_to_ir/program_to_ir.h"
#endif  // TINT_BUILD_WGSL_REAdDER

namespace tint::glsl::writer {

Result<Output> Generate(const Program& program,
                        const Options& options,
                        const std::string& entry_point) {
    if (!program.IsValid()) {
        return Failure{program.Diagnostics()};
    }

    Output output;

    if (options.use_tint_ir) {
#if TINT_BUILD_WGSL_READER
        // Convert the AST program to an IR module.
        auto converted = wgsl::reader::ProgramToIR(program);
        if (!converted) {
            return converted.Failure();
        }

        auto ir = converted.Move();

        // Lower from WGSL-dialect to core-dialect
        if (auto res = wgsl::reader::Lower(ir); !res) {
            return res.Failure();
        }

        // Raise from core-dialect to GLSL-dialect.
        if (auto res = raise::Raise(ir); !res) {
            return res.Failure();
        }

        // Generate the GLSL code.
        auto impl = std::make_unique<Printer>(ir);
        auto result = impl->Generate(options.version);
        if (!result) {
            return result.Failure();
        }
        output.glsl = impl->Result();
#else
        return Failure{"use_tint_ir requires building with TINT_BUILD_WGSL_READER"};
#endif
    } else {
        // Sanitize the program.
        auto sanitized_result = Sanitize(program, options, entry_point);
        if (!sanitized_result.program.IsValid()) {
            return Failure{sanitized_result.program.Diagnostics()};
        }

        // Generate the GLSL code.
        auto impl = std::make_unique<ASTPrinter>(sanitized_result.program, options.version);
        if (!impl->Generate()) {
            return Failure{impl->Diagnostics()};
        }

        output.glsl = impl->Result();
        output.needs_internal_uniform_buffer = sanitized_result.needs_internal_uniform_buffer;
        output.bindpoint_to_data = std::move(sanitized_result.bindpoint_to_data);

        // Collect the list of entry points in the sanitized program.
        for (auto* func : sanitized_result.program.AST().Functions()) {
            if (func->IsEntryPoint()) {
                auto name = func->name->symbol.Name();
                output.entry_points.push_back({name, func->PipelineStage()});
            }
        }
    }

    return output;
}

}  // namespace tint::glsl::writer
