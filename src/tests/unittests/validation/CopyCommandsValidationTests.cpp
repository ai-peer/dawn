// Copyright 2017 The Dawn Authors
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

#include "common/Constants.h"
#include "common/Math.h"
#include "tests/unittests/validation/ValidationTest.h"
#include "utils/DawnHelpers.h"

class CopyCommandTest : public ValidationTest {
    protected:
        dawn::Buffer CreateBuffer(uint64_t size, dawn::BufferUsageBit usage) {
            dawn::BufferDescriptor descriptor;
            descriptor.size = size;
            descriptor.usage = usage;

            return device.CreateBuffer(&descriptor);
        }

        dawn::Texture Create2DTexture(uint32_t width, uint32_t height, uint32_t mipLevelCount,
                                      uint32_t arrayLayerCount, dawn::TextureFormat format,
                                      dawn::TextureUsageBit usage, uint32_t sampleCount = 1) {
            dawn::TextureDescriptor descriptor;
            descriptor.dimension = dawn::TextureDimension::e2D;
            descriptor.size.width = width;
            descriptor.size.height = height;
            descriptor.size.depth = 1;
            descriptor.arrayLayerCount = arrayLayerCount;
            descriptor.sampleCount = sampleCount;
            descriptor.format = format;
            descriptor.mipLevelCount = mipLevelCount;
            descriptor.usage = usage;
            dawn::Texture tex = device.CreateTexture(&descriptor);
            return tex;
        }

        uint32_t BufferSizeForTextureCopy(uint32_t width, uint32_t height, uint32_t depth) {
            constexpr unsigned int kBytesPerTexel = 4;
            uint32_t rowPitch = Align(width * 4, kTextureRowPitchAlignment);
            return (rowPitch * (height - 1) + width * kBytesPerTexel) * depth;
        }

        void ValidateExpectation(dawn::CommandEncoder encoder, utils::Expectation expectation) {
            if (expectation == utils::Expectation::Success) {
                encoder.Finish();
            } else {
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }
        }

        void TestB2TCopy(utils::Expectation expectation,
                         dawn::Buffer srcBuffer,
                         uint64_t srcOffset,
                         uint32_t srcRowPitch,
                         uint32_t srcImageHeight,
                         dawn::Texture destTexture,
                         uint32_t destLevel,
                         uint32_t destSlice,
                         dawn::Origin3D destOrigin,
                         dawn::Extent3D extent3D) {
            dawn::BufferCopyView bufferCopyView =
                utils::CreateBufferCopyView(srcBuffer, srcOffset, srcRowPitch, srcImageHeight);
            dawn::TextureCopyView textureCopyView =
                utils::CreateTextureCopyView(destTexture, destLevel, destSlice, destOrigin);

            dawn::CommandEncoder encoder = device.CreateCommandEncoder();
            encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &extent3D);

            ValidateExpectation(encoder, expectation);
        }

        void TestT2BCopy(utils::Expectation expectation,
                         dawn::Texture srcTexture,
                         uint32_t srcLevel,
                         uint32_t srcSlice,
                         dawn::Origin3D srcOrigin,
                         dawn::Buffer destBuffer,
                         uint64_t destOffset,
                         uint32_t destRowPitch,
                         uint32_t destImageHeight,
                         dawn::Extent3D extent3D) {
            dawn::BufferCopyView bufferCopyView =
                utils::CreateBufferCopyView(destBuffer, destOffset, destRowPitch, destImageHeight);
            dawn::TextureCopyView textureCopyView =
                utils::CreateTextureCopyView(srcTexture, srcLevel, srcSlice, srcOrigin);

            dawn::CommandEncoder encoder = device.CreateCommandEncoder();
            encoder.CopyTextureToBuffer(&textureCopyView, &bufferCopyView, &extent3D);

            ValidateExpectation(encoder, expectation);
        }

        void TestT2TCopy(utils::Expectation expectation,
                         dawn::Texture srcTexture,
                         uint32_t srcLevel,
                         uint32_t srcSlice,
                         dawn::Origin3D srcOrigin,
                         dawn::Texture dstTexture,
                         uint32_t dstLevel,
                         uint32_t dstSlice,
                         dawn::Origin3D dstOrigin,
                         dawn::Extent3D extent3D) {
            dawn::TextureCopyView srcTextureCopyView =
                utils::CreateTextureCopyView(srcTexture, srcLevel, srcSlice, srcOrigin);
            dawn::TextureCopyView dstTextureCopyView =
                utils::CreateTextureCopyView(dstTexture, dstLevel, dstSlice, dstOrigin);

            dawn::CommandEncoder encoder = device.CreateCommandEncoder();
            encoder.CopyTextureToTexture(&srcTextureCopyView, &dstTextureCopyView, &extent3D);

            ValidateExpectation(encoder, expectation);
        }
};

class CopyCommandTest_B2B : public CopyCommandTest {
};

// TODO(cwallez@chromium.org): Test that copies are forbidden inside renderpasses

// Test a successfull B2B copy
TEST_F(CopyCommandTest_B2B, Success) {
    dawn::Buffer source = CreateBuffer(16, dawn::BufferUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(16, dawn::BufferUsageBit::TransferDst);

    // Copy different copies, including some that touch the OOB condition
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(source, 0, destination, 0, 16);
        encoder.CopyBufferToBuffer(source, 8, destination, 0, 8);
        encoder.CopyBufferToBuffer(source, 0, destination, 8, 8);
        encoder.Finish();
    }

    // Empty copies are valid
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(source, 0, destination, 0, 0);
        encoder.CopyBufferToBuffer(source, 0, destination, 16, 0);
        encoder.CopyBufferToBuffer(source, 16, destination, 0, 0);
        encoder.Finish();
    }
}

