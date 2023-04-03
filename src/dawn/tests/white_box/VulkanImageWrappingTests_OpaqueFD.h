// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_TESTS_WHITE_BOX_VULKANIMAGEWRAPPINGTESTS_OPAQUEFD_H_
#define SRC_DAWN_TESTS_WHITE_BOX_VULKANIMAGEWRAPPINGTESTS_OPAQUEFD_H_

#include <unistd.h>

#include <memory>
#include <utility>
#include <vector>

#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/FencedDeleter.h"
#include "dawn/native/vulkan/ResourceMemoryAllocatorVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"
#include "dawn/tests/white_box/VulkanImageWrappingTests.h"
#include "gtest/gtest.h"

namespace dawn::native::vulkan {

class ExternalSemaphoreOpaqueFD : public VulkanImageWrappingTestBackend::ExternalSemaphore {
  public:
    explicit ExternalSemaphoreOpaqueFD(int handle);
    ~ExternalSemaphoreOpaqueFD() override;
    int AcquireHandle();

  private:
    int mHandle = -1;
};

class ExternalTextureOpaqueFD : public VulkanImageWrappingTestBackend::ExternalTexture {
  public:
    ExternalTextureOpaqueFD(dawn::native::vulkan::Device* device,
                            int fd,
                            VkDeviceMemory allocation,
                            VkImage handle,
                            VkDeviceSize allocationSize,
                            uint32_t memoryTypeIndex);

    ~ExternalTextureOpaqueFD() override;

    int Dup() const;

  private:
    dawn::native::vulkan::Device* mDevice;
    int mFd = -1;
    VkDeviceMemory mAllocation = VK_NULL_HANDLE;
    VkImage mHandle = VK_NULL_HANDLE;

  public:
    const VkDeviceSize allocationSize;
    const uint32_t memoryTypeIndex;
};

class VulkanImageWrappingTestBackendOpaqueFD : public VulkanImageWrappingTestBackend {
  public:
    explicit VulkanImageWrappingTestBackendOpaqueFD(const wgpu::Device& device);

    bool SupportsTestParams(const TestParams& params) const override;

    std::unique_ptr<VulkanImageWrappingTestBackend::ExternalTexture> CreateTexture(
        uint32_t width,
        uint32_t height,
        wgpu::TextureFormat format,
        wgpu::TextureUsage usage) override;
    wgpu::Texture WrapImage(const wgpu::Device& device,
                            const ExternalTexture* texture,
                            const ExternalImageDescriptorVkForTesting& descriptor,
                            std::vector<std::unique_ptr<ExternalSemaphore>> semaphores) override;
    bool ExportImage(const wgpu::Texture& texture,
                     ExternalImageExportInfoVkForTesting* exportInfo) override;

  private:
    // Creates a VkImage with external memory
    ::VkResult CreateImage(dawn::native::vulkan::Device* deviceVk,
                           uint32_t width,
                           uint32_t height,
                           VkFormat format,
                           VkImage* image);

    // Allocates memory for an image
    ::VkResult AllocateMemory(dawn::native::vulkan::Device* deviceVk,
                              VkImage handle,
                              VkDeviceMemory* allocation,
                              VkDeviceSize* allocationSize,
                              uint32_t* memoryTypeIndex);
    // Binds memory to an image
    ::VkResult BindMemory(dawn::native::vulkan::Device* deviceVk,
                          VkImage handle,
                          VkDeviceMemory memory);

    // Extracts a file descriptor representing memory on a device
    int GetMemoryFd(dawn::native::vulkan::Device* deviceVk, VkDeviceMemory memory);

    // Prepares and exports memory for an image on a given device
    void CreateBindExportImage(dawn::native::vulkan::Device* deviceVk,
                               uint32_t width,
                               uint32_t height,
                               VkFormat format,
                               VkImage* handle,
                               VkDeviceMemory* allocation,
                               VkDeviceSize* allocationSize,
                               uint32_t* memoryTypeIndex,
                               int* memoryFd);

    wgpu::Device mDevice;
    dawn::native::vulkan::Device* mDeviceVk;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_TESTS_WHITE_BOX_VULKANIMAGEWRAPPINGTESTS_OPAQUEFD_H_
