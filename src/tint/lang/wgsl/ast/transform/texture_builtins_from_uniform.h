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

#ifndef SRC_TINT_TEXTURE_BUILTINS_FROM_UNIFORM_H_
#define SRC_TINT_TEXTURE_BUILTINS_FROM_UNIFORM_H_

#include <unordered_map>
#include <unordered_set>

#include "src/tint/lang/wgsl/ast/transform/transform.h"
#include "tint/binding_point.h"

#include "tint/texture_builtins_from_uniform_options.h"

// Forward declarations
namespace tint {
class CloneContext;
}  // namespace tint

namespace tint::ast::transform {

/// TextureBuiltinsFromUniform is a transform that implements calls to textureNumLevels() and
/// textureNumSamples() by retrieving the texture information from a uniform buffer. As counterparts
/// builtin functions are not available in some version of GLSL.
///
/// The generated uniform buffer will have the form:
/// ```
/// struct internal_uniform {
///  texture_builtin_values : array<vec4<u32>, 1u>,
/// };
///
/// @group(0) @binding(0) var tex : texture_2d<f32>;
/// ```
/// The binding group and number used for this uniform buffer is provided via
/// the `Config` transform input.
///
/// This transform must run before `CombineSamplers` transform so that the binding point of the
/// original texture object can be preserved.
class TextureBuiltinsFromUniform final : public Castable<TextureBuiltinsFromUniform, Transform> {
  public:
    /// Constructor
    TextureBuiltinsFromUniform();
    /// Destructor
    ~TextureBuiltinsFromUniform() override;

    /// Configuration options for the TextureBuiltinsFromUniform transform.
    struct Config final : public Castable<Config, Data> {
        /// Constructor
        /// @param ubo_bp the binding point to use for the generated uniform buffer.
        explicit Config(BindingPoint ubo_bp);

        /// Copy constructor
        Config(const Config&);

        /// Copy assignment
        /// @return this Config
        Config& operator=(const Config&);

        /// Destructor
        ~Config() override;

        /// The binding point to use for the generated uniform buffer.
        BindingPoint ubo_binding;
    };

    /// Information produced about what the transform did.
    /// If there were no calls to the textureNumLevels() or textureNumSamples() builtin, then no
    /// Result will be emitted.
    struct Result final : public Castable<Result, Data> {
        /// Using for shorter names
        /// Records the field and the byte offset of the data to push in the internal uniform
        /// buffer.
        using DataEntry = TextureBuiltinsFromUniformOptions::DataEntry;
        /// Maps from binding point to data entry with the information to populate the data.
        using BindingPointDataInfo = TextureBuiltinsFromUniformOptions::BindingPointDataInfo;

        /// Constructor
        /// @param bindpoint_to_data_in mapping from bind point of global texture variables to the
        /// byte offset and data type needed to be pushed into the internal uniform buffer.
        explicit Result(BindingPointDataInfo bindpoint_to_data_in);

        /// Copy constructor
        Result(const Result&);

        /// Destructor
        ~Result() override;

        /// A map of globl texture variable binding point to the byte offset and data type to push
        /// into the internal uniform buffer.
        BindingPointDataInfo bindpoint_to_data;
    };

    /// @copydoc Transform::Apply
    ApplyResult Apply(const Program* program,
                      const DataMap& inputs,
                      DataMap& outputs) const override;

  private:
    struct State;
};

}  // namespace tint::ast::transform

#endif  // SRC_TINT_TEXTURE_BUILTINS_FROM_UNIFORM_H_
