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

#ifndef SRC_TINT_AST_EXTENSION_H_
#define SRC_TINT_AST_EXTENSION_H_

#include <string>
#include <unordered_set>
#include <utility>

#include "src/tint/ast/access.h"
#include "src/tint/ast/expression.h"

namespace tint {
namespace ast {

/// An instance of this class represents one extension mentioned in a
/// "enable" derictive. Example:
///       // Enable an extension named "f16"
///       enable f16;
class Extension : public Castable<Extension, Node> {
 public:
  enum class Type {
    // TODO(Zhaoming): Shall we have a internal reserved extension name and type
    // for test?
    // kInternalExtensionForTintTest = -2,
    kNotAnExtension = -1,
    /*
    TODO(Zhaoming): Define a type for each supported extension.
    For example:
    ```
        kF16,
    ```
    */
  };

  /// Convert a string of extension name into one of Type enum value,
  /// the result will be Type::kNotAnExtension if the name is not a
  /// known extension name. A extension node of type kNotAnExtension must not
  /// exist in the AST tree, and using a unknown extension name in WGSL code
  /// should result in a shader-creation error.
  /// @param name string of the extension name
  /// @return the Type enum value for the extension of given name, or
  /// kNotAnExtension if no known extension has the given name
  static Type NameToType(const std::string& name);

  /// Convert the Type enum value to corresponding extension name
  /// string. If the given enum value is kNotAnExtension or don't have a known
  /// name, return an empty string instead.
  /// @param type the Type enum value
  /// @return string of the extension name corresponding to the given type, or
  /// an empty string if the given enum value is kNotAnExtension or don't have a
  /// known corresponding name
  static std::string TypeToName(Type type);

  /// Create a extension
  /// @param name the name of extension
  Extension(ProgramID pid, const Source& src, const std::string& name);
  /// Move constructor
  Extension(Extension&&);

  ~Extension() override;

  /// Clones this node and all transitive child nodes using the `CloneContext`
  /// `ctx`.
  /// @param ctx the clone context
  /// @return the newly cloned node
  const Extension* Clone(CloneContext* ctx) const override;

  /// The extension name
  const std::string name;

  /// The extension type
  const Type type;
};

///  A set of extension types
using ExtensionSet = std::unordered_set<Extension::Type>;

}  // namespace ast
}  // namespace tint

#endif  // SRC_TINT_AST_EXTENSION_H_
