// Copyright 2018 The Dawn Authors
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

#include "tests/unittests/validation/ValidationTest.h"

#include "utils/WGPUHelpers.h"
#include "common/Math.h"

namespace {

class QueueSubmitValidationTest : public ValidationTest {
};

static void StoreTrueMapWriteCallback(WGPUBufferMapAsyncStatus status,
                                      void*,
                                      uint64_t,
                                      void* userdata) {
    *static_cast<bool*>(userdata) = true;
}

// Test submitting with a mapped buffer is disallowed
TEST_F(QueueSubmitValidationTest, SubmitWithMappedBuffer) {
    // Create a map-write buffer.
    wgpu::BufferDescriptor descriptor;
    descriptor.usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;
    descriptor.size = 4;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    // Create a fake copy destination buffer
    descriptor.usage = wgpu::BufferUsage::CopyDst;
    wgpu::Buffer targetBuffer = device.CreateBuffer(&descriptor);

    // Create a command buffer that reads from the mappable buffer.
    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(buffer, 0, targetBuffer, 0, 4);
        commands = encoder.Finish();
    }

    wgpu::Queue queue = device.GetDefaultQueue();

    // Submitting when the buffer has never been mapped should succeed
    queue.Submit(1, &commands);

    // Map the buffer, submitting when the buffer is mapped should fail
    bool mapWriteFinished = false;
    buffer.MapWriteAsync(StoreTrueMapWriteCallback, &mapWriteFinished);
    queue.Submit(0, nullptr);
    ASSERT_TRUE(mapWriteFinished);

    ASSERT_DEVICE_ERROR(queue.Submit(1, &commands));

    // Unmap the buffer, queue submit should succeed
    buffer.Unmap();
    queue.Submit(1, &commands);
}

class QueueWriteBufferValidationTest : public ValidationTest {
  private:
    void SetUp() override {
        ValidationTest::SetUp();
        queue = device.GetDefaultQueue();
    }

  protected:
    wgpu::Buffer CreateBuffer(uint64_t size) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = size;
        descriptor.usage = wgpu::BufferUsage::CopyDst;
        return device.CreateBuffer(&descriptor);
    }

    wgpu::Queue queue;
};

// Test the success case for WriteBuffer
TEST_F(QueueWriteBufferValidationTest, Success) {
    wgpu::Buffer buf = CreateBuffer(4);

    uint32_t foo = 0x01020304;
    queue.WriteBuffer(buf, 0, &foo, sizeof(foo));
}

// Test error case for WriteBuffer out of bounds
TEST_F(QueueWriteBufferValidationTest, OutOfBounds) {
    wgpu::Buffer buf = CreateBuffer(4);

    uint32_t foo[2] = {0, 0};
    ASSERT_DEVICE_ERROR(queue.WriteBuffer(buf, 0, foo, 8));
}

// Test error case for WriteBuffer out of bounds with an overflow
TEST_F(QueueWriteBufferValidationTest, OutOfBoundsOverflow) {
    wgpu::Buffer buf = CreateBuffer(1024);

    uint32_t foo[2] = {0, 0};

    // An offset that when added to "4" would overflow to be zero and pass validation without
    // overflow checks.
    uint64_t offset = uint64_t(int64_t(0) - int64_t(4));

    ASSERT_DEVICE_ERROR(queue.WriteBuffer(buf, offset, foo, 4));
}

// Test error case for WriteBuffer with the wrong usage
TEST_F(QueueWriteBufferValidationTest, WrongUsage) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::Vertex;
    wgpu::Buffer buf = device.CreateBuffer(&descriptor);

    uint32_t foo = 0;
    ASSERT_DEVICE_ERROR(queue.WriteBuffer(buf, 0, &foo, sizeof(foo)));
}

// Test WriteBuffer with unaligned size
TEST_F(QueueWriteBufferValidationTest, UnalignedSize) {
    wgpu::Buffer buf = CreateBuffer(4);

    uint16_t value = 123;
    ASSERT_DEVICE_ERROR(queue.WriteBuffer(buf, 0, &value, sizeof(value)));
}

// Test WriteBuffer with unaligned offset
TEST_F(QueueWriteBufferValidationTest, UnalignedOffset) {
    wgpu::Buffer buf = CreateBuffer(8);

    uint32_t value = 0x01020304;
    ASSERT_DEVICE_ERROR(queue.WriteBuffer(buf, 2, &value, sizeof(value)));
}

