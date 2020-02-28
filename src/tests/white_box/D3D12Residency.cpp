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

#include "dawn_native/D3D12Backend.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr uint64_t kBytesPerPixel = 4;
constexpr wgpu::TextureFormat kDefaultFormat = wgpu::TextureFormat::RGBA8Unorm;

class D3D12ResidencyTests : public DawnTest {
  protected:
    void AllocateTextures(uint32_t textureSize, uint32_t bytesToAllocate) {
        uint64_t kBytesPerResource = textureSize * textureSize * kBytesPerPixel;
        uint64_t numberOfResources = bytesToAllocate / kBytesPerResource;

        for (uint64_t i = 0; i < numberOfResources; i++) {
            mTextures.push_back(Create2DTexture(textureSize, wgpu::TextureUsage::CopyDst));
        }
    }

    wgpu::Texture Create2DTexture(uint32_t textureSize, wgpu::TextureUsage usage) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size.width = textureSize;
        descriptor.size.height = textureSize;
        descriptor.size.depth = 1;
        descriptor.arrayLayerCount = 1;
        descriptor.sampleCount = 1;
        descriptor.format = kDefaultFormat;
        descriptor.mipLevelCount = 1;
        descriptor.usage = usage;

        return device.CreateTexture(&descriptor);
    }

    const dawn_native::d3d12::VideoMemoryInfo& GetVideoMemoryInfo() const {
        dawn_native::d3d12::Device* d3dDevice =
            reinterpret_cast<dawn_native::d3d12::Device*>(device.Get());
        return d3dDevice->GetVideoMemoryInfo();
    }

    bool IsIntegratedGraphics() const {
        dawn_native::d3d12::Device* d3dDevice =
            reinterpret_cast<dawn_native::d3d12::Device*>(device.Get());
        return d3dDevice->GetDeviceInfo().isUMA;
    }

    static void MapReadCallback(WGPUBufferMapAsyncStatus status,
                                const void* data,
                                uint64_t,
                                void* userdata) {
        ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
        ASSERT_NE(nullptr, data);

        static_cast<D3D12ResidencyTests*>(userdata)->mMappedReadData = data;
    }

    static void MapWriteCallback(WGPUBufferMapAsyncStatus status,
                                 void* data,
                                 uint64_t,
                                 void* userdata) {
        ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
        ASSERT_NE(nullptr, data);

        static_cast<D3D12ResidencyTests*>(userdata)->mMappedWriteData = data;
    }
    void TouchTextures(uint32_t textureSize, uint32_t beginIndex, uint32_t endIndex) {
        // Initialize a source texture on the GPU to serve as a source to quickly copy data to the
        // rest of the textures.
        wgpu::Texture sourceTexture =
            Create2DTexture(textureSize, wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst);

        std::vector<uint8_t> data(kBytesPerPixel * textureSize * textureSize, 0);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<uint8_t>(1);
        }

        wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
            device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
        wgpu::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(stagingBuffer, 0, 0, 0);
        wgpu::TextureCopyView srcTextureCopyView =
            utils::CreateTextureCopyView(sourceTexture, 0, 0, {0, 0, 0});
        wgpu::Extent3D copySize = {textureSize, textureSize, 1};

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&bufferCopyView, &srcTextureCopyView, &copySize);

        // Perform a copy on the range of textures to ensure the are moved to dedicated GPU memory.
        for (uint32_t i = beginIndex; i < endIndex; i++) {
            wgpu::TextureCopyView dstTextureCopyView =
                utils::CreateTextureCopyView(mTextures[i], 0, 0, {0, 0, 0});
            encoder.CopyTextureToTexture(&srcTextureCopyView, &dstTextureCopyView, &copySize);
        }
        wgpu::CommandBuffer copy = encoder.Finish();
        queue.Submit(1, &copy);
    }

    std::vector<wgpu::Texture> mTextures;

    void* mMappedWriteData = nullptr;
    const void* mMappedReadData = nullptr;
};

