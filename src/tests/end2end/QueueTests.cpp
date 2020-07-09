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

// This file contains test for deprecated parts of Dawn's API while following WebGPU's evolution.
// It contains test for the "old" behavior that will be deleted once users are migrated, tests that
// a deprecation warning is emitted when the "old" behavior is used, and tests that an error is
// emitted when both the old and the new behavior are used (when applicable).

#include "tests/DawnTest.h"

#include "utils/TextureFormatUtils.h"
#include "utils/WGPUHelpers.h"

class QueueTests : public DawnTest {};

// Test that GetDefaultQueue always returns the same object.
TEST_P(QueueTests, GetDefaultQueueSameObject) {
    wgpu::Queue q1 = device.GetDefaultQueue();
    wgpu::Queue q2 = device.GetDefaultQueue();
    EXPECT_EQ(q1.Get(), q2.Get());
}

DAWN_INSTANTIATE_TEST(QueueTests,
                      D3D12Backend(),
                      MetalBackend(),
                      NullBackend(),
                      OpenGLBackend(),
                      VulkanBackend());

class QueueWriteBufferTests : public DawnTest {};

// Test the simplest WriteBuffer setting one u32 at offset 0.
TEST_P(QueueWriteBufferTests, SmallDataAtZero) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    uint32_t value = 0x01020304;
    queue.WriteBuffer(buffer, 0, &value, sizeof(value));

    EXPECT_BUFFER_U32_EQ(value, buffer, 0);
}

// Test an empty WriteBuffer
TEST_P(QueueWriteBufferTests, ZeroSized) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    uint32_t initialValue = 0x42;
    queue.WriteBuffer(buffer, 0, &initialValue, sizeof(initialValue));

    queue.WriteBuffer(buffer, 0, nullptr, 0);

    // The content of the buffer isn't changed
    EXPECT_BUFFER_U32_EQ(initialValue, buffer, 0);
}

// Call WriteBuffer at offset 0 via a u32 twice. Test that data is updated accoordingly.
TEST_P(QueueWriteBufferTests, SetTwice) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    uint32_t value = 0x01020304;
    queue.WriteBuffer(buffer, 0, &value, sizeof(value));

    EXPECT_BUFFER_U32_EQ(value, buffer, 0);

    value = 0x05060708;
    queue.WriteBuffer(buffer, 0, &value, sizeof(value));

    EXPECT_BUFFER_U32_EQ(value, buffer, 0);
}

// Test that WriteBuffer offset works.
TEST_P(QueueWriteBufferTests, SmallDataAtOffset) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4000;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    constexpr uint64_t kOffset = 2000;
    uint32_t value = 0x01020304;
    queue.WriteBuffer(buffer, kOffset, &value, sizeof(value));

    EXPECT_BUFFER_U32_EQ(value, buffer, kOffset);
}

// Stress test for many calls to WriteBuffer
TEST_P(QueueWriteBufferTests, ManyWriteBuffer) {
    // Note: Increasing the size of the buffer will likely cause timeout issues.
    // In D3D12, timeout detection occurs when the GPU scheduler tries but cannot preempt the task
    // executing these commands in-flight. If this takes longer than ~2s, a device reset occurs and
    // fails the test. Since GPUs may or may not complete by then, this test must be disabled OR
    // modified to be well-below the timeout limit.

    // TODO (jiawei.shao@intel.com): find out why this test fails on Intel Vulkan Linux bots.
    DAWN_SKIP_TEST_IF(IsIntel() && IsVulkan() && IsLinux());
    // TODO(https://bugs.chromium.org/p/dawn/issues/detail?id=228): Re-enable
    // once the issue with Metal on 10.14.6 is fixed.
    DAWN_SKIP_TEST_IF(IsMacOS() && IsIntel() && IsMetal());

    constexpr uint64_t kSize = 4000 * 1000;
    constexpr uint32_t kElements = 250 * 250;
    wgpu::BufferDescriptor descriptor;
    descriptor.size = kSize;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    std::vector<uint32_t> expectedData;
    for (uint32_t i = 0; i < kElements; ++i) {
        queue.WriteBuffer(buffer, i * sizeof(uint32_t), &i, sizeof(i));
        expectedData.push_back(i);
    }

    EXPECT_BUFFER_U32_RANGE_EQ(expectedData.data(), buffer, 0, kElements);
}

