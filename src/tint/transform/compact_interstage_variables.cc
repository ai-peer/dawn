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

#include "src/tint/transform/compact_interstage_variables.h"

#include <memory>
#include <unordered_set>
#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/call.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/variable.h"
#include "src/tint/text/unicode.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::CompactInterstageVariables);
// TINT_INSTANTIATE_TYPEINFO(tint::transform::CompactInterstageVariables::Config);

namespace tint::transform {

namespace {}  // namespace

// CompactInterstageVariables::Data::Data(Remappings&& r) : remappings(std::move(r)) {}
// CompactInterstageVariables::Data::Data(const Data&) = default;
// CompactInterstageVariables::Data::~Data() = default;

// CompactInterstageVariables::Config::Config(Target t, bool pu) : target(t), preserve_unicode(pu)
// {} CompactInterstageVariables::Config::Config(const Config&) = default;
// CompactInterstageVariables::Config::~Config() = default;

CompactInterstageVariables::CompactInterstageVariables() = default;
CompactInterstageVariables::~CompactInterstageVariables() = default;

bool CompactInterstageVariables::ShouldRun(const Program* program, const DataMap&) const {
    for (auto* func_ast : program->AST().Functions()) {
        if (!func_ast->IsEntryPoint()) {
            continue;
        }

        // AddEntryPointInOutVariables

        for (auto* param : func_ast->params) {
            if (ast::HasAttribute<ast::LocationAttribute>(param->attributes)) {
            }
        }

        for (auto* attr : func_ast->return_type_attributes) {
            if (auto* a = attr->As<ast::LocationAttribute>()) {
                printf("!!! %s\n", a->Name().c_str());
            }
        }
    }
    return false;
}

void CompactInterstageVariables::Run(CloneContext& ctx,
                                     const DataMap& config,
                                     DataMap& outputs) const {
    for (auto* func_ast : ctx.src->AST().Functions()) {
        for (auto* param : func_ast->params) {
            if (ast::HasAttribute<ast::LocationAttribute>(param->attributes)) {
                auto* sem = ctx.src->Sem().Get<sem::Parameter>(param);
                printf("!!! %u \n", sem->Location().value());
            }
        }
    }

    return;
}

}  // namespace tint::transform
