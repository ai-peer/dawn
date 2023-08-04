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

gbm_bo_flags GBM_BO_USE_TEXTURING = gbm_bo_flags(1 << 5);
gbm_bo_flags GBM_BO_USE_GPU_DATA_BUFFER = gbm_bo_flags(1 << 18);

template <wgpu::FeatureName FenceFeature>
class Backend : public SharedTextureMemoryTestBackend {
  public:
    static SharedTextureMemoryTestBackend* GetInstance() {
        static Backend* b = new Backend();
        return b;
    }

    std::string Name() const override {
        switch (FenceFeature) {
            case wgpu::FeatureName::SharedFenceVkSemaphoreOpaqueFD:
                return "dma buf, opaque fd";
            case wgpu::FeatureName::SharedFenceVkSemaphoreSyncFD:
                return "dma buf, sync fd";
            default:
                UNREACHABLE();
        }
    }

    std::vector<wgpu::FeatureName> RequiredFeatures() const override {
        return {wgpu::FeatureName::SharedTextureMemoryDmaBuf, FenceFeature};
    }

    static void FillVkImageDesc(uint32_t width,
                                uint32_t height,
                                uint32_t format,
                                gbm_bo_flags usage,
                                wgpu::SharedTextureMemoryVkImageDescriptor* vkImageDesc) {
        vkImageDesc->vkExtent3D = {width, height, 1};
        switch (format) {
            case GBM_FORMAT_R8:
                vkImageDesc->vkFormat = VK_FORMAT_R8_UNORM;
                break;
            case GBM_FORMAT_GR88:
                vkImageDesc->vkFormat = VK_FORMAT_R8G8_UNORM;
                break;
            case GBM_FORMAT_ABGR8888:
                vkImageDesc->vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
                break;
            case GBM_FORMAT_ARGB8888:
                vkImageDesc->vkFormat = VK_FORMAT_B8G8R8A8_UNORM;
                break;
            case GBM_FORMAT_ABGR2101010:
                vkImageDesc->vkFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
                break;
            case GBM_FORMAT_NV12:
                vkImageDesc->vkFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
                break;
            default:
                UNREACHABLE();
        }

        vkImageDesc->vkUsageFlags =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (usage & GBM_BO_USE_RENDERING) {
            vkImageDesc->vkUsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (usage & GBM_BO_USE_TEXTURING) {
            vkImageDesc->vkUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if (usage & GBM_BO_USE_GPU_DATA_BUFFER) {
            vkImageDesc->vkUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
    }

    // Create one basic shared texture memory. It should support most operations.
    wgpu::SharedTextureMemory CreateSharedTextureMemory(wgpu::Device& device) override {
        auto format = GBM_FORMAT_ABGR8888;
        auto usage = gbm_bo_flags(GBM_BO_USE_RENDERING | GBM_BO_USE_TEXTURING | GBM_BO_USE_LINEAR);

        ASSERT(gbm_device_is_format_supported(mGbmDevice, format, usage));

        gbm_bo* bo = gbm_bo_create(mGbmDevice, 16, 16, format, usage);
        EXPECT_NE(bo, nullptr) << "Failed to create GBM buffer object";

        wgpu::SharedTextureMemoryDmaBufDescriptor dmaBufDesc;
        dmaBufDesc.memoryFD = gbm_bo_get_fd(bo);
        dmaBufDesc.drmModifier = gbm_bo_get_modifier(bo);

        uint64_t planeOffsets[GBM_MAX_PLANES];
        uint32_t planeStrides[GBM_MAX_PLANES];
        dmaBufDesc.planeCount = gbm_bo_get_plane_count(bo);
        dmaBufDesc.planeOffsets = planeOffsets;
        dmaBufDesc.planeStrides = planeStrides;
        ASSERT(dmaBufDesc.planeCount <= GBM_MAX_PLANES);

        for (uint32_t plane = 0; plane < dmaBufDesc.planeCount; ++plane) {
            planeStrides[plane] = gbm_bo_get_stride_for_plane(bo, plane);
            planeOffsets[plane] = gbm_bo_get_offset(bo, plane);
        }

        wgpu::SharedTextureMemoryDescriptor desc;
        wgpu::SharedTextureMemoryVkImageDescriptor vkImageDesc;
        FillVkImageDesc(16, 16, format, usage, &vkImageDesc);
        desc.nextInChain = &vkImageDesc;
        vkImageDesc.nextInChain = &dmaBufDesc;

        wgpu::SharedTextureMemory memory = device.ImportSharedTextureMemory(&desc);
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
                 GBM_FORMAT_ABGR2101010,
                 GBM_FORMAT_NV12,
             }) {
            for (gbm_bo_flags usage : {
                     gbm_bo_flags(0),
                     GBM_BO_USE_LINEAR,
                     GBM_BO_USE_TEXTURING,
                     gbm_bo_flags(GBM_BO_USE_TEXTURING | GBM_BO_USE_LINEAR),
                     GBM_BO_USE_RENDERING,
                     gbm_bo_flags(GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR),
                     GBM_BO_USE_GPU_DATA_BUFFER,
                     gbm_bo_flags(GBM_BO_USE_GPU_DATA_BUFFER | GBM_BO_USE_LINEAR),
                     gbm_bo_flags(GBM_BO_USE_RENDERING | GBM_BO_USE_TEXTURING),
                     gbm_bo_flags(GBM_BO_USE_RENDERING | GBM_BO_USE_TEXTURING | GBM_BO_USE_LINEAR),
                 }) {
                if (!gbm_device_is_format_supported(mGbmDevice, format, usage)) {
                    continue;
                }
                for (uint32_t size : {4, 64}) {
                    gbm_bo* bo = gbm_bo_create(mGbmDevice, size, size, format, usage);
                    EXPECT_NE(bo, nullptr) << "Failed to create GBM buffer object";

                    wgpu::SharedTextureMemoryDmaBufDescriptor dmaBufDesc;
                    dmaBufDesc.memoryFD = gbm_bo_get_fd(bo);
                    dmaBufDesc.drmModifier = gbm_bo_get_modifier(bo);

                    uint64_t planeOffsets[GBM_MAX_PLANES];
                    uint32_t planeStrides[GBM_MAX_PLANES];
                    dmaBufDesc.planeCount = gbm_bo_get_plane_count(bo);
                    dmaBufDesc.planeOffsets = planeOffsets;
                    dmaBufDesc.planeStrides = planeStrides;
                    ASSERT(dmaBufDesc.planeCount <= GBM_MAX_PLANES);

                    for (uint32_t plane = 0; plane < dmaBufDesc.planeCount; ++plane) {
                        planeStrides[plane] = gbm_bo_get_stride_for_plane(bo, plane);
                        planeOffsets[plane] = gbm_bo_get_offset(bo, plane);
                    }

                    wgpu::SharedTextureMemoryDescriptor desc;
                    wgpu::SharedTextureMemoryVkImageDescriptor vkImageDesc;
                    FillVkImageDesc(size, size, format, usage, &vkImageDesc);
                    desc.nextInChain = &vkImageDesc;
                    vkImageDesc.nextInChain = &dmaBufDesc;

                    std::vector<wgpu::SharedTextureMemory> perDeviceMemories;
                    for (auto& device : devices) {
                        perDeviceMemories.push_back(device.ImportSharedTextureMemory(&desc));
                    }
                    memories.push_back(std::move(perDeviceMemories));

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
                UNREACHABLE();
        }
    }

  private:
    Backend() {
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
        if (renderNodeFd < 0) {
            return;
        }

        // Might be failed to create GBM device and mGbmDevice is nullptr.
        mGbmDevice = gbm_create_device(renderNodeFd);
        close(renderNodeFd);
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
