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

class TextureZeroInitTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
    }
    dawn::TextureDescriptor CreateTextureDescriptor(uint32_t mipLevelCount,
                                                    uint32_t arrayLayerCount,
                                                    dawn::TextureUsageBit usage,
                                                    dawn::TextureFormat format) {
        dawn::TextureDescriptor descriptor;
        descriptor.dimension = dawn::TextureDimension::e2D;
        descriptor.size.width = kSize;
        descriptor.size.height = kSize;
        descriptor.size.depth = 1;
        descriptor.arrayLayerCount = arrayLayerCount;
        descriptor.sampleCount = 1;
        descriptor.format = format;
        descriptor.mipLevelCount = mipLevelCount;
        descriptor.usage = usage;
        return descriptor;
    }
    dawn::TextureViewDescriptor CreateTextureViewDescriptor(uint32_t baseMipLevel,
                                                            uint32_t baseArrayLayer) {
        dawn::TextureViewDescriptor descriptor;
        descriptor.format = kColorFormat;
        descriptor.baseArrayLayer = baseArrayLayer;
        descriptor.arrayLayerCount = 1;
        descriptor.baseMipLevel = baseMipLevel;
        descriptor.mipLevelCount = 1;
        descriptor.dimension = dawn::TextureViewDimension::e2D;
        return descriptor;
    }
    dawn::RenderPipeline CreatePipelineForTest() {
        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.cVertexStage.module = CreateVertexShader();
        pipelineDescriptor.cFragmentStage.module =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment,
                                      "#version 450\n"
                                      "layout(location = 0) out vec4 fragColor;"
                                      "void main() {\n"
                                      "   fragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                                      "}\n");

        pipelineDescriptor.cDepthStencilState.depthCompare = dawn::CompareFunction::Equal;
        pipelineDescriptor.cDepthStencilState.stencilFront.compare = dawn::CompareFunction::Equal;
        pipelineDescriptor.depthStencilState = &pipelineDescriptor.cDepthStencilState;
        return device.CreateRenderPipeline(&pipelineDescriptor);
    }
    dawn::ShaderModule CreateVertexShader() {
        return utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(#version 450
        const vec2 pos[6] = vec2[6](vec2(-1.0f, -1.0f),
                                    vec2(-1.0f,  1.0f),
                                    vec2( 1.0f, -1.0f),
                                    vec2( 1.0f,  1.0f),
                                    vec2(-1.0f,  1.0f),
                                    vec2( 1.0f, -1.0f)
                                    );

        void main() {
           gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
        })");
    }
    constexpr static uint32_t kSize = 128;
    constexpr static dawn::TextureFormat kColorFormat = dawn::TextureFormat::RGBA8Unorm;
    constexpr static dawn::TextureFormat kDepthStencilFormat =
        dawn::TextureFormat::Depth24PlusStencil8;
};

// This tests that the code path of CopyTextureToBuffer clears correctly to Zero after first usage
TEST_P(TextureZeroInitTest, CopyTextureToBufferSource) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    // Texture's first usage is in EXPECT_PIXEL_RGBA8_EQ's call to CopyTextureToBuffer
    RGBA8 filledWithZeros(0, 0, 0, 0);
    EXPECT_PIXEL_RGBA8_EQ(filledWithZeros, texture, 0, 0);
}

// Test that non-zero mip level clears subresource to Zero after first use
// This goes through the BeginRenderPass's code path
TEST_P(TextureZeroInitTest, RenderingMipMapClearsToZero) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        4, 1, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    dawn::TextureViewDescriptor viewDescriptor = CreateTextureViewDescriptor(2, 0);
    dawn::TextureView view = texture.CreateView(&viewDescriptor);

    utils::BasicRenderPass renderPass = utils::BasicRenderPass(kSize, kSize, texture, kColorFormat);

    renderPass.renderPassInfo.cColorAttachmentsInfoPtr[0]->attachment = view;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        // Texture's first usage is in BeginRenderPass's call to RecordRenderPass
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.EndPass();
    }
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    uint32_t mipSize = kSize >> 2;
    std::vector<RGBA8> expected(mipSize * mipSize, {0, 0, 0, 0});

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), renderPass.color, 0, 0, mipSize, mipSize, 2, 0);
}