// Test using WriteBuffer for lots of data
TEST_P(QueueWriteBufferTests, LargeWriteBuffer) {
    constexpr uint64_t kSize = 4000 * 1000;
    constexpr uint32_t kElements = 1000 * 1000;
    wgpu::BufferDescriptor descriptor;
    descriptor.size = kSize;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    std::vector<uint32_t> expectedData;
    for (uint32_t i = 0; i < kElements; ++i) {
        expectedData.push_back(i);
    }

    queue.WriteBuffer(buffer, 0, expectedData.data(), kElements * sizeof(uint32_t));

    EXPECT_BUFFER_U32_RANGE_EQ(expectedData.data(), buffer, 0, kElements);
}

// Test using WriteBuffer for super large data block
TEST_P(QueueWriteBufferTests, SuperLargeWriteBuffer) {
    constexpr uint64_t kSize = 12000 * 1000;
    constexpr uint64_t kElements = 3000 * 1000;
    wgpu::BufferDescriptor descriptor;
    descriptor.size = kSize;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    std::vector<uint32_t> expectedData;
    for (uint32_t i = 0; i < kElements; ++i) {
        expectedData.push_back(i);
    }

    queue.WriteBuffer(buffer, 0, expectedData.data(), kElements * sizeof(uint32_t));

    EXPECT_BUFFER_U32_RANGE_EQ(expectedData.data(), buffer, 0, kElements);
}

DAWN_INSTANTIATE_TEST(QueueWriteBufferTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());

class QueueWriteTextureTests : public DawnTest {
  protected:
    static constexpr wgpu::TextureFormat kTextureFormat = wgpu::TextureFormat::RGBA8Unorm;

    struct TextureSpec {
        wgpu::Origin3D copyOrigin;
        wgpu::Extent3D textureSize;
        uint32_t level;
    };

    struct DataSpec {
        uint64_t size;
        uint64_t offset;
        uint32_t bytesPerRow;
        uint32_t rowsPerImage;
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

    static DataSpec MinimumDataSpec(uint32_t width,
                                    uint32_t rowsPerImage,
                                    uint32_t arrayLayer = 1,
                                    bool testZeroRowsPerImage = true) {
        const uint32_t bytesPerRow = width * utils::GetTexelBlockSizeInBytes(kTextureFormat);
        const uint32_t totalDataSize = utils::GetBytesInBufferTextureCopy(
            kTextureFormat, width, bytesPerRow, rowsPerImage, arrayLayer);
        uint32_t appliedRowsPerImage = testZeroRowsPerImage ? 0 : rowsPerImage;
        return {totalDataSize, 0, bytesPerRow, appliedRowsPerImage};
    }

