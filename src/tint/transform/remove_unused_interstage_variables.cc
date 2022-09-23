// Copyright 2022 The Tint Authors.
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

#include "src/tint/transform/remove_unused_interstage_variables.h"

#include <functional>

#include "src/tint/program_builder.h"
#include "src/tint/sem/variable.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::RemoveUnusedInterstageVariables);
TINT_INSTANTIATE_TYPEINFO(tint::transform::RemoveUnusedInterstageVariables::Config);

namespace tint::transform {

RemoveUnusedInterstageVariables::RemoveUnusedInterstageVariables() = default;

RemoveUnusedInterstageVariables::~RemoveUnusedInterstageVariables() = default;

bool RemoveUnusedInterstageVariables::ShouldRun(const Program* program, const DataMap&) const {
    for (auto* node : program->AST().GlobalVariables()) {
        if (node->Is<ast::Override>()) {
            return true;
        }
    }
    return false;
}

void RemoveUnusedInterstageVariables::Run(CloneContext& ctx,
                                          const DataMap& config,
                                          DataMap&) const {
    const auto* data = config.Get<Config>();
    if (!data) {
        ctx.dst->Diagnostics().add_error(diag::System::Transform,
                                         "Missing unused inter-stage variable transform data");
        return;
    }

    for (auto* ty : ctx.src->AST().TypeDecls()) {
        // if (auto* struct_ty = ty->As<ast::Struct>()) {
        //     for (auto* member : struct_ty->members) {
        //         for (auto* attr : member->attributes) {
        //             if (attr->IsAnyOf<ast::BuiltinAttribute, ast::InterpolateAttribute,
        //             ast::InvariantAttribute,
        //                  ast::LocationAttribute>()) {
        //                 ctx.Remove(member->attributes, attr);
        //             }
        //         }
        //     }
        // }
        if (auto* attr = ty->As<ast::LocationAttribute>()) {
        }
    }

    ctx.Clone();
}

RemoveUnusedInterstageVariables::Config::Config() = default;

RemoveUnusedInterstageVariables::Config::Config(const Config&) = default;

RemoveUnusedInterstageVariables::Config::~Config() = default;

RemoveUnusedInterstageVariables::Config& RemoveUnusedInterstageVariables::Config::operator=(
    const Config&) = default;

}  // namespace tint::transform
