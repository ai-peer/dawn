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

#ifndef SRC_TINT_LANG_HLSL_WRITER_COMMON_OPTIONS_H_
#define SRC_TINT_LANG_HLSL_WRITER_COMMON_OPTIONS_H_

#include <bitset>
#include <optional>
#include <vector>

#include "src/tint/api/common/binding_point.h"
#include "src/tint/api/options/array_length_from_uniform.h"
#include "src/tint/api/options/binding_remapper.h"
#include "src/tint/api/options/external_texture.h"
#include "src/tint/utils/reflection/reflection.h"

namespace tint::hlsl::writer {
namespace binding {

/// The HLSL register type.
/// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-variable-register
enum class RegisterType {
    kNone,
    kConstantBuffer,       // b#
    kTexture,              // t#
    kBufferOffset,         // c#
    kSampler,              // s#
    kUnorderedAccessView,  // u#
};

/// Uniform binding
struct Uniform {
    /// The binding value
    uint32_t value = 0;
};

/// Storage binding
struct Storage {
    /// The binding value
    uint32_t value = 0;
    /// The register space
    uint32_t space = 0;
    /// The binding register type
    RegisterType register_type;
};

/// Texture binding
struct Texture {
    /// The binding value
    uint32_t value = 0;
};

/// Sampler binding
struct Sampler {
    /// The binding value
    uint32_t value = 0;
};

/// External texture binding
struct ExternalTexture {
    /// The binding metadata
    Uniform metadata;
    /// The binding value for plane 0
    uint32_t plane0;
    /// The binding value for plane 1
    uint32_t plane1;
};

}  // namespace binding

/// Configuration options used for generating HLSL.
struct Options {
    /// Constructor
    Options();
    /// Destructor
    ~Options();
    /// Copy constructor
    Options(const Options&);
    /// Copy assignment
    /// @returns this Options
    Options& operator=(const Options&);

    /// Set to `true` to disable software robustness that prevents out-of-bounds accesses.
    bool disable_robustness = false;

    /// The binding point to use for information passed via root constants.
    std::optional<BindingPoint> root_constant_binding_point;

    /// Set to `true` to disable workgroup memory zero initialization
    bool disable_workgroup_init = false;

    /// Options used in the binding mappings for external textures
    ExternalTextureOptions external_texture_options = {};

    /// Options used to specify a mapping of binding points to indices into a UBO
    /// from which to load buffer sizes.
    ArrayLengthFromUniformOptions array_length_from_uniform = {};

    /// Options used in the bindings remapper
    BindingRemapperOptions binding_remapper_options = {};

    /// Interstage locations actually used as inputs in the next stage of the pipeline.
    /// This is potentially used for truncating unused interstage outputs at current shader stage.
    std::bitset<16> interstage_locations;

    /// Set to `true` to run the TruncateInterstageVariables transform.
    bool truncate_interstage_variables = false;

    /// Set to `true` to generate polyfill for `reflect` builtin for vec2<f32>
    bool polyfill_reflect_vec2_f32 = false;

    /// The binding points that will be ignored in the rebustness transform.
    std::vector<BindingPoint> binding_points_ignored_in_robustness_transform;

    /// TODO(crbug/tint/1501): The below are not fully hooked up yet, use the above options to
    /// configure the transforms.

    /// Uniform binding points
    std::unordered_map<BindingPoint, binding::Uniform> uniforms;
    /// Storage binding points
    std::unordered_map<BindingPoint, binding::Storage> storage;
    /// Texture binding points
    std::unordered_map<BindingPoint, binding::Texture> texture;
    /// Sampler binding points
    std::unordered_map<BindingPoint, binding::Sampler> sampler;
    /// External texture binding points
    std::unordered_map<BindingPoint, binding::ExternalTexture> external_texture;

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(disable_robustness,
                 root_constant_binding_point,
                 disable_workgroup_init,
                 external_texture_options,
                 array_length_from_uniform,
                 binding_remapper_options,
                 interstage_locations,
                 truncate_interstage_variables,
                 polyfill_reflect_vec2_f32,
                 array_length_from_uniform,
                 interstage_locations,
                 root_constant_binding_point,
                 external_texture_options,
                 binding_remapper_options,
                 binding_points_ignored_in_robustness_transform,
                 uniforms,
                 storage,
                 texture,
                 sampler,
                 external_texture);
};

}  // namespace tint::hlsl::writer

#endif  // SRC_TINT_LANG_HLSL_WRITER_COMMON_OPTIONS_H_
