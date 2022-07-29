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

#include "src/tint/transform/mat2_to_vec2.h"

// #include <unordered_map>
// #include <unordered_set>
// #include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/ast/struct.h"
#include "src/tint/sem/variable.h"
// #include "src/tint/utils/map.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::Mat2ToVec2);

namespace tint::transform {

Mat2ToVec2::Mat2ToVec2() = default;

Mat2ToVec2::~Mat2ToVec2() = default;

void Mat2ToVec2::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    auto& sem = ctx.src->Sem();

    const Program* s = ctx.src;
    ProgramBuilder* d = ctx.dst;
    std::unordered_map<const ast::Struct*, const ast::Struct*> modified_structs;

    // Process all structs.
    ctx.ReplaceAll([&](const ast::Struct* ast_str) -> const ast::Struct* {
        auto* str = sem.Get(ast_str);
        if (!str->UsedAs(ast::StorageClass::kUniform)) {
            return nullptr;
        }
        ast::StructMemberList members;
        bool modified = false;
        for (auto* mem : str->Members()) {
            auto* mat = mem->Type()->As<sem::Matrix>();
            if (!mat || mat->columns() != 2) {
                continue;
            }
            auto* vecTySem = d->create<sem::Vector>(mat->type(), 2u);
            for (uint32_t i = 0u; i < mat->rows(); ++i) {
                auto* vecTy = CreateASTTypeFor(ctx, vecTySem);
                members.push_back(d->Member(d->Symbols().New(s->Symbols().NameFor(mem->Name()) + std::to_string(i)), vecTy));
            }
            modified = true;
        }
        if (modified) {
            auto* new_str = d->Structure(ctx.Clone(str->Name()), std::move(members));
            modified_structs[ast_str] = new_str;
            return new_str;
        }
        return nullptr;
    });

    // Process all vars using modified structs.
    ctx.ReplaceAll([&](const ast::Var* var) -> const ast::Var* {
        if (var->declared_storage_class != ast::StorageClass::kUniform) {
            return nullptr;
        }
        auto* str = sem.Get(var->type)->As<sem::Struct>();
        if (!str) {
            return nullptr;
        }
        if (auto* new_struct = modified_structs[str->Declaration()]) {
            return d->Var(ctx.Clone(var->symbol), d->ty.Of(new_struct), var->declared_storage_class, ctx.Clone(var->attributes));
        }
        return nullptr;
    });

    ctx.Clone();
}

}  // namespace tint::transform
