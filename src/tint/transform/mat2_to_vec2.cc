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

#include "src/tint/transform/mat2_to_vec2.h"

#include <unordered_map>
#include <utility>

#include "src/tint/ast/struct.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/member_accessor_expression.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::Mat2ToVec2);

namespace tint::transform {

Mat2ToVec2::Mat2ToVec2() = default;

Mat2ToVec2::~Mat2ToVec2() = default;

void Mat2ToVec2::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    auto& sem = ctx.src->Sem();

    const Program* s = ctx.src;
    ProgramBuilder* d = ctx.dst;

    std::unordered_map<const sem::StructMember*, ast::StructMemberList> decomposed;

    // Process all structs.
    ctx.ReplaceAll([&](const ast::Struct* ast_str) -> const ast::Struct* {
        auto* str = sem.Get(ast_str);
        if (!str->UsedAs(ast::StorageClass::kUniform)) {
            return nullptr;
        }
        ast::StructMemberList members;
        bool modified = false;
        for (auto* member : str->Members()) {
            auto* mat = member->Type()->As<sem::Matrix>();
            if (!mat || mat->columns() != 2) {
                continue;
            }
            ast::StructMemberList& replacements = decomposed[member];
            auto* vecTySem = d->create<sem::Vector>(mat->type(), 2u);
            for (uint32_t i = 0u; i < mat->rows(); ++i) {
                auto* vecTy = CreateASTTypeFor(ctx, vecTySem);
                std::string name = (s->Symbols().NameFor(member->Name()) + std::to_string(i));
                tint::Symbol symbol = d->Symbols().New(name);
                auto* new_member = d->Member(symbol, vecTy);
                members.push_back(new_member);
                replacements.push_back(new_member);
            }
            modified = true;
        }
        if (modified) {
            return d->Structure(ctx.Clone(str->Name()), std::move(members));
        }
        return nullptr;
    });

    ctx.ReplaceAll([&](const ast::IndexAccessorExpression* expr) -> const ast::Expression* {
        auto* access = ctx.src->Sem().Get<sem::StructMemberAccess>(expr->object);
        if (!access) {
            return nullptr;
        }
        auto it = decomposed.find(access->Member());
        if (it == decomposed.end()) {
            return nullptr;
        }
        auto* type = CreateASTTypeFor(ctx, access->Member()->Type());
        ast::ExpressionList args;

        for (auto* member : it->second) {
            auto* arg = d->MemberAccessor(ctx.Clone(expr->object), member->symbol);
            args.push_back(arg);
        }
        return d->Construct(type, args);
    });

    ctx.Clone();
}

}  // namespace tint::transform
