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

#ifndef SRC_TINT_RESOLVER_SEM_HELPER_H_
#define SRC_TINT_RESOLVER_SEM_HELPER_H_

#include <string>

#include "src/tint/diagnostic/diagnostic.h"
#include "src/tint/program_builder.h"
#include "src/tint/resolver/dependency_graph.h"
#include "src/tint/utils/map.h"

namespace tint::resolver {
class SemHelper {
 public:
  SemHelper(ProgramBuilder* builder, DependencyGraph& dependencies);
  ~SemHelper();

  /// Sem is a helper for obtaining the semantic node for the given AST node.
  template <typename SEM = sem::Info::InferFromAST,
            typename AST_OR_TYPE = CastableBase>
  auto* Sem(const AST_OR_TYPE* ast) const {
    using T = sem::Info::GetResultType<SEM, AST_OR_TYPE>;
    auto* sem = builder_->Sem().Get(ast);
    if (!sem) {
      TINT_ICE(Resolver, builder_->Diagnostics())
          << "AST node '" << ast->TypeInfo().name << "' had no semantic info\n"
          << "At: " << ast->source << "\n"
          << "Pointer: " << ast;
    }
    return const_cast<T*>(As<T>(sem));
  }

  /// @returns the resolved symbol (function, type or variable) for the given
  /// ast::Identifier or ast::TypeName cast to the given semantic type.
  template <typename SEM = sem::Node>
  SEM* ResolvedSymbol(const ast::Node* node) const {
    auto* resolved = utils::Lookup(dependencies_.resolved_symbols, node);
    return resolved ? const_cast<SEM*>(builder_->Sem().Get<SEM>(resolved))
                    : nullptr;
  }

  /// @returns the resolved type of the ast::Expression `expr`
  /// @param expr the expression
  sem::Type* TypeOf(const ast::Expression* expr) const;

  /// @returns the semantic type of the AST literal `lit`
  /// @param lit the literal
  sem::Type* TypeOf(const ast::LiteralExpression* lit);

  /// @returns the type name of the given semantic type, unwrapping
  /// references.
  std::string TypeNameOf(const sem::Type* ty) const;

  /// @returns the type name of the given semantic type, without unwrapping
  /// references.
  std::string RawTypeNameOf(const sem::Type* ty) const;

 private:
  ProgramBuilder* builder_;
  DependencyGraph& dependencies_;
};

}  // namespace tint::resolver

#endif  // SRC_TINT_RESOLVER_SEM_HELPER_H_
