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
#include "dawn_native/vulkan/TextureVk.h"
#include "dawn_native/vulkan/VulkanError.h"
#include "dawn_native/vulkan/external_memory/MemoryService.h"

namespace dawn_native { namespace vulkan { namespace external_memory {

    Service::Service(Device* device) : mDevice(device) {
        mSupported = device->GetDeviceInfo().HasExt(DeviceExt::ExternalMemoryFD);
    }

    Service::~Service() = default;

    bool Service::SupportsImportMemory(VkFormat format,
                                       VkImageType type,
                                       VkImageTiling tiling,
                                       VkImageUsageFlags usage,
                                       VkImageCreateFlags flags) {
        // Early out before we try using extension functions
        if (!mSupported) {
            return false;
        }

        VkPhysicalDeviceExternalImageFormatInfo externalFormatInfo;
        externalFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHR;
        externalFormatInfo.pNext = nullptr;
        externalFormatInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

        VkPhysicalDeviceImageFormatInfo2 formatInfo;
        formatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR;
        formatInfo.pNext = &externalFormatInfo;
        formatInfo.format = format;
        formatInfo.type = type;
        formatInfo.tiling = tiling;
        formatInfo.usage = usage;
        formatInfo.flags = flags;

        VkExternalImageFormatProperties externalFormatProperties;
        externalFormatProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR;
        externalFormatProperties.pNext = nullptr;

        VkImageFormatProperties2 formatProperties;
        formatProperties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR;
        formatProperties.pNext = &externalFormatProperties;

        VkResult result = VkResult::WrapUnsafe(mDevice->fn.GetPhysicalDeviceImageFormatProperties2(
            ToBackend(mDevice->GetAdapter())->GetPhysicalDevice(), &formatInfo, &formatProperties));

        // If handle not supported, result == VK_ERROR_FORMAT_NOT_SUPPORTED
        if (result != VK_SUCCESS) {
            return false;
        }

        VkFlags memoryFlags =
            externalFormatProperties.externalMemoryProperties.externalMemoryFeatures;

        if ((memoryFlags & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT_KHR) != 0 &&
            !mDevice->GetDeviceInfo().HasExt(DeviceExt::DedicatedAllocation)) {
            return false;
        }
        return (memoryFlags & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHR) != 0;
    }

    bool Service::SupportsCreateImage(const ExternalImageDescriptor* descriptor,
                                      VkFormat format,
                                      VkImageUsageFlags usage) {
        return mSupported;
    }

    ResultOrError<MemoryImportParams> Service::GetMemoryImportParams(
        const ExternalImageDescriptor* descriptor,
        VkImage image) {
        if (descriptor->type != ExternalImageDescriptorType::OpaqueFD) {
            return DAWN_VALIDATION_ERROR("ExternalImageDescriptor is not an OpaqueFD descriptor");
        }
        const ExternalImageDescriptorOpaqueFD* opaqueFDDescriptor =
            static_cast<const ExternalImageDescriptorOpaqueFD*>(descriptor);

        VkDevice device = mDevice->GetVkDevice();

        // Get the valid memory types for the VkImage.
        VkMemoryRequirements memoryRequirements;
        mDevice->fn.GetImageMemoryRequirements(device, image, &memoryRequirements);

        VkMemoryFdPropertiesKHR fdProperties;
        fdProperties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
        fdProperties.pNext = nullptr;

        // Get the valid memory types that the external memory can be imported as.
        mDevice->fn.GetMemoryFdPropertiesKHR(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
                                             opaqueFDDescriptor->memoryFD, &fdProperties);

        // Choose the best memory type that satisfies both the image's constraint and the import's
        // constraint.
        memoryRequirements.memoryTypeBits &= fdProperties.memoryTypeBits;
        int memoryTypeIndex =
            mDevice->FindBestMemoryTypeIndex(memoryRequirements, false /* mappable */);
        if (memoryTypeIndex == -1) {
            return DAWN_VALIDATION_ERROR("Unable to find appropriate memory type for import");
        }
        MemoryImportParams params = {memoryRequirements.size,
                                     static_cast<uint32_t>(memoryTypeIndex)};

        return params;
    }

