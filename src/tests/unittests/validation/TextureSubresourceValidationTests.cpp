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

// #include "tests/DawnTest.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

#include "tests/unittests/validation/ValidationTest.h"

namespace {

class TextureSubresourceValidationTest : public ValidationTest {
};
    
constexpr uint32_t kSize = 32u;
constexpr static wgpu::TextureFormat kColorFormat = wgpu::TextureFormat::RGBA8Unorm;

wgpu::TextureDescriptor CreateTextureDescriptor(uint32_t mipLevelCount,
                                                    uint32_t arrayLayerCount,
                                                    wgpu::TextureUsage usage,
                                                    wgpu::TextureFormat format) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
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

wgpu::TextureViewDescriptor CreateTextureViewDescriptor(uint32_t baseMipLevel,
                                                            uint32_t baseArrayLayer) {
        wgpu::TextureViewDescriptor descriptor;
        descriptor.format = kColorFormat;
        descriptor.baseArrayLayer = baseArrayLayer;
        descriptor.arrayLayerCount = 1;
        descriptor.baseMipLevel = baseMipLevel;
        descriptor.mipLevelCount = 1;
        descriptor.dimension = wgpu::TextureViewDimension::e2D;
        return descriptor;
    }

wgpu::ShaderModule CreateBasicVertexShaderForTest(wgpu::Device& device) {
        return utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(#version 450
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

wgpu::ShaderModule CreateSampledTextureFragmentShaderForTest(wgpu::Device &device) {
        return utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment,
                                         R"(#version 450
            layout(set = 0, binding = 0) uniform sampler sampler0;
            layout(set = 0, binding = 1) uniform texture2D texture0;
            layout(location = 0) out vec4 fragColor;
            void main() {
                fragColor = texelFetch(sampler2D(texture0, sampler0), ivec2(gl_FragCoord), 0);
            })");
    }

TEST_F(TextureSubresourceValidationTest, MipmapLevelsTest) {
    // Create needed resources
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(2, 1, wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    wgpu::TextureViewDescriptor samplerTextureView =
        CreateTextureViewDescriptor(0, 0);
    wgpu::TextureViewDescriptor renderTextureView =
        CreateTextureViewDescriptor(1, 0);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    // Create render pipeline
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.cColorStates[0].format = kColorFormat;
    renderPipelineDescriptor.vertexStage.module = CreateBasicVertexShaderForTest(device);
    renderPipelineDescriptor.cFragmentStage.module = CreateSampledTextureFragmentShaderForTest(device);
    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    // Create bindgroup
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                     {{0, sampler}, {1, texture.CreateView(&samplerTextureView)}});

    // Encode pass and submit
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor renderPassDesc({texture.CreateView(&renderTextureView)});
    renderPassDesc.cColorAttachments[0].clearColor = {1.0, 1.0, 1.0, 1.0};
    renderPassDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.SetPipeline(renderPipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(6, 1, 0, 0);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();
    wgpu::Queue queue = device.CreateQueue();
    queue.Submit(1, &commands);
}

TEST_F(TextureSubresourceValidationTest, ArrayLayersTest) {
    // Create needed resources
    wgpu::TextureDescriptor descriptor =
        CreateTextureDescriptor(1, 2, wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment, kColorFormat);
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    wgpu::TextureViewDescriptor samplerTextureView =
        CreateTextureViewDescriptor(0, 0);
    wgpu::TextureViewDescriptor renderTextureView =
        CreateTextureViewDescriptor(0, 1);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    // Create render pipeline
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
    renderPipelineDescriptor.cColorStates[0].format = kColorFormat;
    renderPipelineDescriptor.vertexStage.module = CreateBasicVertexShaderForTest(device);
    renderPipelineDescriptor.cFragmentStage.module = CreateSampledTextureFragmentShaderForTest(device);
    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);

    // Create bindgroup
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                     {{0, sampler}, {1, texture.CreateView(&samplerTextureView)}});

    // Encode pass and submit
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor renderPassDesc({texture.CreateView(&renderTextureView)});
    renderPassDesc.cColorAttachments[0].clearColor = {1.0, 1.0, 1.0, 1.0};
    renderPassDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.SetPipeline(renderPipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(6, 1, 0, 0);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();
    wgpu::Queue queue = device.CreateQueue();
    queue.Submit(1, &commands);
}

}  // anonymous namespace
