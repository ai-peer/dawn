// Copyright 2022 The Dawn Authors
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

#include "dawn/tests/DawnTest.h"

#include "dawn/utils/TestUtils.h"
#include "dawn/utils/WGPUHelpers.h"

enum class Type { B2TCopy, T2BCopy };

constexpr static wgpu::Extent3D kCopySize = {1, 1, 2};
constexpr static uint64_t kOffset = 0;
constexpr static uint64_t kBytesPerRow = 256;
constexpr static uint64_t kRowsPerImagePadding = 1;
constexpr static uint64_t kRowsPerImage = kRowsPerImagePadding + kCopySize.height;
constexpr static wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;
constexpr static uint32_t kBytesPerBlock = 4;

// Tests in this file are used to expose an error on D3D12 about required minimum buffer size.
// See detailed bug reports at crbug.com/dawn/1278, 1288, 1289.

// When we do B2T or T2B copy from/to a buffer with paddings, it may wrongly calculate
// the required buffer size on D3D12.

// Using the data in this test as an example, in which copySize = {1, 1, 2}, offset = 0, bytesPerRow
// = 256, and rowsPerImage = 2 (there is 1-row padding for every image), and assuming we are copying
// a non-compressed format like rgba8unorm, the required minimum buffer size should be:
//   offset + bytesPerRow * rowsPerImage * (copySize.depthOrArrayLayers - 1)
//     + bytesPerRow * (copySize.height - 1) + bytesPerBlock * copySize.width.
// It is 0 + 256 * 2 * (2 - 1) + 256 * (1 - 1) + 4 * 1 = 516.

// However, the required minimum buffer size on D3D12 (including WARP) is:
//   offset + bytesPerRow * rowsPerImage * (copySize.depthOrArrayLayers - 1)
//     + bytesPerRow * (rowsPerImage - 1) + bytesPerBlock * copySize.width.
// Or
//   offset + bytesPerRow * rowsPerImage * copySize.depthOrArrayLayers
//     + bytesPerBlock * copySize.width - bytesPerRow.
// It is 0 + 256 * 2 * (2 - 1) + 256 * (2 - 1) + 4 * 1 = 772.

// It looks like D3D12 requires unnecessary buffer storage for rowsPerImagePadding in the last
// image. It does respect bytesPerRowPadding in the last row and doesn't require storage for
// that part, though.

class RequiredBufferSizeInCopyTests : public DawnTest {
  protected:
    void DoTest(const uint64_t bufferSize, Type copyType) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = bufferSize;
        descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

        wgpu::TextureDescriptor texDesc = {};
        texDesc.dimension = wgpu::TextureDimension::e3D;
        texDesc.size = kCopySize;
        texDesc.format = kFormat;
        texDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
        wgpu::Texture texture = device.CreateTexture(&texDesc);

        wgpu::ImageCopyTexture imageCopyTexture =
            utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
        wgpu::ImageCopyBuffer imageCopyBuffer =
            utils::CreateImageCopyBuffer(buffer, kOffset, kBytesPerRow, kRowsPerImage);

        // Initialize copied data and set expected data for buffer and texture.
        ASSERT(sizeof(uint32_t) == kBytesPerBlock);
        std::vector<uint32_t> data(bufferSize / kBytesPerBlock, 1);
        std::vector<uint32_t> expectedBufferData(bufferSize / kBytesPerBlock, 0);
        std::vector<uint32_t> expectedTextureData(kCopySize.depthOrArrayLayers, 0);
        uint64_t imageSize = kBytesPerRow * kRowsPerImage;
        // Initialize the first element on every image to be 0x80808080
        ASSERT(bufferSize >= (imageSize * (kCopySize.depthOrArrayLayers - 1) + kBytesPerBlock));
        for (uint32_t i = 0; i < kCopySize.depthOrArrayLayers; ++i) {
            data[i * imageSize / kBytesPerBlock] = 0x80808080;
            expectedBufferData[i * imageSize / kBytesPerBlock] = 0x80808080;
            expectedTextureData[i] = 0x80808080;
        }

        // Do B2T copy or T2B copy
        wgpu::CommandEncoder encoder = this->device.CreateCommandEncoder();
        switch (copyType) {
            case Type::T2BCopy: {
                wgpu::TextureDataLayout textureDataLayout =
                    utils::CreateTextureDataLayout(kOffset, kBytesPerRow, kRowsPerImage);

                queue.WriteTexture(&imageCopyTexture, data.data(), bufferSize, &textureDataLayout,
                                   &kCopySize);

                encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &kCopySize);
                break;
            }
            case Type::B2TCopy:
                queue.WriteBuffer(buffer, 0, data.data(), bufferSize);
                encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &kCopySize);
                break;
        }
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // Verify the data in buffer (T2B copy) or texture (B2T copy)
        switch (copyType) {
            case Type::T2BCopy:
                EXPECT_BUFFER_U32_RANGE_EQ(expectedBufferData.data(), buffer, 0, bufferSize / 4);
                break;
            case Type::B2TCopy:
                EXPECT_TEXTURE_EQ(expectedTextureData.data(), texture, {0, 0, 0}, kCopySize);
                break;
        }
    }
};

TEST_P(RequiredBufferSizeInCopyTests, AbundantBufferSize) {
    uint64_t size = kOffset + kBytesPerRow * kRowsPerImage * kCopySize.depthOrArrayLayers;
    DoTest(size, Type::T2BCopy);
    DoTest(size, Type::B2TCopy);
}

TEST_P(RequiredBufferSizeInCopyTests, BufferSizeOnBoundary) {
    uint64_t size = kOffset + kBytesPerRow * kRowsPerImage * (kCopySize.depthOrArrayLayers - 1) +
                    kBytesPerRow * (kRowsPerImage - 1) + kBytesPerBlock * kCopySize.width;
    DoTest(size, Type::T2BCopy);
    DoTest(size, Type::B2TCopy);

    // TODO(crbug.com/dawn/1278, 1288, 1289): Required buffer size for copy is wrong on D3D12.
    DAWN_SUPPRESS_TEST_IF(IsD3D12());
    size -= kBytesPerBlock;
    DoTest(size, Type::T2BCopy);
    DoTest(size, Type::B2TCopy);
}

TEST_P(RequiredBufferSizeInCopyTests, MininumBufferSize) {
    // TODO(crbug.com/dawn/1278, 1288, 1289): Required buffer size for copy is wrong on D3D12.
    DAWN_SUPPRESS_TEST_IF(IsD3D12());
    uint64_t size =
        kOffset + utils::RequiredBytesInCopy(kBytesPerRow, kRowsPerImage, kCopySize, kFormat);
    DoTest(size, Type::T2BCopy);
    DoTest(size, Type::B2TCopy);
}

DAWN_INSTANTIATE_TEST(RequiredBufferSizeInCopyTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
