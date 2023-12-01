// Copyright 2023 The Dawn & Tint Authors
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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/FencedDeleter.h"
#include "dawn/native/vulkan/ResourceMemoryAllocatorVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"
#include "dawn/tests/end2end/SharedTextureMemoryTests.h"
#include "dawn/webgpu_cpp.h"

namespace dawn::native::vulkan {
namespace {

template <typename CreateFn>
auto CreateSharedTextureMemoryHelperImpl(native::vulkan::Device* deviceVk,
                                         uint32_t size,
                                         VkFormat format,
                                         VkImageUsageFlags usage,
                                         bool dedicatedAllocation,
                                         CreateFn createFn) {
    VkExternalMemoryImageCreateInfo externalInfo{};
    externalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    VkImageCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.pNext = &externalInfo;
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = format;
    createInfo.extent = {size, size, 1};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage vkImage;
    EXPECT_EQ(deviceVk->fn.CreateImage(deviceVk->GetVkDevice(), &createInfo, nullptr, &*vkImage),
              VK_SUCCESS);

    // Create the image memory and associate it with the container
    VkMemoryRequirements requirements;
    deviceVk->fn.GetImageMemoryRequirements(deviceVk->GetVkDevice(), vkImage, &requirements);

    int bestType = deviceVk->GetResourceMemoryAllocator()->FindBestTypeIndex(
        requirements, native::vulkan::MemoryKind::Opaque);
    EXPECT_GE(bestType, 0);

    VkMemoryDedicatedAllocateInfo dedicatedInfo;
    dedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedInfo.pNext = nullptr;
    dedicatedInfo.image = vkImage;
    dedicatedInfo.buffer = VkBuffer{};

    VkExportMemoryAllocateInfoKHR externalAllocateInfo;
    externalAllocateInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
    externalAllocateInfo.pNext = dedicatedAllocation ? &dedicatedInfo : nullptr;
    externalAllocateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    VkMemoryAllocateInfo allocateInfo;
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = &externalAllocateInfo;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = static_cast<uint32_t>(bestType);

    VkDeviceMemory vkDeviceMemory;
    EXPECT_EQ(deviceVk->fn.AllocateMemory(deviceVk->GetVkDevice(), &allocateInfo, nullptr,
                                          &*vkDeviceMemory),
              VK_SUCCESS);

    EXPECT_EQ(deviceVk->fn.BindImageMemory(deviceVk->GetVkDevice(), vkImage, vkDeviceMemory, 0),
              VK_SUCCESS);

    VkMemoryGetFdInfoKHR getFdInfo;
    getFdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    getFdInfo.pNext = nullptr;
    getFdInfo.memory = vkDeviceMemory;
    getFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

    int memoryFD = -1;
    deviceVk->fn.GetMemoryFdKHR(deviceVk->GetVkDevice(), &getFdInfo, &memoryFD);
    EXPECT_GE(memoryFD, 0) << "Failed to get file descriptor for external memory";

    wgpu::SharedTextureMemoryOpaqueFDDescriptor opaqueFDDesc;
    opaqueFDDesc.vkImageCreateInfo = &createInfo;
    opaqueFDDesc.memoryFD = memoryFD;
    opaqueFDDesc.memoryTypeIndex = allocateInfo.memoryTypeIndex;
    opaqueFDDesc.allocationSize = allocateInfo.allocationSize;
    opaqueFDDesc.dedicatedAllocation = dedicatedAllocation;

    wgpu::SharedTextureMemoryDescriptor desc;
    desc.nextInChain = &opaqueFDDesc;

    auto ret = createFn(&desc);

    close(memoryFD);
    deviceVk->GetFencedDeleter()->DeleteWhenUnused(vkDeviceMemory);
    deviceVk->GetFencedDeleter()->DeleteWhenUnused(vkImage);

    return ret;
}

template <wgpu::FeatureName FenceFeature, bool DedicatedAllocation>
class Backend : public SharedTextureMemoryTestVulkanBackend {
  public:
    static SharedTextureMemoryTestBackend* GetInstance() {
        static Backend b;
        return &b;
    }

    std::string Name() const override {
        std::string name = "OpaqueFD";
        if (DedicatedAllocation) {
            name += ", DedicatedAlloc";
        }
        switch (FenceFeature) {
            case wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD:
                name += ", OpaqueFDFence";
                break;
            case wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD:
                name += ", SyncFDFence";
                break;
            default:
                DAWN_UNREACHABLE();
        }
        return name;
    }

    std::vector<wgpu::FeatureName> RequiredFeatures(const wgpu::Adapter&) const override {
        return {wgpu::FeatureName::SharedTextureMemoryOpaqueFD,
                wgpu::FeatureName::DawnMultiPlanarFormats, FenceFeature};
    }

    template <typename CreateFn>
    auto CreateSharedTextureMemoryHelper(native::vulkan::Device* deviceVk,
                                         uint32_t size,
                                         VkFormat format,
                                         VkImageUsageFlags usage,
                                         CreateFn createFn) {
        return CreateSharedTextureMemoryHelperImpl(deviceVk, size, format, usage,
                                                   DedicatedAllocation, createFn);
    }

    // Create one basic shared texture memory. It should support most operations.
    wgpu::SharedTextureMemory CreateSharedTextureMemory(const wgpu::Device& device) override {
        return CreateSharedTextureMemoryHelper(
            native::vulkan::ToBackend(native::FromAPI(device.Get())), 16, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT,
            [&](const wgpu::SharedTextureMemoryDescriptor* desc) {
                return device.ImportSharedTextureMemory(desc);
            });
    }

