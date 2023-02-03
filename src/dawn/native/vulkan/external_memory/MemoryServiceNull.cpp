// Copyright 2019 The Dawn Authors
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

#include "dawn/native/vulkan/external_memory/MemoryServiceNull.h"
#include "dawn/native/vulkan/DeviceVk.h"

namespace dawn::native::vulkan::external_memory {

MemoryServiceNull::MemoryServiceNull(Device* device) : Service(device) {}

MemoryServiceNull::~MemoryServiceNull() = default;

// static
bool MemoryServiceNull::CheckSupport(const VulkanDeviceInfo& deviceInfo) {
    return false;
}

bool MemoryServiceNull::SupportsImportMemory(VkFormat format,
                                             VkImageType type,
                                             VkImageTiling tiling,
                                             VkImageUsageFlags usage,
                                             VkImageCreateFlags flags) {
    return false;
}

bool MemoryServiceNull::SupportsCreateImage(const ExternalImageDescriptor* descriptor,
                                            VkFormat format,
                                            VkImageUsageFlags usage,
                                            bool* supportsDisjoint) {
    *supportsDisjoint = false;
    return false;
}

ResultOrError<MemoryImportParams> MemoryServiceNull::GetMemoryImportParams(
    const ExternalImageDescriptor* descriptor,
    VkImage image) {
    return DAWN_UNIMPLEMENTED_ERROR("Using null memory service to interop inside Vulkan");
}

uint32_t MemoryServiceNull::GetQueueFamilyIndex() {
    return VK_QUEUE_FAMILY_EXTERNAL_KHR;
}

ResultOrError<VkDeviceMemory> MemoryServiceNull::ImportMemory(
    ExternalMemoryHandle handle,
    const MemoryImportParams& importParams,
    VkImage image) {
    return DAWN_UNIMPLEMENTED_ERROR("Using null memory service to interop inside Vulkan");
}

ResultOrError<VkImage> MemoryServiceNull::CreateImage(const ExternalImageDescriptor* descriptor,
                                                      const VkImageCreateInfo& baseCreateInfo) {
    return DAWN_UNIMPLEMENTED_ERROR("Using null memory service to interop inside Vulkan");
}

bool MemoryServiceNull::Supported() const {
    return mSupported;
}

}  // namespace dawn::native::vulkan::external_memory