// Test B2B copies with OOB
TEST_F(CopyCommandTest_B2B, OutOfBounds) {
    dawn::Buffer source = CreateBuffer(16, dawn::BufferUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(16, dawn::BufferUsageBit::TransferDst);

    // OOB on the source
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(source, 8, destination, 0, 12);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // OOB on the destination
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(source, 0, destination, 8, 12);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Test B2B copies with incorrect buffer usage
TEST_F(CopyCommandTest_B2B, BadUsage) {
    dawn::Buffer source = CreateBuffer(16, dawn::BufferUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(16, dawn::BufferUsageBit::TransferDst);
    dawn::Buffer vertex = CreateBuffer(16, dawn::BufferUsageBit::Vertex);

    // Source with incorrect usage
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(vertex, 0, destination, 0, 16);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Destination with incorrect usage
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(source, 0, vertex, 0, 16);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Test B2B copies with unaligned data size
TEST_F(CopyCommandTest_B2B, UnalignedSize) {
    dawn::Buffer source = CreateBuffer(16, dawn::BufferUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(16, dawn::BufferUsageBit::TransferDst);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(source, 8, destination, 0, sizeof(uint8_t));
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// Test B2B copies with unaligned offset
TEST_F(CopyCommandTest_B2B, UnalignedOffset) {
    dawn::Buffer source = CreateBuffer(16, dawn::BufferUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(16, dawn::BufferUsageBit::TransferDst);

    // Unaligned source offset
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(source, 9, destination, 0, 4);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Unaligned destination offset
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(source, 8, destination, 1, 4);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Test B2B copies with buffers in error state cause errors.
TEST_F(CopyCommandTest_B2B, BuffersInErrorState) {
    dawn::BufferDescriptor errorBufferDescriptor;
    errorBufferDescriptor.size = 4;
    errorBufferDescriptor.usage = dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::TransferSrc;
    ASSERT_DEVICE_ERROR(dawn::Buffer errorBuffer = device.CreateBuffer(&errorBufferDescriptor));

    constexpr uint64_t bufferSize = 4;
    dawn::Buffer validBuffer = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);

    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(errorBuffer, 0, validBuffer, 0, 4);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(validBuffer, 0, errorBuffer, 0, 4);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

class CopyCommandTest_B2T : public CopyCommandTest {
};

// Test a successfull B2T copy
TEST_F(CopyCommandTest_B2T, Success) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                     dawn::TextureUsageBit::TransferDst);

    // Different copies, including some that touch the OOB condition
    {
        // Copy 4x4 block in corner of first mip.
        TestB2TCopy(utils::Expectation::Success, source, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                    {4, 4, 1});
        // Copy 4x4 block in opposite corner of first mip.
        TestB2TCopy(utils::Expectation::Success, source, 0, 256, 0, destination, 0, 0, {12, 12, 0},
                    {4, 4, 1});
        // Copy 4x4 block in the 4x4 mip.
        TestB2TCopy(utils::Expectation::Success, source, 0, 256, 0, destination, 2, 0, {0, 0, 0},
                    {4, 4, 1});
        // Copy with a buffer offset
        TestB2TCopy(utils::Expectation::Success, source, bufferSize - 4, 256, 0, destination, 0, 0,
                    {0, 0, 0}, {1, 1, 1});
    }

    // Copies with a 256-byte aligned row pitch but unaligned texture region
    {
        // Unaligned region
        TestB2TCopy(utils::Expectation::Success, source, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                    {3, 4, 1});
        // Unaligned region with texture offset
        TestB2TCopy(utils::Expectation::Success, source, 0, 256, 0, destination, 0, 0, {5, 7, 0},
                    {2, 3, 1});
        // Unaligned region, with buffer offset
        TestB2TCopy(utils::Expectation::Success, source, 31 * 4, 256, 0, destination, 0, 0,
                    {0, 0, 0}, {3, 3, 1});
    }

    // Empty copies are valid
    {
        // An empty copy
        TestB2TCopy(utils::Expectation::Success, source, 0, 0, 0, destination, 0, 0, {0, 0, 0},
                    {0, 0, 1});
        // An empty copy with depth = 0
        TestB2TCopy(utils::Expectation::Success, source, 0, 0, 0, destination, 0, 0, {0, 0, 0},
                    {0, 0, 0});
        // An empty copy touching the end of the buffer
        TestB2TCopy(utils::Expectation::Success, source, bufferSize, 0, 0, destination, 0, 0,
                    {0, 0, 0}, {0, 0, 1});
        // An empty copy touching the side of the texture
        TestB2TCopy(utils::Expectation::Success, source, 0, 0, 0, destination, 0, 0, {16, 16, 0},
                    {0, 0, 1});
    }
}

// Test OOB conditions on the buffer
TEST_F(CopyCommandTest_B2T, OutOfBoundsOnBuffer) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                     dawn::TextureUsageBit::TransferDst);

    // OOB on the buffer because we copy too many pixels
    TestB2TCopy(utils::Expectation::Failure, source, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                {4, 5, 1});

    // OOB on the buffer because of the offset
    TestB2TCopy(utils::Expectation::Failure, source, 4, 256, 0, destination, 0, 0, {0, 0, 0},
                {4, 4, 1});

    // OOB on the buffer because (row pitch * (height - 1) + width) * depth overflows
    TestB2TCopy(utils::Expectation::Failure, source, 0, 512, 0, destination, 0, 0, {0, 0, 0},
                {4, 3, 1});

    // Not OOB on the buffer although row pitch * height overflows
    // but (row pitch * (height - 1) + width) * depth does not overlow
    {
        uint32_t sourceBufferSize = BufferSizeForTextureCopy(7, 3, 1);
        ASSERT_TRUE(256 * 3 > sourceBufferSize) << "row pitch * height should overflow buffer";
        dawn::Buffer sourceBuffer = CreateBuffer(sourceBufferSize, dawn::BufferUsageBit::TransferSrc);

        TestB2TCopy(utils::Expectation::Success, source, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                    {7, 3, 1});
    }
}

// Test OOB conditions on the texture
TEST_F(CopyCommandTest_B2T, OutOfBoundsOnTexture) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                                     dawn::TextureUsageBit::TransferDst);

    // OOB on the texture because x + width overflows
    TestB2TCopy(utils::Expectation::Failure, source, 0, 256, 0, destination, 0, 0, {13, 12, 0},
                {4, 4, 1});

    // OOB on the texture because y + width overflows
    TestB2TCopy(utils::Expectation::Failure, source, 0, 256, 0, destination, 0, 0, {12, 13, 0},
                {4, 4, 1});

    // OOB on the texture because we overflow a non-zero mip
    TestB2TCopy(utils::Expectation::Failure, source, 0, 256, 0, destination, 2, 0, {1, 0, 0},
                {4, 4, 1});

    // OOB on the texture even on an empty copy when we copy to a non-existent mip.
    TestB2TCopy(utils::Expectation::Failure, source, 0, 0, 0, destination, 5, 0, {0, 0, 0},
                {0, 0, 1});

    // OOB on the texture because slice overflows
    TestB2TCopy(utils::Expectation::Failure, source, 0, 0, 0, destination, 0, 2, {0, 0, 0},
                {0, 0, 1});
}

// Test that we force Z=0 and Depth=1 on copies to 2D textures
TEST_F(CopyCommandTest_B2T, ZDepthConstraintFor2DTextures) {
    dawn::Buffer source = CreateBuffer(16 * 4, dawn::BufferUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                     dawn::TextureUsageBit::TransferDst);

    // Z=1 on an empty copy still errors
    TestB2TCopy(utils::Expectation::Failure, source, 0, 0, 0, destination, 0, 0, {0, 0, 1},
                {0, 0, 1});

    // Depth > 1 on an empty copy still errors
    TestB2TCopy(utils::Expectation::Failure, source, 0, 0, 0, destination, 0, 0, {0, 0, 0},
                {0, 0, 2});
}

// Test B2T copies with incorrect buffer usage
TEST_F(CopyCommandTest_B2T, IncorrectUsage) {
    dawn::Buffer source = CreateBuffer(16 * 4, dawn::BufferUsageBit::TransferSrc);
    dawn::Buffer vertex = CreateBuffer(16 * 4, dawn::BufferUsageBit::Vertex);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                     dawn::TextureUsageBit::TransferDst);
    dawn::Texture sampled = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                 dawn::TextureUsageBit::Sampled);

    // Incorrect source usage
    TestB2TCopy(utils::Expectation::Failure, vertex, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                {4, 4, 1});

    // Incorrect destination usage
    TestB2TCopy(utils::Expectation::Failure, source, 0, 256, 0, sampled, 0, 0, {0, 0, 0},
                {4, 4, 1});
}