    ResultOrError<VkDeviceMemory> Service::ImportMemory(ExternalMemoryHandle handle,
                                                        const MemoryImportParams& importParams,
                                                        VkImage image) {
        if (handle < 0) {
            return DAWN_VALIDATION_ERROR("Trying to import memory with invalid handle");
        }

        VkMemoryDedicatedRequirements dedicatedMemoryRequirements;
        dedicatedMemoryRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
        dedicatedMemoryRequirements.pNext = nullptr;

        VkMemoryRequirements2 requirements;
        requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        requirements.pNext = &dedicatedMemoryRequirements;

        VkImageMemoryRequirementsInfo2 requirementsInfo;
        requirementsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        requirementsInfo.pNext = nullptr;
        requirementsInfo.image = image;

        bool useDedicatedAllocation = false;
        if (mDevice->GetDeviceInfo().HasExt(DeviceExt::GetMemoryRequirements2)) {
            // If we have the extension, use it to get memory requirements.
            mDevice->fn.GetImageMemoryRequirements2(mDevice->GetVkDevice(), &requirementsInfo,
                                                    &requirements);

            bool hasDedicatedAllocation =
                mDevice->GetDeviceInfo().HasExt(DeviceExt::DedicatedAllocation);
            if (dedicatedMemoryRequirements.requiresDedicatedAllocation &&
                !hasDedicatedAllocation) {
                return DAWN_VALIDATION_ERROR("Cannot import dedicated allocation");
            }

            useDedicatedAllocation = hasDedicatedAllocation &&
                                     (dedicatedMemoryRequirements.requiresDedicatedAllocation ||
                                      dedicatedMemoryRequirements.prefersDedicatedAllocation);
        } else {
            // Otherwise, use the unextended version. We only need GetMemoryRequirements2 if the
            // imported image is a dedicated allocation. If it is dedicated, allocation will fail
            // later.
            mDevice->fn.GetImageMemoryRequirements(mDevice->GetVkDevice(), requirementsInfo.image,
                                                   &requirements.memoryRequirements);
        }

        if (requirements.memoryRequirements.size > importParams.allocationSize) {
            return DAWN_VALIDATION_ERROR("Requested allocation size is too small for image");
        }

        VkImportMemoryFdInfoKHR importMemoryFdInfo;
        importMemoryFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        importMemoryFdInfo.pNext = nullptr;
        importMemoryFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        importMemoryFdInfo.fd = handle;

        VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo;
        if (useDedicatedAllocation) {
            importMemoryFdInfo.pNext = &memoryDedicatedAllocateInfo;
            memoryDedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
            memoryDedicatedAllocateInfo.pNext = nullptr;
            memoryDedicatedAllocateInfo.image = image;
            memoryDedicatedAllocateInfo.buffer = VkBuffer{};
        }

        VkMemoryAllocateInfo allocateInfo;
        allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.pNext = &importMemoryFdInfo;
        allocateInfo.allocationSize = importParams.allocationSize;
        allocateInfo.memoryTypeIndex = importParams.memoryTypeIndex;

        VkDeviceMemory allocatedMemory = VK_NULL_HANDLE;
        DAWN_TRY(CheckVkSuccess(mDevice->fn.AllocateMemory(mDevice->GetVkDevice(), &allocateInfo,
                                                           nullptr, &*allocatedMemory),
                                "vkAllocateMemory"));
        return allocatedMemory;
    }

    ResultOrError<VkImage> Service::CreateImage(const ExternalImageDescriptor* descriptor,
                                                const VkImageCreateInfo& baseCreateInfo) {
        VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo;
        externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        externalMemoryImageCreateInfo.pNext = nullptr;
        externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

        VkImageCreateInfo createInfo = baseCreateInfo;
        createInfo.pNext = &externalMemoryImageCreateInfo;
        createInfo.flags = VK_IMAGE_CREATE_ALIAS_BIT_KHR;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        ASSERT(IsSampleCountSupported(mDevice, createInfo));

        VkImage image;
        DAWN_TRY(CheckVkSuccess(
            mDevice->fn.CreateImage(mDevice->GetVkDevice(), &createInfo, nullptr, &*image),
            "CreateImage"));
        return image;
    }

}}}  // namespace dawn_native::vulkan::external_memory
