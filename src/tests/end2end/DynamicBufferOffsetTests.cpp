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

constexpr uint32_t kRTSize = 400;
constexpr static dawn::TextureFormat kColorFormat = dawn::TextureFormat::R8G8B8A8Uint;

class DynamicBufferOffsetTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
    }

    dawn::BindGroupLayout mBindGroupLayout;

    dawn::RenderPipeline MakeTestPipeline() {
        dawn::ShaderModule vsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
                #version 450
                layout(std140, set = 0, binding = 0) uniform uBuffer1 {
                    uvec2 value1;
                };
                layout(std140, set = 0, binding = 1) uniform uBuffer2 {
                    uvec2 value2;
                };
                layout(location = 0) out uvec4 color;
                void main() {
                    const vec2 pos[3] = vec2[3](vec2(-1.0f, 0.0f), vec2(-1.0f, -1.0f), vec2(0.0f, -1.0f));
                    color = uvec4(value1.x, value1.y, value2.x, value2.y);
                    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
                })");

        dawn::ShaderModule fsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
                 #version 450
                 layout(location = 0) flat in uvec4 color;
                 layout(location = 0) out uvec4 fragColor;
                 void main() {
                     fragColor = color;
                 })");

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.cVertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        mBindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, dawn::ShaderStageBit::Vertex, dawn::BindingType::DynamicUniformBuffer},
                     {1, dawn::ShaderStageBit::Vertex, dawn::BindingType::DynamicUniformBuffer}});
        dawn::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &mBindGroupLayout);
        pipelineDescriptor.layout = pipelineLayout;
        pipelineDescriptor.cColorStates[0]->format = kColorFormat;

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }
};

// Make sure that calling a marker API without a debugging tool attached doesn't cause a failure.
TEST_P(DynamicBufferOffsetTests, Basic) {
    dawn::RenderPipeline pipeline = MakeTestPipeline();

    dawn::TextureDescriptor textureDescriptor;
    textureDescriptor.dimension = dawn::TextureDimension::e2D;
    textureDescriptor.size.width = kRTSize;
    textureDescriptor.size.height = kRTSize;
    textureDescriptor.size.depth = 1;
    textureDescriptor.arrayLayerCount = 1;
    textureDescriptor.sampleCount = 1;
    textureDescriptor.format = kColorFormat;
    textureDescriptor.mipLevelCount = 1;
    textureDescriptor.usage =
        dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
    dawn::Texture color = device.CreateTexture(&textureDescriptor);

    utils::BasicRenderPass renderPass =
        utils::BasicRenderPass(kRTSize, kRTSize, color, kColorFormat);
    std::array<uint32_t, 68> uniformData = {1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // paddings
                                            5, 6};
    std::array<uint32_t, 68> uniformData2 = {3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // paddings
                                             7, 8};

    constexpr uint32_t uniformDataSize = sizeof(uniformData);

    dawn::Buffer uniformBuffer1 = utils::CreateBufferFromData(
        device, uniformData.data(), uniformDataSize, dawn::BufferUsageBit::Uniform);
    dawn::Buffer uniformBuffer2 = utils::CreateBufferFromData(
        device, uniformData2.data(), uniformDataSize, dawn::BufferUsageBit::Uniform);
    dawn::BindGroup bindGroup = utils::MakeBindGroup(
        device, mBindGroupLayout,
        {{0, uniformBuffer1, 0, uniformDataSize}, {1, uniformBuffer2, 0, uniformDataSize}});

    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder renderPassEncoder =
            commandEncoder.BeginRenderPass(&renderPass.renderPassInfo);
        renderPassEncoder.SetPipeline(pipeline);
        renderPassEncoder.SetBindGroup(0, bindGroup, 0, nullptr);
        renderPassEncoder.Draw(3, 1, 0, 0);
        renderPassEncoder.EndPass();
        dawn::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);
        EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 3, 4), renderPass.color, 0, 0);
    }
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        std::array<uint64_t, 2> offsets = {0, 0};
        dawn::RenderPassEncoder renderPassEncoder =
            commandEncoder.BeginRenderPass(&renderPass.renderPassInfo);
        renderPassEncoder.SetPipeline(pipeline);
        renderPassEncoder.SetBindGroup(0, bindGroup, 2, offsets.data());
        renderPassEncoder.Draw(3, 1, 0, 0);
        renderPassEncoder.EndPass();
        dawn::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);
        EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 3, 4), renderPass.color, 0, 0);
    }
    {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        std::array<uint64_t, 2> offsets = {256, 256};
        dawn::RenderPassEncoder renderPassEncoder =
            commandEncoder.BeginRenderPass(&renderPass.renderPassInfo);
        renderPassEncoder.SetPipeline(pipeline);
        renderPassEncoder.SetBindGroup(0, bindGroup, 2, offsets.data());
        renderPassEncoder.Draw(3, 1, 0, 0);
        renderPassEncoder.EndPass();
        dawn::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);
        EXPECT_PIXEL_RGBA8_EQ(RGBA8(5, 6, 7, 8), renderPass.color, 0, 0);
    }
}

DAWN_INSTANTIATE_TEST(DynamicBufferOffsetTests, MetalBackend, VulkanBackend);
