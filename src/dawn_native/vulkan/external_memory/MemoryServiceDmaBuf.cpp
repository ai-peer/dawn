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
#include "dawn_native/vulkan/external_memory/MemoryService.h"

namespace dawn_native { namespace vulkan { namespace external_memory {

    Service::Service(Device* device) : mDevice(device) {
        const VulkanDeviceInfo& deviceInfo = mDevice->GetDeviceInfo();
        const VulkanGlobalInfo& globalInfo =
            ToBackend(mDevice->GetAdapter())->GetBackend()->GetGlobalInfo();

        mSupported = globalInfo.getPhysicalDeviceProperties2 &&
                     globalInfo.externalMemoryCapabilities &&
                     deviceInfo.externalMemory &&
                     deviceInfo.externalMemoryFD &&
                     deviceInfo.externalMemoryDmaBuf &&
                     deviceInfo.imageDrmFormatModifier &&
                     deviceInfo.getMemoryRequirements2;
    }

    Service::~Service() = default;

    bool Service::Supported(VkFormat format,
                            VkImageType type,
                            VkImageTiling tiling,
                            VkImageUsageFlags usage,
                            VkImageCreateFlags flags) {
        return mSupported;
    }

    ResultOrError<VkDeviceSize> Service::GetAllocationSize(
            const ExternalImageDescriptor* descriptor, VkImage image) {
        VkDevice device = mDevice->GetVkDevice();
        VkImageMemoryRequirementsInfo2 memReqsInfo;
        memReqsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        memReqsInfo.pNext = nullptr;
        memReqsInfo.image = image;
        VkMemoryRequirements2 memReqs;
        memReqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        memReqs.pNext = nullptr;
        mDevice->fn.GetImageMemoryRequirements2KHR(device, &memReqsInfo, &memReqs);
        VkDeviceSize allocationSize = memReqs.memoryRequirements.size;
        return allocationSize;
    }

    ResultOrError<uint32_t> Service::GetMemoryTypeIndex(
            const ExternalImageDescriptor* descriptor, VkImage image) {
        if (descriptor->type != ExternalImageDescriptorType::DmaBuf) {
            return DAWN_VALIDATION_ERROR("ExternalImageDescriptor is not a dma-buf descriptor");
        }
        const ExternalImageDescriptorDmaBuf* dmaBufDescriptor =
                static_cast<const ExternalImageDescriptorDmaBuf*>(descriptor);

        VkDevice device = mDevice->GetVkDevice();
        VkImageMemoryRequirementsInfo2 memReqsInfo;
        memReqsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        memReqsInfo.pNext = nullptr;
        memReqsInfo.image = image;
        VkMemoryRequirements2 memReqs;
        memReqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        memReqs.pNext = nullptr;
        mDevice->fn.GetImageMemoryRequirements2KHR(device, &memReqsInfo, &memReqs);

        VkMemoryFdPropertiesKHR fdProperties;
        fdProperties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
        fdProperties.pNext = nullptr;
        mDevice->fn.GetMemoryFdPropertiesKHR(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                dmaBufDescriptor->primeFD, &fdProperties);
        uint32_t memoryTypeBits = fdProperties.memoryTypeBits &
                memReqs.memoryRequirements.memoryTypeBits;
        if (memoryTypeBits == 0) {
            return DAWN_VALIDATION_ERROR("Unable to find appropriate memory type for import");
        }
        return ffs(memoryTypeBits) - 1; // Just return the first compatible memory type

    }

    ResultOrError<VkDeviceMemory> Service::ImportMemory(ExternalMemoryHandle handle,
                                                        VkDeviceSize allocationSize,
                                                        uint32_t memoryTypeIndex,
                                                        VkImage image) {
        if (handle < 0) {
            return DAWN_VALIDATION_ERROR("Trying to import memory with invalid handle");
        }

        VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo;
        memoryDedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        memoryDedicatedAllocateInfo.pNext = nullptr;
        memoryDedicatedAllocateInfo.image = image;
        memoryDedicatedAllocateInfo.buffer = VK_NULL_HANDLE;
        VkImportMemoryFdInfoKHR importMemoryFdInfo;
        importMemoryFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        importMemoryFdInfo.pNext = &memoryDedicatedAllocateInfo;
        importMemoryFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
        importMemoryFdInfo.fd = handle;
        VkMemoryAllocateInfo memoryAllocateInfo;
        VkDeviceMemory allocatedMemory = VK_NULL_HANDLE;
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = &importMemoryFdInfo;
        memoryAllocateInfo.allocationSize = allocationSize;
        memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
        DAWN_TRY(CheckVkSuccess(mDevice->fn.AllocateMemory(mDevice->GetVkDevice(),
                                                           &memoryAllocateInfo, nullptr,
                                                           &allocatedMemory), "vkAllocateMemory"));
        return allocatedMemory;
    }

}}}  // namespace dawn_native::vulkan::external_memory
