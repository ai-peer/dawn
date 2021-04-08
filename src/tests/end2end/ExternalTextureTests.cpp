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
      public:
        void initBuffers() {
            static const uint32_t indexData[3] = {
                0,
                1,
                2,
            };
            indexBuffer = utils::CreateBufferFromData(device, indexData, sizeof(indexData),
                                                      wgpu::BufferUsage::Index);

            static const float vertexData[12] = {
                -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
            };
            vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                                       wgpu::BufferUsage::Vertex);
        }

      protected:
        static constexpr uint32_t kWidth = 4;
        static constexpr uint32_t kHeight = 4;
        static constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;
        static constexpr wgpu::TextureUsage kSampledUsage = wgpu::TextureUsage::Sampled;

        wgpu::Buffer indexBuffer;
        wgpu::Buffer vertexBuffer;
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

// Ensure that we can create a bind group layout and bind group with an external texture.
TEST_P(ExternalTextureTests, BindExternalTexture) {
    wgpu::Texture texture = Create2DTexture(device, kWidth, kHeight, kFormat, kSampledUsage);
    wgpu::TextureView view = texture.CreateView();

    wgpu::ExternalTextureDescriptor externalDesc;
    externalDesc.plane0 = view;
    externalDesc.format = kFormat;

    wgpu::ExternalTexture externalTexture = device.CreateExternalTexture(&externalDesc);

    wgpu::BindGroup bindGroup;
    wgpu::BindGroupLayout bgl;

    bgl = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::ExternalTextureAllowedType::YuvOrRgba}});

    bindGroup = utils::MakeBindGroup(device, bgl, {{0, externalTexture}});
}

TEST_P(ExternalTextureTests, SampleExternalTexture) {
    wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
        [[builtin(position)]] var<out> Position : vec4<f32>;
        [[location(0)]] var<in> pos : vec4<f32>;
        [[stage(vertex)]] fn main() -> void {
            Position = pos;
            return;
        })");

    const wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
        [[builtin(frag_coord)]] var<in> FragCoord : vec4<f32>;        
        [[group(0), binding(0)]] var mySampler: sampler;
        [[group(0), binding(1)]] var myTexture: texture_external;

        [[location(0)]] var<out> FragColor : vec4<f32>;
        [[stage(fragment)]] fn main() -> void {
            FragColor = textureSample(myTexture, mySampler, FragCoord.xy / vec2<f32>(4.0, 4.0));
            return;
        })");

    initBuffers();

    wgpu::Texture texture =
        Create2DTexture(device, kWidth, kHeight, kFormat,
                        wgpu::TextureUsage::Sampled | wgpu::TextureUsage::RenderAttachment);
    wgpu::Texture renderTexture =
        Create2DTexture(device, kWidth, kHeight, kFormat,
                        wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::RenderAttachment);

    // Create a texture view for the external texture
    wgpu::TextureView view = texture.CreateView();

    {
        utils::ComboRenderPassDescriptor renderPass({view}, nullptr);
        renderPass.cColorAttachments[0].clearColor = {0.0f, 1.0f, 0.0f, 1.0f};
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.EndPass();

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    // Create an ExternalTextureDescriptor from the texture view
    wgpu::ExternalTextureDescriptor externalDesc;
    externalDesc.plane0 = view;
    externalDesc.format = kFormat;

    // Import the external texture
    wgpu::ExternalTexture externalTexture = device.CreateExternalTexture(&externalDesc);

    // Create a sampler and bind group
    wgpu::Sampler sampler = device.CreateSampler();
    wgpu::BindGroup bindGroup;
    wgpu::BindGroupLayout bgl;

    bgl = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering},
                 {1, wgpu::ShaderStage::Fragment, wgpu::ExternalTextureAllowedType::YuvOrRgba}});
    bindGroup = utils::MakeBindGroup(device, bgl, {{0, sampler}, {1, externalTexture}});

    // Pipeline Creation
    utils::ComboRenderPipelineDescriptor2 descriptor;
    descriptor.layout = utils::MakeBasicPipelineLayout(device, &bgl);
    descriptor.vertex.module = vsModule;
    descriptor.vertex.bufferCount = 1;
    descriptor.cFragment.module = fsModule;
    descriptor.cBuffers[0].arrayStride = 4 * sizeof(float);
    descriptor.cBuffers[0].attributeCount = 1;
    descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
    descriptor.cTargets[0].format = kFormat;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline2(&descriptor);

    wgpu::TextureView renderView = renderTexture.CreateView();
    utils::ComboRenderPassDescriptor renderPass({renderView}, nullptr);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
    {
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.DrawIndexed(3);
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_PIXEL_RGBA8_EQ(RGBA8::kGreen, renderTexture, 0, 0);
}

DAWN_INSTANTIATE_TEST(ExternalTextureTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());