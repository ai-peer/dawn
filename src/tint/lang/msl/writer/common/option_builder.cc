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

#include "src/tint/lang/msl/writer/common/option_builder.h"

#include "src/tint/utils/containers/hashset.h"

namespace tint::msl::writer {

bool ValidateBindingOptions(const Options& options, diag::List& diagnostics) {
    tint::Hashset<tint::BindingPoint, 8> seen_wgsl_bindings{};
    tint::Hashset<binding::BindingInfo, 8> seen_msl_bindings{};

    auto wgsl_seen = [&diagnostics, &seen_wgsl_bindings](const tint::BindingPoint& info) -> bool {
        if (seen_wgsl_bindings.Contains(info)) {
            std::stringstream str;
            str << "found duplicate WGSL binding point: " << info;

            diagnostics.add_error(diag::System::Writer, str.str());
            return true;
        }
        seen_wgsl_bindings.Add(info);
        return false;
    };

    auto msl_seen = [&diagnostics, &seen_msl_bindings](const binding::BindingInfo& info) -> bool {
        if (seen_msl_bindings.Contains(info)) {
            std::stringstream str;
            str << "found duplicate MSL binding point: [resource type: " << info.type
                << ", binding: " << info.binding << "]";
            diagnostics.add_error(diag::System::Writer, str.str());
            return true;
        }
        seen_msl_bindings.Add(info);
        return false;
    };

    auto valid = [&wgsl_seen, &msl_seen](const auto& hsh) -> bool {
        for (const auto& it : hsh) {
            const auto& src_binding = it.first;
            const auto& dst_binding = it.second;

            if (wgsl_seen(src_binding)) {
                return false;
            }

            if (msl_seen(dst_binding)) {
                return false;
            }
        }
        return true;
    };

    if (!valid(options.bindings.uniform)) {
        diagnostics.add_note(diag::System::Writer, "when processing uniform", {});
        return false;
    }
    if (!valid(options.bindings.storage)) {
        diagnostics.add_note(diag::System::Writer, "when processing storage", {});
        return false;
    }
    if (!valid(options.bindings.texture)) {
        diagnostics.add_note(diag::System::Writer, "when processing texture", {});
        return false;
    }
    if (!valid(options.bindings.storage_texture)) {
        diagnostics.add_note(diag::System::Writer, "when processing storage_texture", {});
        return false;
    }
    if (!valid(options.bindings.sampler)) {
        diagnostics.add_note(diag::System::Writer, "when processing sampler", {});
        return false;
    }

    for (const auto& it : options.bindings.external_texture) {
        const auto& src_binding = it.first;
        const auto& plane0 = it.second.plane0;
        const auto& plane1 = it.second.plane1;
        const auto& metadata = it.second.metadata;

        // Validate with the actual source regardless of what the remapper will do
        if (wgsl_seen(src_binding)) {
            diagnostics.add_note(diag::System::Writer, "when processing external_texture", {});
            return false;
        }

        if (msl_seen(plane0)) {
            diagnostics.add_note(diag::System::Writer, "when processing external_texture", {});
            return false;
        }
        if (msl_seen(plane1)) {
            diagnostics.add_note(diag::System::Writer, "when processing external_texture", {});
            return false;
        }
        if (msl_seen(metadata)) {
            diagnostics.add_note(diag::System::Writer, "when processing external_texture", {});
            return false;
        }
    }

    return true;
}

// The remapped binding data and external texture data need to coordinate in order to put things in
// the correct place when we're done.
//
// When the data comes in we have a list of all WGSL origin (group,binding) pairs to MSL
// (resource_type,binding) pairs in the `uniform`, `storage`, `texture`, and `sampler` arrays.
//
// MSL always sets the destination `group` value to `0` as MSL doesn't have the concept of `group`.
//
// The printer will execute the multiplanar transform first, which means we need to emit the
// multiplanar `metadata` and `plane1` into a previously unused `group` such that when the remapper
// executes we don't accidentally confuse it with another existing binding. We then remap the
// emitted `metadata` and `plane1` data back to `group` 0.
void PopulateRemapperAndMultiplanarOptions(const Options& options,
                                           RemapperData& remapper_data,
                                           ExternalTextureOptions& external_texture) {
    auto create_remappings = [&remapper_data](const auto& hsh) {
        for (const auto& it : hsh) {
            const BindingPoint& src_binding_point = it.first;
            const binding::Uniform& dst_binding_point = it.second;

            // Bindings which go to the same slot in MSL do not need to be re-bound.
            if (src_binding_point.group == 0 &&
                src_binding_point.binding == dst_binding_point.binding) {
                continue;
            }

            remapper_data.emplace(src_binding_point, BindingPoint{0, dst_binding_point.binding});
        }
    };

    create_remappings(options.bindings.uniform);
    create_remappings(options.bindings.storage);
    create_remappings(options.bindings.texture);
    create_remappings(options.bindings.storage_texture);
    create_remappings(options.bindings.sampler);

    // External textures are re-bound to their plane0 location
    for (const auto& it : options.bindings.external_texture) {
        const BindingPoint& src_binding_point = it.first;
        const binding::BindingInfo& plane0 = it.second.plane0;
        const binding::BindingInfo& plane1 = it.second.plane1;
        const binding::BindingInfo& metadata = it.second.metadata;

        BindingPoint plane0_binding_point{0, plane0.binding};
        BindingPoint plane1_binding_point{0, plane1.binding};
        BindingPoint metadata_binding_point{0, metadata.binding};

        external_texture.bindings_map.emplace(
            src_binding_point,
            ExternalTextureOptions::BindingPoints{plane1_binding_point, metadata_binding_point});

        // Bindings which go to the same slot in SPIR-V do not need to be re-bound.
        if (src_binding_point.group == plane0.group &&
            src_binding_point.binding == plane0.binding) {
            continue;
        }

        remapper_data.emplace(src_binding_point, BindingPoint{plane0.group, plane0.binding});
    }
}

}  // namespace tint::msl::writer

namespace std {

/// Custom std::hash specialization for tint::msl::writer::binding::BindingInfo so
/// they can be used as keys for std::unordered_map and std::unordered_set.
template <>
class hash<tint::msl::writer::binding::BindingInfo> {
  public:
    /// @param info the binding to create a hash for
    /// @return the hash value
    inline std::size_t operator()(const tint::msl::writer::binding::BindingInfo& info) const {
        return tint::Hash(info.type, info.binding);
    }
};

}  // namespace std