    std::vector<std::vector<wgpu::SharedTextureMemory>> CreatePerDeviceSharedTextureMemories(
        const std::vector<wgpu::Device>& devices) override {
        DAWN_ASSERT(!devices.empty());

        std::vector<std::vector<wgpu::SharedTextureMemory>> memories;
        for (VkFormat format : {
                 VK_FORMAT_R8_UNORM,
                 VK_FORMAT_R8G8_UNORM,
                 VK_FORMAT_R8G8B8A8_UNORM,
                 VK_FORMAT_B8G8R8A8_UNORM,
                 VK_FORMAT_A2B10G10R10_UNORM_PACK32,
             }) {
            for (VkImageUsageFlags usage : {
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                         VK_IMAGE_USAGE_STORAGE_BIT,
                 }) {
                for (uint32_t size : {4, 64}) {
                    CreateSharedTextureMemoryHelper(
                        native::vulkan::ToBackend(native::FromAPI(devices[0].Get())), size, format,
                        usage, [&](const wgpu::SharedTextureMemoryDescriptor* desc) {
                            std::vector<wgpu::SharedTextureMemory> perDeviceMemories;
                            for (auto& device : devices) {
                                perDeviceMemories.push_back(device.ImportSharedTextureMemory(desc));
                            }
                            memories.push_back(std::move(perDeviceMemories));
                            return true;
                        });
                }
            }
        }
        return memories;
    }

    wgpu::SharedFence ImportFenceTo(const wgpu::Device& importingDevice,
                                    const wgpu::SharedFence& fence) override {
        wgpu::SharedFenceExportInfo exportInfo;
        fence.ExportInfo(&exportInfo);

        switch (exportInfo.type) {
            case wgpu::SharedFenceType::VkSemaphoreOpaqueFD: {
                wgpu::SharedFenceVkSemaphoreOpaqueFDExportInfo vkExportInfo;
                exportInfo.nextInChain = &vkExportInfo;
                fence.ExportInfo(&exportInfo);

                wgpu::SharedFenceVkSemaphoreOpaqueFDDescriptor vkDesc;
                vkDesc.handle = vkExportInfo.handle;

                wgpu::SharedFenceDescriptor fenceDesc;
                fenceDesc.nextInChain = &vkDesc;
                return importingDevice.ImportSharedFence(&fenceDesc);
            }
            case wgpu::SharedFenceType::VkSemaphoreSyncFD: {
                wgpu::SharedFenceVkSemaphoreSyncFDExportInfo vkExportInfo;
                exportInfo.nextInChain = &vkExportInfo;
                fence.ExportInfo(&exportInfo);

                wgpu::SharedFenceVkSemaphoreSyncFDDescriptor vkDesc;
                vkDesc.handle = vkExportInfo.handle;

                wgpu::SharedFenceDescriptor fenceDesc;
                fenceDesc.nextInChain = &vkDesc;
                return importingDevice.ImportSharedFence(&fenceDesc);
            }
            default:
                DAWN_UNREACHABLE();
        }
    }
};

class SharedTextureMemoryOpaqueFDValidationTest : public SharedTextureMemoryTests {};

// Test that the Vulkan image must be created with VK_IMAGE_USAGE_TRANSFER_DST_BIT.
TEST_P(SharedTextureMemoryOpaqueFDValidationTest, RequiresCopyDst) {
    native::vulkan::Device* deviceVk = native::vulkan::ToBackend(native::FromAPI(device.Get()));

    CreateSharedTextureMemoryHelperImpl(
        deviceVk, 4, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false,
        [&](const wgpu::SharedTextureMemoryDescriptor* desc) {
            ASSERT_DEVICE_ERROR_MSG(device.ImportSharedTextureMemory(desc),
                                    testing::HasSubstr("TRANSFER_DST"));
            return true;
        });

    CreateSharedTextureMemoryHelperImpl(
        deviceVk, 4, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false,
        [&](const wgpu::SharedTextureMemoryDescriptor* desc) {
            device.ImportSharedTextureMemory(desc);
            return true;
        });
}

// Test that the Vulkan image must be created with VK_IMAGE_USAGE_STORAGE_BIT if it is BGRA8Unorm.
TEST_P(SharedTextureMemoryOpaqueFDValidationTest, BGRARequiresStorage) {
    native::vulkan::Device* deviceVk = native::vulkan::ToBackend(native::FromAPI(device.Get()));

    CreateSharedTextureMemoryHelperImpl(
        deviceVk, 4, VK_FORMAT_B8G8R8A8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false,
        [&](const wgpu::SharedTextureMemoryDescriptor* desc) {
            ASSERT_DEVICE_ERROR_MSG(device.ImportSharedTextureMemory(desc),
                                    testing::HasSubstr("STORAGE_BIT"));
            return true;
        });
}

DAWN_INSTANTIATE_PREFIXED_TEST_P(
    Vulkan,
    SharedTextureMemoryNoFeatureTests,
    {VulkanBackend()},
    {Backend<wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD, false>::GetInstance(),
     Backend<wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD, false>::GetInstance()});

// Only test DedicatedAllocation == false because validation never actually creates an allocation.
// Passing true wouldn't give extra coverage.
DAWN_INSTANTIATE_PREFIXED_TEST_P(
    Vulkan,
    SharedTextureMemoryOpaqueFDValidationTest,
    {VulkanBackend()},
    {Backend<wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD, false>::GetInstance(),
     Backend<wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD, false>::GetInstance()});

DAWN_INSTANTIATE_PREFIXED_TEST_P(
    Vulkan,
    SharedTextureMemoryTests,
    {VulkanBackend()},
    {Backend<wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD, false>::GetInstance(),
     Backend<wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD, false>::GetInstance(),
     Backend<wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD, true>::GetInstance(),
     Backend<wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD, true>::GetInstance()});

}  // anonymous namespace
}  // namespace dawn::native::vulkan
