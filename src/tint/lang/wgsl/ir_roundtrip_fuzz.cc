// Copyright 2023 The Tint Authors.
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

// GEN_BUILD:CONDITION(tint_build_wgsl_reader && tint_build_wgsl_writer)

#include <iostream>

#include "src/tint/cmd/fuzz/wgsl/wgsl_fuzz.h"
#include "src/tint/lang/core/ir/disassembler.h"
#include "src/tint/lang/wgsl/helpers/apply_substitute_overrides.h"
#include "src/tint/lang/wgsl/reader/lower/lower.h"
#include "src/tint/lang/wgsl/reader/parser/parser.h"
#include "src/tint/lang/wgsl/reader/program_to_ir/program_to_ir.h"
#include "src/tint/lang/wgsl/writer/ir_to_program/ir_to_program.h"
#include "src/tint/lang/wgsl/writer/raise/raise.h"
#include "src/tint/lang/wgsl/writer/writer.h"

namespace tint::wgsl {
namespace {

bool IsUnsupported(const tint::ast::Enable* enable) {
    for (auto ext : enable->extensions) {
        switch (ext->name) {
            case tint::wgsl::Extension::kChromiumExperimentalDp4A:
            case tint::wgsl::Extension::kChromiumExperimentalFullPtrParameters:
            case tint::wgsl::Extension::kChromiumExperimentalPixelLocal:
            case tint::wgsl::Extension::kChromiumExperimentalPushConstant:
            case tint::wgsl::Extension::kChromiumInternalDualSourceBlending:
            case tint::wgsl::Extension::kChromiumInternalRelaxedUniformLayout:
                return true;
            default:
                break;
        }
    }
    return false;
}

}  // namespace

void IRRoundtripFuzzer(const tint::Program& program) {
    if (program.AST().Enables().Any(IsUnsupported)) {
        return;
    }

    auto transformed = tint::wgsl::ApplySubstituteOverrides(program);
    auto& src = transformed ? transformed.value() : program;
    if (!src.IsValid()) {
        return;
    }

    auto ir = tint::wgsl::reader::ProgramToIR(src);
    if (!ir) {
        TINT_ICE() << ir.Failure();
        return;
    }

    if (auto res = tint::wgsl::reader::Lower(ir.Get()); !res) {
        TINT_ICE() << res.Failure();
        return;
    }

    if (auto res = tint::wgsl::writer::Raise(ir.Get()); !res) {
        TINT_ICE() << res.Failure();
        return;
    }

    auto dst = tint::wgsl::writer::IRToProgram(ir.Get());
    if (!dst.IsValid()) {
        std::cerr << "IR:\n" << core::ir::Disassemble(ir.Get()) << std::endl;
        if (auto result = tint::wgsl::writer::Generate(dst, {}); result) {
            std::cerr << "WGSL:\n" << result->wgsl << std::endl << std::endl;
        }
        TINT_ICE() << dst.Diagnostics();
        return;
    }

    return;
}

}  // namespace tint::wgsl

TINT_WGSL_PROGRAM_FUZZER(tint::wgsl::IRRoundtripFuzzer);
