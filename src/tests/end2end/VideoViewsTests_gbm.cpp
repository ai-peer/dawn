// Copyright 2021 The Dawn Authors
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

#include "VideoViewsTests.h"

#include "common/Assert.h"
#include "dawn_native/VulkanBackend.h"

#include <fcntl.h>
#include <gbm.h>

class VideoViewsTestBackendGbm : public VideoViewsTestBackend {
  public:
    void OnSetUp(WGPUDevice device) override {
        mWGPUDevice = device;
        mGbmDevice = CreateGbmDevice();
    }

    void OnTearDown() override {
        gbm_device_destroy(mGbmDevice);
    }

    bool IsSupported() override {
        // TODO(chromium:1258986): Add DISJOINT vkImage support for multi-plannar formats.
        return !IsNv12GbmBoDisjoint();
    }

  private:
    gbm_device* CreateGbmDevice() {
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
            if (renderNodeFd >= 0)
                break;
        }
        ASSERT(renderNodeFd > 0);
        ;

        gbm_device* gbmDevice = gbm_create_device(renderNodeFd);
        ASSERT(gbmDevice != nullptr);
        return gbmDevice;
    }

    static uint32_t GetGbmBoFormat(wgpu::TextureFormat format) {
        switch (format) {
            case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
                return GBM_FORMAT_NV12;
            default:
                UNREACHABLE();
                return -1;
        }
    }

    WGPUTextureFormat ToWGPUTextureFormat(wgpu::TextureFormat format) {
        switch (format) {
            case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
                return WGPUTextureFormat_R8BG8Biplanar420Unorm;
            default:
                UNREACHABLE();
                return WGPUTextureFormat_Undefined;
        }
    }

    WGPUTextureUsage ToWGPUTextureUsage(wgpu::TextureUsage usage) {
        switch (usage) {
            case wgpu::TextureUsage::TextureBinding:
                return WGPUTextureUsage_TextureBinding;
            default:
                UNREACHABLE();
                return WGPUTextureUsage_None;
        }
    }

    // Checks if the plane handles of a NV12 gbm_bo are same.
    bool IsNv12GbmBoDisjoint() {
        uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING | GBM_BO_USE_HW_VIDEO_DECODER |
                         GBM_BO_USE_SW_WRITE_RARELY;
        gbm_bo* gbmBo = gbm_bo_create(mGbmDevice, 1, 1, GBM_FORMAT_NV12, flags);
        if (gbmBo == nullptr) {
            return true;
        }
        gbm_bo_handle plane0Handle = gbm_bo_get_handle_for_plane(gbmBo, 0);
        for (int plane = 1; plane < gbm_bo_get_plane_count(gbmBo); ++plane) {
            if (gbm_bo_get_handle_for_plane(gbmBo, plane).u32 != plane0Handle.u32) {
                gbm_bo_destroy(gbmBo);
                return true;
            }
        }
        gbm_bo_destroy(gbmBo);
        return false;
    }

    bool CreateVideoTextureForTest(
        wgpu::TextureFormat format,
        wgpu::TextureUsage usage,
        bool isCheckerboard,
        VideoViewsTestBackend::PlatformTexture* platformTextureOut) override {
        uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING | GBM_BO_USE_HW_VIDEO_DECODER |
                         GBM_BO_USE_SW_WRITE_RARELY;
        gbm_bo* gbmBo =
            gbm_bo_create(mGbmDevice, VideoViewsTests::kYUVImageDataWidthInTexels,
                          VideoViewsTests::kYUVImageDataHeightInTexels, GBM_FORMAT_NV12, flags);
        if (gbmBo == nullptr) {
            return false;
        }

        void* mapHandle = nullptr;
        uint32_t strideBytes = 0;
        void* addr = gbm_bo_map(gbmBo, 0, 0, VideoViewsTests::kYUVImageDataWidthInTexels,
                                VideoViewsTests::kYUVImageDataHeightInTexels, GBM_BO_TRANSFER_WRITE,
                                &strideBytes, &mapHandle);
        EXPECT_NE(addr, nullptr);
        std::vector<uint8_t> initialData =
            VideoViewsTests::GetTestTextureData(format, isCheckerboard);
        std::memcpy(addr, initialData.data(), initialData.size());

        gbm_bo_unmap(gbmBo, mapHandle);

        WGPUTextureDescriptor textureDesc = {};
        textureDesc.format = ToWGPUTextureFormat(format);
        textureDesc.dimension = WGPUTextureDimension_2D;
        textureDesc.usage = ToWGPUTextureUsage(usage);
        textureDesc.size = {VideoViewsTests::kYUVImageDataWidthInTexels,
                            VideoViewsTests::kYUVImageDataHeightInTexels, 1};
        textureDesc.mipLevelCount = 1;
        textureDesc.sampleCount = 1;

        WGPUDawnTextureInternalUsageDescriptor internalDesc = {};
        internalDesc.chain.sType = WGPUSType_DawnTextureInternalUsageDescriptor;
        internalDesc.internalUsage = WGPUTextureUsage_CopySrc;
        textureDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&internalDesc);

        dawn_native::vulkan::ExternalImageDescriptorDmaBuf descriptor = {};
        descriptor.cTextureDescriptor = &textureDesc;
        descriptor.isInitialized = true;

        descriptor.memoryFD = gbm_bo_get_fd(gbmBo);
        descriptor.stride = gbm_bo_get_stride(gbmBo);
        descriptor.drmModifier = gbm_bo_get_modifier(gbmBo);
        descriptor.waitFDs = {};

        platformTextureOut->wgpuTexture =
            wgpu::Texture::Acquire(dawn_native::vulkan::WrapVulkanImage(mWGPUDevice, &descriptor));
        platformTextureOut->platformHandle = gbmBo;
        return true;
    }

    void DestroyVideoTextureForTest(
        VideoViewsTestBackend::PlatformTexture& platformTexture) override {
        // Exports the signal and ignores it.
        dawn_native::vulkan::ExternalImageExportInfoDmaBuf exportInfo;
        dawn_native::vulkan::ExportVulkanImage(platformTexture.wgpuTexture.Get(),
                                               VK_IMAGE_LAYOUT_GENERAL, &exportInfo);
        for (int fd : exportInfo.semaphoreHandles) {
            ASSERT_NE(fd, -1);
            close(fd);
        }
        ASSERT_NE(platformTexture.platformHandle, nullptr);
        gbm_bo* gbmBo = static_cast<gbm_bo*>(platformTexture.platformHandle);
        gbm_bo_destroy(gbmBo);
    }

    WGPUDevice mWGPUDevice = nullptr;
    gbm_device* mGbmDevice = nullptr;
};

// static
BackendTestConfig VideoViewsTestBackend::Backend() {
    return VulkanBackend();
}

// static
std::unique_ptr<VideoViewsTestBackend> VideoViewsTestBackend::Create() {
    return std::make_unique<VideoViewsTestBackendGbm>();
}