TEST_F(CopyCommandTest_B2T, IncorrectRowPitch) {
    uint64_t bufferSize = BufferSizeForTextureCopy(128, 16, 1);
    dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(128, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
        dawn::TextureUsageBit::TransferDst);

    // Default row pitch is not 256-byte aligned
    TestB2TCopy(utils::Expectation::Failure, source, 0, 0, 0, destination, 0, 0, {0, 0, 0},
                {3, 4, 1});

    // Row pitch is not 256-byte aligned
    TestB2TCopy(utils::Expectation::Failure, source, 0, 128, 0, destination, 0, 0, {0, 0, 0},
                {4, 4, 1});

    // Row pitch is less than width * bytesPerPixel
    TestB2TCopy(utils::Expectation::Failure, source, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                {65, 1, 1});
}

TEST_F(CopyCommandTest_B2T, ImageHeightConstraint) {
    uint64_t bufferSize = BufferSizeForTextureCopy(5, 5, 1);
    dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferDst);

    // Image height is zero (Valid)
    TestB2TCopy(utils::Expectation::Success, source, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                {4, 4, 1});

    // Image height is equal to copy height (Valid)
    TestB2TCopy(utils::Expectation::Success, source, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                {4, 4, 1});

    // Image height is larger than copy height (Valid)
    TestB2TCopy(utils::Expectation::Success, source, 0, 256, 4, destination, 0, 0, {0, 0, 0},
                {4, 4, 1});

    // Image height is less than copy height (Invalid)
    TestB2TCopy(utils::Expectation::Failure, source, 0, 256, 3, destination, 0, 0, {0, 0, 0},
                {4, 4, 1});
}

// Test B2T copies with incorrect buffer offset usage
TEST_F(CopyCommandTest_B2T, IncorrectBufferOffset) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                     dawn::TextureUsageBit::TransferDst);

    // Correct usage
    TestB2TCopy(utils::Expectation::Success, source, bufferSize - 4, 256, 0, destination, 0, 0,
                {0, 0, 0}, {1, 1, 1});

    // Incorrect usages
    {
        TestB2TCopy(utils::Expectation::Failure, source, bufferSize - 5, 256, 0, destination, 0, 0,
                    {0, 0, 0}, {1, 1, 1});
        TestB2TCopy(utils::Expectation::Failure, source, bufferSize - 6, 256, 0, destination, 0, 0,
                    {0, 0, 0}, {1, 1, 1});
        TestB2TCopy(utils::Expectation::Failure, source, bufferSize - 7, 256, 0, destination, 0, 0,
                    {0, 0, 0}, {1, 1, 1});
    }
}

// Test multisampled textures cannot be used in B2T copies.
TEST_F(CopyCommandTest_B2T, CopyToMultisampledTexture) {
    uint64_t bufferSize = BufferSizeForTextureCopy(16, 16, 1);
    dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(2, 2, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferDst, 4);

    TestB2TCopy(utils::Expectation::Failure, source, 0, 256, 0, destination, 0, 0, {0, 0, 0},
                {2, 2, 1});
}

// Test B2T copies with buffer or texture in error state causes errors.
TEST_F(CopyCommandTest_B2T, BufferOrTextureInErrorState) {
    dawn::BufferDescriptor errorBufferDescriptor;
    errorBufferDescriptor.size = 4;
    errorBufferDescriptor.usage = dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::TransferSrc;
    ASSERT_DEVICE_ERROR(dawn::Buffer errorBuffer = device.CreateBuffer(&errorBufferDescriptor));

    dawn::TextureDescriptor errorTextureDescriptor;
    errorTextureDescriptor.arrayLayerCount = 0;
    ASSERT_DEVICE_ERROR(dawn::Texture errorTexture = device.CreateTexture(&errorTextureDescriptor));

    dawn::BufferCopyView errorBufferCopyView = utils::CreateBufferCopyView(errorBuffer, 0, 0, 0);
    dawn::TextureCopyView errorTextureCopyView =
        utils::CreateTextureCopyView(errorTexture, 0, 0, {1, 1, 1});

    dawn::Extent3D extent3D = {1, 1, 1};

    {
        dawn::Texture destination =
            Create2DTexture(16, 16, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                            dawn::TextureUsageBit::TransferDst);
        dawn::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(destination, 0, 0, {1, 1, 1});

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&errorBufferCopyView, &textureCopyView, &extent3D);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    {
        uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
        dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);

        dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(source, 0, 0, 0);

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&bufferCopyView, &errorTextureCopyView, &extent3D);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

