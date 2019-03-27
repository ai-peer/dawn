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

#include "common/Assert.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

class MultisampledRenderingTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        InitTexturesForTest();
    }

    void InitTexturesForTest() {
        mMultisampledColorView =
            CreateTextureForOutputAttachment(kColorFormat, kSampleCount).CreateDefaultTextureView();
        mMultisampledDepthStencilView =
            CreateTextureForOutputAttachment(kDepthStencilFormat, kSampleCount)
            .CreateDefaultTextureView();
        mResolveTexture = CreateTextureForOutputAttachment(kColorFormat, 1);
        mResolveView = mResolveTexture.CreateDefaultTextureView();
    }

    dawn::RenderPipeline CreateRenderPipelineWithOneOutputForTest() {
        const char* kFsOneOutput =
            R"(#version 450
            layout(location = 0) out vec4 fragColor;
            layout (std140, set = 0, binding = 0) uniform uBuffer {
                vec4 color;
                float depth;
            };
            void main() {
                fragColor = color;
                gl_FragDepth = depth;
            })";

        return CreateRenderPipelineForTest(kFsOneOutput, 1);
    }

    dawn::RenderPipeline CreateRenderPipelineWithTwoOutputsForTest() {
        const char* kFsTwoOutputs =
            R"(#version 450
            layout(location = 0) out vec4 fragColor1;
            layout(location = 1) out vec4 fragColor2;
            layout (std140, set = 0, binding = 0) uniform uBuffer {
                vec4 color1;
                vec4 color2;
            };
            void main() {
                fragColor1 = color1;
                fragColor2 = color2;
            })";

        return CreateRenderPipelineForTest(kFsTwoOutputs, 2);
    }

    dawn::Texture CreateTextureForOutputAttachment(dawn::TextureFormat format,
                                                   uint32_t sampleCount,
                                                   uint32_t mipLevelCount = 1,
                                                   uint32_t arrayLayerCount = 1) {
        constexpr uint32_t kWidth = 3;
        constexpr uint32_t kHeight = 3;

        dawn::TextureDescriptor descriptor;
        descriptor.dimension = dawn::TextureDimension::e2D;
        descriptor.size.width = kWidth << (mipLevelCount - 1);
        descriptor.size.height = kHeight << (mipLevelCount - 1);
        descriptor.size.depth = 1;
        descriptor.arrayLayerCount = arrayLayerCount;
        descriptor.sampleCount = sampleCount;
        descriptor.format = format;
        descriptor.mipLevelCount = mipLevelCount;
        descriptor.usage =
            dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
        return device.CreateTexture(&descriptor);
    }

    void ExecuteRenderPassForTest(dawn::CommandEncoder commandEncoder,
                                  const dawn::RenderPassDescriptor& renderPass,
                                  const dawn::RenderPipeline& pipeline,
                                  const std::array<float, 8>& uniformData) {
        dawn::Buffer uniformBuffer =
            utils::CreateBufferFromData(device, uniformData.data(), sizeof(uniformData),
                                        dawn::BufferUsageBit::Uniform);
        dawn::BindGroup bindGroup =
            utils::MakeBindGroup(device, mBindGroupLayout,
                                 {{0, uniformBuffer, 0, sizeof(uniformData)}});

        dawn::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);
        renderPassEncoder.SetPipeline(pipeline);
        renderPassEncoder.SetBindGroup(0, bindGroup, 0, nullptr);
        renderPassEncoder.Draw(3, 1, 0, 0);
        renderPassEncoder.EndPass();
    }

    utils::ComboRenderPassDescriptor CreateComboRenderPassDescriptorForTest(
        std::initializer_list<dawn::TextureView> colorViews,
        std::vector<dawn::TextureView> resolveTargetViews,
        dawn::LoadOp colorLoadOp,
        dawn::LoadOp depthStencilLoadOp) {
        ASSERT(colorViews.size() == resolveTargetViews.size());

        utils::ComboRenderPassDescriptor renderPass(colorViews, mMultisampledDepthStencilView);
        for (uint32_t i = 0; i < colorViews.size(); ++i) {
            renderPass.cColorAttachmentsInfoPtr[i]->loadOp = colorLoadOp;
            renderPass.cColorAttachmentsInfoPtr[i]->clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
            renderPass.cColorAttachmentsInfoPtr[i]->resolveTarget = resolveTargetViews[i];
        }

        renderPass.cDepthStencilAttachmentInfo.clearDepth = 1.0f;
        renderPass.cDepthStencilAttachmentInfo.depthLoadOp = depthStencilLoadOp;

        return renderPass;
    }

    constexpr static uint32_t kSampleCount = 4;
    constexpr static dawn::TextureFormat kColorFormat = dawn::TextureFormat::R8G8B8A8Unorm;
    constexpr static dawn::TextureFormat kDepthStencilFormat = dawn::TextureFormat::D32FloatS8Uint;

    dawn::TextureView mMultisampledColorView;
    dawn::TextureView mMultisampledDepthStencilView;
    dawn::Texture mResolveTexture;
    dawn::TextureView mResolveView;
    dawn::BindGroupLayout mBindGroupLayout;

  private:
    // Create a rendering pipeline for drawing a dawn-right triangle.
    dawn::RenderPipeline CreateRenderPipelineForTest(const char* fs, uint32_t numColorAttachments) {
        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);

        const char* vs =
            R"(#version 450
            const vec2 pos[3] = vec2[3](vec2(-1.f, 1.f), vec2(1.f, 1.f), vec2(1.f, -1.f));
            void main() {
                gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
            })";
        pipelineDescriptor.cVertexStage.module =
            utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, vs);

        pipelineDescriptor.cFragmentStage.module =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, fs);


        mBindGroupLayout = utils::MakeBindGroupLayout(
            device, {
                {0, dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer},
            });
        dawn::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &mBindGroupLayout);
        pipelineDescriptor.layout = pipelineLayout;

        pipelineDescriptor.cDepthStencilState.format = kDepthStencilFormat;
        pipelineDescriptor.cDepthStencilState.depthWriteEnabled = true;
        pipelineDescriptor.cDepthStencilState.depthCompare = dawn::CompareFunction::Less;
        pipelineDescriptor.depthStencilState = &(pipelineDescriptor.cDepthStencilState);

        pipelineDescriptor.sampleCount = kSampleCount;

        pipelineDescriptor.colorStateCount = numColorAttachments;
        for (uint32_t i = 0; i < numColorAttachments; ++i) {
            pipelineDescriptor.cColorStates[i]->format = kColorFormat;
        }

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }
};

