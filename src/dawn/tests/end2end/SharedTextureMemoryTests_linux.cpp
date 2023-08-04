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

#include <fcntl.h>
#include <gbm.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <string>
#include <utility>
#include <vector>

#include "dawn/tests/end2end/SharedTextureMemoryTests.h"
#include "dawn/webgpu_cpp.h"

namespace dawn {
namespace {

template <wgpu::FeatureName FenceFeature>
class Backend : public SharedTextureMemoryTestBackend {
  public:
    static SharedTextureMemoryTestBackend* GetInstance() {
        static Backend b;
        return &b;
    }

    std::string Name() const override {
        switch (FenceFeature) {
            case wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD:
                return "dma buf, opaque fd";
            case wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD:
                return "dma buf, sync fd";
            default:
                DAWN_UNREACHABLE();
        }
    }

    std::vector<wgpu::FeatureName> RequiredFeatures() const override {
        return {wgpu::FeatureName::SharedTextureMemoryDmaBuf, FenceFeature};
    }

    static std::string MakeLabel(const wgpu::SharedTextureMemoryDmaBufDescriptor& desc) {
        // Internally, the GBM enums are defined as their fourcc values. Cast to that and use
        // it as the label. The fourcc value is a four-character name that can be
        // interpreted as a 32-bit integer enum ('ABGR', 'r011', etc.)
        return std::string(reinterpret_cast<const char*>(&desc.drmFormat), 4) + " " +
               "modifier:" + std::to_string(desc.drmModifier) + " " + std::to_string(desc.width) +
               "x" + std::to_string(desc.height);
    }

    // Create one basic shared texture memory. It should support most operations.
    wgpu::SharedTextureMemory CreateSharedTextureMemory(wgpu::Device& device) override {
        auto format = GBM_FORMAT_ABGR8888;
        auto usage = GBM_BO_USE_LINEAR;

        DAWN_ASSERT(gbm_device_is_format_supported(mGbmDevice, format, usage));

        gbm_bo* bo = gbm_bo_create(mGbmDevice, 16, 16, format, usage);
        EXPECT_NE(bo, nullptr) << "Failed to create GBM buffer object";

        wgpu::SharedTextureMemoryDmaBufDescriptor dmaBufDesc;
        dmaBufDesc.width = 16;
        dmaBufDesc.height = 16;
        dmaBufDesc.drmFormat = format;
        dmaBufDesc.drmModifier = gbm_bo_get_modifier(bo);

        int planeFDs[GBM_MAX_PLANES];
        uint64_t planeOffsets[GBM_MAX_PLANES];
        uint32_t planeStrides[GBM_MAX_PLANES];
        dmaBufDesc.planeCount = gbm_bo_get_plane_count(bo);
        dmaBufDesc.planeFDs = planeFDs;
        dmaBufDesc.planeOffsets = planeOffsets;
        dmaBufDesc.planeStrides = planeStrides;
        DAWN_ASSERT(dmaBufDesc.planeCount <= GBM_MAX_PLANES);

        for (uint32_t plane = 0; plane < dmaBufDesc.planeCount; ++plane) {
            planeFDs[plane] = gbm_bo_get_fd(bo);  // gbm_bo_get_handle_for_plane(bo, plane);
            planeStrides[plane] = gbm_bo_get_stride_for_plane(bo, plane);
            planeOffsets[plane] = gbm_bo_get_offset(bo, plane);
        }

        std::string label = MakeLabel(dmaBufDesc);
        wgpu::SharedTextureMemoryDescriptor desc;
        desc.label = label.c_str();
        desc.nextInChain = &dmaBufDesc;

        wgpu::SharedTextureMemory memory = device.ImportSharedTextureMemory(&desc);
        for (uint32_t plane = 0; plane < dmaBufDesc.planeCount; ++plane) {
            close(planeFDs[plane]);
        }
        gbm_bo_destroy(bo);

        return memory;
    }