// Test WriteBuffer with destroyed buffer
TEST_F(QueueWriteBufferValidationTest, DestroyedBuffer) {
    wgpu::Buffer buf = CreateBuffer(4);
    buf.Destroy();

    uint32_t value = 0;
    ASSERT_DEVICE_ERROR(queue.WriteBuffer(buf, 0, &value, sizeof(value)));
}

// Test WriteBuffer with mapped buffer
TEST_F(QueueWriteBufferValidationTest, MappedBuffer) {
    // CreateBufferMapped
    {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = 4;
        descriptor.usage = wgpu::BufferUsage::CopyDst;
        wgpu::CreateBufferMappedResult result = device.CreateBufferMapped(&descriptor);

        uint32_t value = 0;
        ASSERT_DEVICE_ERROR(queue.WriteBuffer(result.buffer, 0, &value, sizeof(value)));
    }

    // MapReadAsync
    {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = 4;
        descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
        wgpu::Buffer buf = device.CreateBuffer(&descriptor);

        buf.MapReadAsync(nullptr, nullptr);
        uint32_t value = 0;
        ASSERT_DEVICE_ERROR(queue.WriteBuffer(buf, 0, &value, sizeof(value)));
    }

    // MapWriteAsync
    {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = 4;
        descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
        wgpu::Buffer buf = device.CreateBuffer(&descriptor);

        buf.MapReadAsync(nullptr, nullptr);
        uint32_t value = 0;
        ASSERT_DEVICE_ERROR(queue.WriteBuffer(buf, 0, &value, sizeof(value)));
    }
}

class QueueWriteTextureValidationTest : public ValidationTest {
  private:
    void SetUp() override {
        ValidationTest::SetUp();
        queue = device.GetDefaultQueue();
    }

  protected:
    wgpu::Texture Create2DTexture(uint32_t width,
                                  uint32_t height,
                                  uint32_t mipLevelCount,
                                  uint32_t arrayLayerCount,
                                  wgpu::TextureFormat format,
                                  wgpu::TextureUsage usage,
                                  uint32_t sampleCount = 1) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size.width = width;
        descriptor.size.height = height;
        descriptor.size.depth = arrayLayerCount;
        descriptor.sampleCount = sampleCount;
        descriptor.format = format;
        descriptor.mipLevelCount = mipLevelCount;
        descriptor.usage = usage;
        wgpu::Texture tex = device.CreateTexture(&descriptor);
        return tex;
    }

    uint64_t RequiredBytesInCopy(uint32_t bytesPerRow,
                                 uint32_t rowsPerImage,
                                 wgpu::Extent3D copyExtent,
                                 wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm) {
        if (copyExtent.width == 0 || copyExtent.height == 0 || copyExtent.depth == 0) {
            return 0;
        } else {
            uint64_t bytesPerImage = bytesPerRow * rowsPerImage;
            uint64_t bytesInLastSlice =
                bytesPerRow * (copyExtent.height - 1) +
                (copyExtent.width * utils::TextureFormatPixelSize(format));
            return bytesPerImage * (copyExtent.depth - 1) + bytesInLastSlice;
        }
    }

    void TestWriteTexture(const void* data,
                          size_t dataSize,
                          uint32_t dataOffset,
                          uint32_t dataBytesPerRow,
                          uint32_t dataRowsPerImage,
                          wgpu::Texture texture,
                          uint32_t textLevel,
                          wgpu::Origin3D textOrigin,
                          wgpu::Extent3D size) {
        wgpu::TextureDataLayout textureDataLayout;
        textureDataLayout.offset = dataOffset;
        textureDataLayout.bytesPerRow = dataBytesPerRow;
        textureDataLayout.rowsPerImage = dataRowsPerImage;

        wgpu::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(texture, textLevel, textOrigin);

        queue.WriteTexture(&textureCopyView, data, dataSize, &textureDataLayout, &size);
    }

    wgpu::Queue queue;
};