// Test using one multisampled color attachment with resolve target can render correctly.
TEST_P(MultisampledRenderingTest, OneMultisampledColorAttachmentWithResolveTarget) {
    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPipeline pipeline = CreateRenderPipelineWithOneOutputForTest();

    // In first render pass we draw a green triangle with depth value == 0.2f.
    {
        utils::ComboRenderPassDescriptor renderPass = CreateComboRenderPassDescriptorForTest(
            {mMultisampledColorView}, {mResolveView}, dawn::LoadOp::Clear, dawn::LoadOp::Clear);
        std::array<float, 8> kData = {0.0f, 0.8f, 0.0f, 1.0f, // color
                                      0.2f,                   // depth
                                      0.0f, 0.0f, 0.0f};
        ExecuteRenderPassForTest(commandEncoder, renderPass, pipeline, kData);
    }

    // In second render pass we draw a red triangle with depth value == 0.5f.
    // This red triangle should not be displayed becuase it is behind the green one that is drawn in
    // the last render pass.
    {
        utils::ComboRenderPassDescriptor renderPass = CreateComboRenderPassDescriptorForTest(
            {mMultisampledColorView}, {mResolveView}, dawn::LoadOp::Load, dawn::LoadOp::Load);

        std::array<float, 8> kUniformData = {0.8f, 0.0f, 0.0f, 1.0f, // color
                                             0.5f,                   // depth
                                             0.0f, 0.0f, 0.0f};
        ExecuteRenderPassForTest(commandEncoder, renderPass, pipeline, kUniformData);
    }

    dawn::CommandBuffer commandBuffer = commandEncoder.Finish();
    dawn::Queue queue = device.CreateQueue();
    queue.Submit(1, &commandBuffer);

    // The color of the pixel in the middle of mResolveTexture should be (0, 102, 0, 255) if MSAA
    // resolve runs correctly.
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 102, 0, 255), mResolveTexture, 1, 1);
}

