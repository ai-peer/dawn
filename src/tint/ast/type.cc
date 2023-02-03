// Copyright 2020 The Tint Authors.
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

#include "src/tint/ast/type.h"

#include "src/tint/ast/identifier.h"
#include "src/tint/clone_context.h"
#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::Type);

namespace tint::ast {

Type::Type(ProgramID pid, NodeID nid, const Source& src, const Identifier* n)
    : Base(pid, nid, src), name(n) {
    TINT_ASSERT(AST, name);
    TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, name, program_id);
}

Type::Type(Type&&) = default;

Type::~Type() = default;

const Type* Type::Clone(CloneContext* ctx) const {
    auto src = ctx->Clone(source);
    auto n = ctx->Clone(name);
    return ctx->dst->create<Type>(src, n);
}

}  // namespace tint::ast
