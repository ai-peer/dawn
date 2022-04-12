// Copyright 2021 The Tint Authors.
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

#ifndef SRC_TINT_TRANSFORM_MULTIPLANAR_EXTERNAL_TEXTURE_H_
#define SRC_TINT_TRANSFORM_MULTIPLANAR_EXTERNAL_TEXTURE_H_

#include <unordered_map>
#include <utility>

#include "src/tint/ast/struct_member.h"
#include "src/tint/sem/binding_point.h"
#include "src/tint/sem/builtin_type.h"
#include "src/tint/transform/transform.h"
#include "src/tint/writer/multiplanar_external_texture_options.h"

namespace tint::transform {

/// BindingPoint alias
using BindingPoint = writer::MultiplanarExternalTextureOptions::BindingPoint;

// BindingPoints alias
using BindingPoints = writer::MultiplanarExternalTextureOptions::BindingPoints;

/// Within the MultiplanarExternalTexture transform, each instance of a
/// texture_external binding is unpacked into two texture_2d<f32> bindings
/// representing two possible planes of a texture and a uniform buffer binding
/// representing a struct of parameters. Calls to textureLoad or
/// textureSampleLevel that contain a texture_external parameter will be
/// transformed into a newly generated version of the function, which can
/// perform the desired operation on a single RGBA plane or on seperate Y and UV
/// planes.
class MultiplanarExternalTexture
    : public Castable<MultiplanarExternalTexture, Transform> {
 public:
  /// BindingsMap alias
  using BindingsMap = writer::MultiplanarExternalTextureOptions::BindingsMap;

  /// NewBindingPoints is consumed by the MultiplanarExternalTexture transform.
  /// Data holds information about location of each texture_external binding and
  /// which binding slots it should expand into.
  struct NewBindingPoints : public Castable<Data, transform::Data> {
    /// Constructor
    /// @param bm a map to the new binding slots to use.
    explicit NewBindingPoints(BindingsMap bm);

    /// Destructor
    ~NewBindingPoints() override;

    /// A map of new binding points to use.
    const BindingsMap bindings_map;
  };

  /// Constructor
  MultiplanarExternalTexture();
  /// Destructor
  ~MultiplanarExternalTexture() override;

  /// @param program the program to inspect
  /// @param data optional extra transform-specific input data
  /// @returns true if this transform should be run for the given program
  bool ShouldRun(const Program* program,
                 const DataMap& data = {}) const override;

 protected:
  struct State;

  /// Runs the transform using the CloneContext built for transforming a
  /// program. Run() is responsible for calling Clone() on the CloneContext.
  /// @param ctx the CloneContext primed with the input program and
  /// ProgramBuilder
  /// @param inputs optional extra transform-specific input data
  /// @param outputs optional extra transform-specific output data
  void Run(CloneContext& ctx,
           const DataMap& inputs,
           DataMap& outputs) const override;
};

}  // namespace tint::transform

#endif  // SRC_TINT_TRANSFORM_MULTIPLANAR_EXTERNAL_TEXTURE_H_
