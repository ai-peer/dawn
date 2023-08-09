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

#ifndef SRC_TINT_TEXTURE_BUILTINS_FROM_UNIFORM_OPTIONS_H_
#define SRC_TINT_TEXTURE_BUILTINS_FROM_UNIFORM_OPTIONS_H_

#include <unordered_map>
#include <utility>

#include "tint/binding_point.h"

namespace tint {

/// Options used to specify a mapping of binding points to indices into a UBO
/// from which to load buffer sizes.
/// We may want to apply this to all values from uniform in the future.
/// e.g. array length, num work groups, push constants, etc.
struct TextureBuiltinsFromUniformOptions {
    enum class DataType {
        TextureNumLevels,
        TextureNumSamples,
    };

    using DataEntry = std::pair<TextureBuiltinsFromUniformOptions::DataType, uint32_t>;
    using BindingPointDataInfo = std::unordered_map<BindingPoint, DataEntry>;

    /// The binding point to use to generate a uniform buffer from which to read
    /// buffer sizes. Default to a {max bind group+1, last binding}.
    BindingPoint ubo_binding = {5, 30};

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(ubo_binding);
};

}  // namespace tint

#endif  // SRC_TINT_TEXTURE_BUILTINS_FROM_UNIFORM_OPTIONS_H_