    static void PackTextureData(const RGBA8* srcData,
                                uint32_t width,
                                uint32_t height,
                                uint32_t srcTexelsPerRow,
                                RGBA8* dstData,
                                uint32_t dstTexelsPerRow) {
        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                unsigned int src = x + y * srcTexelsPerRow;
                unsigned int dst = x + y * dstTexelsPerRow;
                dstData[dst] = srcData[src];
            }
        }
    }

    static void FillData(RGBA8* data, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            data[i] = RGBA8(static_cast<uint8_t>(i % 256), static_cast<uint8_t>((i / 256) % 256),
                            static_cast<uint8_t>((i / 256 / 256) % 256), 255);
        }
    }

    void DoTest(const TextureSpec& textureSpec,
                const DataSpec& dataSpec,
                const wgpu::Extent3D& copySize) {
        // Create data of size `size` and populate it
        const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(kTextureFormat);
        std::vector<RGBA8> data(dataSpec.size / bytesPerTexel);
        FillData(data.data(), data.size());

        // Create a texture that is `width` x `height` with (`level` + 1) mip levels.
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = textureSpec.textureSize;
        descriptor.sampleCount = 1;
        descriptor.format = kTextureFormat;
        descriptor.mipLevelCount = textureSpec.level + 1;
        descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
        wgpu::Texture texture = device.CreateTexture(&descriptor);

        wgpu::Extent3D mipSize = {textureSpec.textureSize.width >> textureSpec.level,
                                  textureSpec.textureSize.height >> textureSpec.level,
                                  textureSpec.textureSize.depth};
        uint32_t texelBlocksPerRow =
            dataSpec.bytesPerRow / utils::GetTexelBlockSizeInBytes(kTextureFormat);
        uint32_t appliedRowsPerImage =
            dataSpec.rowsPerImage > 0 ? dataSpec.rowsPerImage : mipSize.height;
        uint32_t bytesPerImage = dataSpec.bytesPerRow * appliedRowsPerImage;

        const uint32_t maxArrayLayer = textureSpec.copyOrigin.z + copySize.depth;

        wgpu::TextureDataLayout textureDataLayout;
        textureDataLayout.offset = dataSpec.offset;
        textureDataLayout.bytesPerRow = dataSpec.bytesPerRow;
        textureDataLayout.rowsPerImage = dataSpec.rowsPerImage;

        wgpu::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(texture, textureSpec.level, textureSpec.copyOrigin);

        queue.WriteTexture(&textureCopyView, data.data(), dataSpec.size, &textureDataLayout,
                           &copySize);

        uint64_t dataOffset = dataSpec.offset;
        const uint32_t texelCountLastLayer =
            texelBlocksPerRow * (mipSize.height - 1) + mipSize.width;
        for (uint32_t slice = textureSpec.copyOrigin.z; slice < maxArrayLayer; ++slice) {
            // Pack the data in the specified copy region to have the same
            // format as the expected texture data.
            std::vector<RGBA8> expected(texelCountLastLayer);
            PackTextureData(&data[dataOffset / bytesPerTexel], copySize.width, copySize.height,
                            dataSpec.bytesPerRow / bytesPerTexel, expected.data(), copySize.width);

            EXPECT_TEXTURE_RGBA8_EQ(expected.data(), texture, textureSpec.copyOrigin.x,
                                    textureSpec.copyOrigin.y, copySize.width, copySize.height,
                                    textureSpec.level, slice)
                << "Write to texture failed copying " << dataSpec.size << "-byte data with offset "
                << dataSpec.offset << " and bytes per row " << dataSpec.bytesPerRow << " to [("
                << textureSpec.copyOrigin.x << ", " << textureSpec.copyOrigin.y << "), ("
                << textureSpec.copyOrigin.x + copySize.width << ", "
                << textureSpec.copyOrigin.y + copySize.height << ")) region of "
                << textureSpec.textureSize.width << " x " << textureSpec.textureSize.height
                << " texture at mip level " << textureSpec.level << " layer " << slice << std::endl;
            dataOffset += bytesPerImage;
        }
    }
};

// Test that writing to an entire texture with 256-byte aligned dimensions works
TEST_P(QueueWriteTextureTests, FullTextureAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.level = 0;

    DoTest(textureSpec, MinimumDataSpec(kWidth, kHeight), {kWidth, kHeight, 1});
}

// Test that writing to an entire texture without 256-byte aligned dimensions works
TEST_P(QueueWriteTextureTests, FullTextureUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, 1};
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.level = 0;

    DoTest(textureSpec, MinimumDataSpec(kWidth, kHeight), {kWidth, kHeight, 1});
}

