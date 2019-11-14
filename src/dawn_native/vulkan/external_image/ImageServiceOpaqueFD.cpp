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

#include "common/Assert.h"
#include "dawn_native/vulkan/AdapterVk.h"
#include "dawn_native/vulkan/BackendVk.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/VulkanError.h"
#include "dawn_native/vulkan/external_image/ImageService.h"

namespace dawn_native { namespace vulkan { namespace external_image {

    Service::Service(Device* device) : mDevice(device), mSupported(true) {}

    Service::~Service() = default;

    bool Service::Supported(const ExternalImageDescriptor* descriptor, VkFormat format) {
        return mSupported;
    }

    ResultOrError<VkImage> Service::CreateImage(const ExternalImageDescriptor* descriptor,
                                                      VkImageType type,
                                                      VkFormat format,
                                                      VkExtent3D extent,
                                                      uint32_t mipLevels,
                                                      uint32_t arrayLayers,
                                                      VkSampleCountFlagBits samples,
                                                      VkImageUsageFlags usage) {
        VkImageCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = VK_IMAGE_CREATE_ALIAS_BIT_KHR;
        createInfo.imageType = type;
        createInfo.format = format;
        createInfo.extent = extent;
        createInfo.mipLevels = mipLevels;
        createInfo.arrayLayers = arrayLayers;
        createInfo.samples = samples;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // We always set VK_IMAGE_USAGE_TRANSFER_DST_BIT unconditionally beause the Vulkan images
        // that are used in vkCmdClearColorImage() must have been created with this flag, which is
        // also required for the implementation of robust resource initialization.
        createInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImage image;
        DAWN_TRY(CheckVkSuccess(
            mDevice->fn.CreateImage(mDevice->GetVkDevice(), &createInfo, nullptr, &image),
            "CreateImage"));
        return image;
    }

}}}  // namespace dawn_native::vulkan::external_image