// Test that non-zero array layers clears subresource to Zero after first use.
// This goes through the BeginRenderPass's code path
TEST_P(TextureZeroInitTest, RenderingArrayLayerClearsToZero) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 4, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    dawn::TextureViewDescriptor viewDescriptor = CreateTextureViewDescriptor(0, 2);
    dawn::TextureView view = texture.CreateView(&viewDescriptor);

    utils::BasicRenderPass renderPass = utils::BasicRenderPass(kSize, kSize, texture, kColorFormat);

    renderPass.renderPassInfo.cColorAttachmentsInfoPtr[0]->attachment = view;
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.EndPass();
    }
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<RGBA8> expected(kSize * kSize, {0, 0, 0, 0});

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), renderPass.color, 0, 0, kSize, kSize, 0, 2);
}

// This tests CopyBufferToTexture fully overwrites copy so lazy init is not needed.
// TODO(natlee@microsoft.com): Add backdoor to dawn native to query the number of zero-inited
// subresources
TEST_P(TextureZeroInitTest, CopyBufferToTexture) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        4, 1,
        dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::Sampled |
            dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
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

    std::vector<RGBA8> expected(kSize * kSize, {100, 100, 100, 100});

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), texture, 0, 0, kSize, kSize, 0, 0);
}

// Test for a copy only to a subset of the subresource, lazy init is necessary to clear the other
// half.
TEST_P(TextureZeroInitTest, CopyBufferToTextureHalf) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        4, 1,
        dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::Sampled |
            dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    std::vector<uint8_t> data(4 * kSize * kSize, 100);
    dawn::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, data.data(), static_cast<uint32_t>(data.size()), dawn::BufferUsageBit::TransferSrc);

    dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(stagingBuffer, 0, 0, 0);
    dawn::TextureCopyView textureCopyView = utils::CreateTextureCopyView(texture, 0, 0, {0, 0, 0});
    dawn::Extent3D copySize = {kSize / 2, kSize, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<RGBA8> expected100((kSize / 2) * kSize, {100, 100, 100, 100});
    std::vector<RGBA8> expectedZeros((kSize / 2) * kSize, {0, 0, 0, 0});
    // first half filled with 100, by the buffer data
    EXPECT_TEXTURE_RGBA8_EQ(expected100.data(), texture, 0, 0, kSize / 2, kSize, 0, 0);
    // second half should be cleared
    EXPECT_TEXTURE_RGBA8_EQ(expectedZeros.data(), texture, kSize / 2, 0, kSize / 2, kSize, 0, 0);
}

// This tests CopyTextureToTexture fully overwrites copy so lazy init is not needed.
TEST_P(TextureZeroInitTest, CopyTextureToTexture) {
    dawn::TextureDescriptor srcDescriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::Sampled | dawn::TextureUsageBit::TransferSrc, kColorFormat);
    dawn::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    dawn::TextureCopyView srcTextureCopyView =
        utils::CreateTextureCopyView(srcTexture, 0, 0, {0, 0, 0});

    dawn::TextureDescriptor dstDescriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferDst |
            dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
    dawn::Texture dstTexture = device.CreateTexture(&dstDescriptor);

    dawn::TextureCopyView dstTextureCopyView =
        utils::CreateTextureCopyView(dstTexture, 0, 0, {0, 0, 0});

    dawn::Extent3D copySize = {kSize, kSize, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcTextureCopyView, &dstTextureCopyView, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<RGBA8> expected(kSize * kSize, {0, 0, 0, 0});

    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), srcTexture, 0, 0, kSize, kSize, 0, 0);
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), dstTexture, 0, 0, kSize, kSize, 0, 0);
}