class CopyCommandTest_T2B : public CopyCommandTest {
};

// Test a successfull T2B copy
TEST_F(CopyCommandTest_T2B, Success) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Texture source = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferDst);

    // Different copies, including some that touch the OOB condition
    {
        // Copy from 4x4 block in corner of first mip.
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 256, 0,
                    {4, 4, 1});
        // Copy from 4x4 block in opposite corner of first mip.
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {12, 12, 0}, destination, 0, 256, 0,
                    {4, 4, 1});
        // Copy from 4x4 block in the 4x4 mip.
        TestT2BCopy(utils::Expectation::Success, source, 2, 0, {0, 0, 0}, destination, 0, 256, 0,
                    {4, 4, 1});
        // Copy with a buffer offset
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination,
                    bufferSize - 4, 256, 0, {1, 1, 1});
    }

    // Copies with a 256-byte aligned row pitch but unaligned texture region
    {
        // Unaligned region
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 256, 0,
                    {3, 4, 1});
        // Unaligned region with texture offset
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {5, 7, 0}, destination, 0, 256, 0,
                    {2, 3, 1});
        // Unaligned region, with buffer offset
        TestT2BCopy(utils::Expectation::Success, source, 2, 0, {0, 0, 0}, destination, 31 * 4, 256,
                    0, {3, 3, 1});
    }

    // Empty copies are valid
    {
        // An empty copy
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0, 0,
                    {0, 0, 1});
        // An empty copy with depth = 0
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0, 0,
                    {0, 0, 0});
        // An empty copy touching the end of the buffer
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, bufferSize,
                    0, 0, {0, 0, 1});
        // An empty copy touching the side of the texture
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {16, 16, 0}, destination, 0, 0, 0,
                    {0, 0, 1});
    }
}

// Test OOB conditions on the texture
TEST_F(CopyCommandTest_T2B, OutOfBoundsOnTexture) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Texture source = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferDst);

    // OOB on the texture because x + width overflows
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {13, 12, 0}, destination, 0, 256, 0,
                {4, 4, 1});

    // OOB on the texture because y + width overflows
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {12, 13, 0}, destination, 0, 256, 0,
                {4, 4, 1});

    // OOB on the texture because we overflow a non-zero mip
    TestT2BCopy(utils::Expectation::Failure, source, 2, 0, {1, 0, 0}, destination, 0, 256, 0,
                {4, 4, 1});

    // OOB on the texture even on an empty copy when we copy from a non-existent mip.
    TestT2BCopy(utils::Expectation::Failure, source, 5, 0, {0, 0, 0}, destination, 0, 0, 0,
                {0, 0, 1});
}

// Test OOB conditions on the buffer
TEST_F(CopyCommandTest_T2B, OutOfBoundsOnBuffer) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Texture source = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferDst);

    // OOB on the buffer because we copy too many pixels
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 256, 0,
                {4, 5, 1});

    // OOB on the buffer because of the offset
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 4, 256, 0,
                {4, 4, 1});

    // OOB on the buffer because (row pitch * (height - 1) + width) * depth overflows
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 512, 0,
                {4, 3, 1});

    // Not OOB on the buffer although row pitch * height overflows
    // but (row pitch * (height - 1) + width) * depth does not overlow
    {
        uint32_t destinationBufferSize = BufferSizeForTextureCopy(7, 3, 1);
        ASSERT_TRUE(256 * 3 > destinationBufferSize) << "row pitch * height should overflow buffer";
        dawn::Buffer destinationBuffer = CreateBuffer(destinationBufferSize, dawn::BufferUsageBit::TransferDst);
        TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destinationBuffer, 0, 256,
                    0, {7, 3, 1});
    }
}

// Test that we force Z=0 and Depth=1 on copies from to 2D textures
TEST_F(CopyCommandTest_T2B, ZDepthConstraintFor2DTextures) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Texture source = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferDst);

    // Z=1 on an empty copy still errors
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 1}, destination, 0, 0, 0,
                {0, 0, 1});

    // Depth > 1 on an empty copy still errors
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 0, 0,
                {0, 0, 2});
}

// Test T2B copies with incorrect buffer usage
TEST_F(CopyCommandTest_T2B, IncorrectUsage) {
    uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
    dawn::Texture source = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferSrc);
    dawn::Texture sampled = Create2DTexture(16, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                                 dawn::TextureUsageBit::Sampled);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferDst);
    dawn::Buffer vertex = CreateBuffer(bufferSize, dawn::BufferUsageBit::Vertex);

    // Incorrect source usage
    TestT2BCopy(utils::Expectation::Failure, sampled, 0, 0, {0, 0, 0}, destination, 0, 256, 0,
                {4, 4, 1});

    // Incorrect destination usage
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, vertex, 0, 256, 0, {4, 4, 1});
}

TEST_F(CopyCommandTest_T2B, IncorrectRowPitch) {
    uint64_t bufferSize = BufferSizeForTextureCopy(128, 16, 1);
    dawn::Texture source = Create2DTexture(128, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
        dawn::TextureUsageBit::TransferDst);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);

    // Default row pitch is not 256-byte aligned
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 256, 0,
                {3, 4, 1});

    // Row pitch is not 256-byte aligned
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 257, 0,
                {4, 4, 1});

    // Row pitch is less than width * bytesPerPixel
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 256, 0,
                {65, 1, 1});
}

TEST_F(CopyCommandTest_T2B, ImageHeightConstraint) {
    uint64_t bufferSize = BufferSizeForTextureCopy(5, 5, 1);
    dawn::Texture source = Create2DTexture(16, 16, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                           dawn::TextureUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferDst);

    // Image height is zero (Valid)
    TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 256, 0,
                {4, 4, 1});

    // Image height is equal to copy height (Valid)
    TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 256, 4,
                {4, 4, 1});

    // Image height exceeds copy height (Valid)
    TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 256, 5,
                {4, 4, 1});

    // Image height is less than copy height (Invalid)
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 256, 3,
                {4, 4, 1});
}

