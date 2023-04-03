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

#ifndef SRC_DAWN_TESTS_WHITE_BOX_VULKANIMAGEWRAPPINGTESTS_DMABUF_H_
#define SRC_DAWN_TESTS_WHITE_BOX_VULKANIMAGEWRAPPINGTESTS_DMABUF_H_

#include <fcntl.h>
#include <gbm.h>
#include <gtest/gtest.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/tests/white_box/VulkanImageWrappingTests.h"

namespace dawn::native::vulkan {

class ExternalSemaphoreDmaBuf : public VulkanImageWrappingTestBackend::ExternalSemaphore {
  public:
    explicit ExternalSemaphoreDmaBuf(int handle);
    ~ExternalSemaphoreDmaBuf() override;
    int AcquireHandle();

  private:
    int mHandle = -1;
};

class ExternalTextureDmaBuf : public VulkanImageWrappingTestBackend::ExternalTexture {
  public:
    ExternalTextureDmaBuf(
        gbm_bo* bo,
        int fd,
        std::array<PlaneLayout, ExternalImageDescriptorDmaBuf::kMaxPlanes> planeLayouts,
        uint64_t drmModifier);

    ~ExternalTextureDmaBuf() override;

    int Dup() const;

  private:
    gbm_bo* mGbmBo = nullptr;
    int mFd = -1;

  public:
    const std::array<PlaneLayout, ExternalImageDescriptorDmaBuf::kMaxPlanes> planeLayouts;
    const uint64_t drmModifier;
};

class VulkanImageWrappingTestBackendDmaBuf : public VulkanImageWrappingTestBackend {
  public:
    explicit VulkanImageWrappingTestBackendDmaBuf(const wgpu::Device& device);

    ~VulkanImageWrappingTestBackendDmaBuf() override;

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

    void CreateGbmDevice();

  private:
    gbm_bo* CreateGbmBo(uint32_t width, uint32_t height, bool linear);

    gbm_device* mGbmDevice = nullptr;
    dawn::native::vulkan::Device* mDeviceVk;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_TESTS_WHITE_BOX_VULKANIMAGEWRAPPINGTESTS_OPAQUEFD_H_