// Test that reading pixels from a 256-byte aligned texture works
TEST_P(QueueWriteTextureTests, PixelReadAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    DataSpec pixelData = MinimumDataSpec(1, 1);

    constexpr wgpu::Extent3D kCopySize = {1, 1, 1};
    constexpr wgpu::Extent3D kTextureSize = {kWidth, kHeight, 1};
    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = kTextureSize;
    defaultTextureSpec.level = 0;

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {0, 0, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, 0, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {0, kHeight - 1, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, kHeight - 1, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 3, kHeight / 7, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 7, kHeight / 3, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }
}

// Test that writing pixels to a texture that is not 256-byte aligned works
TEST_P(QueueWriteTextureTests, PixelReadUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;
    DataSpec pixelData = MinimumDataSpec(1, 1);

    constexpr wgpu::Extent3D kCopySize = {1, 1, 1};
    constexpr wgpu::Extent3D kTextureSize = {kWidth, kHeight, 1};
    TextureSpec defaultTextureSpec;
    defaultTextureSpec.textureSize = kTextureSize;
    defaultTextureSpec.level = 0;

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {0, 0, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, 0, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {0, kHeight - 1, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth - 1, kHeight - 1, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 3, kHeight / 7, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }

    {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.copyOrigin = {kWidth / 7, kHeight / 3, 0};
        DoTest(textureSpec, pixelData, kCopySize);
    }
}

// Test that writing regions with 256-byte aligned sizes works
TEST_P(QueueWriteTextureTests, TextureRegionAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    for (unsigned int w : {64, 128, 256}) {
        for (unsigned int h : {16, 32, 48}) {
            TextureSpec textureSpec;
            textureSpec.copyOrigin = {0, 0, 0};
            textureSpec.level = 0;
            textureSpec.textureSize = {kWidth, kHeight, 1};
            DoTest(textureSpec, MinimumDataSpec(w, h), {w, h, 1});
        }
    }
}

// Test that writing regions without 256-byte aligned sizes works
TEST_P(QueueWriteTextureTests, TextureRegionUnaligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.copyOrigin = {0, 0, 0};
    defaultTextureSpec.level = 0;
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int w : {13, 63, 65}) {
        for (unsigned int h : {17, 19, 63}) {
            TextureSpec textureSpec = defaultTextureSpec;
            DoTest(textureSpec, MinimumDataSpec(w, h), {w, h, 1});
        }
    }
}

// Test that writing to mips with 256-byte aligned sizes works
TEST_P(QueueWriteTextureTests, TextureMipAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.copyOrigin = {0, 0, 0};
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.level = i;
        DoTest(textureSpec, MinimumDataSpec(kWidth >> i, kHeight >> i),
               {kWidth >> i, kHeight >> i, 1});
    }
}

// Test that writing to mips without 256-byte aligned sizes works
TEST_P(QueueWriteTextureTests, TextureMipUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec defaultTextureSpec;
    defaultTextureSpec.copyOrigin = {0, 0, 0};
    defaultTextureSpec.textureSize = {kWidth, kHeight, 1};

    for (unsigned int i = 1; i < 4; ++i) {
        TextureSpec textureSpec = defaultTextureSpec;
        textureSpec.level = i;
        DoTest(textureSpec, MinimumDataSpec(kWidth >> i, kHeight >> i),
               {kWidth >> i, kHeight >> i, 1});
    }
}

// Test that writing with a 512-byte aligned data offset works
TEST_P(QueueWriteTextureTests, DataOffsetAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.textureSize = {kWidth, kHeight, 1};
    textureSpec.level = 0;

    for (unsigned int i = 0; i < 3; ++i) {
        DataSpec DataSpec = MinimumDataSpec(kWidth, kHeight);
        uint64_t offset = 512 * i;
        DataSpec.size += offset;
        DataSpec.offset += offset;
        DoTest(textureSpec, DataSpec, {kWidth, kHeight, 1});
    }
}

// Test that writing without a 512-byte aligned data offset works
TEST_P(QueueWriteTextureTests, DataOffsetUnaligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.textureSize = {kWidth, kHeight, 1};
    textureSpec.level = 0;

    const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(kTextureFormat);
    for (uint32_t i = bytesPerTexel; i < 512; i += bytesPerTexel * 9) {
        DataSpec DataSpec = MinimumDataSpec(kWidth, kHeight);
        DataSpec.size += i;
        DataSpec.offset += i;
        DoTest(textureSpec, DataSpec, {kWidth, kHeight, 1});
    }
}

// Test that writing without a 512-byte aligned data offset that is greater than the bytes per row
// works
TEST_P(QueueWriteTextureTests, DataOffsetUnalignedSmallRowPitch) {
    constexpr uint32_t kWidth = 32;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.textureSize = {kWidth, kHeight, 1};
    textureSpec.level = 0;

    const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(kTextureFormat);
    for (uint32_t i = 256 + bytesPerTexel; i < 512; i += bytesPerTexel * 9) {
        DataSpec DataSpec = MinimumDataSpec(kWidth, kHeight);
        DataSpec.size += i;
        DataSpec.offset += i;
        DoTest(textureSpec, DataSpec, {kWidth, kHeight, 1});
    }
}

