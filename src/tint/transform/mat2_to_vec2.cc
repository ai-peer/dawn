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

namespace {

bool NeedsDecomposition(const sem::Struct* str) {
    if (!str->UsedAs(ast::StorageClass::kUniform)) {
        return false;
    }
    for (auto* member : str->Members()) {
        auto* mat = member->Type()->As<sem::Matrix>();
        if (mat && mat->rows() == 2) {
            return true;
        }
    }
    return false;
}

}  // namespace

Mat2ToVec2::Mat2ToVec2() = default;

Mat2ToVec2::~Mat2ToVec2() = default;

bool Mat2ToVec2::ShouldRun(const Program* program, const DataMap&) const {
    for (auto* node : program->ASTNodes().Objects()) {
        if (auto* str = node->As<const ast::Struct>()) {
            if (NeedsDecomposition(program->Sem().Get(str))) {
                return true;
            }
        }
    }
    return false;
}

void Mat2ToVec2::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    auto& sem = ctx.src->Sem();

    ProgramBuilder& b = *ctx.dst;

    using StructMemberList = utils::Vector<const ast::StructMember*, 8>;
    std::unordered_map<const sem::StructMember*, StructMemberList> decomposed;

    // Process all structs.
    ctx.ReplaceAll([&](const ast::Struct* ast_str) -> const ast::Struct* {
        auto* str = sem.Get(ast_str);
        if (!NeedsDecomposition(str)) {
            return nullptr;
        }
        StructMemberList members;
        for (auto* member : str->Members()) {
            auto* mat = member->Type()->As<sem::Matrix>();
            if (!mat || mat->rows() != 2) {
                members.Push(ctx.Clone(member->Declaration()));
                continue;
            }
            StructMemberList& replacements = decomposed[member];
            auto* vec_sem_type = b.create<sem::Vector>(mat->type(), 2u);
            uint32_t padding = member->Size() - vec_sem_type->Size() * mat->columns();
            for (uint32_t i = 0u; i < mat->columns(); ++i) {
                auto* vec_type = CreateASTTypeFor(ctx, vec_sem_type);
                std::string name = (ctx.src->Symbols().NameFor(member->Name()) + std::to_string(i));
                tint::Symbol symbol = b.Symbols().New(name);
                utils::Vector<const ast::Attribute*, 1> attributes;

                // Copy @align to first vector member (if required).
                if (i == 0 && member->Align() != vec_sem_type->Align()) {
                    attributes.Push(b.MemberAlign(member->Align()));
                }

                // Copy @size to last vector member (if required).
                if (i == mat->columns() - 1 && padding > 0) {
                    attributes.Push(b.MemberSize(padding + vec_sem_type->Size()));
                }
                auto* new_member = b.Member(symbol, vec_type, attributes);
                members.Push(new_member);
                replacements.Push(new_member);
            }
        }
        return b.create<ast::Struct>(ctx.Clone(str->Name()), std::move(members), utils::Empty);
    });

    ctx.ReplaceAll([&](const ast::MemberAccessorExpression* expr) -> const ast::Expression* {
        auto* access = sem.Get<sem::StructMemberAccess>(expr);
        if (!access) {
            return nullptr;
        }
        auto it = decomposed.find(access->Member());
        if (it == decomposed.end()) {
            return nullptr;
        }
        auto* type = CreateASTTypeFor(ctx, access->Member()->Type());
        utils::Vector<const ast::Expression*, 4> args;

        for (auto* member : it->second) {
            auto* arg = b.MemberAccessor(ctx.Clone(expr->structure), member->symbol);
            args.Push(arg);
        }
        return b.Construct(type, args);
    });

    ctx.ReplaceAll([&](const ast::AssignmentStatement* stmt) -> const ast::Statement* {
        auto* lhs_decl = stmt->lhs->As<ast::MemberAccessorExpression>();
        auto* rhs_decl = stmt->rhs->As<ast::MemberAccessorExpression>();
        if (!lhs_decl || !rhs_decl) {
            return nullptr;
        }
        auto* lhs_access = sem.Get<sem::StructMemberAccess>(stmt->lhs);
        auto it = decomposed.find(lhs_access->Member());
        if (it == decomposed.end()) {
            return nullptr;
        }
        utils::Vector<const ast::Statement*, 4> stmts;

        for (auto* member : it->second) {
            auto* lhs =
                b.MemberAccessor(ctx.CloneWithoutTransform(lhs_decl->structure), member->symbol);
            auto* rhs =
                b.MemberAccessor(ctx.CloneWithoutTransform(rhs_decl->structure), member->symbol);
            stmts.Push(b.Assign(lhs, rhs));
        }

        return b.Block(stmts);
    });

    ctx.Clone();
}

}  // namespace tint::transform