// Test T2B copies with incorrect buffer offset usage
TEST_F(CopyCommandTest_T2B, IncorrectBufferOffset) {
    uint64_t bufferSize = BufferSizeForTextureCopy(128, 16, 1);
    dawn::Texture source = Create2DTexture(128, 16, 5, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                           dawn::TextureUsageBit::TransferSrc);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferDst);

    // Correct usage
    TestT2BCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, bufferSize - 4,
                256, 0, {1, 1, 1});

    // Incorrect usages
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, bufferSize - 5,
                256, 0, {1, 1, 1});
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, bufferSize - 6,
                256, 0, {1, 1, 1});
    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, bufferSize - 7,
                256, 0, {1, 1, 1});
}

// Test multisampled textures cannot be used in T2B copies.
TEST_F(CopyCommandTest_T2B, CopyFromMultisampledTexture) {
    dawn::Texture source = Create2DTexture(2, 2, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                                           dawn::TextureUsageBit::TransferSrc, 4);
    uint64_t bufferSize = BufferSizeForTextureCopy(16, 16, 1);
    dawn::Buffer destination = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferDst);

    TestT2BCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 256, 0,
                {2, 2, 1});
}

// Test T2B copies with buffer or texture in error state cause errors.
TEST_F(CopyCommandTest_T2B, BufferOrTextureInErrorState) {
    dawn::BufferDescriptor errorBufferDescriptor;
    errorBufferDescriptor.size = 4;
    errorBufferDescriptor.usage = dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::TransferSrc;
    ASSERT_DEVICE_ERROR(dawn::Buffer errorBuffer = device.CreateBuffer(&errorBufferDescriptor));

    dawn::TextureDescriptor errorTextureDescriptor;
    errorTextureDescriptor.arrayLayerCount = 0;
    ASSERT_DEVICE_ERROR(dawn::Texture errorTexture = device.CreateTexture(&errorTextureDescriptor));

    dawn::BufferCopyView errorBufferCopyView = utils::CreateBufferCopyView(errorBuffer, 0, 0, 0);
    dawn::TextureCopyView errorTextureCopyView =
        utils::CreateTextureCopyView(errorTexture, 0, 0, {1, 1, 1});

    dawn::Extent3D extent3D = {1, 1, 1};

    {
        uint64_t bufferSize = BufferSizeForTextureCopy(4, 4, 1);
        dawn::Buffer source = CreateBuffer(bufferSize, dawn::BufferUsageBit::TransferSrc);

        dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(source, 0, 0, 0);

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyTextureToBuffer(&errorTextureCopyView, &bufferCopyView, &extent3D);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    {
        dawn::Texture destination =
            Create2DTexture(16, 16, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm,
                            dawn::TextureUsageBit::TransferDst);
        dawn::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(destination, 0, 0, {1, 1, 1});

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyTextureToBuffer(&textureCopyView, &errorBufferCopyView, &extent3D);
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

class CopyCommandTest_T2T : public CopyCommandTest {};

TEST_F(CopyCommandTest_T2T, Success) {
    dawn::Texture source = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                           dawn::TextureUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferDst);

    // Different copies, including some that touch the OOB condition
    {
        // Copy a region along top left boundary
        TestT2TCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {4, 4, 1});

        // Copy entire texture
        TestT2TCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {16, 16, 1});

        // Copy a region along bottom right boundary
        TestT2TCopy(utils::Expectation::Success, source, 0, 0, {8, 8, 0}, destination, 0, 0,
                    {8, 8, 0}, {8, 8, 1});

        // Copy region into mip
        TestT2TCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 2, 0,
                    {0, 0, 0}, {4, 4, 1});

        // Copy mip into region
        TestT2TCopy(utils::Expectation::Success, source, 2, 0, {0, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {4, 4, 1});

        // Copy between slices
        TestT2TCopy(utils::Expectation::Success, source, 0, 1, {0, 0, 0}, destination, 0, 1,
                    {0, 0, 0}, {16, 16, 1});
    }

    // Empty copies are valid
    {
        // An empty copy
        TestT2TCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {0, 0, 1});

        // An empty copy with depth = 0
        TestT2TCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {0, 0, 0});

        // An empty copy touching the side of the source texture
        TestT2TCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0,
                    {16, 16, 0}, {0, 0, 1});

        // An empty copy touching the side of the destination texture
        TestT2TCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0,
                    {16, 16, 0}, {0, 0, 1});
    }
}

TEST_F(CopyCommandTest_T2T, IncorrectUsage) {
    dawn::Texture source = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                           dawn::TextureUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferDst);

    // Incorrect source usage causes failure
    TestT2TCopy(utils::Expectation::Failure, destination, 0, 0, {0, 0, 0}, destination, 0, 0,
                {0, 0, 0}, {16, 16, 1});

    // Incorrect destination usage causes failure
    TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, source, 0, 0, {0, 0, 0},
                {16, 16, 1});
}

TEST_F(CopyCommandTest_T2T, OutOfBounds) {
    dawn::Texture source = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                           dawn::TextureUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferDst);

    // OOB on source
    {
        // x + width overflows
        TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {1, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {16, 16, 1});

        // y + height overflows
        TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 1, 0}, destination, 0, 0,
                    {0, 0, 0}, {16, 16, 1});

        // non-zero mip overflows
        TestT2TCopy(utils::Expectation::Failure, source, 1, 0, {0, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {9, 9, 1});

        // empty copy on non-existent mip fails
        TestT2TCopy(utils::Expectation::Failure, source, 6, 0, {0, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {0, 0, 1});

        // empty copy from non-existent slice fails
        TestT2TCopy(utils::Expectation::Failure, source, 0, 2, {0, 0, 0}, destination, 0, 0,
                    {0, 0, 0}, {0, 0, 1});
    }

    // OOB on destination
    {
        // x + width overflows
        TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 0,
                    {1, 0, 0}, {16, 16, 1});

        // y + height overflows
        TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 0,
                    {0, 1, 0}, {16, 16, 1});

        // non-zero mip overflows
        TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 1, 0,
                    {0, 0, 0}, {9, 9, 1});

        // empty copy on non-existent mip fails
        TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 6, 0,
                    {0, 0, 0}, {0, 0, 1});

        // empty copy on non-existent slice fails
        TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 2,
                    {0, 0, 0}, {0, 0, 1});
    }
}