// Test that writing with a greater bytes per row than needed on a 256-byte aligned texture works
TEST_P(QueueWriteTextureTests, RowPitchAligned) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.textureSize = {kWidth, kHeight, 1};
    textureSpec.level = 0;

    DataSpec DataSpec = MinimumDataSpec(kWidth, kHeight);
    for (unsigned int i = 1; i < 4; ++i) {
        DataSpec.bytesPerRow += 256;
        DataSpec.size += 256 * kHeight;
        DoTest(textureSpec, DataSpec, {kWidth, kHeight, 1});
    }
}

// Test that writing with a greater bytes per row than needed on a texture that is not 256-byte
// aligned works
TEST_P(QueueWriteTextureTests, RowPitchUnaligned) {
    constexpr uint32_t kWidth = 259;
    constexpr uint32_t kHeight = 127;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.textureSize = {kWidth, kHeight, 1};
    textureSpec.level = 0;

    DataSpec DataSpec = MinimumDataSpec(kWidth, kHeight);
    for (unsigned int i = 1; i < 4; ++i) {
        DataSpec.bytesPerRow += 256;
        DataSpec.size += 256 * kHeight;
        DoTest(textureSpec, DataSpec, {kWidth, kHeight, 1});
    }
}

// Test that writing to whole texture 2D array layers works.
TEST_P(QueueWriteTextureTests, Texture2DArrayRegion) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, 0};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};
    textureSpec.level = 0;

    DoTest(textureSpec, MinimumDataSpec(kWidth, kHeight, kLayers), {kWidth, kHeight, kLayers});
}

// Test that writing a range of texture 2D array layers works.
TEST_P(QueueWriteTextureTests, Texture2DArraySubRegion) {
    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 3u;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};
    textureSpec.level = 0;

    DoTest(textureSpec, MinimumDataSpec(kWidth, kHeight, kCopyLayers),
           {kWidth, kHeight, kCopyLayers});
}

// Test that writing into a range of texture 2D array layers with.
// RowsPerImage not equal to the height of the texture works.
TEST_P(QueueWriteTextureTests, Texture2DArrayRegionNonzeroRowsPerImage) {
    // TODO(jiawei.shao@intel.com): investigate why copies with multiple texture array layers fail
    // with swiftshader.
    DAWN_SKIP_TEST_IF(IsSwiftshader());

    constexpr uint32_t kWidth = 256;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kLayers = 6u;
    constexpr uint32_t kBaseLayer = 2u;
    constexpr uint32_t kCopyLayers = 3u;

    constexpr uint32_t kRowsPerImage = kHeight * 2;

    TextureSpec textureSpec;
    textureSpec.copyOrigin = {0, 0, kBaseLayer};
    textureSpec.textureSize = {kWidth, kHeight, kLayers};
    textureSpec.level = 0;

    DataSpec DataSpec = MinimumDataSpec(kWidth, kRowsPerImage, kCopyLayers, false);
    DataSpec.rowsPerImage = kRowsPerImage;
    DoTest(textureSpec, DataSpec, {kWidth, kHeight, kCopyLayers});
}

// Test with bytesPerRow not divisible by 256.
TEST_P(QueueWriteTextureTests, BytesPerRowNotDivisibleBy256) {
    constexpr uint32_t kWidth = 257;
    constexpr uint32_t kHeight = 129;
    constexpr uint32_t kLayers = 65;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight, kLayers};
    textureSpec.copyOrigin = {1, 2, 3};
    textureSpec.level = 0;

    constexpr wgpu::Extent3D copyExtent = {17, 19, 21};

    // Test with bytesPerRow divisible by blockWidth
    for (uint32_t b = 0; b < 4; ++b) {
        DataSpec DataSpec;
        DataSpec.offset = 0;
        DataSpec.bytesPerRow =  (copyExtent.width + b) * utils::GetTexelBlockSizeInBytes(kTextureFormat);
        DataSpec.rowsPerImage = 23;
        DataSpec.size = utils::RequiredBytesInCopy(DataSpec.bytesPerRow, DataSpec.rowsPerImage,
                                                   copyExtent, kTextureFormat);
        DoTest(textureSpec, DataSpec, copyExtent);
    }
}

DAWN_INSTANTIATE_TEST(QueueWriteTextureTests,
                      // D3D12Backend(),
                      MetalBackend(),
                      // OpenGLBackend(),
                      // VulkanBackend()
);
