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
    constexpr static uint32_t kSize = 128;
};

// This tests that the code path of CopyTextureToBuffer clears correctly to black after first usage
TEST_P(TextureResetTest, RecycleTextureMemoryClear) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = kSize;
    descriptor.size.height = kSize;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
    descriptor.mipLevelCount = 1;
    descriptor.usage = dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
    dawn::Texture texture = device.CreateTexture(&descriptor);

    RGBA8 filledWithZeros(0, 0, 0, 0);

    // Texture's first usage is in EXPECT_PIXEL_RGBA8_EQ's call to CopyTextureToBuffer
    EXPECT_PIXEL_RGBA8_EQ(filledWithZeros, texture, 0, 0);
}

// Test that non-zero mip level clears subresource to black after first use
// This goes through the BeginRenderPass's code path
TEST_P(TextureResetTest, MipMapClearsToBlack) {
    constexpr uint32_t mipLevels = 4;

    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = kSize;
    descriptor.size.height = kSize;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
    descriptor.mipLevelCount = mipLevels;
    descriptor.usage = dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
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

    std::vector<RGBA8> expected;
    RGBA8 filledWithZeros(0, 0, 0, 0);
    for (uint32_t i = 0; i < kSize * kSize; ++i) {
        expected.push_back(filledWithZeros);
    }
    uint32_t mipSize = kSize >> 2;
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), renderPass.color, 0, 0, mipSize, mipSize, 2, 0);
}

// Test that non-zero array layers clears subresource to black after first use
// This goes through the BeginRenderPass's code path
TEST_P(TextureResetTest, ArrayLayerClearsToBlack) {
    constexpr uint32_t arrayLayers = 4;

    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = kSize;
    descriptor.size.height = kSize;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = arrayLayers;
    descriptor.sampleCount = 1;
    descriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
    descriptor.mipLevelCount = 1;
    descriptor.usage = dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
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

    std::vector<RGBA8> expected;
    RGBA8 filledWithZeros(0, 0, 0, 0);
    for (uint32_t i = 0; i < kSize * kSize; ++i) {
        expected.push_back(filledWithZeros);
    }

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), renderPass.color, 0, 0, kSize, kSize, 0, 2);
}

// This tests that the CopyBufferToTexture's code path clears to black
TEST_P(TextureResetTest, CopyBufferToTexture) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = kSize;
    descriptor.size.height = kSize;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
    descriptor.mipLevelCount = 4;
    descriptor.usage = dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::Sampled |
                       dawn::TextureUsageBit::TransferSrc;
    dawn::Texture texture = device.CreateTexture(&descriptor);

    // Initialize buffer with arbitrary data
    std::vector<uint8_t> data(4 * kSize * kSize, 0);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = 100;
    }
    dawn::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), dawn::BufferUsageBit::TransferSrc);
    dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(stagingBuffer, 0, 0, 0);
    dawn::TextureCopyView textureCopyView = utils::CreateTextureCopyView(texture, 0, 0, {0, 0, 0});
    dawn::Extent3D copySize = {kSize, kSize, 1};
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    { encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize); }
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<RGBA8> expected;
    RGBA8 filledWith100(100, 100, 100, 100);
    for (uint32_t i = 0; i < kSize * kSize; ++i) {
        expected.push_back(filledWith100);
    }

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), texture, 0, 0, kSize, kSize, 0, 0);
}

// This Tests the CopyTextureToTexture's code path clears to black
TEST_P(TextureResetTest, CopyTextureToTexture) {
    dawn::TextureDescriptor dstDescriptor;
    dstDescriptor.dimension = dawn::TextureDimension::e2D;
    dstDescriptor.size.width = kSize;
    dstDescriptor.size.height = kSize;
    dstDescriptor.size.depth = 1;
    dstDescriptor.arrayLayerCount = 1;
    dstDescriptor.sampleCount = 1;
    dstDescriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
    dstDescriptor.mipLevelCount = 1;
    dstDescriptor.usage = dawn::TextureUsageBit::Sampled | dawn::TextureUsageBit::TransferSrc;
    dawn::Texture srcTexture = device.CreateTexture(&dstDescriptor);

    dawn::TextureCopyView srcTextureCopyView =
        utils::CreateTextureCopyView(srcTexture, 0, 0, {0, 0, 0});

    dawn::TextureDescriptor srcDescriptor;
    srcDescriptor.dimension = dawn::TextureDimension::e2D;
    srcDescriptor.size.width = kSize;
    srcDescriptor.size.height = kSize;
    srcDescriptor.size.depth = 1;
    srcDescriptor.arrayLayerCount = 1;
    srcDescriptor.sampleCount = 1;
    srcDescriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
    srcDescriptor.mipLevelCount = 1;
    srcDescriptor.usage = dawn::TextureUsageBit::OutputAttachment |
                          dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::TransferSrc;
    dawn::Texture dstTexture = device.CreateTexture(&srcDescriptor);

    dawn::TextureCopyView dstTextureCopyView =
        utils::CreateTextureCopyView(dstTexture, 0, 0, {0, 0, 0});

    dawn::Extent3D copySize = {kSize, kSize, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    { encoder.CopyTextureToTexture(&srcTextureCopyView, &dstTextureCopyView, &copySize); }
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<RGBA8> expected;
    RGBA8 filledWithZeros(0, 0, 0, 0);
    for (uint32_t i = 0; i < kSize * kSize; ++i) {
        expected.push_back(filledWithZeros);
    }
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), srcTexture, 0, 0, kSize, kSize, 0, 0);
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), dstTexture, 0, 0, kSize, kSize, 0, 0);
}

DAWN_INSTANTIATE_TEST(TextureResetTest,
                      ForceWorkaround(VulkanBackend,
                                      "nonzero_clear_resources_on_creation_for_testing"));