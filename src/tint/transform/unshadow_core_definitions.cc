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

#include "src/tint/transform/unshadow_core_definitions.h"

#include <string>
#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/sem/builtin_type.h"
#include "src/tint/type/short_name.h"
#include "src/tint/utils/hashset.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::UnshadowCoreDefinitions);

namespace tint::transform {

namespace {

bool IsCoreDefinition(std::string_view name) {
    return sem::ParseBuiltinType(name) != sem::BuiltinType::kNone ||
           type::ParseShortName(name) != type::ShortName::kUndefined;
}

}  // namespace

UnshadowCoreDefinitions::UnshadowCoreDefinitions() = default;

UnshadowCoreDefinitions::~UnshadowCoreDefinitions() = default;

Transform::ApplyResult UnshadowCoreDefinitions::Apply(const Program* src,
                                                      const DataMap&,
                                                      DataMap&) const {
    utils::Vector<std::string, 8> needs_renaming;
    for (auto* decl : src->ASTNodes().Objects()) {
        auto symbol = Switch(
            decl,  //
            [&](const ast::TypeDecl* d) { return d->name; },
            [&](const ast::Variable* d) { return d->symbol; },
            [&](const ast::Function* d) { return d->symbol; });
        if (!symbol.IsValid()) {
            continue;
        }
        auto name = src->Symbols().NameFor(symbol);
        if (IsCoreDefinition(name)) {
            needs_renaming.Push(std::move(name));
        }
    }

    if (needs_renaming.IsEmpty()) {
        return SkipTransform;
    }

    ProgramBuilder b;
    for (auto& name : needs_renaming) {
        // Pre-register all the symbols that need renaming.
        // Because this is done before the clone, the AST symbols will be renamed.
        b.Symbols().Register(name);
    }

    CloneContext ctx(&b, src);
    ctx.Clone();
    return Program(std::move(b));
}

}  // namespace tint::transform
