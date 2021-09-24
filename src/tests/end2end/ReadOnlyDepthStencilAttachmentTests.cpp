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

#include "tests/DawnTest.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr static uint32_t kSize = 4;
constexpr static wgpu::TextureFormat kFormat = wgpu::TextureFormat::Depth32Float;

class ReadOnlyDepthStencilAttachmentTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
    }

    wgpu::RenderPipeline CreateRenderPipeline() {
        utils::ComboRenderPipelineDescriptor pipelineDescriptor;

        // Draw a rectangle via two triangles. The depth value of the top of the rectangle is 1.0.
        // The depth value of the bottom is 0.0. The depth value gradually change from 1.0 to 0.0
        // from the top to the bottom.
        pipelineDescriptor.vertex.module = utils::CreateShaderModule(device, R"(
            [[stage(vertex)]]
            fn main([[builtin(vertex_index)]] VertexIndex : u32) -> [[builtin(position)]] vec4<f32> {
                var pos = array<vec3<f32>, 6>(
                    vec3<f32>(-1.0,  1.0, 1.0),
                    vec3<f32>(-1.0, -1.0, 0.0),
                    vec3<f32>( 1.0,  1.0, 1.0),
                    vec3<f32>( 1.0,  1.0, 1.0),
                    vec3<f32>(-1.0, -1.0, 0.0),
                    vec3<f32>( 1.0, -1.0, 0.0));
                return vec4<f32>(pos[VertexIndex], 1.0);
            })");

        pipelineDescriptor.cFragment.module = utils::CreateShaderModule(device, R"(
        [[group(0), binding(0)]] var samp : sampler;
        [[group(0), binding(1)]] var tex : texture_depth_2d;

        [[stage(fragment)]]
        fn main([[builtin(position)]] FragCoord : vec4<f32>) -> [[location(0)]] vec4<f32> {
            return vec4<f32>(textureSample(tex, samp, FragCoord.xy), 0.0, 0.0, 0.0);
        })");

        wgpu::DepthStencilState* depthStencil = pipelineDescriptor.EnableDepthStencil(kFormat);
        depthStencil->depthCompare = wgpu::CompareFunction::LessEqual;

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    wgpu::Texture CreateTexture(wgpu::TextureFormat format, wgpu::TextureUsage usage) {
        wgpu::TextureDescriptor descriptor = {};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {kSize, kSize, 1};
        descriptor.format = format;
        descriptor.usage = usage;
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        return device.CreateTexture(&descriptor);
    }

    void clearDepthStencilTexture(wgpu::TextureView view, float clearDepth) {
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();

        utils::ComboRenderPassDescriptor passDescriptor({}, view);
        passDescriptor.cDepthStencilAttachmentInfo.clearDepth = clearDepth;

        wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
        pass.EndPass();

        wgpu::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);
    }

    void DoTest(wgpu::Texture colorTexture) {
        wgpu::Texture depthStencilTexture = CreateTexture(
            kFormat, wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding);
        wgpu::TextureViewDescriptor viewDesc = {};
        viewDesc.aspect = wgpu::TextureAspect::DepthOnly;
        wgpu::TextureView depthStencilView = depthStencilTexture.CreateView(&viewDesc);
        clearDepthStencilTexture(depthStencilView, 0.5);

        wgpu::SamplerDescriptor samplerDescriptor = {};
        samplerDescriptor.minFilter = wgpu::FilterMode::Nearest;
        samplerDescriptor.magFilter = wgpu::FilterMode::Nearest;
        samplerDescriptor.mipmapFilter = wgpu::FilterMode::Nearest;
        samplerDescriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
        samplerDescriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
        samplerDescriptor.addressModeW = wgpu::AddressMode::ClampToEdge;

        wgpu::Sampler sampler = device.CreateSampler(&samplerDescriptor);

        wgpu::RenderPipeline pipeline = CreateRenderPipeline();
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                         {{0, sampler}, {1, depthStencilView}});
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();

        {
            utils::ComboRenderPassDescriptor passDescriptor({colorTexture.CreateView()},
                                                            depthStencilView);
            passDescriptor.cDepthStencilAttachmentInfo.depthReadOnly = true;
            passDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Load;
            passDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;

            wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.Draw(6);
            pass.EndPass();

            wgpu::CommandBuffer commands = commandEncoder.Finish();
            queue.Submit(1, &commands);
        }
    }
};

TEST_P(ReadOnlyDepthStencilAttachmentTests, Depth) {
    wgpu::Texture colorTexture =
        CreateTexture(wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);

    DoTest(colorTexture);

    const std::vector<RGBA8> kExpectedTopColors(kSize * kSize / 2, {0, 0, 0, 0});
    const std::vector<RGBA8> kExpectedBottomColors(kSize * kSize / 2, {128, 0, 0, 0});
    EXPECT_TEXTURE_EQ(kExpectedTopColors.data(), colorTexture, {0, 0}, {kSize, kSize / 2});
    EXPECT_TEXTURE_EQ(kExpectedBottomColors.data(), colorTexture, {0, kSize / 2},
                      {kSize, kSize / 2});
}

DAWN_INSTANTIATE_TEST(ReadOnlyDepthStencilAttachmentTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