// Test the success case for WriteTexture
TEST_F(QueueWriteTextureValidationTest, Success) {
    const uint64_t dataSize = RequiredBytesInCopy(256, 0, {4, 4, 1});
    const void* data = malloc(dataSize);
    wgpu::Texture destination =
        Create2DTexture(16, 16, 5, 4, wgpu::TextureFormat::RGBA8Unorm, wgpu::TextureUsage::CopyDst);

    // Different copies, including some that touch the OOB condition
    {
        // Copy 4x4 block in corner of first mip.
        TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {0, 0, 0}, {4, 4, 1});
        // Copy 4x4 block in opposite corner of first mip.
        TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {12, 12, 0}, {4, 4, 1});
        // Copy 4x4 block in the 4x4 mip.
        TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 2, {0, 0, 0}, {4, 4, 1});
        // Copy with a data offset
        TestWriteTexture(&data, dataSize, dataSize - 4, 256, 0, destination, 0, {0, 0, 0}, {1, 1, 1});
    }

    // Copies with a 256-byte aligned bytes per row but unaligned texture region
    {
        // Unaligned region
        TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {0, 0, 0},
                    {3, 4, 1});
        // Unaligned region with texture offset
        TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {5, 7, 0},
                    {2, 3, 1});
        // Unaligned region, with data offset
        TestWriteTexture(&data, dataSize, 31 * 4, 256, 0, destination, 0, {0, 0, 0},
                    {3, 3, 1});
    }

    // Empty copies are valid
    {
        // An empty copy
        TestWriteTexture(&data, dataSize, 0, 0, 0, destination, 0, {0, 0, 0},
                    {0, 0, 1});
        // An empty copy with depth = 0
        TestWriteTexture(&data, dataSize, 0, 0, 0, destination, 0, {0, 0, 0},
                    {0, 0, 0});
        // An empty copy touching the end of the data
        TestWriteTexture(&data, dataSize, dataSize, 0, 0, destination, 0, {0, 0, 0},
                    {0, 0, 1});
        // An empty copy touching the side of the texture
        TestWriteTexture(&data, dataSize, 0, 0, 0, destination, 0, {16, 16, 0},
                    {0, 0, 1});
        // An empty copy with depth = 1 and bytesPerRow > 0
        TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {0, 0, 0},
                    {0, 0, 1});
        // An empty copy with height > 0, depth = 0, bytesPerRow > 0 and rowsPerImage > 0
        TestWriteTexture(&data, dataSize, 0, 256, 16, destination, 0, {0, 0, 0},
                    {0, 1, 0});
    }
}

// Test OOB conditions on the data
TEST_F(QueueWriteTextureValidationTest, OutOfBoundsOnData) {
    const uint64_t dataSize = RequiredBytesInCopy(256, 0, {4, 4, 1});
    const void* data = malloc(dataSize);
    wgpu::Texture destination =
        Create2DTexture(16, 16, 5, 1, wgpu::TextureFormat::RGBA8Unorm, wgpu::TextureUsage::CopyDst);

    // OOB on the data because we copy too many pixels
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {0, 0, 0},
                {4, 5, 1}));

    // OOB on the data because of the offset
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 4, 256, 0, destination, 0, {0, 0, 0},
                {4, 4, 1}));

    // OOB on the data because RequiredBytesInCopy overflows
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 512, 0, destination, 0, {0, 0, 0},
                {4, 3, 1}));

    // Not OOB on the data although bytes per row * height overflows
    // but RequiredBytesInCopy * depth does not overflow
    {
        uint32_t sourceDataSize = RequiredBytesInCopy(256, 0, {7, 3, 1});
        ASSERT_TRUE(256 * 3 > sourceDataSize) << "bytes per row * height should overflow data";
        const void* sourceData = malloc(sourceDataSize);

        TestWriteTexture(&sourceData, sourceDataSize, 0, 256, 0, destination, 0, {0, 0, 0},
                    {7, 3, 1});
    }
}

// Test OOB conditions on the texture
TEST_F(QueueWriteTextureValidationTest, OutOfBoundsOnTexture) {
    const uint64_t dataSize = RequiredBytesInCopy(256, 0, {4, 4, 1});
    const void* data = malloc(dataSize);
    wgpu::Texture destination =
        Create2DTexture(16, 16, 5, 2, wgpu::TextureFormat::RGBA8Unorm, wgpu::TextureUsage::CopyDst);

    // OOB on the texture because x + width overflows
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {13, 12, 0},
                {4, 4, 1}));

    // OOB on the texture because y + width overflows
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {12, 13, 0},
                {4, 4, 1}));

    // OOB on the texture because we overflow a non-zero mip
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 2, {1, 0, 0},
                {4, 4, 1}));

    // OOB on the texture even on an empty copy when we copy to a non-existent mip.
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 0, 0, destination, 5, {0, 0, 0},
                {0, 0, 1}));

    // OOB on the texture because slice overflows
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 0, 0, destination, 0, {0, 0, 2},
                {0, 0, 1}));
}

