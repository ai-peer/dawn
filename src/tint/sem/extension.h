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

#ifndef SRC_TINT_SEM_EXTENSION_H_
#define SRC_TINT_SEM_EXTENSION_H_

#include "src/tint/ast/enable.h"
#include "src/tint/sem/node.h"

namespace tint::sem {

/// Expression holds the semantic information for expression nodes.
class Extension : public Castable<Extension, Node> {
 public:
  /// Constructor
  /// @param declaration the AST node
  explicit Extension(const ast::Enable* declaration);

  /// Destructor
  ~Extension() override;

  /// @returns the AST node
  const ast::Enable* Declaration() const { return declaration_; }

 protected:
  /// The AST enable node for this extension
  const ast::Enable* const declaration_;
};

}  // namespace tint::sem

#endif  // SRC_TINT_SEM_EXTENSION_H_