TEST_F(CopyCommandTest_T2T, 2DTextureDepthConstraints) {
    dawn::Texture source = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                           dawn::TextureUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferDst);

    // Empty copy on source with z > 0 fails
    TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 1}, destination, 0, 0, {0, 0, 0},
                {0, 0, 1});

    // Empty copy on destination with z > 0 fails
    TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 0, {0, 0, 1},
                {0, 0, 1});

    // Empty copy with depth > 1 fails
    TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 0, {0, 0, 0},
                {0, 0, 2});
}

TEST_F(CopyCommandTest_T2T, 2DTextureDepthStencil) {
    dawn::Texture source = Create2DTexture(16, 16, 1, 1, dawn::TextureFormat::D32FloatS8Uint,
                                           dawn::TextureUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 1, 1, dawn::TextureFormat::D32FloatS8Uint,
                                                dawn::TextureUsageBit::TransferDst);

    // Success when entire depth stencil subresource is copied
    TestT2TCopy(utils::Expectation::Success, source, 0, 0, {0, 0, 0}, destination, 0, 0, {0, 0, 0},
                {16, 16, 1});

    // Failure when depth stencil subresource is partially copied
    TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 0, {0, 0, 0},
                {15, 15, 1});
}

TEST_F(CopyCommandTest_T2T, FormatsMismatch) {
    dawn::Texture source = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Uint,
                                           dawn::TextureUsageBit::TransferSrc);
    dawn::Texture destination = Create2DTexture(16, 16, 5, 2, dawn::TextureFormat::R8G8B8A8Unorm,
                                                dawn::TextureUsageBit::TransferDst);

    // Failure when formats don't match
    TestT2TCopy(utils::Expectation::Failure, source, 0, 0, {0, 0, 0}, destination, 0, 0, {0, 0, 0},
                {0, 0, 1});
}

TEST_F(CopyCommandTest_T2T, MultisampledCopies) {
    dawn::Texture sourceMultiSampled1x = Create2DTexture(
        16, 16, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm, dawn::TextureUsageBit::TransferSrc, 1);
    dawn::Texture sourceMultiSampled4x = Create2DTexture(
        16, 16, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm, dawn::TextureUsageBit::TransferSrc, 4);
    dawn::Texture destinationMultiSampled4x = Create2DTexture(
        16, 16, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm, dawn::TextureUsageBit::TransferDst, 4);

    // Success when entire multisampled subresource is copied
    {
        TestT2TCopy(utils::Expectation::Success, sourceMultiSampled4x, 0, 0, {0, 0, 0},
                    destinationMultiSampled4x, 0, 0, {0, 0, 0}, {16, 16, 1});
    }

    // Failures
    {
        // An empty copy with mismatched samples fails
        TestT2TCopy(utils::Expectation::Failure, sourceMultiSampled1x, 0, 0, {0, 0, 0},
                    destinationMultiSampled4x, 0, 0, {0, 0, 0}, {0, 0, 1});

        // A copy fails when samples are greater than 1, and entire subresource isn't copied
        TestT2TCopy(utils::Expectation::Failure, sourceMultiSampled4x, 0, 0, {0, 0, 0},
                    destinationMultiSampled4x, 0, 0, {0, 0, 0}, {15, 15, 1});
    }
}

class CopyCommandTest_CompressedTextureFormats : public CopyCommandTest {
  protected:
    dawn::Texture Create2DTexture(dawn::TextureFormat format,
                                  uint32_t mipmapLevels = 1,
                                  uint32_t width = kWidth,
                                  uint32_t height = kHeight) {
        constexpr dawn::TextureUsageBit kUsage = dawn::TextureUsageBit::TransferDst |
                                                 dawn::TextureUsageBit::TransferSrc |
                                                 dawn::TextureUsageBit::Sampled;
        constexpr uint32_t kArrayLayers = 1;
        return CopyCommandTest::Create2DTexture(width, height, mipmapLevels, kArrayLayers, format,
                                                kUsage, 1);
    }

    static uint32_t CompressedFormatBlockSizeInBytes(dawn::TextureFormat format) {
        switch (format) {
            case dawn::TextureFormat::BC1RGBAUnorm:
            case dawn::TextureFormat::BC1RGBAUnormSrgb:
            case dawn::TextureFormat::BC4RSnorm:
            case dawn::TextureFormat::BC4RUnorm:
                return 8;
            case dawn::TextureFormat::BC2RGBAUnorm:
            case dawn::TextureFormat::BC2RGBAUnormSrgb:
            case dawn::TextureFormat::BC3RGBAUnorm:
            case dawn::TextureFormat::BC3RGBAUnormSrgb:
            case dawn::TextureFormat::BC5RGSnorm:
            case dawn::TextureFormat::BC5RGUnorm:
            case dawn::TextureFormat::BC6HRGBSfloat:
            case dawn::TextureFormat::BC6HRGBUfloat:
            case dawn::TextureFormat::BC7RGBAUnorm:
            case dawn::TextureFormat::BC7RGBAUnormSrgb:
                return 16;
            default:
                EXPECT_TRUE(false);
                return 0;
        }
    }

    static constexpr uint32_t kWidth = 16;
    static constexpr uint32_t kHeight = 16;

    const std::array<dawn::TextureFormat, 14> kBCFormats = {
        dawn::TextureFormat::BC1RGBAUnorm,  dawn::TextureFormat::BC1RGBAUnormSrgb,
        dawn::TextureFormat::BC2RGBAUnorm,  dawn::TextureFormat::BC2RGBAUnormSrgb,
        dawn::TextureFormat::BC3RGBAUnorm,  dawn::TextureFormat::BC3RGBAUnormSrgb,
        dawn::TextureFormat::BC4RUnorm,     dawn::TextureFormat::BC4RSnorm,
        dawn::TextureFormat::BC5RGUnorm,    dawn::TextureFormat::BC5RGSnorm,
        dawn::TextureFormat::BC6HRGBUfloat, dawn::TextureFormat::BC6HRGBSfloat,
        dawn::TextureFormat::BC7RGBAUnorm,  dawn::TextureFormat::BC7RGBAUnormSrgb};
};

