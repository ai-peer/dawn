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

// TODO: ValuesFromUniform

/// ArrayLengthFromUniform is a transform that implements calls to arrayLength()
/// by calculating the length from the total size of the storage buffer, which
/// is received via a uniform buffer.
///
/// The generated uniform buffer will have the form:
/// ```
/// struct buffer_size_struct {
///  buffer_size : array<u32, 8>;
/// };
///
/// @group(0) @binding(30)
/// var<uniform> buffer_size_ubo : buffer_size_struct;
/// ```
/// The binding group and number used for this uniform buffer is provided via
/// the `Config` transform input. The `Config` struct also defines the mapping
/// from a storage buffer's `BindingPoint` to the array index that will be used
/// to get the size of that buffer.
///
/// This transform assumes that the `SimplifyPointers`
/// transforms have been run before it so that arguments to the arrayLength
/// builtin always have the form `&resource.array`.
///
/// @note Depends on the following transforms to have been run first:
/// * SimplifyPointers
class TextureBuiltinsFromUniform final
    : public utils::Castable<TextureBuiltinsFromUniform, Transform> {
  public:
    /// Constructor
    TextureBuiltinsFromUniform();
    /// Destructor
    ~TextureBuiltinsFromUniform() override;

    /// Configuration options for the TextureBuiltinsFromUniform transform.
    // TODO: this needs to be an option under writer generator
    struct Config final : public utils::Castable<Config, Data> {
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
    /// If there were no calls to the arrayLength() builtin, then no Result will
    /// be emitted.
    struct Result final : public utils::Castable<Result, Data> {
        // bindingPoint -> dataType, offset
        using DataEntry = TextureBuiltinsFromUniformOptions::DataEntry;
        using BindingPointDataInfo = TextureBuiltinsFromUniformOptions::BindingPointDataInfo;

        /// Constructor
        /// @param used_size_indices Indices into the UBO that are statically used.
        explicit Result(BindingPointDataInfo bindpoint_to_data_in);

        /// Copy constructor
        Result(const Result&);

        /// Destructor
        ~Result() override;

        BindingPointDataInfo bindpoint_to_data;
        // std::unordered_map<tint::TextureBuiltinsFromUniformOptions::DataType, uint32_t> offsets;
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
