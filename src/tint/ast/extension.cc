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

#include "src/tint/ast/extension.h"

#include "src/tint/program_builder.h"
#include "src/tint/sem/variable.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::Extension);

namespace tint {
namespace ast {

Extension::Kind Extension::NameToKind(const std::string& name) {
  /*
  TODO(Zhaoming): Deal with each supported extension, convert its name to
  corresponding kind. For example:
  ```
      if (name == "f16") {
        return Extension::Kind::kF16;
      }
  ```
  */
  // The reserved internal extension name for testing
  if (name == "InternalExtensionForTesting") {
    return Extension::Kind::kInternalExtensionForTesting;
  }

  return Extension::Kind::kNotAnExtension;
}

std::string Extension::KindToName(Kind kind) {
  switch (kind) {
    /*
    TODO(Zhaoming): Deal with each supported extension, convert its kind to
    corresponding name. For example:
    ```
        case (Kind::kF16): {
          return "f16";
        }
    ```
    */
    // The reserved internal extension for testing
    case Kind::kInternalExtensionForTesting: {
      return "InternalExtensionForTesting";
    }
    case Kind::kNotAnExtension: {
      // Return an empty string for kNotAnExtension
      return {};
    }
    default:
      // Return an empty string in case that we don't know the name
      // for the given extension kind
      return {};
  }
}

Extension::Extension(ProgramID pid, const Source& src, const std::string& name)
    : Base(pid, src), name(name), kind(NameToKind(name)) {}

Extension::Extension(Extension&&) = default;

Extension::~Extension() = default;

const Extension* Extension::Clone(CloneContext* ctx) const {
  auto src = ctx->Clone(source);
  return ctx->dst->create<Extension>(src, name);
}
}  // namespace ast
}  // namespace tint
