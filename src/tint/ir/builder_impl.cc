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

#include "src/tint/ir/builder_impl.h"

#include "src/tint/ir/module.h"
#include "src/tint/program.h"
#include "src/tint/sem/module.h"
#include "src/tint/ast/alias.h"
#include "src/tint/ast/static_assert.h"
#include "src/tint/ast/function.h"

namespace tint::ir {

BuilderImpl::BuilderImpl(const Program* program) : program_(program), ir_(program) {}

BuilderImpl::~BuilderImpl() = default;

bool BuilderImpl::Build() {
  auto* sem = program_->Sem().Module();

  for (auto* decl : sem->DependencyOrderedDeclarations()) {
    bool ok = Switch(
      decl,  //
      // [&](const ast::Struct* str) {
      //   return false;
      // },
      [&](const ast::Alias*) {
        // Folded away and doesn't appear in the IR.
        return true;
      },
      // [&](const ast::Const*) {
      //   return false;
      // },
      // [&](const ast::Override*) {
      //   return false;
      // },
      [&](const ast::Function* func) {
       return EmitFunction(func);
      },
      // [&](const ast::Enable*) {
      //   return false;
      // },
      [&](const ast::StaticAssert*) {
        // Evaluated by the resolver, drop from the IR.
        return true;
      },
      [&](Default) {
          TINT_ICE(IR, diagnostics_) << "unhandled type: " << decl->TypeInfo().name;
          return false;
      });
    if (!ok) {
      return false;
    }
  }

  return true;
}

Function* BuilderImpl::Func(const ast::Function* f) {
  return functions_.Create<ir::Function>(f);
}

bool BuilderImpl::EmitFunction(const ast::Function* ast_func) {
  if (ast_func->IsEntryPoint()) {
    entry_points_.Push(functions_.Count());
  }

  auto* ir_func = Function(ast_func);


  ir_func->control_flow.Push(flow_nodes_.Create<ir::BlockFlowNode>());

  // auto* sem = program_->Sem().Get(func);

  return false;
}

}  // namespace tint::ir