TEST_P(D3D12ResidencyTests, OvercommitSmallResources) {
    DAWN_SKIP_TEST_IF(IsIntegratedGraphics());

    // Allocate 1.5x the available budget to ensure some textures must be non-resident.
    // Use 512 x 512 images to make each texture 1MB, which will use sub-allocated resources
    // internally.
    constexpr uint64_t kSize = 512;
    AllocateTextures(kSize, GetVideoMemoryInfo().dawnBudget * 1.5);

    // Copy data to the first half of textures. Since we allocated 1.5x Dawn's budget, about 75% of
    // these should have been paged out previously. Touching these will ensure all of them get
    // paged-in.
    TouchTextures(kSize, 0, mTextures.size() / 2);

    // Copy data to the second half of textures. About 25% of these should already be resident, and
    // the remainder must be paged back in after evicting the first half of textures.
    TouchTextures(kSize, mTextures.size() / 2, mTextures.size());
}

TEST_P(D3D12ResidencyTests, OvercommitLargeResources) {
    DAWN_SKIP_TEST_IF(IsIntegratedGraphics());

    // Allocate 1.5x the available budget to ensure some textures must be non-resident.
    // Use 2048 x 2048 images to make each texture 16MB, which must be directly allocated
    // internally.
    constexpr uint64_t kSize = 2048;
    AllocateTextures(kSize, GetVideoMemoryInfo().dawnBudget * 1.5);

    // Copy data to the first half of textures. Since we allocated 1.5x Dawn's budget, about 75% of
    // these should have been paged out previously. Touching these will ensure all of them get
    // paged-in.
    TouchTextures(kSize, 0, mTextures.size() / 2);

    // Copy data to the second half of textures. About 25% of these should already be resident, and
    // the remainder must be paged back in after evicting the first half of textures.
    TouchTextures(kSize, mTextures.size() / 2, mTextures.size());
}

TEST_P(D3D12ResidencyTests, AsyncMappedBufferRead) {
    DAWN_SKIP_TEST_IF(IsIntegratedGraphics());

    // Create a mappable buffer.
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    uint32_t data = 12345;
    buffer.SetSubData(0, sizeof(uint32_t), &data);

    // Allocate 1.5x the available budget to ensure resources are being paged out.
    constexpr uint64_t kSize = 512;
    AllocateTextures(kSize, GetVideoMemoryInfo().dawnBudget * 1.5);

    buffer.MapReadAsync(MapReadCallback, this);

    while (mMappedReadData == nullptr) {
        WaitABit();
    }

    ASSERT_EQ(data, *reinterpret_cast<const uint32_t*>(mMappedReadData));

    buffer.Unmap();
}

TEST_P(D3D12ResidencyTests, AsyncMappedBufferWrite) {
    DAWN_SKIP_TEST_IF(IsIntegratedGraphics());

    // Create a mappable buffer.
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    // Allocate 1.5x the available budget to ensure resources are being paged out.
    constexpr uint64_t kSize = 512;
    AllocateTextures(kSize, GetVideoMemoryInfo().dawnBudget * 1.5);

    // The mappable buffer should currently be non-resident. Try to write to the buffer.
    uint32_t data = 12345;
    buffer.MapWriteAsync(MapWriteCallback, this);

    while (mMappedWriteData == nullptr) {
        WaitABit();
    }

    memcpy(mMappedWriteData, &data, sizeof(uint32_t));
    buffer.Unmap();

    EXPECT_BUFFER_U32_EQ(data, buffer, 0);

    // Map the buffer again.
    mMappedWriteData = nullptr;
    buffer.MapWriteAsync(MapWriteCallback, this);

    while (mMappedWriteData == nullptr) {
        WaitABit();
    }

    // Load enough textures to ensure least recently used resources are evicted.
    TouchTextures(kSize, mTextures.size(), mTextures.size());

    // Write to the mapped buffer, which should not have been evicted.
    data = 23456;
    memcpy(mMappedWriteData, &data, sizeof(uint32_t));

    buffer.Unmap();

    EXPECT_BUFFER_U32_EQ(data, buffer, 0);
}

TEST_P(D3D12ResidencyTests, SetExternalMemory) {
    DAWN_SKIP_TEST_IF(IsIntegratedGraphics());

    // 256MB
    constexpr uint64_t kMemoryReservation = 262144000;

    // Reserve external memory.
    dawn_native::d3d12::SetExternalMemoryReservation(device.Get(), kMemoryReservation);

    // Ensure the previous operation is reflected in the device's video memory info.
    ASSERT_EQ(GetVideoMemoryInfo().externalReservation, kMemoryReservation);
}

DAWN_INSTANTIATE_TEST(D3D12ResidencyTests, D3D12Backend());