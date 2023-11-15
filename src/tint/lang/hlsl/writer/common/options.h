// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_TINT_LANG_HLSL_WRITER_COMMON_OPTIONS_H_
#define SRC_TINT_LANG_HLSL_WRITER_COMMON_OPTIONS_H_

#include <bitset>
#include <optional>
#include <unordered_map>
#include <vector>

#include "src/tint/api/common/binding_point.h"
#include "src/tint/api/options/array_length_from_uniform.h"
#include "src/tint/api/options/binding_remapper.h"
#include "src/tint/api/options/external_texture.h"
#include "src/tint/lang/core/access.h"
#include "src/tint/utils/reflection/reflection.h"

namespace tint::hlsl::writer {
namespace binding {

/// The HLSL register type.
/// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-variable-register
enum class RegisterType : uint8_t {
    kConstantBuffer,       // b#
    kTexture,              // t#
    kBufferOffset,         // c#
    kSampler,              // s#
    kUnorderedAccessView,  // u#
};

/// Generic binding point
struct BindingInfo {
    /// The binding
    uint32_t binding = 0;
    /// The space
    uint32_t space = 0;
    /// The register type
    RegisterType register_type;

    /// Equality operator
    /// @param rhs the BindingInfo to compare against
    /// @returns true if this BindingInfo is equal to `rhs`
    inline bool operator==(const BindingInfo& rhs) const {
        return binding == rhs.binding && register_type == rhs.register_type;
    }
    /// Inequality operator
    /// @param rhs the BindingInfo to compare against
    /// @returns true if this BindingInfo is not equal to `rhs`
    inline bool operator!=(const BindingInfo& rhs) const { return !(*this == rhs); }

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(binding);
};
using Uniform = BindingInfo;
using Storage = BindingInfo;
using Texture = BindingInfo;
using StorageTexture = BindingInfo;
using Sampler = BindingInfo;

/// An external texture
struct ExternalTexture {
    /// Metadata
    BindingInfo metadata{};
    /// Plane0 binding data
    BindingInfo plane0{};
    /// Plane1 binding data
    BindingInfo plane1{};

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(metadata, plane0, plane1);
};

}  // namespace binding

// Maps the WGSL binding point to the HLSL binding for uniforms
using UniformBindings = std::unordered_map<BindingPoint, binding::Uniform>;
// Maps the WGSL binding point to the HLSL binding for storage
using StorageBindings = std::unordered_map<BindingPoint, binding::Storage>;
// Maps the WGSL binding point to the HLSL binding for textures
using TextureBindings = std::unordered_map<BindingPoint, binding::Texture>;
// Maps the WGSL binding point to the HLSL binding for storage textures
using StorageTextureBindings = std::unordered_map<BindingPoint, binding::StorageTexture>;
// Maps the WGSL binding point to the HLSL binding for samplers
using SamplerBindings = std::unordered_map<BindingPoint, binding::Sampler>;
// Maps the WGSL binding point to the plane0, plane1, and metadata information for external textures
using ExternalTextureBindings = std::unordered_map<BindingPoint, binding::ExternalTexture>;

/// Binding information
struct Bindings {
    /// Uniform bindings
    UniformBindings uniform{};
    /// Storage bindings
    StorageBindings storage{};
    /// Texture bindings
    TextureBindings texture{};
    /// Storage texture bindings
    StorageTextureBindings storage_texture{};
    /// Sampler bindings
    SamplerBindings sampler{};
    /// External bindings
    ExternalTextureBindings external_texture{};

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(uniform, storage, texture, storage_texture, sampler, external_texture);
};

/// kMaxInterStageLocations == D3D11_PS_INPUT_REGISTER_COUNT - 2
/// D3D11_PS_INPUT_REGISTER_COUNT == D3D12_PS_INPUT_REGISTER_COUNT
constexpr uint32_t kMaxInterStageLocations = 30;

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

    /// Set to `true` to disable workgroup memory zero initialization
    bool disable_workgroup_init = false;

    /// Set to `true` to run the TruncateInterstageVariables transform.
    bool truncate_interstage_variables = false;

    /// Set to `true` to generate polyfill for `reflect` builtin for vec2<f32>
    bool polyfill_reflect_vec2_f32 = false;

    /// Options used to specify a mapping of binding points to indices into a UBO
    /// from which to load buffer sizes.
    ArrayLengthFromUniformOptions array_length_from_uniform = {};

    /// Interstage locations actually used as inputs in the next stage of the pipeline.
    /// This is potentially used for truncating unused interstage outputs at current shader stage.
    std::bitset<kMaxInterStageLocations> interstage_locations;

    /// The binding point to use for information passed via root constants.
    std::optional<BindingPoint> root_constant_binding_point;

    /// The binding points that will be ignored in the rebustness transform.
    std::vector<BindingPoint> binding_points_ignored_in_robustness_transform;

    /// AccessControls is a map of binding point to new access control
    std::unordered_map<BindingPoint, core::Access> access_controls;

    /// Bindings
    Bindings bindings;

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(disable_robustness,
                 disable_workgroup_init,
                 truncate_interstage_variables,
                 polyfill_reflect_vec2_f32,
                 array_length_from_uniform,
                 interstage_locations,
                 root_constant_binding_point,
                 binding_points_ignored_in_robustness_transform,
                 access_controls,
                 bindings);
};

}  // namespace tint::hlsl::writer

#endif  // SRC_TINT_LANG_HLSL_WRITER_COMMON_OPTIONS_H_
