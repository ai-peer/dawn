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

#ifndef SRC_TINT_WRITER_MULTIPLANAR_EXTERNAL_TEXTURE_OPTIONS_H_
#define SRC_TINT_WRITER_MULTIPLANAR_EXTERNAL_TEXTURE_OPTIONS_H_

#include <unordered_map>

#include "src/tint/sem/binding_point.h"

namespace tint::writer {

/// Options used to specify the mapping of external texture binding points to
/// the pair of binding points for the texture_2d plane #1 and the uniform.
struct MultiplanarExternalTextureOptions {
  /// Constructor
  MultiplanarExternalTextureOptions();
  /// Destructor
  ~MultiplanarExternalTextureOptions();
  /// Copy constructor
  MultiplanarExternalTextureOptions(const MultiplanarExternalTextureOptions&);
  /// Copy assignment
  /// @returns this MultiplanarExternalTextureOptions
  MultiplanarExternalTextureOptions& operator=(
      const MultiplanarExternalTextureOptions&);
  /// Move constructor
  MultiplanarExternalTextureOptions(MultiplanarExternalTextureOptions&&);

  /// BindingPoint is an alias to sem::BindingPoint
  using BindingPoint = sem::BindingPoint;

  /// This struct identifies the binding groups and locations for new bindings
  /// to use when transforming a texture_external instance.
  struct BindingPoints {
    /// The desired binding location of the texture_2d representing plane #1
    /// when a texture_external binding is expanded.
    BindingPoint plane_1;
    /// The desired binding location of the ExternalTextureParams uniform when a
    /// texture_external binding is expanded.
    BindingPoint params;
  };

  /// BindingsMap is a map where the key is the binding location of a
  /// texture_external and the value is a struct containing the desired
  /// locations for new bindings expanded from the texture_external instance.
  using BindingsMap = std::unordered_map<BindingPoint, BindingPoints>;

  /// A map of new binding points to use.
  BindingsMap bindings_map;

  // NOTE: Update src/tint/fuzzers/data_builder.h when adding or changing any
  // struct members.
};

}  // namespace tint::writer

#endif  // SRC_TINT_WRITER_MULTIPLANAR_EXTERNAL_TEXTURE_OPTIONS_H_