// This Tests the CopyTextureToTexture's copy only to a subset of the subresource, lazy init is
// necessary to clear the other half.
TEST_P(TextureZeroInitTest, CopyTextureToTextureHalf) {
    dawn::TextureDescriptor srcDescriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::Sampled | dawn::TextureUsageBit::TransferSrc |
            dawn::TextureUsageBit::TransferDst,
        kColorFormat);
    dawn::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    // fill srcTexture with 100
    {
        std::vector<uint8_t> data(4 * kSize * kSize, 100);
        dawn::Buffer stagingBuffer =
            utils::CreateBufferFromData(device, data.data(), static_cast<uint32_t>(data.size()),
                                        dawn::BufferUsageBit::TransferSrc);
        dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(stagingBuffer, 0, 0, 0);
        dawn::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(srcTexture, 0, 0, {0, 0, 0});
        dawn::Extent3D copySize = {kSize, kSize, 1};
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);
        dawn::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    dawn::TextureCopyView srcTextureCopyView =
        utils::CreateTextureCopyView(srcTexture, 0, 0, {0, 0, 0});

    dawn::TextureDescriptor dstDescriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferDst |
            dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
    dawn::Texture dstTexture = device.CreateTexture(&dstDescriptor);

    dawn::TextureCopyView dstTextureCopyView =
        utils::CreateTextureCopyView(dstTexture, 0, 0, {0, 0, 0});
    dawn::Extent3D copySize = {kSize / 2, kSize, 1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToTexture(&srcTextureCopyView, &dstTextureCopyView, &copySize);
    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<RGBA8> expectedWithZeros((kSize / 2) * kSize, {0, 0, 0, 0});
    std::vector<RGBA8> expectedWith100(kSize * kSize, {100, 100, 100, 100});

    EXPECT_TEXTURE_RGBA8_EQ(expectedWith100.data(), srcTexture, 0, 0, kSize, kSize, 0, 0);
    EXPECT_TEXTURE_RGBA8_EQ(expectedWith100.data(), dstTexture, 0, 0, kSize / 2, kSize, 0, 0);
    EXPECT_TEXTURE_RGBA8_EQ(expectedWithZeros.data(), dstTexture, kSize / 2, 0, kSize / 2, kSize, 0,
                            0);
}

// This tests the texture with depth attachment and load op load will init depth stencil texture to
// 0s.
TEST_P(TextureZeroInitTest, RenderingLoadingDepth) {
    dawn::TextureDescriptor srcDescriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::TransferSrc | dawn::TextureUsageBit::TransferDst |
            dawn::TextureUsageBit::OutputAttachment,
        kColorFormat);
    dawn::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    dawn::TextureDescriptor depthStencilDescriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc,
        kDepthStencilFormat);
    dawn::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    utils::ComboRenderPassDescriptor renderPassDescriptor({srcTexture.CreateDefaultView()},
                                                          depthStencilTexture.CreateDefaultView());
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = dawn::LoadOp::Load;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = dawn::LoadOp::Clear;
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearStencil = 0;

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.SetPipeline(CreatePipelineForTest());
    pass.Draw(6, 1, 0, 0);
    pass.EndPass();
    dawn::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    // Expect the texture to be red because depth test passed.
    std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), srcTexture, 0, 0, kSize, kSize, 0, 0);
}

// This tests the texture with stencil attachment and load op load will init depth stencil texture
// to 0s.
TEST_P(TextureZeroInitTest, RenderingLoadingStencil) {
    dawn::TextureDescriptor srcDescriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::TransferSrc | dawn::TextureUsageBit::TransferDst |
            dawn::TextureUsageBit::OutputAttachment,
        kColorFormat);
    dawn::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    dawn::TextureDescriptor depthStencilDescriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc,
        kDepthStencilFormat);
    dawn::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    utils::ComboRenderPassDescriptor renderPassDescriptor({srcTexture.CreateDefaultView()},
                                                          depthStencilTexture.CreateDefaultView());
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = dawn::LoadOp::Clear;
    renderPassDescriptor.cDepthStencilAttachmentInfo.clearDepth = 0.0f;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = dawn::LoadOp::Load;

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.SetPipeline(CreatePipelineForTest());
    pass.Draw(6, 1, 0, 0);
    pass.EndPass();
    dawn::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    // Expect the texture to be red because stencil test passed.
    std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), srcTexture, 0, 0, kSize, kSize, 0, 0);
}

