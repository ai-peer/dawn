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

#include "dawn_native/VulkanBackend.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/external_memory/MemoryService.h"

namespace dawn_native { namespace vulkan { namespace external_image {

    Service::Service(Device* device) : mDevice(device) {
        DAWN_UNUSED(mDevice);
        DAWN_UNUSED(mSupported);
    }

    Service::~Service() = default;

    bool Service::Supported(const ExternalImageDescriptor* descriptor, VkFormat format) {
        return false;
    }

    ResultOrError<VkImage> CreateImage(const ExternalImageDescriptor* descriptor,
                                       VkImageType type,
                                       VkFormat format,
                                       VkExtent3D extent,
                                       uint32_t mipLevels,
                                       uint32_t arrayLayers,
                                       VkSampleCountFlagBits samples,
                                       VkImageUsageFlags usage) {
        return DAWN_UNIMPLEMENTED_ERROR("Using null image service to interop inside Vulkan");
    }

}}}  // namespace dawn_native::vulkan::external_image
