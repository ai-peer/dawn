// Copyright 2019 The Dawn Authors
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
#include "utils/DawnHelpers.h"

class TextureResetTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
    }
    dawn::TextureDescriptor CreateTextureDescriptor(uint32_t mipLevelCount,
                                                    uint32_t arrayLayerCount,
                                                    dawn::TextureUsageBit usage) {
        dawn::TextureDescriptor descriptor;
        descriptor.dimension = dawn::TextureDimension::e2D;
        descriptor.size.width = kSize;
        descriptor.size.height = kSize;
        descriptor.size.depth = 1;
        descriptor.arrayLayerCount = arrayLayerCount;
        descriptor.sampleCount = 1;
        descriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
        descriptor.mipLevelCount = mipLevelCount;
        descriptor.usage = usage;
        return descriptor;
    }
    constexpr static uint32_t kSize = 128;
};

// This tests that the code path of CopyTextureToBuffer clears correctly to black after first usage
TEST_P(TextureResetTest, RecycleTextureMemoryClear) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    // Texture's first usage is in EXPECT_PIXEL_RGBA8_EQ's call to CopyTextureToBuffer
    RGBA8 filledWithZeros(0, 0, 0, 0);
    EXPECT_PIXEL_RGBA8_EQ(filledWithZeros, texture, 0, 0);
}

// Test that non-zero mip level clears subresource to black after first use
// This goes through the BeginRenderPass's code path
TEST_P(TextureResetTest, MipMapClearsToBlack) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        4, 1, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kSize, kSize);
    renderPass.color = texture;

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        // Texture's first usage is in BeginRenderPass's call to RecordRenderPass
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.EndPass();
    }
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 filledWithZeros(0, 0, 0, 0);
    std::vector<RGBA8> expected(kSize * kSize, filledWithZeros);

    uint32_t mipSize = kSize >> 2;
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), renderPass.color, 0, 0, mipSize, mipSize, 0, 0);
}

// Test that non-zero array layers clears subresource to black after first use
// This goes through the BeginRenderPass's code path
TEST_P(TextureResetTest, ArrayLayerClearsToBlack) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 4, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kSize, kSize);
    renderPass.color = texture;

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.EndPass();
    }
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 filledWithZeros(0, 0, 0, 0);
    std::vector<RGBA8> expected(kSize * kSize, filledWithZeros);

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), renderPass.color, 0, 0, kSize, kSize, 0, 0);
}

// This tests that the CopyBufferToTexture's code path clears to black
TEST_P(TextureResetTest, CopyBufferToTexture) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        4, 1,
        dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::Sampled |
            dawn::TextureUsageBit::TransferSrc);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    std::vector<uint8_t> data(4 * kSize * kSize, 100);
    dawn::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), dawn::BufferUsageBit::TransferSrc);
    dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(stagingBuffer, 0, 0, 0);
    dawn::TextureCopyView textureCopyView = utils::CreateTextureCopyView(texture, 0, 0, {0, 0, 0});
    dawn::Extent3D copySize = {kSize, kSize, 1};
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 filledWith100(100, 100, 100, 100);
    std::vector<RGBA8> expected(kSize * kSize, filledWith100);

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), texture, 0, 0, kSize, kSize, 0, 0);
}

// Test for a copy only to a subset of the subresource
TEST_P(TextureResetTest, CopyBufferToTextureHalf) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        4, 1,
        dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::Sampled |
            dawn::TextureUsageBit::TransferSrc);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    std::vector<uint8_t> data(4 * kSize * kSize, 100);
    dawn::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), dawn::BufferUsageBit::TransferSrc);
    dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(stagingBuffer, 0, 0, 0);
    dawn::TextureCopyView textureCopyView = utils::CreateTextureCopyView(texture, 0, 0, {0, 0, 0});
    dawn::Extent3D copySize = {kSize / 2, kSize / 2, 1};
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 filledWithZeros(0, 0, 0, 0);
    RGBA8 filledWith100(100, 100, 100, 100);
    std::vector<RGBA8> expected100(kSize / 2 * kSize / 2, filledWith100);
    std::vector<RGBA8> expectedZeros(kSize / 2 * kSize / 2, filledWithZeros);
    // first half filled with 100, by the buffer data
    EXPECT_TEXTURE_RGBA8_EQ(expected100.data(), texture, 0, 0, kSize / 2, kSize / 2, 0, 0);
    // second half should be cleared
    EXPECT_TEXTURE_RGBA8_EQ(expectedZeros.data(), texture, kSize / 2, kSize / 2, kSize / 2,
                            kSize / 2, 0, 0);
}

// This Tests the CopyTextureToTexture's code path clears to black
TEST_P(TextureResetTest, CopyTextureToTexture) {
    dawn::TextureDescriptor dstDescriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::Sampled | dawn::TextureUsageBit::TransferSrc);
    dawn::Texture srcTexture = device.CreateTexture(&dstDescriptor);

    dawn::TextureCopyView srcTextureCopyView =
        utils::CreateTextureCopyView(srcTexture, 0, 0, {0, 0, 0});

    dawn::TextureDescriptor srcDescriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferDst |
            dawn::TextureUsageBit::TransferSrc);
    dawn::Texture dstTexture = device.CreateTexture(&srcDescriptor);

    dawn::TextureCopyView dstTextureCopyView =
        utils::CreateTextureCopyView(dstTexture, 0, 0, {0, 0, 0});

    dawn::Extent3D copySize = {kSize, kSize, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcTextureCopyView, &dstTextureCopyView, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 filledWithZeros(0, 0, 0, 0);
    std::vector<RGBA8> expected(kSize * kSize, filledWithZeros);

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), srcTexture, 0, 0, kSize, kSize, 0, 0);
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), dstTexture, 0, 0, kSize, kSize, 0, 0);
}

// This Tests the CopyTextureToTexture's code path clears to black
TEST_P(TextureResetTest, CopyTextureToTextureHalf) {
    dawn::TextureDescriptor dstDescriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::Sampled | dawn::TextureUsageBit::TransferSrc);
    dawn::Texture srcTexture = device.CreateTexture(&dstDescriptor);

    dawn::TextureCopyView srcTextureCopyView =
        utils::CreateTextureCopyView(srcTexture, 0, 0, {0, 0, 0});

    dawn::TextureDescriptor srcDescriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferDst |
            dawn::TextureUsageBit::TransferSrc);
    dawn::Texture dstTexture = device.CreateTexture(&srcDescriptor);

    dawn::TextureCopyView dstTextureCopyView =
        utils::CreateTextureCopyView(dstTexture, 0, 0, {0, 0, 0});
    dawn::Extent3D copySize = {kSize / 2, kSize / 2, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcTextureCopyView, &dstTextureCopyView, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    RGBA8 filledWithZeros(0, 0, 0, 0);
    std::vector<RGBA8> expected(kSize * kSize, filledWithZeros);

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), srcTexture, 0, 0, kSize, kSize, 0, 0);
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), dstTexture, 0, 0, kSize, kSize, 0, 0);
}

DAWN_INSTANTIATE_TEST(TextureResetTest,
                      ForceWorkarounds(VulkanBackend,
                                       {"nonzero_clear_resources_on_creation_for_testing"}));