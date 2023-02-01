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

#include "src/tint/ast/templated_identifier_expression.h"

#include <utility>

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::TemplatedIdentifierExpression);

namespace tint::ast {

TemplatedIdentifierExpression::TemplatedIdentifierExpression(
    ProgramID pid,
    NodeID nid,
    const Source& src,
    const Symbol& sym,
    utils::VectorRef<const ast::Expression*> args)
    : Base(pid, nid, src, sym), arguments(std::move(args)) {
    TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, symbol, program_id);
    TINT_ASSERT(AST, symbol.IsValid());
    for (auto* arg : args) {
        TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, arg, program_id);
    }
}

TemplatedIdentifierExpression::TemplatedIdentifierExpression(TemplatedIdentifierExpression&&) =
    default;

TemplatedIdentifierExpression::~TemplatedIdentifierExpression() = default;

const TemplatedIdentifierExpression* TemplatedIdentifierExpression::Clone(CloneContext* ctx) const {
    // Clone arguments outside of create() call to have deterministic ordering
    auto src = ctx->Clone(source);
    auto sym = ctx->Clone(symbol);
    auto args = ctx->Clone(arguments);
    return ctx->dst->create<TemplatedIdentifierExpression>(src, sym, args);
}

}  // namespace tint::ast
