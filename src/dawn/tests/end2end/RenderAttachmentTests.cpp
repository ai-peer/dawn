// Copyright 2021 The Dawn Authors
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

#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

class RenderAttachmentTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        vsModule = utils::CreateShaderModule(device, R"(
        @stage(vertex)
        fn main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 1.0);
        })");
    }

    wgpu::ShaderModule vsModule;
};

// Test that it is ok to have more fragment outputs than color attachments.
// There should be no backend validation errors or indexing out-of-bounds.
TEST_P(RenderAttachmentTest, MoreFragmentOutputsThanAttachments) {
    wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
        struct Output {
            @location(0) color0 : vec4<f32>,
            @location(1) color1 : vec4<f32>,
            @location(2) color2 : vec4<f32>,
            @location(3) color3 : vec4<f32>,
        }

        @stage(fragment)
        fn main() -> Output {
            var output : Output;
            output.color0 = vec4<f32>(1.0, 0.0, 0.0, 1.0);
            output.color1 = vec4<f32>(0.0, 1.0, 0.0, 1.0);
            output.color2 = vec4<f32>(0.0, 0.0, 1.0, 1.0);
            output.color3 = vec4<f32>(1.0, 1.0, 0.0, 1.0);
            return output;
        })");

    // Fragment outputs 1, 2, 3 are written in the shader, but unused by the pipeline.
    utils::ComboRenderPipelineDescriptor pipelineDesc;
    pipelineDesc.vertex.module = vsModule;
    pipelineDesc.cFragment.module = fsModule;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::PointList;
    pipelineDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
    pipelineDesc.cFragment.targetCount = 1;

    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    wgpu::TextureDescriptor textureDesc;
    textureDesc.size = {1, 1, 1};
    textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;

    wgpu::Texture renderTarget = device.CreateTexture(&textureDesc);
    utils::ComboRenderPassDescriptor renderPass({renderTarget.CreateView()});
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
    pass.SetPipeline(pipeline);
    pass.Draw(1);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8::kRed, renderTarget, 0, 0);
}

// Test that it is ok to have the following attachments unwritten by setting write masks
//  - empty attachment
//  - attachment format unmatched with fragment shader output
//  - attachment with fewer components than that of the fragment shader output
// There should be no backend validation errors or indexing out-of-bounds.
TEST_P(RenderAttachmentTest, UnwrittenByWriteMask) {
    wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
        struct Output {
            @location(0) color0 : vec4<f32>,
            // location(1) is left empty
            @location(2) color2 : vec4<i32>,
            @location(3) color3 : vec4<u32>,
        }

        @stage(fragment)
        fn main() -> Output {
            var output : Output;
            output.color0 = vec4<f32>(1.0, 0.0, 0.0, 1.0);
            output.color2 = vec4<i32>(12, 34, 56, 78);
            output.color3 = vec4<u32>(255u, 127u, 63u, 1u);
            return output;
        })");

    // Fragment outputs 1, 2, 3 are written in the shader, but unused by the pipeline.
    utils::ComboRenderPipelineDescriptor pipelineDesc;
    pipelineDesc.vertex.module = vsModule;
    pipelineDesc.cFragment.module = fsModule;
    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::PointList;
    // format matches shader module output
    pipelineDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
    pipelineDesc.cTargets[0].writeMask = wgpu::ColorWriteMask::All;
    // Empty (sparse) attachment, unwritten
    pipelineDesc.cTargets[1].format = wgpu::TextureFormat::Undefined;
    pipelineDesc.cTargets[1].writeMask = wgpu::ColorWriteMask::None;
    // format not aligned with that of the shader module, unwritten
    pipelineDesc.cTargets[2].format = wgpu::TextureFormat::RGBA8Unorm;
    pipelineDesc.cTargets[2].writeMask = wgpu::ColorWriteMask::None;
    // format has fewer components than output
    pipelineDesc.cTargets[3].format = wgpu::TextureFormat::R32Uint;
    pipelineDesc.cTargets[3].writeMask = wgpu::ColorWriteMask::All;
    pipelineDesc.cFragment.targetCount = 4;

    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDesc);
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    wgpu::TextureDescriptor textureDesc0;
    textureDesc0.size = {1, 1, 1};
    textureDesc0.format = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc0.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture renderTarget0 = device.CreateTexture(&textureDesc0);

    wgpu::TextureDescriptor textureDesc2;
    textureDesc2.size = {1, 1, 1};
    textureDesc2.format = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc2.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture renderTarget2 = device.CreateTexture(&textureDesc2);

    wgpu::TextureDescriptor textureDesc3;
    textureDesc3.size = {1, 1, 1};
    textureDesc3.format = wgpu::TextureFormat::R32Uint;
    textureDesc3.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture renderTarget3 = device.CreateTexture(&textureDesc3);

    // Readback buffer for renderTarget3
    wgpu::BufferDescriptor readbackBufferDesc;
    readbackBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    readbackBufferDesc.size = 4;
    wgpu::Buffer readbackBuffer = device.CreateBuffer(&readbackBufferDesc);

    utils::ComboRenderPassDescriptor renderPass(
        {renderTarget0.CreateView(), {}, renderTarget2.CreateView(), renderTarget3.CreateView()});
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
    pass.SetPipeline(pipeline);
    pass.Draw(1);
    pass.End();
    {
        wgpu::ImageCopyBuffer bufferView = utils::CreateImageCopyBuffer(readbackBuffer, 0, 256);
        wgpu::ImageCopyTexture textureView =
            utils::CreateImageCopyTexture(renderTarget3, 0, {0, 0, 0});
        wgpu::Extent3D extent{1, 1, 1};
        encoder.CopyTextureToBuffer(&textureView, &bufferView, &extent);
    }
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    uint32_t expectedRenderData[] = {255};

    EXPECT_PIXEL_RGBA8_EQ(RGBA8::kRed, renderTarget0, 0, 0);
    EXPECT_BUFFER_U32_RANGE_EQ(expectedRenderData, readbackBuffer, 0,
                               readbackBufferDesc.size / sizeof(uint32_t));
}

DAWN_INSTANTIATE_TEST(RenderAttachmentTest,
                      D3D12Backend(),
                      D3D12Backend({}, {"use_d3d12_render_pass"}),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
