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

namespace {

    wgpu::Texture Create2DTexture(wgpu::Device device,
                                  uint32_t width,
                                  uint32_t height,
                                  wgpu::TextureFormat format,
                                  wgpu::TextureUsage usage) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size.width = width;
        descriptor.size.height = height;
        descriptor.size.depthOrArrayLayers = 1;
        descriptor.sampleCount = 1;
        descriptor.format = format;
        descriptor.mipLevelCount = 1;
        descriptor.usage = usage;
        return device.CreateTexture(&descriptor);
    }

    class ExternalTextureTests : public DawnTest {
      protected:
        static constexpr uint32_t kWidth = 4;
        static constexpr uint32_t kHeight = 4;
        static constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;
        static constexpr wgpu::TextureUsage kSampledUsage = wgpu::TextureUsage::Sampled;
    };
}  // anonymous namespace

TEST_P(ExternalTextureTests, CreateExternalTextureSuccess) {
    wgpu::Texture texture = Create2DTexture(device, kWidth, kHeight, kFormat, kSampledUsage);

    // Create a texture view for the external texture
    wgpu::TextureView view = texture.CreateView();

    // Create an ExternalTextureDescriptor from the texture view
    wgpu::ExternalTextureDescriptor externalDesc;
    externalDesc.plane0 = view;
    externalDesc.format = kFormat;

    // Import the external texture
    wgpu::ExternalTexture externalTexture = device.CreateExternalTexture(&externalDesc);

    ASSERT_NE(externalTexture.Get(), nullptr);
}

TEST_P(ExternalTextureTests, BindExternalTexture) {
    wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;
        [[stage(vertex)]] fn main() -> void {
            Position = vec4<f32>(1.0, 0.0, 0.0, 1.0);
            return;
        })");

    const wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
        [[builtin(frag_coord)]] var<in> FragCoord : vec4<f32>;        
        [[group(0), binding(0)]] var externalTexture: texture_external;


        [[location(0)]] var<out> FragColor : vec4<f32>;
        [[stage(fragment)]] fn main() -> void {
            FragColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
            return;
        })");

    wgpu::Texture texture = Create2DTexture(device, kWidth, kHeight, kFormat, kSampledUsage);

    // Create a texture view for the external texture
    wgpu::TextureView view = texture.CreateView();

    // Create an ExternalTextureDescriptor from the texture view
    wgpu::ExternalTextureDescriptor externalDesc;
    externalDesc.plane0 = view;
    externalDesc.format = kFormat;

    // Import the external texture
    wgpu::ExternalTexture externalTexture = device.CreateExternalTexture(&externalDesc);

    ASSERT_NE(externalTexture.Get(), nullptr);

    wgpu::BindGroup bindGroup;
    wgpu::BindGroupLayout bgl;

    bgl = utils::MakeBindGroupLayout(device, {{0, wgpu::ShaderStage::Fragment, kFormat}});
    bindGroup = utils::MakeBindGroup(device, bgl, {{0, externalTexture}});

    wgpu::PipelineLayout pipelineLayout;
    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

    utils::ComboRenderPipelineDescriptor2 descriptor;
    descriptor.layout = pipelineLayout;
    descriptor.vertex.module = vsModule;
    descriptor.cFragment.module = fsModule;

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kWidth, kHeight);
    descriptor.cTargets[0].format = renderPass.colorFormat;

    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline2(&descriptor);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    {
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
}

DAWN_INSTANTIATE_TEST(ExternalTextureTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());