// Tests to verify that bufferOffset must be a multiple of the compressed texture blocks in bytes
// in buffer-to-texture or texture-to-buffer copies with compressed texture formats.
TEST_F(CopyCommandTest_CompressedTextureFormats, BufferOffset) {
    dawn::Buffer buffer =
        CreateBuffer(512, dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst);

    for (dawn::TextureFormat bcFormat : kBCFormats) {
        dawn::Texture texture = Create2DTexture(bcFormat);

        // Valid usages of BufferOffset in B2T and T2B copies with compressed texture formats.
        {
            uint32_t validBufferOffset = CompressedFormatBlockSizeInBytes(bcFormat);
            TestB2TCopy(utils::Expectation::Success, buffer, validBufferOffset, 256, 4, texture, 0,
                        0, {0, 0, 0}, {4, 4, 1});
            TestT2BCopy(utils::Expectation::Success, texture, 0, 0, {0, 0, 0}, buffer,
                        validBufferOffset, 256, 4, {4, 4, 1});
        }

        // Failures on invalid bufferOffset.
        {
            uint32_t kInvalidBufferOffset = CompressedFormatBlockSizeInBytes(bcFormat) / 2;
            TestB2TCopy(utils::Expectation::Failure, buffer, kInvalidBufferOffset, 256, 4, texture,
                        0, 0, {0, 0, 0}, {4, 4, 1});
            TestT2BCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, buffer,
                        kInvalidBufferOffset, 256, 4, {4, 4, 1});
        }
    }
}

// Tests to verify that RowPitch must not be smaller than (width / blockWidth) * blockSizeInBytes
// and it is valid to use 0 as RowPitch in buffer-to-texture or texture-to-buffer copies with
// compressed texture formats.
// Note that in Dawn we require RowPitch be a multiple of 256, which ensures RowPitch will always be
// the multiple of compressed texture block width in bytes.
TEST_F(CopyCommandTest_CompressedTextureFormats, RowPitch) {
    dawn::Buffer buffer =
        CreateBuffer(1024, dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst);

    // Failures on small RowPitch.
    {
        constexpr uint32_t kSmallRowPitch = 256;
        constexpr uint32_t kTestWidth = 160;
        constexpr uint32_t kTestHeight = 160;
        // The smallest valid row pitch is 320 (160 / 4 * 8 for BC1 and BC4) or 640 (160 / 4 * 16
        // for others)
        for (dawn::TextureFormat bcFormat : kBCFormats) {
            dawn::Texture texture = Create2DTexture(bcFormat, 1, kTestWidth, kTestHeight);

            TestB2TCopy(utils::Expectation::Failure, buffer, 0, kSmallRowPitch, 4, texture, 0, 0,
                        {0, 0, 0}, {kTestWidth, 4, 1});
            TestT2BCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, buffer, 0,
                        kSmallRowPitch, 4, {kTestWidth, 4, 1});
        }
    }

    // Test RowPitch == 0.
    {
        constexpr uint32_t kZeroRowPitch = 0;
        constexpr uint32_t kTestWidth = 128;
        constexpr uint32_t kTestHeight = 128;
        for (dawn::TextureFormat bcFormat : kBCFormats) {
            dawn::Texture texture = Create2DTexture(bcFormat, 1, kTestWidth, kTestHeight);

            TestB2TCopy(utils::Expectation::Success, buffer, 0, kZeroRowPitch, 4, texture, 0, 0,
                        {0, 0, 0}, {kTestWidth, 4, 1});
            TestT2BCopy(utils::Expectation::Success, texture, 0, 0, {0, 0, 0}, buffer, 0,
                        kZeroRowPitch, 4, {kTestWidth, 4, 1});
        }
    }
}

// Tests to verify that imageHeight must be a multiple of the compressed texture block height in
// buffer-to-texture or texture-to-buffer copies with compressed texture formats.
TEST_F(CopyCommandTest_CompressedTextureFormats, ImageHeight) {
    dawn::Buffer buffer =
        CreateBuffer(512, dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst);

    for (dawn::TextureFormat bcFormat : kBCFormats) {
        dawn::Texture texture = Create2DTexture(bcFormat);

        // Valid usages of imageHeight in B2T and T2B copies with compressed texture formats.
        {
            constexpr uint32_t kValidImageHeight = 8;
            TestB2TCopy(utils::Expectation::Success, buffer, 0, 256, kValidImageHeight, texture, 0,
                        0, {0, 0, 0}, {4, 4, 1});
            TestT2BCopy(utils::Expectation::Success, texture, 0, 0, {0, 0, 0}, buffer, 0, 256,
                        kValidImageHeight, {4, 4, 1});
        }

        // Failures on invalid imageHeight.
        {
            constexpr uint32_t kInvalidImageHeight = 3;
            TestB2TCopy(utils::Expectation::Failure, buffer, 0, 256, kInvalidImageHeight, texture,
                        0, 0, {0, 0, 0}, {4, 4, 1});
            TestT2BCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, buffer, 0, 256,
                        kInvalidImageHeight, {4, 4, 1});
        }
    }
}

