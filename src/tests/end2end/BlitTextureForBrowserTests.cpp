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

#include "tests/DawnTest.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr uint32_t kRTSize = 400;
constexpr uint32_t kBufferElementsCount = kMinDynamicBufferOffsetAlignment / sizeof(uint32_t) + 2;
constexpr uint32_t kBufferSize = kBufferElementsCount * sizeof(uint32_t);
constexpr uint32_t kBindingSize = 8;

class BlitTextureForBrowserTests : public DawnTest {
  protected:
    static constexpr wgpu::TextureFormat kTextureFormat = wgpu::TextureFormat::RGBA8Unorm;

    struct TextureSpec {
        wgpu::Origin3D copyOrigin;
        wgpu::Extent3D textureSize;
        uint32_t level;
    };

    static std::vector<RGBA8> GetExpectedTextureData(const utils::TextureDataCopyLayout& layout) {
        std::vector<RGBA8> textureData(layout.texelBlockCount);
        for (uint32_t layer = 0; layer < layout.mipSize.depth; ++layer) {
            const uint32_t texelIndexOffsetPerSlice = layout.texelBlocksPerImage * layer;
            for (uint32_t y = 0; y < layout.mipSize.height; ++y) {
                for (uint32_t x = 0; x < layout.mipSize.width; ++x) {
                    uint32_t i = x + y * layout.texelBlocksPerRow;
                    textureData[texelIndexOffsetPerSlice + i] =
                        RGBA8(static_cast<uint8_t>((x + layer * x) % 256),
                              static_cast<uint8_t>((y + layer * y) % 256),
                              static_cast<uint8_t>(x / 256), static_cast<uint8_t>(y / 256));
                }
            }
        }

        return textureData;
    }

    void DoTest(const TextureSpec& srcSpec,
                const TextureSpec& dstSpec,
                const wgpu::Extent3D& copySize) {
        wgpu::TextureDescriptor srcDescriptor;
        srcDescriptor.dimension = wgpu::TextureDimension::e2D;
        srcDescriptor.size = srcSpec.textureSize;
        srcDescriptor.sampleCount = 1;
        srcDescriptor.format = kTextureFormat;
        srcDescriptor.mipLevelCount = srcSpec.level + 1;
        srcDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;
        wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

        wgpu::Texture dstTexture;
        if (copyWithinSameTexture) {
            dstTexture = srcTexture;
        } else {
            wgpu::TextureDescriptor dstDescriptor;
            dstDescriptor.dimension = wgpu::TextureDimension::e2D;
            dstDescriptor.size = dstSpec.textureSize;
            dstDescriptor.sampleCount = 1;
            dstDescriptor.format = kTextureFormat;
            dstDescriptor.mipLevelCount = dstSpec.level + 1;
            dstDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;
            dstTexture = device.CreateTexture(&dstDescriptor);
        }

        device.GetDefaultQueue()->BlitTextureForBrowser

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        // Create an upload buffer and use it to populate the current slice of the texture in
        // `level` mip level
        const utils::TextureDataCopyLayout copyLayout =
            utils::GetTextureDataCopyLayoutForTexture2DAtLevel(
                kTextureFormat,
                {srcSpec.textureSize.width, srcSpec.textureSize.height, copySize.depth},
                srcSpec.level, 0);

        const std::vector<RGBA8> textureArrayCopyData = GetExpectedTextureData(copyLayout);

        wgpu::Buffer uploadBuffer = utils::CreateBufferFromData(
            device, textureArrayCopyData.data(), copyLayout.byteLength, wgpu::BufferUsage::CopySrc);
        wgpu::BufferCopyView bufferCopyView =
            utils::CreateBufferCopyView(uploadBuffer, 0, copyLayout.bytesPerRow, 0);
        wgpu::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(srcTexture, srcSpec.level, {0, 0, srcSpec.copyOrigin.z});
        encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copyLayout.mipSize);

        const wgpu::Extent3D copySizePerSlice = {copySize.width, copySize.height, 1};
        // Perform the texture to texture copy
        wgpu::TextureCopyView srcTextureCopyView =
            utils::CreateTextureCopyView(srcTexture, srcSpec.level, srcSpec.copyOrigin);
        wgpu::TextureCopyView dstTextureCopyView =
            utils::CreateTextureCopyView(dstTexture, dstSpec.level, dstSpec.copyOrigin);

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        const TextureCopyView *source, const TextureCopyView *destination,
            const Extent3D *copySize

                device.GetDefaultQueue()
                    ->BlitTextureForBrowser(&srcTextureCopyView, &dstTextureCopyView, &copySize);

        // Texels in single slice.
        const uint32_t texelCountInCopyRegion = utils::GetTexelCountInCopyRegion(
            copyLayout.bytesPerRow, copyLayout.bytesPerImage / copyLayout.bytesPerRow,
            copySizePerSlice, kTextureFormat);
        std::vector<RGBA8> expected(texelCountInCopyRegion);
        for (uint32_t slice = 0; slice < copySize.depth; ++slice) {
            std::fill(expected.begin(), expected.end(), RGBA8());
            const uint32_t texelIndexOffset = copyLayout.texelBlocksPerImage * slice;
            const uint32_t expectedTexelArrayDataStartIndex =
                texelIndexOffset +
                (srcSpec.copyOrigin.x + srcSpec.copyOrigin.y * copyLayout.texelBlocksPerRow);
            PackTextureData(&textureArrayCopyData[expectedTexelArrayDataStartIndex], copySize.width,
                            copySize.height, copyLayout.texelBlocksPerRow, expected.data(),
                            copySize.width);

            EXPECT_TEXTURE_RGBA8_EQ(expected.data(), dstTexture, dstSpec.copyOrigin.x,
                                    dstSpec.copyOrigin.y, copySize.width, copySize.height,
                                    dstSpec.level, dstSpec.copyOrigin.z + slice)
                << "Texture to Texture copy failed copying region [(" << srcSpec.copyOrigin.x
                << ", " << srcSpec.copyOrigin.y << "), (" << srcSpec.copyOrigin.x + copySize.width
                << ", " << srcSpec.copyOrigin.y + copySize.height << ")) from "
                << srcSpec.textureSize.width << " x " << srcSpec.textureSize.height
                << " texture at mip level " << srcSpec.level << " layer "
                << srcSpec.copyOrigin.z + slice << " to [(" << dstSpec.copyOrigin.x << ", "
                << dstSpec.copyOrigin.y << "), (" << dstSpec.copyOrigin.x + copySize.width << ", "
                << dstSpec.copyOrigin.y + copySize.height << ")) region of "
                << dstSpec.textureSize.width << " x " << dstSpec.textureSize.height
                << " texture at mip level " << dstSpec.level << " layer "
                << dstSpec.copyOrigin.z + slice << std::endl;
        }
    }
}

TEST_P(BlitTextureForBrowserTests, DirectBlit) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.level = 0;
    textureSpec.textureSize = {kWidth, kHeight, 1};
    DoTest(textureSpec, textureSpec, {kWidth, kHeight, 1});
}

DAWN_INSTANTIATE_TEST(DynamicBufferOffsetTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
