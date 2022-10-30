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

#include "src/tint/ir/loop.h"

#include "src/tint/ast/for_loop_statement.h"
#include "src/tint/ast/loop_statement.h"
#include "src/tint/ast/while_statement.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::Loop);

namespace tint::ir {

Loop::Loop(const ast::Statement* stmt) : Base(), source(stmt) {
    TINT_ASSERT(IR, stmt->Is<ast::LoopStatement>() || stmt->Is<ast::WhileStatement>() ||
                        stmt->Is<ast::ForLoopStatement>());
}

Loop::~Loop() = default;

}  // namespace tint::ir
