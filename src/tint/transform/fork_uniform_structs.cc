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

#include "src/tint/transform/fork_uniform_structs.h"

#include <unordered_map>
#include <utility>

#include "src/tint/ast/struct.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/member_accessor_expression.h"
#include "src/tint/sem/variable.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::ForkUniformStructs);

namespace tint::transform {

namespace {

bool NeedsForking(const sem::Struct* str) {
    if (str->UsedAs(ast::StorageClass::kUniform) && str->StorageClassUsage().size() > 1) {
        return true;
    }
    return false;
}

}  // namespace

ForkUniformStructs::ForkUniformStructs() = default;

ForkUniformStructs::~ForkUniformStructs() = default;

bool ForkUniformStructs::ShouldRun(const Program* program, const DataMap&) const {
    for (auto* node : program->ASTNodes().Objects()) {
        if (auto* str = node->As<const ast::Struct>()) {
            if (NeedsForking(program->Sem().Get(str))) {
                return true;
            }
        }
    }
    return false;
}

void ForkUniformStructs::Run(CloneContext& ctx, const DataMap&, DataMap&) const {
    auto& sem = ctx.src->Sem();

    ProgramBuilder& b = *ctx.dst;

    using StructMemberList = utils::Vector<const ast::StructMember*, 8>;
    std::unordered_map<const sem::StructMember*, StructMemberList> decomposed_members;
    std::unordered_map<const sem::Struct*, const ast::Struct*> forked_structs;

    // Process all structs.
    ctx.ReplaceAll([&](const ast::Struct* ast_str) -> const ast::Struct* {
        auto* str = sem.Get(ast_str);
        if (!NeedsForking(str)) {
            return nullptr;
        }
        StructMemberList members;
        for (auto* member : str->Members()) {
            members.Push(ctx.Clone(member->Declaration()));
        }
        std::string name = ctx.src->Symbols().NameFor(str->Name());
        auto* new_str = b.Structure(b.Symbols().New(name), std::move(members));
        forked_structs[str] = new_str;
        return nullptr;
    });

    ctx.ReplaceAll([&](const ast::AssignmentStatement* stmt) -> const ast::AssignmentStatement* {
        auto* str = sem.Get(stmt->rhs)->Type()->UnwrapRef()->As<sem::Struct>();
        if (!str) {
            return nullptr;
        }
        auto* rhs = sem.Get<sem::VariableUser>(stmt->rhs);
        if (!rhs || rhs->Variable()->StorageClass() != ast::StorageClass::kUniform) {
            return nullptr;
        }
        auto it = forked_structs[str];
        if (!it) {
            return nullptr;
        }
        utils::Vector<const ast::Expression*, 10> args;
        for (auto* member : str->Members()) {
            auto* uniform = ctx.Clone(stmt->rhs);
            args.Push(b.MemberAccessor(uniform, ctx.Clone(member->Name())));
        }
        auto* struct_type = CreateASTTypeFor(ctx, str);
        return b.Assign(ctx.Clone(stmt->lhs), b.Construct(struct_type, args));
    });

    ctx.ReplaceAll([&](const ast::Var* var) -> const ast::Variable* {
        auto* global = sem.Get<sem::GlobalVariable>(var);

        if (!global || global->StorageClass() != ast::StorageClass::kUniform) {
            return nullptr;
        }
        auto* str = global->Type()->UnwrapRef()->As<sem::Struct>();
        if (!str) {
            return nullptr;
        }
        auto it = forked_structs.find(str);
        if (it == forked_structs.end()) {
            return nullptr;
        }

        return b.Var(ctx.Clone(var->symbol), b.ty.Of(it->second), ctx.Clone(var->attributes),
                     var->declared_storage_class);
    });

    ctx.Clone();
}

}  // namespace tint::transform
