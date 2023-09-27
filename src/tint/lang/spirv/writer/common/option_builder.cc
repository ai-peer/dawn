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

#include "src/tint/lang/spirv/writer/common/option_builder.h"

namespace tint::spirv::writer {

// The remapped binding data and external texture data need to coordinate in order to put things in
// the correct place when we're done.
//
// When the data comes in we have a list of all WGSL origin (group,binding) pairs to SPIR-V
// (group,binding) pairs in the `uniform`, `storage`, `texture`, and `sampler` arrays.
//
// The `external_texture` array stores a WGSL origin (group,binding) pair for the external textures
// which provide `plane0`, `plane1`, and `metadata` SPIR-V (group,binding) pairs.
//
// If the remapper is run first, then the `external_texture` will end up being moved from the WGSL
// point, or the SPIR-V point (or the `plane0` value). There will also, possibly, have been bindings
// moved aside in order to place the `external_texture` bindings.
//
// If multiplanar runs first, care needs to be taken that when the texture is split and we create
// `plane1` and `metadata` that they do not collide with existing bindings. If they would collide
// then we need to place them elsewhere and have the remapper place them in the correct locations.
//
// # Example
// WGSL:
//   @group(0) @binding(0) var<uniform> u: Uniforms;
//   @group(0) @binding(1) var s: sampler;
//   @group(0) @binding(2) var t: texture_external;
//
// Given that program, Dawn may decide to do the remappings such that:
//   * WGSL u (0, 0) -> SPIR-V (0, 1)
//   * WGSL s (0, 1) -> SPIR-V (0, 2)
//   * WGSL t (0, 2):
//     * plane0 -> SPIR-V (0, 3)
//     * plane1 -> SPIR-V (0, 4)
//     * metadata -> SPIR-V (0, 0)
//
// In this case, if we run binding remapper first, then tell multiplanar to look for the texture at
// (0, 3) instead of the original (0, 2).
//
// If multiplanar runs first, then metadata (0, 0) needs to be placed elsewhere and then remapped
// back to (0, 0) by the remapper. (Otherwise, we'll have two `@group(0) @binding(0)` items in the
// program.)
//
// # Status
// The below method assumes we run binding remapper first. So it will setup the binding data and
// switch the value used by the multiplanar.
void PopulateRemapperAndMultiplanarOptions(const Options& options,
                                           RemapperData& remapper_data,
                                           ExternalTextureOptions& external_texture) {
    auto CreateRemappings = [&remapper_data](const auto& hsh) {
        for (const auto it : hsh) {
            const BindingPoint& srcBindingPoint = it.first;
            const binding::Uniform& dstBindingPoint = it.second;

            // Bindings which go to the same slot in SPIR-V do not need to be re-bound.
            if (srcBindingPoint.group == dstBindingPoint.group &&
                srcBindingPoint.binding == dstBindingPoint.binding) {
                continue;
            }

            remapper_data.emplace(srcBindingPoint,
                                  BindingPoint{dstBindingPoint.group, dstBindingPoint.binding});
        }
    };

    CreateRemappings(options.bindings.uniform);
    CreateRemappings(options.bindings.storage);
    CreateRemappings(options.bindings.texture);
    CreateRemappings(options.bindings.sampler);

    // External textures are re-bound to their plane0 location
    for (const auto it : options.bindings.external_texture) {
        const BindingPoint& srcBindingPoint = it.first;
        const binding::BindingInfo& plane0 = it.second.plane0;
        const binding::BindingInfo& plane1 = it.second.plane1;
        const binding::BindingInfo& metadata = it.second.metadata;

        BindingPoint plane0_binding_point{plane0.group, plane0.binding};
        BindingPoint plane1_binding_point{plane1.group, plane1.binding};
        BindingPoint metadata_binding_point{metadata.group, metadata.binding};

        // Use the re-bound spir-v plane0 value for the lookup key.
        external_texture.bindings_map.emplace(
            plane0_binding_point,
            ExternalTextureOptions::BindingPoints{plane1_binding_point, metadata_binding_point});

        // Bindings which go to the same slot in SPIR-V do not need to be re-bound.
        if (srcBindingPoint.group == plane0.group && srcBindingPoint.binding == plane0.binding) {
            continue;
        }

        remapper_data.emplace(srcBindingPoint, BindingPoint{plane0.group, plane0.binding});
    }
}

}  // namespace tint::spirv::writer
