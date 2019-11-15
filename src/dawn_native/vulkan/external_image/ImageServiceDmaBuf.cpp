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

    namespace {

        ResultOrError<uint32_t> GetModifierPlaneCount(const VulkanFunctions& fn,
                                                      VkPhysicalDevice physicalDevice,
                                                      VkFormat format,
                                                      uint64_t modifier) {
            VkDrmFormatModifierPropertiesListEXT formatModifierPropsList;
            formatModifierPropsList.sType =
                VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
            formatModifierPropsList.pNext = nullptr;
            formatModifierPropsList.drmFormatModifierCount = 0;
            formatModifierPropsList.pDrmFormatModifierProperties = nullptr;
            VkFormatProperties2 formatProps;
            formatProps.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
            formatProps.pNext = &formatModifierPropsList;
            fn.GetPhysicalDeviceFormatProperties2KHR(physicalDevice, format, &formatProps);
            uint32_t modifierCount = formatModifierPropsList.drmFormatModifierCount;
            std::vector<VkDrmFormatModifierPropertiesEXT> formatModifierProps(modifierCount);
            formatModifierPropsList.pDrmFormatModifierProperties = formatModifierProps.data();
            fn.GetPhysicalDeviceFormatProperties2KHR(physicalDevice, format, &formatProps);

            for (const auto& props : formatModifierProps) {
                if (props.drmFormatModifier == modifier) {
                    uint32_t count = props.drmFormatModifierPlaneCount;
                    return count;
                }
            }
            return DAWN_VALIDATION_ERROR("DRM format modifier not supported");
        }

    } // anonymous namespace

    Service::Service(Device* device) : mDevice(device) {
        const VulkanDeviceInfo& deviceInfo = mDevice->GetDeviceInfo();
        const VulkanGlobalInfo& globalInfo =
            ToBackend(mDevice->GetAdapter())->GetBackend()->GetGlobalInfo();

        mSupported = globalInfo.getPhysicalDeviceProperties2 &&
                     globalInfo.externalMemoryCapabilities &&
                     deviceInfo.externalMemory &&
                     deviceInfo.externalMemoryFD &&
                     deviceInfo.externalMemoryDmaBuf &&
                     deviceInfo.imageDrmFormatModifier;
    }

    Service::~Service() = default;

    bool Service::Supported(const ExternalImageDescriptor* descriptor, VkFormat format) {
        // Early out before we try using extension functions
        if (!mSupported) {
            return false;
        }
        if (descriptor->type != ExternalImageDescriptorType::DmaBuf) {
            return false;
        }
        const ExternalImageDescriptorDmaBuf* dmaBufDescriptor =
                static_cast<const ExternalImageDescriptorDmaBuf*>(descriptor);

        // Verify plane count for the modifier.
        VkPhysicalDevice physicalDevice = ToBackend(mDevice->GetAdapter())->GetPhysicalDevice();
        ResultOrError<uint32_t> planeCountResult = GetModifierPlaneCount(mDevice->fn,
                physicalDevice, format, dmaBufDescriptor->drmModifier);
        if (planeCountResult.IsError()) {
            return false;
        }
        uint32_t planeCount = planeCountResult.AcquireSuccess();
        if (planeCount == 0) {
            return false;
        }
        // TODO(hob): Support multi-plane formats like I915_FORMAT_MOD_Y_TILED_CCS.
        if (planeCount > 1) {
            return false;
        }

        // Verify that the external memory can actually be imported
        VkPhysicalDeviceImageDrmFormatModifierInfoEXT drmModifierInfo;
        drmModifierInfo.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
        drmModifierInfo.pNext = nullptr;
        drmModifierInfo.drmFormatModifier = dmaBufDescriptor->drmModifier;
        drmModifierInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo;
        externalImageFormatInfo.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
        externalImageFormatInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        externalImageFormatInfo.pNext = &drmModifierInfo;
        VkPhysicalDeviceImageFormatInfo2 imageFormatInfo;
        imageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
        imageFormatInfo.format = format;
        imageFormatInfo.type = VK_IMAGE_TYPE_2D;
        imageFormatInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
        imageFormatInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageFormatInfo.flags = 0;
        imageFormatInfo.pNext = &externalImageFormatInfo;

        VkExternalImageFormatProperties externalImageFormatProps;
        externalImageFormatProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
        externalImageFormatProps.pNext = nullptr;
        VkImageFormatProperties2 imageFormatProps;
        imageFormatProps.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
        imageFormatProps.pNext = &externalImageFormatProps;

        VkResult result = mDevice->fn.GetPhysicalDeviceImageFormatProperties2KHR(
                physicalDevice, &imageFormatInfo, &imageFormatProps);
        if (result != VK_SUCCESS) {
            return false;
        }
        VkExternalMemoryFeatureFlags featureFlags = externalImageFormatProps
                .externalMemoryProperties
                .externalMemoryFeatures;
        return featureFlags & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
    }

    ResultOrError<VkImage> Service::CreateImage(const ExternalImageDescriptor* descriptor,
                                                      VkImageType type,
                                                      VkFormat format,
                                                      VkExtent3D extent,
                                                      uint32_t mipLevels,
                                                      uint32_t arrayLayers,
                                                      VkSampleCountFlagBits samples,
                                                      VkImageUsageFlags usage) {
        if (descriptor->type != ExternalImageDescriptorType::DmaBuf) {
            return DAWN_VALIDATION_ERROR("ExternalImageDescriptor is not a dma-buf descriptor");
        }
        const ExternalImageDescriptorDmaBuf* dmaBufDescriptor =
                static_cast<const ExternalImageDescriptorDmaBuf*>(descriptor);
        VkPhysicalDevice physicalDevice = ToBackend(mDevice->GetAdapter())->GetPhysicalDevice();
        VkDevice device = mDevice->GetVkDevice();

        VkSubresourceLayout planeLayout;
        planeLayout.offset = 0;
        planeLayout.size = 0; // VK_EXT_image_drm_format_modifier mandates size = 0.
        planeLayout.rowPitch = dmaBufDescriptor->stride;
        planeLayout.arrayPitch = 0; // Not an array texture
        planeLayout.depthPitch = 0; // Not a depth texture

        uint32_t planeCount;
        DAWN_TRY_ASSIGN(planeCount, GetModifierPlaneCount(mDevice->fn, physicalDevice, format,
                                                          dmaBufDescriptor->drmModifier));
        VkImageDrmFormatModifierExplicitCreateInfoEXT explicitCreateInfo;
        explicitCreateInfo.sType =
                VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
        explicitCreateInfo.pNext = NULL;
        explicitCreateInfo.drmFormatModifier = dmaBufDescriptor->drmModifier;
        explicitCreateInfo.drmFormatModifierPlaneCount = planeCount;
        explicitCreateInfo.pPlaneLayouts = &planeLayout;
        VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo;
        externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        externalMemoryImageCreateInfo.pNext = &explicitCreateInfo;
        externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
        VkImageCreateInfo imageCreateInfo;
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext = &externalMemoryImageCreateInfo;
        imageCreateInfo.flags = 0;
        imageCreateInfo.imageType = type;
        imageCreateInfo.format = format;
        imageCreateInfo.extent = extent;
        imageCreateInfo.mipLevels = mipLevels;
        imageCreateInfo.arrayLayers = arrayLayers;
        imageCreateInfo.samples = samples;
        imageCreateInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
        imageCreateInfo.usage = usage;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.queueFamilyIndexCount = 0;
        imageCreateInfo.pQueueFamilyIndices = nullptr;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // We always set VK_IMAGE_USAGE_TRANSFER_DST_BIT unconditionally beause the Vulkan images
        // that are used in vkCmdClearColorImage() must have been created with this flag, which is
        // also required for the implementation of robust resource initialization.
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VkImage image;
        DAWN_TRY(CheckVkSuccess(
            mDevice->fn.CreateImage(device, &imageCreateInfo, nullptr, &image),
            "CreateImage"));
        return image;
    }

}}}  // namespace dawn_native::vulkan::external_image