// Test that we force Depth=1 on writes to 2D textures
TEST_F(QueueWriteTextureValidationTest, DepthConstraintFor2DTextures) {
    const uint64_t dataSize = RequiredBytesInCopy(0, 0, {0, 0, 2});
    const void* data = malloc(dataSize);
    wgpu::Texture destination =
        Create2DTexture(16, 16, 5, 1, wgpu::TextureFormat::RGBA8Unorm, wgpu::TextureUsage::CopyDst);

    // Depth > 1 on an empty copy still errors
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 0, 0,
                destination, 0, {0, 0, 0}, {0, 0, 2}));
}

// Test WriteTexture with incorrect texture usage
TEST_F(QueueWriteTextureValidationTest, IncorrectUsage) {
    const uint64_t dataSize = RequiredBytesInCopy(256, 0, {4, 4, 1});
    const void* data = malloc(dataSize);
    wgpu::Texture sampled =
        Create2DTexture(16, 16, 5, 1, wgpu::TextureFormat::RGBA8Unorm, wgpu::TextureUsage::Sampled);

    // Incorrect destination usage
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, sampled, 0, {0, 0, 0},
                {4, 4, 1}));
}

TEST_F(QueueWriteTextureValidationTest, IncorrectBytesPerRow) {
    uint64_t dataSize = RequiredBytesInCopy(256, 0, {128, 16, 1});
    const void* data = malloc(dataSize);
    wgpu::Texture destination = Create2DTexture(128, 16, 5, 1, wgpu::TextureFormat::RGBA8Unorm,
                                                wgpu::TextureUsage::CopyDst);

    // bytes per row is 0
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 0, 0, destination, 0, {0, 0, 0},
                {64, 4, 1}));

    // bytes per row doesn't have to be 256-byte aligned
    TestWriteTexture(&data, dataSize, 0, 128, 0, destination, 0, {0, 0, 0}, {4, 4, 1});

    // bytes per row is less than width * bytesPerPixel
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {0, 0, 0},
                {65, 1, 1}));
}

// Test with bytesPerRow not divisible by 256.
TEST_F(QueueWriteTextureValidationTest, BytesPerRowNotDivisibleBy256) {
    const void* data = malloc(128);
    wgpu::Texture destination = Create2DTexture(3, 7, 1, 1, wgpu::TextureFormat::RGBA8Unorm,
                                                    wgpu::TextureUsage::CopyDst);

    // bytesPerRow set to 4, this is the minimal valid value with width = 1.
    TestWriteTexture(&data, 128, 0, 4, 0, destination, 0, {0, 0, 0}, {1, 7, 1});

    // bytesPerRow set to 2 is below the minimum
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, 128, 0, 2, 0, destination, 0,
                                        {0, 0, 0}, {1, 7, 1}));

    // bytesPerRow = 13 is valid since a row takes 12 bytes.
    TestWriteTexture(&data, 128, 0, 13, 0, destination, 0, {0, 0, 0}, {3, 7, 1});

    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, 128, 0, 11, 0, destination, 0,
                                        {0, 0, 0}, {3, 7, 1}));
}

TEST_F(QueueWriteTextureValidationTest, ImageHeightConstraint) {
    uint64_t dataSize = RequiredBytesInCopy(256, 0, {4, 4, 1});
    const void* data = malloc(dataSize);
    wgpu::Texture destination =
        Create2DTexture(16, 16, 1, 1, wgpu::TextureFormat::RGBA8Unorm, wgpu::TextureUsage::CopyDst);

    // Image height is zero (Valid)
    TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {0, 0, 0},
                {4, 4, 1});

    // Image height is equal to copy height (Valid)
    TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {0, 0, 0},
                {4, 4, 1});

    // Image height is larger than copy height (Valid)
    TestWriteTexture(&data, dataSize, 0, 256, 5, destination, 0, {0, 0, 0},
                {4, 4, 1});

    // Image height is less than copy height (Invalid)
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 3, destination, 0, {0, 0, 0},
                {4, 4, 1}));
}

