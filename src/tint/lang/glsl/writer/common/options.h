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

#ifndef SRC_TINT_LANG_GLSL_WRITER_COMMON_OPTIONS_H_
#define SRC_TINT_LANG_GLSL_WRITER_COMMON_OPTIONS_H_

#include <optional>
#include <string>
#include <unordered_map>

#include "src/tint/api/options/texture_builtins_from_uniform.h"
#include "src/tint/lang/core/access.h"
#include "src/tint/lang/glsl/writer/common/version.h"
#include "src/tint/lang/wgsl/sem/sampler_texture_pair.h"

namespace tint::glsl::writer {
namespace binding {

/// Generic binding point
struct BindingInfo {
    /// The group
    uint32_t group = 0;
    /// The binding
    uint32_t binding = 0;

    /// Equality operator
    /// @param rhs the BindingInfo to compare against
    /// @returns true if this BindingInfo is equal to `rhs`
    inline bool operator==(const BindingInfo& rhs) const {
        return group == rhs.group && binding == rhs.binding;
    }
    /// Inequality operator
    /// @param rhs the BindingInfo to compare against
    /// @returns true if this BindingInfo is not equal to `rhs`
    inline bool operator!=(const BindingInfo& rhs) const { return !(*this == rhs); }

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(group, binding);
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

// Maps the WGSL binding point to the SPIR-V group,binding for uniforms
using UniformBindings = std::unordered_map<BindingPoint, binding::Uniform>;
// Maps the WGSL binding point to the SPIR-V group,binding for storage
using StorageBindings = std::unordered_map<BindingPoint, binding::Storage>;
// Maps the WGSL binding point to the SPIR-V group,binding for textures
using TextureBindings = std::unordered_map<BindingPoint, binding::Texture>;
// Maps the WGSL binding point to the SPIR-V group,binding for storage textures
using StorageTextureBindings = std::unordered_map<BindingPoint, binding::StorageTexture>;
// Maps the WGSL binding point to the SPIR-V group,binding for samplers
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

using SamplerTexturePair = sem::SamplerTexturePair;
using BindingMap = std::unordered_map<SamplerTexturePair, std::string>;

/// Configuration options used for generating GLSL.
struct Options {
    /// Constructor
    Options();

    /// Destructor
    ~Options();

    /// Copy constructor
    Options(const Options&);

    /// Set to `true` to disable software robustness that prevents out-of-bounds accesses.
    bool disable_robustness = false;

    /// Set to `true` to disable workgroup memory zero initialization
    bool disable_workgroup_init = false;

    /// The GLSL version to emit
    Version version;

    /// A map of SamplerTexturePair to combined sampler names for the
    /// CombineSamplers transform
    BindingMap binding_map;

    /// The binding point to use for placeholder samplers.
    BindingPoint placeholder_binding_point;

    /// Options used to map WGSL textureNumLevels/textureNumSamples builtins to internal uniform
    /// buffer values. If not specified, emits corresponding GLSL builtins
    /// textureQueryLevels/textureSamples directly.
    std::optional<TextureBuiltinsFromUniformOptions> texture_builtins_from_uniform = std::nullopt;

    /// The bindings
    Bindings bindings;

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(disable_robustness,
                 disable_workgroup_init,
                 version,
                 binding_map,
                 placeholder_binding_point,
                 texture_builtins_from_uniform,
                 bindings);
};

}  // namespace tint::glsl::writer

#endif  // SRC_TINT_LANG_GLSL_WRITER_COMMON_OPTIONS_H_
