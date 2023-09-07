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

#include "src/tint/lang/msl/writer/ast_raise/pixel_local.h"

#include <utility>

#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/wgsl/program/clone_context.h"
#include "src/tint/lang/wgsl/sem/module.h"

TINT_INSTANTIATE_TYPEINFO(tint::msl::writer::PixelLocal);

using namespace tint::core::number_suffixes;  // NOLINT
using namespace tint::core::fluent_types;     // NOLINT

namespace tint::msl::writer {

/// PIMPL state for the transform
struct PixelLocal::State {
    /// The source program
    const Program* const src;
    /// The target program builder
    ProgramBuilder b;
    /// The clone context
    program::CloneContext ctx = {&b, src, /* auto_clone_symbols */ true};

    /// Constructor
    /// @param program the source program
    explicit State(const Program* program) : src(program) {}

    /// Runs the transform
    /// @returns the new program or SkipTransform if the transform is not required
    ApplyResult Run() {
        auto& sem = src->Sem();
        if (!sem.Module()->Extensions().Contains(
                core::Extension::kChromiumExperimentalPixelLocal)) {
            return SkipTransform;
        }

        bool made_changes = false;

        if (!made_changes) {
            return SkipTransform;
        }

        ctx.Clone();
        return resolver::Resolve(b);
    }
};

PixelLocal::PixelLocal() = default;

PixelLocal::~PixelLocal() = default;

ast::transform::Transform::ApplyResult PixelLocal::Apply(const Program* src,
                                                         const ast::transform::DataMap&,
                                                         ast::transform::DataMap&) const {
    return State(src).Run();
}

}  // namespace tint::msl::writer