// Test WriteTexture with incorrect data offset usage
TEST_F(QueueWriteTextureValidationTest, IncorrectDataOffset) {
    uint64_t dataSize = RequiredBytesInCopy(256, 0, {4, 4, 1});
    const void* data = malloc(dataSize);
    wgpu::Texture destination =
        Create2DTexture(16, 16, 5, 1, wgpu::TextureFormat::RGBA8Unorm, wgpu::TextureUsage::CopyDst);

    // Correct usage
    TestWriteTexture(&data, dataSize, dataSize - 4, 256, 0, destination, 0,
                {0, 0, 0}, {1, 1, 1});

    // Incorrect usages
    {
        ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, dataSize - 5, 256, 0, destination, 0,
                    {0, 0, 0}, {1, 1, 1}));
        ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, dataSize - 6, 256, 0, destination, 0,
                    {0, 0, 0}, {1, 1, 1}));
        ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, dataSize - 7, 256, 0, destination, 0,
                    {0, 0, 0}, {1, 1, 1}));
    }
}

// Test multisampled textures can be used in WriteTexture.
TEST_F(QueueWriteTextureValidationTest, WriteToMultisampledTexture) {
    uint64_t dataSize = RequiredBytesInCopy(256, 0, {2, 2, 1});
    const void* data = malloc(dataSize);
    wgpu::Texture destination = Create2DTexture(2, 2, 1, 1, wgpu::TextureFormat::RGBA8Unorm,
                                                wgpu::TextureUsage::CopyDst, 4);

    TestWriteTexture(&data, dataSize, 0, 256, 0, destination, 0, {0, 0, 0}, {2, 2, 1});
}

// Test WriteTexture with texture in error state causes errors.
TEST_F(QueueWriteTextureValidationTest, TextureInErrorState) {
    wgpu::TextureDescriptor errorTextureDescriptor;
    errorTextureDescriptor.size.depth = 0;
    ASSERT_DEVICE_ERROR(wgpu::Texture errorTexture = device.CreateTexture(&errorTextureDescriptor));
    wgpu::TextureCopyView errorTextureCopyView =
        utils::CreateTextureCopyView(errorTexture, 0, {0, 0, 0});

    wgpu::Extent3D extent3D = {1, 1, 1};

    {
        const void* data = malloc(4);
        wgpu::TextureDataLayout textureDataLayout;
        textureDataLayout.offset = 0;
        textureDataLayout.bytesPerRow = 0;
        textureDataLayout.rowsPerImage = 0;

        ASSERT_DEVICE_ERROR(
            queue.WriteTexture(&errorTextureCopyView, &data, 4, &textureDataLayout, &extent3D)
        );
    }
}

// Regression tests for a bug in the computation of texture data size in Dawn.
TEST_F(QueueWriteTextureValidationTest, TextureWriteDataSizeLastRowComputation) {
    constexpr uint32_t kBytesPerRow = 256;
    constexpr uint32_t kWidth = 4;
    constexpr uint32_t kHeight = 4;

    constexpr std::array<wgpu::TextureFormat, 2> kFormats = {wgpu::TextureFormat::RGBA8Unorm,
                                                             wgpu::TextureFormat::RG8Unorm};

    {
        // kBytesPerRow * (kHeight - 1) + kWidth is not large enough to be the valid data size in
        // this test because the data sizes in WriteTexture are not in texels but in bytes.
        constexpr uint32_t kInvalidDataSize = kBytesPerRow * (kHeight - 1) + kWidth;

        for (wgpu::TextureFormat format : kFormats) {
            const void* data = malloc(kInvalidDataSize);
            wgpu::Texture destination =
                Create2DTexture(kWidth, kHeight, 1, 1, format, wgpu::TextureUsage::CopyDst);
            ASSERT_DEVICE_ERROR(TestWriteTexture(&data, kInvalidDataSize, 0, kBytesPerRow, 0,
                        destination, 0, {0, 0, 0}, {kWidth, kHeight, 1}));
        }
    }

    {
        for (wgpu::TextureFormat format : kFormats) {
            uint32_t validDataSize = RequiredBytesInCopy(kBytesPerRow, 0, {kWidth, kHeight, 1}, format);
            wgpu::Texture destination =
                Create2DTexture(kWidth, kHeight, 1, 1, format, wgpu::TextureUsage::CopyDst);

            // Verify the return value of RequiredBytesInCopu() is exactly the minimum valid
            // data size in this test.
            {
                uint32_t invalidDataSize = validDataSize - 1;
                const void* data = malloc(invalidDataSize);
                ASSERT_DEVICE_ERROR(TestWriteTexture(&data, invalidDataSize, 0, kBytesPerRow, 0,
                    destination, 0, {0, 0, 0}, {kWidth, kHeight, 1}));
            }

            {
                const void* data = malloc(validDataSize);
                TestWriteTexture(&data, validDataSize, 0, kBytesPerRow, 0, destination, 0,
                            {0, 0, 0}, {kWidth, kHeight, 1});
            }
        }
    }
}

