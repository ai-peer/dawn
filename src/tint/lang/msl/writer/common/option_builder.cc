/// Copyright 2023 The Dawn & Tint Authors
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
            str << "found duplicate MSL binding point: [binding: " << info.binding << "]";
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
// (binding) in the `uniform`, `storage`, `texture`, and `sampler` arrays.
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

        // Use the re-bound msl plane0 value for the lookup key.
        external_texture.bindings_map.emplace(
            plane0_binding_point,
            ExternalTextureOptions::BindingPoints{plane1_binding_point, metadata_binding_point});

        // Bindings which go to the same slot in MSL do not need to be re-bound.
        if (src_binding_point == plane0_binding_point) {
            continue;
        }

        remapper_data.emplace(src_binding_point, plane0_binding_point);
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
        return tint::Hash(info.binding);
    }
};

}  // namespace std
