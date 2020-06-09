// Copyright 2020 The Dawn Authors
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

#ifndef DAWNNATIVE_VULKAN_VULKANEXTENSIONS_H_
#define DAWNNATIVE_VULKAN_VULKANEXTENSIONS_H_

#include <bitset>
#include <unordered_map>

namespace dawn_native { namespace vulkan {

    extern const uint32_t VulkanVersion_1_1;
    extern const uint32_t NeverPromoted;

    // The list of known instance extensions. They must be in dependency order (this is checked
    // inside EnsureDependencies)
    enum class InstanceExt {
        GetPhysicalDeviceProperties2,
        ExternalMemoryCapabilities,
        ExternalSemaphoreCapabilities,

        Surface,
        FuchsiaImagePipeSurface,
        MetalSurface,
        WaylandSurface,
        Win32Surface,
        XcbSurface,
        XlibSurface,

        DebugReport,

        EnumCount,
    };

    // A bitset wrapper that is indexed with InstanceExt.
    struct InstanceExtSet {
        std::bitset<static_cast<size_t>(InstanceExt::EnumCount)> extensionBitSet;
        void Set(InstanceExt extension, bool enabled);
        bool Has(InstanceExt extension) const;
    };

    // Information about a known instance extension.
    struct InstanceExtInfo {
        InstanceExt index;
        const char* name;
        // The version in which this extension was promoted as built with VK_MAKE_VERSION,
        // or NeverPromoted if it was never promoted.
        uint32_t versionPromoted;
    };

    // Returns the information about a known InstanceExt
    const InstanceExtInfo& GetInstanceExtInfo(InstanceExt ext);
    // Returns a map that maps a Vulkan extension name to its InstanceExt.
    std::unordered_map<std::string, InstanceExt> CreateInstanceExtNameMap();

    // Sets entries in `extensions` to true if the were promoted in Vulkan version `version`
    void MarkPromotedExtensions(InstanceExtSet* extensions, uint32_t version);
    // From a set of extensions advertised as supported by the instance (or promoted), remove all
    // extensions that don't have all their transitive dependencies in advertisedExts.
    InstanceExtSet EnsureDependencies(const InstanceExtSet& advertisedExts);

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_VULKANEXTENSIONS_H_