// Test write from data to mip map of non square texture
TEST_F(QueueWriteTextureValidationTest, WriteToMipmapOfNonSquareTexture) {
    uint64_t dataSize = RequiredBytesInCopy(256, 0, {4, 2, 1});
    const void* data = malloc(dataSize);
    uint32_t maxMipmapLevel = 3;
    wgpu::Texture destination = Create2DTexture(
        4, 2, maxMipmapLevel, 1, wgpu::TextureFormat::RGBA8Unorm, wgpu::TextureUsage::CopyDst);

    // Copy to top level mip map
    TestWriteTexture(&data, dataSize, 0, 256, 0, destination, maxMipmapLevel - 1,
                {0, 0, 0}, {1, 1, 1});
    // Copy to high level mip map
    TestWriteTexture(&data, dataSize, 0, 256, 0, destination, maxMipmapLevel - 2,
                {0, 0, 0}, {2, 1, 1});
    // Mip level out of range
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, destination, maxMipmapLevel,
                {0, 0, 0}, {1, 1, 1}));
    // Copy origin out of range
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, destination, maxMipmapLevel - 2,
                {1, 0, 0}, {2, 1, 1}));
    // Copy size out of range
    ASSERT_DEVICE_ERROR(TestWriteTexture(&data, dataSize, 0, 256, 0, destination, maxMipmapLevel - 2,
                {0, 0, 0}, {2, 2, 1}));
}

class WriteTextureTest_CompressedTextureFormats : public QueueWriteTextureValidationTest {
  public:
    WriteTextureTest_CompressedTextureFormats() : QueueWriteTextureValidationTest() {
        device = CreateDeviceFromAdapter(adapter, {"texture_compression_bc"});
    }

  protected:
    wgpu::Texture Create2DTexture(wgpu::TextureFormat format,
                                  uint32_t mipmapLevels = 1,
                                  uint32_t width = kWidth,
                                  uint32_t height = kHeight) {
        constexpr wgpu::TextureUsage kUsage = wgpu::TextureUsage::CopyDst;
        constexpr uint32_t kArrayLayers = 1;
        return QueueWriteTextureValidationTest::Create2DTexture(width, height, mipmapLevels, kArrayLayers,
                                                                format, kUsage, 1);
    }

    void TestWriteTexture(const void* data,
                          size_t dataSize,
                          uint32_t dataOffset,
                          uint32_t dataBytesPerRow,
                          uint32_t dataRowsPerImage,
                          wgpu::Texture texture,
                          uint32_t textLevel,
                          wgpu::Origin3D textOrigin,
                          wgpu::Extent3D size) {
        QueueWriteTextureValidationTest::TestWriteTexture(
            data, dataSize, dataOffset, dataBytesPerRow, dataRowsPerImage,
            texture, textLevel, textOrigin, size
        );
    }

    static constexpr uint32_t kWidth = 16;
    static constexpr uint32_t kHeight = 16;
};

// Tests to verify that data offset must be a multiple of the compressed texture blocks in bytes
TEST_F(WriteTextureTest_CompressedTextureFormats, DataOffset) {
    const void* data = malloc(512);

    for (wgpu::TextureFormat bcFormat : utils::kBCFormats) {
        wgpu::Texture texture = Create2DTexture(bcFormat);

        // Valid usages of data offset.
        {
            uint32_t validDataOffset = utils::CompressedFormatBlockSizeInBytes(bcFormat);
            QueueWriteTextureValidationTest::TestWriteTexture(data, 512, validDataOffset, 256, 4,
                             texture, 0, {0, 0, 0}, {4, 4, 1});
        }

        // Failures on invalid data offset.
        {
            uint32_t kInvalidDataOffset = utils::CompressedFormatBlockSizeInBytes(bcFormat) / 2;
            ASSERT_DEVICE_ERROR(TestWriteTexture(data, 512, kInvalidDataOffset, 256, 4,
                                                 texture, 0, {0, 0, 0}, {4, 4, 1}));
        }
    }
}