// This tests the texture with depth stencil attachment and load op load will init depth stencil
// texture to 0s.
TEST_P(TextureZeroInitTest, RenderingLoadingDepthStencil) {
    dawn::TextureDescriptor srcDescriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::TransferSrc | dawn::TextureUsageBit::TransferDst |
            dawn::TextureUsageBit::OutputAttachment,
        kColorFormat);
    dawn::Texture srcTexture = device.CreateTexture(&srcDescriptor);

    dawn::TextureDescriptor depthStencilDescriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc,
        kDepthStencilFormat);
    dawn::Texture depthStencilTexture = device.CreateTexture(&depthStencilDescriptor);

    utils::ComboRenderPassDescriptor renderPassDescriptor({srcTexture.CreateDefaultView()},
                                                          depthStencilTexture.CreateDefaultView());
    renderPassDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = dawn::LoadOp::Load;
    renderPassDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = dawn::LoadOp::Load;

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.SetPipeline(CreatePipelineForTest());
    pass.Draw(6, 1, 0, 0);
    pass.EndPass();
    dawn::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    // Expect the texture to be red because both depth and stencil tests passed.
    std::vector<RGBA8> expected(kSize * kSize, {255, 0, 0, 255});
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), srcTexture, 0, 0, kSize, kSize, 0, 0);
}

// This tests the color attachments clear to 0s
TEST_P(TextureZeroInitTest, ColorAttachmentsClear) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
    dawn::Texture texture = device.CreateTexture(&descriptor);
    utils::BasicRenderPass renderPass = utils::BasicRenderPass(kSize, kSize, texture, kColorFormat);
    renderPass.renderPassInfo.cColorAttachmentsInfoPtr[0]->loadOp = dawn::LoadOp::Load;

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();

    dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    pass.EndPass();

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<RGBA8> expected(kSize * kSize, {0, 0, 0, 0});
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), renderPass.color, 0, 0, kSize, kSize, 0, 0);
}

// This tests the clearing of sampled textures in render pass
TEST_P(TextureZeroInitTest, RenderPassSampledTextureClear) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::Sampled, kColorFormat);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    dawn::TextureDescriptor renderTextureDescriptor = CreateTextureDescriptor(
        1, 1, dawn::TextureUsageBit::TransferSrc | dawn::TextureUsageBit::OutputAttachment,
        kColorFormat);
    dawn::Texture renderTexture = device.CreateTexture(&renderTextureDescriptor);

    dawn::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
        device, {{0, dawn::ShaderStageBit::Fragment, dawn::BindingType::Sampler},
                 {1, dawn::ShaderStageBit::Fragment, dawn::BindingType::SampledTexture}});

    utils::ComboRenderPassDescriptor renderPassDesc({renderTexture.CreateDefaultView()});
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);

    // fill half of sampled texture with data, other half should stay clear
    {
        std::vector<uint8_t> data(4 * kSize * kSize, 100);
        dawn::Buffer stagingBuffer =
            utils::CreateBufferFromData(device, data.data(), static_cast<uint32_t>(data.size()),
                                        dawn::BufferUsageBit::TransferSrc);
        dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(stagingBuffer, 0, 0, 0);
        dawn::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(texture, 0, 0, {0, 0, 0});
        dawn::Extent3D copySize = {kSize / 2, kSize, 1};
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);
        dawn::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
    pipelineDescriptor.layout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout);
    pipelineDescriptor.cVertexStage.module = CreateVertexShader();
    pipelineDescriptor.cFragmentStage.module = utils::CreateShaderModule(
        device, dawn::ShaderStage::Fragment,
        "#version 450\n"
        "layout(set = 0, binding = 0) uniform sampler sampler0;"
        "layout(set = 0, binding = 1) uniform texture2D texture0;"
        "layout(location = 0) out vec4 fragColor;"
        "void main() {\n"
        "   fragColor = texture(sampler2D(texture0, sampler0), gl_FragCoord.xy/128);"
        "}\n");
    pipelineDescriptor.cColorStates[0]->format = kColorFormat;
    pass.SetPipeline(device.CreateRenderPipeline(&pipelineDescriptor));

    dawn::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    dawn::Sampler sampler = device.CreateSampler(&samplerDesc);
    dawn::BindGroup bindGroup = utils::MakeBindGroup(
        device, bindGroupLayout, {{0, sampler}, {1, texture.CreateDefaultView()}});

    pass.SetBindGroup(0, bindGroup, 0, nullptr);
    pass.Draw(6, 1, 0, 0);
    pass.EndPass();

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<RGBA8> expectedWith100(kSize * kSize, {100, 100, 100, 100});
    std::vector<RGBA8> expectedWithZeros(kSize * kSize, {0, 0, 0, 0});
    EXPECT_TEXTURE_RGBA8_EQ(expectedWith100.data(), renderTexture, 0, 0, kSize / 2, kSize, 0, 0);
    EXPECT_TEXTURE_RGBA8_EQ(expectedWithZeros.data(), renderTexture, kSize / 2, 0, kSize / 2, kSize,
                            0, 0);
}

