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

#include <string>
#include <unordered_map>

#include "src/tint/api/options/texture_builtins_from_uniform.h"
#include "src/tint/lang/glsl/writer/common/version.h"
#include "src/tint/lang/wgsl/sem/sampler_texture_pair.h"
#include "src/tint/utils/math/hash.h"

namespace tint::glsl::writer::binding {

/// Generic binding point
struct BindingInfo {
    /// The binding
    uint32_t binding = 0;

    /// Equality operator
    /// @param rhs the BindingInfo to compare against
    /// @returns true if this BindingInfo is equal to `rhs`
    inline bool operator==(const BindingInfo& rhs) const { return binding == rhs.binding; }
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

/// A combined texture/sampler pair
// Note, these are the WGSL binding points that are used to create the combined samplers
struct CombinedTextureSamplerPair {
    /// The WGSL texture binding
    BindingPoint texture = {};
    /// The WGSL sampler binding
    BindingPoint sampler = {};

    /// Equality operator
    /// @param rhs the CombinedTextureSamplerPair to compare against
    /// @returns true if this CombinedTextureSamplerPair is equal to `rhs`
    inline bool operator==(const CombinedTextureSamplerPair& rhs) const {
        return texture == rhs.texture && sampler == rhs.sampler;
    }

    /// Less then operator
    /// @param rhs the CombinedTextureSamplerPair to compare against
    /// @returns if this is less then rhs
    inline bool operator<(const CombinedTextureSamplerPair& rhs) const {
        if (texture < rhs.texture) {
            return true;
        }
        if (texture == rhs.texture) {
            return sampler < rhs.sampler;
        }
        return false;
    }

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(texture, sampler);
};

}  // namespace tint::glsl::writer::binding

namespace std {

/// Custom std::hash specialization for tint::glsl::writer::binding::BindingInfo
template <>
class hash<tint::glsl::writer::binding::BindingInfo> {
  public:
    /// @param n the binding info
    /// @return the hash value
    inline std::size_t operator()(const tint::glsl::writer::binding::BindingInfo& n) const {
        return tint::Hash(n.binding);
    }
};

/// Custom std::hash specialization for tint::glsl::writer::binding::CombinedTextureSamplerPair
template <>
class hash<tint::glsl::writer::binding::CombinedTextureSamplerPair> {
  public:
    /// @param n the combined sampler texture pair
    /// @return the hash value
    inline std::size_t operator()(
        const tint::glsl::writer::binding::CombinedTextureSamplerPair& n) const {
        return tint::Hash(n.texture, n.sampler);
    }
};

}  // namespace std

namespace tint::glsl::writer {

/// Maps the WGSL binding point to the SPIR-V group,binding for uniforms
using UniformBindings = std::unordered_map<BindingPoint, binding::Uniform>;
/// Maps the WGSL binding point to the SPIR-V group,binding for storage
using StorageBindings = std::unordered_map<BindingPoint, binding::Storage>;
/// Maps the WGSL binding point to the SPIR-V group,binding for textures
using TextureBindings = std::unordered_map<BindingPoint, binding::Texture>;
/// Maps the WGSL binding point to the SPIR-V group,binding for storage textures
using StorageTextureBindings = std::unordered_map<BindingPoint, binding::StorageTexture>;
/// Maps the WGSL binding point to the SPIR-V group,binding for samplers
using SamplerBindings = std::unordered_map<BindingPoint, binding::Sampler>;
/// Maps the WGSL binding point to the plane0, plane1, and metadata information for external
/// textures
using ExternalTextureBindings = std::unordered_map<BindingPoint, binding::ExternalTexture>;
/// Maps texture/sampler info to a combined sampler name
using CombinedTextureSamplerInfo =
    std::unordered_map<binding::CombinedTextureSamplerPair, std::string>;

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

    /// Combined texture/sampler information
    CombinedTextureSamplerInfo combined_texture_sampler_info{};

    /// The binding point to use for placeholder samplers.
    BindingPoint placeholder_sampler_bind_point;

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(uniform,
                 storage,
                 texture,
                 storage_texture,
                 sampler,
                 external_texture,
                 combined_texture_sampler_info,
                 placeholder_sampler_bind_point);
};

/// Sampler Texture Pair
using SamplerTexturePair = sem::SamplerTexturePair;
/// Binding Map
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

    /// Set to `true` to disable the polyfills on integer division and modulo.
    bool disable_polyfill_integer_div_mod = false;

    /// The GLSL version to emit
    Version version;

    /// Options used to map WGSL textureNumLevels/textureNumSamples builtins to internal uniform
    /// buffer values. If not specified, emits corresponding GLSL builtins
    /// textureQueryLevels/textureSamples directly.
    TextureBuiltinsFromUniformOptions texture_builtins_from_uniform = {};

    /// The bindings
    Bindings bindings{};

    /// Reflect the fields of this class so that it can be used by tint::ForeachField()
    TINT_REFLECT(disable_robustness,
                 disable_workgroup_init,
                 disable_polyfill_integer_div_mod,
                 version,
                 texture_builtins_from_uniform,
                 bindings);
};

}  // namespace tint::glsl::writer

#endif  // SRC_TINT_LANG_GLSL_WRITER_COMMON_OPTIONS_H_