    std::vector<std::vector<wgpu::SharedTextureMemory>> CreatePerDeviceSharedTextureMemories(
        const std::vector<wgpu::Device>& devices) override {
        std::vector<std::vector<wgpu::SharedTextureMemory>> memories;
        for (uint32_t format : {
                 GBM_FORMAT_R8,
                 GBM_FORMAT_GR88,
                 GBM_FORMAT_ABGR8888,
                 GBM_FORMAT_ARGB8888,
                 GBM_FORMAT_XBGR8888,
                 GBM_FORMAT_XRGB8888,
                 GBM_FORMAT_ABGR2101010,
                 GBM_FORMAT_NV12,
             }) {
            for (gbm_bo_flags usage : {
                     gbm_bo_flags(0),
                     GBM_BO_USE_LINEAR,
                     GBM_BO_USE_RENDERING,
                     gbm_bo_flags(GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR),
                 }) {
                if (!gbm_device_is_format_supported(mGbmDevice, format, usage)) {
                    continue;
                }
                for (uint32_t size : {4, 64}) {
                    gbm_bo* bo = gbm_bo_create(mGbmDevice, size, size, format, usage);
                    EXPECT_NE(bo, nullptr) << "Failed to create GBM buffer object";

                    wgpu::SharedTextureMemoryDmaBufDescriptor dmaBufDesc;
                    dmaBufDesc.width = size;
                    dmaBufDesc.height = size;
                    dmaBufDesc.drmFormat = format;
                    dmaBufDesc.drmModifier = gbm_bo_get_modifier(bo);

                    int planeFDs[GBM_MAX_PLANES];
                    uint64_t planeOffsets[GBM_MAX_PLANES];
                    uint32_t planeStrides[GBM_MAX_PLANES];
                    dmaBufDesc.planeCount = gbm_bo_get_plane_count(bo);
                    dmaBufDesc.planeFDs = planeFDs;
                    dmaBufDesc.planeOffsets = planeOffsets;
                    dmaBufDesc.planeStrides = planeStrides;
                    DAWN_ASSERT(dmaBufDesc.planeCount <= GBM_MAX_PLANES);

                    for (uint32_t plane = 0; plane < dmaBufDesc.planeCount; ++plane) {
                        planeFDs[plane] = gbm_bo_get_fd(bo);
                        planeStrides[plane] = gbm_bo_get_stride_for_plane(bo, plane);
                        planeOffsets[plane] = gbm_bo_get_offset(bo, plane);
                    }

                    std::string label = MakeLabel(dmaBufDesc);
                    wgpu::SharedTextureMemoryDescriptor desc;
                    desc.label = label.c_str();
                    desc.nextInChain = &dmaBufDesc;

                    std::vector<wgpu::SharedTextureMemory> perDeviceMemories;
                    for (auto& device : devices) {
                        perDeviceMemories.push_back(device.ImportSharedTextureMemory(&desc));
                    }
                    memories.push_back(std::move(perDeviceMemories));

                    for (uint32_t plane = 0; plane < dmaBufDesc.planeCount; ++plane) {
                        close(planeFDs[plane]);
                    }
                    gbm_bo_destroy(bo);
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
            case wgpu::SharedFenceType::VkSemaphoreZirconHandle: {
                wgpu::SharedFenceVkSemaphoreZirconHandleExportInfo vkExportInfo;
                exportInfo.nextInChain = &vkExportInfo;
                fence.ExportInfo(&exportInfo);

                wgpu::SharedFenceVkSemaphoreZirconHandleDescriptor vkDesc;
                vkDesc.handle = vkExportInfo.handle;

                wgpu::SharedFenceDescriptor fenceDesc;
                fenceDesc.nextInChain = &vkDesc;
                return importingDevice.ImportSharedFence(&fenceDesc);
            }
            default:
                DAWN_UNREACHABLE();
        }
    }

  private:
    void SetUp() override {
        // Render nodes [1] are the primary interface for communicating with the GPU on
        // devices that support DRM. The actual filename of the render node is
        // implementation-specific, so we must scan through all possible filenames to find
        // one that we can use [2].
        //
        // [1] https://dri.freedesktop.org/docs/drm/gpu/drm-uapi.html#render-nodes
        // [2]
        // https://cs.chromium.org/chromium/src/ui/ozone/platform/wayland/gpu/drm_render_node_path_finder.cc
        const uint32_t kRenderNodeStart = 128;
        const uint32_t kRenderNodeEnd = kRenderNodeStart + 16;
        const std::string kRenderNodeTemplate = "/dev/dri/renderD";

        int renderNodeFd = -1;
        for (uint32_t i = kRenderNodeStart; i < kRenderNodeEnd; i++) {
            std::string renderNode = kRenderNodeTemplate + std::to_string(i);
            renderNodeFd = open(renderNode.c_str(), O_RDWR);
            if (renderNodeFd >= 0) {
                break;
            }
        }

        // Failed to get file descriptor for render node and mGbmDevice is nullptr.
        DAWN_TEST_UNSUPPORTED_IF(renderNodeFd < 0);

        mGbmDevice = gbm_create_device(renderNodeFd);
        close(renderNodeFd);
        DAWN_TEST_UNSUPPORTED_IF(mGbmDevice == nullptr);

        // Make sure we can successfully create a basic buffer object.
        gbm_bo* bo = gbm_bo_create(mGbmDevice, 16, 16, GBM_FORMAT_XBGR8888, GBM_BO_USE_LINEAR);
        if (bo != nullptr) {
            gbm_bo_destroy(bo);
        }
        DAWN_TEST_UNSUPPORTED_IF(bo == nullptr);
    }

    void TearDown() override {
        if (mGbmDevice) {
            gbm_device_destroy(mGbmDevice);
            mGbmDevice = nullptr;
        }
    }

    gbm_device* mGbmDevice = nullptr;
};

DAWN_INSTANTIATE_PREFIXED_TEST_P(
    Vulkan,
    SharedTextureMemoryNoFeatureTests,
    {VulkanBackend()},
    {Backend<wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD>::GetInstance(),
     Backend<wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD>::GetInstance()});

DAWN_INSTANTIATE_PREFIXED_TEST_P(
    Vulkan,
    SharedTextureMemoryTests,
    {VulkanBackend()},
    {Backend<wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD>::GetInstance(),
     Backend<wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD>::GetInstance()});

}  // anonymous namespace
}  // namespace dawn
