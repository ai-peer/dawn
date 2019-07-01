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

class ClipSpaceTest : public DawnTest {
  protected:
    dawn::RenderPipeline CreatePipelineForTest() {
        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);

        // Draws two points. Assuming a 2x2 render target they are the following:
        //   - A green point at clipspace coordinate -0.5, -0.5 which should be in texel 0, 0
        //   - A red point at clipspace coordinate 0.5, 0.5 which should be in texel 1, 1
        pipelineDescriptor.cVertexStage.module =
            utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(#version 450
                const vec2 pos[2] = vec2[2](
                    vec2(-0.5f, -0.5f),
                    vec2(0.5f, 0.5f)
                );
                const vec4 color[2] = vec4[2](
                    vec4(0.0f, 1.0f, 0.0f, 1.0f),
                    vec4(1.0f, 0.0f, 0.0f, 1.0f)
                );
                layout(location = 0) out vec4 pointColor;
                void main() {
                    gl_Position = vec4(pos[gl_VertexIndex], 0.0f, 1.0f);
                    pointColor = color[gl_VertexIndex];
                })");

        pipelineDescriptor.cFragmentStage.module =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(#version 450
            layout(location = 0) in vec4 pointColor;
            layout(location = 0) out vec4 fragColor;
            void main() {
               fragColor = pointColor;
            })");

        pipelineDescriptor.primitiveTopology = dawn::PrimitiveTopology::PointList;
        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    dawn::Texture Create2DTextureForTest(dawn::TextureFormat format) {
        dawn::TextureDescriptor textureDescriptor;
        textureDescriptor.dimension = dawn::TextureDimension::e2D;
        textureDescriptor.format = format;
        textureDescriptor.usage =
            dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
        textureDescriptor.arrayLayerCount = 1;
        textureDescriptor.mipLevelCount = 1;
        textureDescriptor.sampleCount = 1;
        textureDescriptor.size = {kSize, kSize, 1};
        return device.CreateTexture(&textureDescriptor);
    }

    static constexpr uint32_t kSize = 2;
};

// Test that the clip space is correctly configured.
TEST_P(ClipSpaceTest, ClipSpace) {
    dawn::Texture colorTexture = Create2DTextureForTest(dawn::TextureFormat::RGBA8Unorm);

    utils::ComboRenderPassDescriptor renderPassDescriptor(
        {colorTexture.CreateDefaultView()});
    renderPassDescriptor.cColorAttachmentsInfoPtr[0]->clearColor = {0.0, 0.0, 0.0, 0.0};
    renderPassDescriptor.cColorAttachmentsInfoPtr[0]->loadOp = dawn::LoadOp::Clear;

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::RenderPassEncoder renderPass = commandEncoder.BeginRenderPass(&renderPassDescriptor);
    renderPass.SetPipeline(CreatePipelineForTest());
    renderPass.Draw(2, 1, 0, 0);
    renderPass.EndPass();
    dawn::CommandBuffer commandBuffer = commandEncoder.Finish();
    dawn::Queue queue = device.CreateQueue();
    queue.Submit(1, &commandBuffer);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8(255, 0, 0, 255), colorTexture, kSize - 1, kSize - 1);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(0, 255, 0, 255), colorTexture, 0, 0);
}

DAWN_INSTANTIATE_TEST(ClipSpaceTest, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend);