// Tests to verify that ImageOffset.x must be a multiple of the compressed texture block width and
// ImageOffset.y must be a multiple of the compressed texture block height in buffer-to-texture,
// texture-to-buffer or texture-to-texture copies with compressed texture formats.
TEST_F(CopyCommandTest_CompressedTextureFormats, ImageOffset) {
    dawn::Buffer buffer =
        CreateBuffer(512, dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst);

    for (dawn::TextureFormat bcFormat : kBCFormats) {
        dawn::Texture texture = Create2DTexture(bcFormat);
        dawn::Texture texture2 = Create2DTexture(bcFormat);

        // Valid usages of ImageOffset in B2T, T2B and T2T copies with compressed texture formats.
        {
            constexpr dawn::Origin3D kValidOrigin3D = {8, 8, 0};
            TestB2TCopy(utils::Expectation::Success, buffer, 0, 256, 4, texture, 0, 0,
                        kValidOrigin3D, {4, 4, 1});
            TestT2BCopy(utils::Expectation::Success, texture, 0, 0, kValidOrigin3D, buffer, 0, 256,
                        4, {4, 4, 1});
            TestT2TCopy(utils::Expectation::Success, texture, 0, 0, kValidOrigin3D, texture2, 0, 0,
                        kValidOrigin3D, {4, 4, 1});
        }

        // Failures on invalid ImageOffset.x.
        {
            constexpr dawn::Origin3D kInvalidOrigin3D = {3, 4, 0};
            TestB2TCopy(utils::Expectation::Failure, buffer, 0, 256, 4, texture, 0, 0,
                        kInvalidOrigin3D, {4, 4, 1});
            TestT2BCopy(utils::Expectation::Failure, texture, 0, 0, kInvalidOrigin3D, buffer, 0,
                        256, 4, {4, 4, 1});
            TestT2TCopy(utils::Expectation::Failure, texture, 0, 0, kInvalidOrigin3D, texture2, 0,
                        0, {0, 0, 0}, {4, 4, 1});
            TestT2TCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, texture2, 0, 0,
                        kInvalidOrigin3D, {4, 4, 1});
        }

        // Failures on invalid ImageOffset.x.
        {
            constexpr dawn::Origin3D kInvalidOrigin3D = {4, 3, 0};
            TestB2TCopy(utils::Expectation::Failure, buffer, 0, 256, 4, texture, 0, 0,
                        kInvalidOrigin3D, {4, 4, 1});
            TestT2BCopy(utils::Expectation::Failure, texture, 0, 0, kInvalidOrigin3D, buffer, 0,
                        256, 4, {4, 4, 1});
            TestT2TCopy(utils::Expectation::Failure, texture, 0, 0, kInvalidOrigin3D, texture2, 0,
                        0, {0, 0, 0}, {4, 4, 1});
            TestT2TCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, texture2, 0, 0,
                        kInvalidOrigin3D, {4, 4, 1});
        }
    }
}

// Tests to verify that ImageExtent.x must be a multiple of the compressed texture block width and
// ImageExtent.y must be a multiple of the compressed texture block height in buffer-to-texture,
// texture-to-buffer or texture-to-texture copies with compressed texture formats.
TEST_F(CopyCommandTest_CompressedTextureFormats, ImageExtent) {
    dawn::Buffer buffer =
        CreateBuffer(512, dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst);

    constexpr uint32_t kMipmapLevels = 3;
    constexpr uint32_t kTestWidth = 60;
    constexpr uint32_t kTestHeight = 60;

    for (dawn::TextureFormat bcFormat : kBCFormats) {
        dawn::Texture texture = Create2DTexture(bcFormat, kMipmapLevels, kTestWidth, kTestHeight);
        dawn::Texture texture2 = Create2DTexture(bcFormat, kMipmapLevels, kTestWidth, kTestHeight);

        // Valid usages of ImageExtent in B2T, T2B and T2T copies with compressed texture formats.
        {
            constexpr dawn::Extent3D kValidExtent3D = {8, 8, 1};
            TestB2TCopy(utils::Expectation::Success, buffer, 0, 256, 8, texture, 0, 0, {0, 0, 0},
                        kValidExtent3D);
            TestT2BCopy(utils::Expectation::Success, texture, 0, 0, {0, 0, 0}, buffer, 0, 256, 8,
                        kValidExtent3D);
            TestT2TCopy(utils::Expectation::Success, texture, 0, 0, {0, 0, 0}, texture2, 0, 0,
                        {0, 0, 0}, kValidExtent3D);
        }

        // Valid usages of ImageExtent in B2T, T2B and T2T copies with compressed texture formats
        // and non-zero mipmap levels.
        {
            constexpr dawn::Extent3D kValidExtent3D = {4, 4, 1};
            constexpr uint32_t kTestMipmapLevel = 2;
            constexpr dawn::Origin3D kTestOrigin = {(kTestWidth >> kTestMipmapLevel) - 3,
                                                    (kTestHeight >> kTestMipmapLevel) - 3, 0};

            TestB2TCopy(utils::Expectation::Success, buffer, 0, 256, 4, texture, kTestMipmapLevel,
                        0, kTestOrigin, kValidExtent3D);
            TestT2BCopy(utils::Expectation::Success, texture, kTestMipmapLevel, 0, kTestOrigin,
                        buffer, 0, 256, 4, kValidExtent3D);
            TestT2TCopy(utils::Expectation::Success, texture, kTestMipmapLevel, 0, kTestOrigin,
                        texture2, 0, 0, {0, 0, 0}, kValidExtent3D);
            TestT2TCopy(utils::Expectation::Success, texture, kTestMipmapLevel, 0, {0, 0, 0},
                        texture2, 0, 0, kTestOrigin, kValidExtent3D);
        }

        // Failures on invalid ImageExtent.x.
        {
            constexpr dawn::Extent3D kInValidExtent3D = {3, 4, 1};
            TestB2TCopy(utils::Expectation::Failure, buffer, 0, 256, 4, texture, 0, 0, {0, 0, 0},
                        kInValidExtent3D);
            TestT2BCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, buffer, 0, 256, 4,
                        kInValidExtent3D);
            TestT2TCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, texture2, 0, 0,
                        {0, 0, 0}, kInValidExtent3D);
            TestT2TCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, texture2, 0, 0,
                        {0, 0, 0}, kInValidExtent3D);
        }

        // Failures on invalid ImageExtent.y.
        {
            constexpr dawn::Extent3D kInValidExtent3D = {3, 4, 1};
            TestB2TCopy(utils::Expectation::Failure, buffer, 0, 256, 4, texture, 0, 0, {0, 0, 0},
                        kInValidExtent3D);
            TestT2BCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, buffer, 0, 256, 4,
                        kInValidExtent3D);
            TestT2TCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, texture2, 0, 0,
                        {0, 0, 0}, kInValidExtent3D);
            TestT2TCopy(utils::Expectation::Failure, texture, 0, 0, {0, 0, 0}, texture2, 0, 0,
                        {0, 0, 0}, kInValidExtent3D);
        }
    }
}