// Test rendering into a multisampled color attachment and doing MSAA resolve in another render pass
// works correctly.
TEST_P(MultisampledRenderingTest, ResolveInAnotherRenderPass) {
    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPipeline pipeline = CreateRenderPipelineWithOneOutputForTest();

    // In first render pass we draw a green triangle and do not set the resolve target.
    {
        utils::ComboRenderPassDescriptor renderPass = CreateComboRenderPassDescriptorForTest(
            {mMultisampledColorView}, {nullptr}, dawn::LoadOp::Clear, dawn::LoadOp::Clear);

        std::array<float, 8> kUniformData = {0.0f, 0.8f, 0.0f, 1.0f, // color
                                             0.2f,                   // depth
                                             0.0f, 0.0f, 0.0f};
        ExecuteRenderPassForTest(commandEncoder, renderPass, pipeline, kUniformData);
    }

    // In second render pass we ony do MSAA resolve with no draw call.
    {
        utils::ComboRenderPassDescriptor renderPass = CreateComboRenderPassDescriptorForTest(
            {mMultisampledColorView}, {mResolveView}, dawn::LoadOp::Load, dawn::LoadOp::Load);

        dawn::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);
        renderPassEncoder.EndPass();
    }

    dawn::CommandBuffer commandBuffer = commandEncoder.Finish();
    dawn::Queue queue = device.CreateQueue();
    queue.Submit(1, &commandBuffer);

    // The color of the pixel in the middle of mResolveTexture should be (0, 102, 0, 255) if MSAA
    // resolve runs correctly.
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 102, 0, 255), mResolveTexture, 1, 1);
}

// Test doing MSAA resolve into multiple resolve targets works correctly.
TEST_P(MultisampledRenderingTest, ResolveIntoMultipleResolveTargets) {
    dawn::TextureView multisampledColorView2 =
        CreateTextureForOutputAttachment(kColorFormat, kSampleCount).CreateDefaultTextureView();
    dawn::Texture resolveTexture2 = CreateTextureForOutputAttachment(kColorFormat, 1);
    dawn::TextureView resolveView2 = resolveTexture2.CreateDefaultTextureView();

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPipeline pipeline = CreateRenderPipelineWithTwoOutputsForTest();

    // Draw a red triangle to the first color attachment, and a blue triangle to the second color
    // attachment, and do MSAA resolve on two render targets in one render pass.
    {
        utils::ComboRenderPassDescriptor renderPass = CreateComboRenderPassDescriptorForTest(
            {mMultisampledColorView, multisampledColorView2}, {mResolveView, resolveView2},
            dawn::LoadOp::Clear, dawn::LoadOp::Clear);

        std::array<float, 8> kUniformData = {0.8f, 0.0f, 0.0f, 1.0f,  // color1
                                             0.0f, 0.0f, 0.8f, 1.0f}; // color2

        ExecuteRenderPassForTest(commandEncoder, renderPass, pipeline, kUniformData);
    }

    dawn::CommandBuffer commandBuffer = commandEncoder.Finish();
    dawn::Queue queue = device.CreateQueue();
    queue.Submit(1, &commandBuffer);

    // If MSAA resolve runs correctly:
    // - The color of the pixel in the mi ddle of mResolveTexture should be (102, 0, 0, 255).
    // - The color of the pixel in the middle of resolveTexture2 should be (0, 0, 102, 255).
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(102, 0, 0, 255), mResolveTexture, 1, 1);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 0, 102, 255), resolveTexture2, 1, 1);
}