// Tests to verify that bytesPerRow must not be less than (width / blockWidth) * blockSizeInBytes.
TEST_F(WriteTextureTest_CompressedTextureFormats, BytesPerRow) {
    const void* data = malloc(1024);

    {
        constexpr uint32_t kTestWidth = 160;
        constexpr uint32_t kTestHeight = 160;

        // Failures on the BytesPerRow that is not large enough.
        {
            constexpr uint32_t kSmallBytesPerRow = 256;
            for (wgpu::TextureFormat bcFormat : utils::kBCFormats) {
                wgpu::Texture texture = Create2DTexture(bcFormat, 1, kTestWidth, kTestHeight);
                ASSERT_DEVICE_ERROR(TestWriteTexture(data, 1024, 0, kSmallBytesPerRow, 4,
                                 texture, 0, {0, 0, 0}, {kTestWidth, 4, 1}));
            }
        }

        // Test it is valid to use a BytesPerRow that is not a multiple of 256.
        {
            for (wgpu::TextureFormat bcFormat : utils::kBCFormats) {
                wgpu::Texture texture = Create2DTexture(bcFormat, 1, kTestWidth, kTestHeight);
                uint32_t ValidBytesPerRow =
                    kTestWidth / 4 * utils::CompressedFormatBlockSizeInBytes(bcFormat);
                ASSERT_NE(0u, ValidBytesPerRow % 256);
                TestWriteTexture(data, 1024, 0, ValidBytesPerRow, 4,
                                 texture, 0, {0, 0, 0}, {kTestWidth, 4, 1});
            }
        }

        // Test the smallest valid BytesPerRow divisible by 256 should work.
        {
            for (wgpu::TextureFormat bcFormat : utils::kBCFormats) {
                wgpu::Texture texture = Create2DTexture(bcFormat, 1, kTestWidth, kTestHeight);
                uint32_t smallestValidBytesPerRow =
                    Align(kTestWidth / 4 * utils::CompressedFormatBlockSizeInBytes(bcFormat), 256);
                TestWriteTexture(data, 1024, 0, smallestValidBytesPerRow,
                                 4, texture, 0, {0, 0, 0}, {kTestWidth, 4, 1});
            }
        }
    }
}

// Tests to verify that bytesPerRow must be a multiple of the compressed texture block width
// This doesn't have to be covered in testing validation of CopyBufferToTexture, but is
// neccessary here since bytesPerRow might not be a multiple of 256.
TEST_F(WriteTextureTest_CompressedTextureFormats, ImageWidth) {
    const void* data = malloc(512);

    for (wgpu::TextureFormat bcFormat : utils::kBCFormats) {
        wgpu::Texture texture = Create2DTexture(bcFormat);

        // Valid usages of bytesPerRow in WriteTexture with compressed texture formats.
        {
            constexpr uint32_t kValidImageWidth = 20;
            TestWriteTexture(data, 512, 0, kValidImageWidth, 0,
                             texture, 0, {0, 0, 0}, {4, 4, 1});
        }

        // Valid bytesPerRow.
        // Note that image width is not a multiple of blockWidth.
        {
            constexpr uint32_t kValidImageWidth = 17;
            TestWriteTexture(data, 512, 0, kValidImageWidth, 0,
                             texture, 0, {0, 0, 0}, {4, 4, 1});
        }
    }
}

// Tests to verify that rowsPerImage must be a multiple of the compressed texture block height
TEST_F(WriteTextureTest_CompressedTextureFormats, ImageHeight) {
    const void* data = malloc(512);

    for (wgpu::TextureFormat bcFormat : utils::kBCFormats) {
        wgpu::Texture texture = Create2DTexture(bcFormat);

        // Valid usages of rowsPerImage in WriteTexture with compressed texture formats.
        {
            constexpr uint32_t kValidImageHeight = 8;
            TestWriteTexture(data, 512, 0, 256, kValidImageHeight,
                             texture, 0, {0, 0, 0}, {4, 4, 1});
        }

        // Failures on invalid rowsPerImage.
        {
            constexpr uint32_t kInvalidImageHeight = 3;
            ASSERT_DEVICE_ERROR(TestWriteTexture(data, 512, 0, 256, kInvalidImageHeight,
                                                 texture, 0, {0, 0, 0}, {4, 4, 1}));
        }
    }
}

