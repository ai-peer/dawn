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

// Tests in this files are used to expose an error on D3D12 about buffer size calculation.
// See detailed bug reports at crbug.com/dawn/1278, 1288, 1289.

// When we do B2T or T2B copy from/to a buffer with paddings, D3D12 may wrongly calculate
// the required buffer size.

// Using the data in this test as an example, in which copySize = {1, 1, 2}, offset = 0, bytesPerRow
// = 256, and rowsPerImage = 2 (there is 1-row padding for every image). And we are copying a
// non-compressed format like rgba8unorm. The required minimum buffer size should be:
//   offset + bytesPerRow * rowsPerImage * (copySize.depthOrArrayLayers - 1)
//     + bytesPerRow * (copySize.height - 1) + bytesPerBlock * copySize.width.
// It is 0 + 256 * 2 * (2 - 1) + 256 * (1 - 1) + 4 * 1 = 516.

// However, D3D12 (including WARP) required minimum buffer size is:
//   offset + bytesPerRow * rowsPerImage * (copySize.depthOrArrayLayers - 1)
//     + bytesPerRow * (rowsPerImage - 1) + bytesPerBlock * copySize.width.
// Or
//   offset + bytesPerRow * rowsPerImage * copySize.depthOrArrayLayers
//     + bytesPerBlock * copySize.width - bytesPerRow.
// It is 0 + 256 * 2 * (2 - 1) + 256 * (2 - 1) + 4 * 1 = 772.

// It looks like D3D12 requires unnecessary buffer storage for rowsPerImagePadding in the last
// image. It does respect bytesPerRowPadding in the last line and doesn't require storage for
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

        wgpu::CommandEncoder encoder = this->device.CreateCommandEncoder();
        switch (copyType) {
            case Type::T2BCopy: {
                std::vector<uint32_t> expectedData(bufferSize / 4, 1);
                wgpu::TextureDataLayout textureDataLayout =
                    utils::CreateTextureDataLayout(kOffset, kBytesPerRow, kRowsPerImage);

                queue.WriteTexture(&imageCopyTexture, expectedData.data(), bufferSize,
                                   &textureDataLayout, &kCopySize);

                encoder.CopyTextureToBuffer(&imageCopyTexture, &imageCopyBuffer, &kCopySize);
                break;
            }
            case Type::B2TCopy:
                encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &kCopySize);
                break;
        }
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }
};

TEST_P(RequiredBufferSizeInCopyTests, T2BCopyWithAbundantBufferSize) {
    uint64_t size = kOffset + kBytesPerRow * kRowsPerImage * kCopySize.depthOrArrayLayers;
    DoTest(size, Type::T2BCopy);
}

TEST_P(RequiredBufferSizeInCopyTests, B2TCopyWithAbundantBufferSize) {
    uint64_t size = kOffset + kBytesPerRow * kRowsPerImage * kCopySize.depthOrArrayLayers;
    DoTest(size, Type::B2TCopy);
}

TEST_P(RequiredBufferSizeInCopyTests, T2BCopyWithMininumBufferSize) {
    // TODO(crbug.com/dawn/1278, 1288, 1289): Required buffer size for copy is wrong on D3D12.
    DAWN_SUPPRESS_TEST_IF(IsD3D12());
    uint64_t size =
        kOffset + utils::RequiredBytesInCopy(kBytesPerRow, kRowsPerImage, kCopySize, kFormat);
    DoTest(size, Type::T2BCopy);
}

TEST_P(RequiredBufferSizeInCopyTests, B2TCopyWithMininumBufferSize) {
    // TODO(crbug.com/dawn/1278, 1288, 1289): Required buffer size for copy is wrong on D3D12.
    DAWN_SUPPRESS_TEST_IF(IsD3D12());
    uint64_t size =
        kOffset + utils::RequiredBytesInCopy(kBytesPerRow, kRowsPerImage, kCopySize, kFormat);
    DoTest(size, Type::B2TCopy);
}

DAWN_INSTANTIATE_TEST(RequiredBufferSizeInCopyTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