// Test using a level or a layer of a 2D array texture as resolve target works correctly.
TEST_P(MultisampledRenderingTest, ResolveInto2DArrayTexture) {
    dawn::TextureView multisampledColorView2 =
        CreateTextureForOutputAttachment(kColorFormat, kSampleCount).CreateDefaultTextureView();

    dawn::TextureViewDescriptor baseTextureViewDescriptor;
    baseTextureViewDescriptor.dimension = dawn::TextureViewDimension::e2D;
    baseTextureViewDescriptor.format = kColorFormat;
    baseTextureViewDescriptor.arrayLayerCount = 1;
    baseTextureViewDescriptor.mipLevelCount = 1;

    constexpr uint32_t kBaseArrayLayer1 = 0;
    constexpr uint32_t kBaseMipLevel1 = 1;
    dawn::Texture resolveTexture1 =
        CreateTextureForOutputAttachment(kColorFormat, 1, kBaseMipLevel1 + 1, 2);
    dawn::TextureViewDescriptor resolveViewDescriptor1 = baseTextureViewDescriptor;
    resolveViewDescriptor1.baseArrayLayer = kBaseArrayLayer1;
    resolveViewDescriptor1.baseMipLevel = kBaseMipLevel1;
    dawn::TextureView resolveView1 = resolveTexture1.CreateTextureView(&resolveViewDescriptor1);

    constexpr uint32_t kBaseArrayLayer2 = 5;
    constexpr uint32_t kBaseMipLevel2 = 3;
    dawn::Texture resolveTexture2 =
        CreateTextureForOutputAttachment(kColorFormat, 1, kBaseMipLevel2 + 1, 6);
    dawn::TextureViewDescriptor resolveViewDescriptor2 = baseTextureViewDescriptor;
    resolveViewDescriptor2.baseArrayLayer = kBaseArrayLayer2;
    resolveViewDescriptor2.baseMipLevel = kBaseMipLevel2;
    dawn::TextureView resolveView2 = resolveTexture2.CreateTextureView(&resolveViewDescriptor2);

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPipeline pipeline = CreateRenderPipelineWithTwoOutputsForTest();

    // Draw a red triangle to the first color attachment, and a blue triangle to the second color
    // attachment, and do MSAA resolve on two render targets in one render pass.
    {
        utils::ComboRenderPassDescriptor renderPass = CreateComboRenderPassDescriptorForTest(
            {mMultisampledColorView, multisampledColorView2}, {resolveView1, resolveView2},
            dawn::LoadOp::Clear, dawn::LoadOp::Clear);

        std::array<float, 8> kUniformData = {0.8f, 0.0f, 0.0f, 1.0f,  // color1
                                             0.0f, 0.0f, 0.8f, 1.0f}; // color2

        ExecuteRenderPassForTest(commandEncoder, renderPass, pipeline, kUniformData);
    }

    dawn::CommandBuffer commandBuffer = commandEncoder.Finish();
    dawn::Queue queue = device.CreateQueue();
    queue.Submit(1, &commandBuffer);

    // If MSAA resolve runs correctly:
    // - The color of the pixel in the middle of resolveTexture1 should be (102, 0, 0, 255).
    // - The color of the pixel in the middle of resolveTexture2 should be (0, 0, 102, 255).
    std::vector<RGBA8> expected1(1, RGBA8(102, 0, 0, 255));
    std::vector<RGBA8> expected2(1, RGBA8(0, 0, 102, 255));
    EXPECT_TEXTURE_RGBA8_EQ(
        expected1.data(), resolveTexture1, 1, 1, 1, 1, kBaseMipLevel1, kBaseArrayLayer1);
    EXPECT_TEXTURE_RGBA8_EQ(
        expected2.data(), resolveTexture2, 1, 1, 1, 1, kBaseMipLevel2, kBaseArrayLayer2);
}

// TODO(jiawei.shao@intel.com): enable multisampled rendering on all Dawn backends.
DAWN_INSTANTIATE_TEST(MultisampledRenderingTest, OpenGLBackend);