// Tests to verify that ImageOffset.x must be a multiple of the compressed texture block width and
// ImageOffset.y must be a multiple of the compressed texture block height
TEST_F(WriteTextureTest_CompressedTextureFormats, ImageOffset) {
    const void* data = malloc(512);

    for (wgpu::TextureFormat bcFormat : utils::kBCFormats) {
        wgpu::Texture texture = Create2DTexture(bcFormat);
        wgpu::Texture texture2 = Create2DTexture(bcFormat);

        constexpr wgpu::Origin3D kSmallestValidOrigin3D = {4, 4, 0};

        // Valid usages of ImageOffset in WriteTexture with compressed texture formats.
        {
            TestWriteTexture(data, 512, 0, 256, 4, texture, 0,
                             kSmallestValidOrigin3D, {4, 4, 1});
        }

        // Failures on invalid ImageOffset.x.
        {
            constexpr wgpu::Origin3D kInvalidOrigin3D = {kSmallestValidOrigin3D.x - 1,
                                                         kSmallestValidOrigin3D.y, 0};
            ASSERT_DEVICE_ERROR(TestWriteTexture(data, 512, 0, 256, 4, texture, 0,
                                                 kInvalidOrigin3D, {4, 4, 1}));
        }

        // Failures on invalid ImageOffset.y.
        {
            constexpr wgpu::Origin3D kInvalidOrigin3D = {kSmallestValidOrigin3D.x,
                                                         kSmallestValidOrigin3D.y - 1, 0};
            ASSERT_DEVICE_ERROR(TestWriteTexture(data, 512, 0, 256, 4, texture, 0,
                                                 kInvalidOrigin3D, {4, 4, 1}));
        }
    }
}

// Tests to verify that ImageExtent.x must be a multiple of the compressed texture block width and
// ImageExtent.y must be a multiple of the compressed texture block height
TEST_F(WriteTextureTest_CompressedTextureFormats, ImageExtent) {
    const void* data = malloc(512);

    constexpr uint32_t kMipmapLevels = 3;
    constexpr uint32_t kTestWidth = 60;
    constexpr uint32_t kTestHeight = 60;

    for (wgpu::TextureFormat bcFormat : utils::kBCFormats) {
        wgpu::Texture texture = Create2DTexture(bcFormat, kMipmapLevels, kTestWidth, kTestHeight);
        wgpu::Texture texture2 = Create2DTexture(bcFormat, kMipmapLevels, kTestWidth, kTestHeight);

        constexpr wgpu::Extent3D kSmallestValidExtent3D = {4, 4, 1};

        // Valid usages of ImageExtent in WriteTexture with compressed texture formats.
        {
            TestWriteTexture(data, 512, 0, 256, 8, texture, 0, {0, 0, 0},
                             kSmallestValidExtent3D);
        }

        // Valid usages of ImageExtent in WriteTexture with compressed texture formats
        // and non-zero mipmap levels.
        {
            constexpr uint32_t kTestMipmapLevel = 2;
            constexpr wgpu::Origin3D kTestOrigin = {
                (kTestWidth >> kTestMipmapLevel) - kSmallestValidExtent3D.width + 1,
                (kTestHeight >> kTestMipmapLevel) - kSmallestValidExtent3D.height + 1, 0};

            TestWriteTexture(data, 512, 0, 256, 4, texture,
                             kTestMipmapLevel, kTestOrigin, kSmallestValidExtent3D);
        }

        // Failures on invalid ImageExtent.x.
        {
            constexpr wgpu::Extent3D kInValidExtent3D = {kSmallestValidExtent3D.width - 1,
                                                         kSmallestValidExtent3D.height, 1};
            ASSERT_DEVICE_ERROR(TestWriteTexture(data, 512, 0, 256, 4, texture, 0, {0, 0, 0},
                                                 kInValidExtent3D));
        }

        // Failures on invalid ImageExtent.y.
        {
            constexpr wgpu::Extent3D kInValidExtent3D = {kSmallestValidExtent3D.width,
                                                         kSmallestValidExtent3D.height - 1, 1};
            ASSERT_DEVICE_ERROR(TestWriteTexture(data, 512, 0, 256, 4, texture, 0, {0, 0, 0},
                                                 kInValidExtent3D));
        }
    }
}

}  // anonymous namespace