// This tests the clearing of sampled textures during compute pass
TEST_P(TextureZeroInitTest, ComputePassSampledTextureClear) {
    dawn::TextureDescriptor descriptor = CreateTextureDescriptor(
        1, 1,
        dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::Sampled |
            dawn::TextureUsageBit::TransferSrc,
        kColorFormat);
    dawn::Texture texture = device.CreateTexture(&descriptor);

    dawn::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
        device, {{0, dawn::ShaderStageBit::Fragment, dawn::BindingType::Sampler},
                 {1, dawn::ShaderStageBit::Compute | dawn::ShaderStageBit::Vertex,
                  dawn::BindingType::SampledTexture}});

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();

    dawn::ComputePassEncoder pass = encoder.BeginComputePass();
    dawn::ComputePipelineDescriptor computePipelineDescriptor;
    computePipelineDescriptor.layout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout);

    dawn::PipelineStageDescriptor computeStage;
    computeStage.module = utils::CreateShaderModule(device, dawn::ShaderStage::Compute,
                                                    R"( #version 450
                                                    void main() { })");
    computeStage.entryPoint = "main";
    computePipelineDescriptor.computeStage = &computeStage;

    pass.SetPipeline(device.CreateComputePipeline(&computePipelineDescriptor));

    dawn::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    dawn::Sampler sampler = device.CreateSampler(&samplerDesc);

    dawn::BindGroup bindGroup = utils::MakeBindGroup(
        device, bindGroupLayout, {{0, sampler}, {1, texture.CreateDefaultView()}});
    pass.SetBindGroup(0, bindGroup, 0, nullptr);
    pass.Dispatch(1, 1, 1);
    pass.EndPass();

    // Copy texture to buffer to read it back and check it's cleared
    dawn::TextureCopyView textureCopyView = utils::CreateTextureCopyView(texture, 0, 0, {0, 0, 0});
    dawn::BufferDescriptor bufferDescriptor;
    bufferDescriptor.usage = dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::TransferSrc;
    bufferDescriptor.size = kSize * kSize * sizeof(uint32_t);
    dawn::Buffer buffer = device.CreateBuffer(&bufferDescriptor);
    dawn::BufferCopyView bufferCopyView =
        utils::CreateBufferCopyView(buffer, 0, sizeof(uint32_t) * kSize, 0);
    dawn::Extent3D copySize = {kSize, kSize, 1};
    encoder.CopyTextureToBuffer(&textureCopyView, &bufferCopyView, &copySize);

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::vector<uint32_t> expected(kSize * kSize * sizeof(uint32_t), 0);
    EXPECT_BUFFER_U32_RANGE_EQ(expected.data(), buffer, 0, kSize * sizeof(uint32_t));
}

DAWN_INSTANTIATE_TEST(
    TextureZeroInitTest,
    ForceWorkarounds(D3D12Backend, {"nonzero_clear_resources_on_creation_for_testing"}),
    ForceWorkarounds(OpenGLBackend, {"nonzero_clear_resources_on_creation_for_testing"}),
    ForceWorkarounds(VulkanBackend, {"nonzero_clear_resources_on_creation_for_testing"}));
