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
#include "utils/TestUtils.h"
#include "utils/WGPUHelpers.h"

constexpr static uint32_t kRTSize = 4;
constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;

class Texture3DTests : public DawnTest {};

TEST_P(Texture3DTests, Sampling) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
        [[stage(vertex)]]
        fn main([[builtin(vertex_index)]] VertexIndex : u32) -> [[builtin(position)]] vec4<f32> {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, 1.0),
                vec2<f32>( 1.0, 1.0),
                vec2<f32>(-1.0, -1.0));

            return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
        })");

    wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
        [[group(0), binding(0)]] var samp : sampler;
        [[group(0), binding(1)]] var tex : texture_3d<f32>;

        [[stage(fragment)]]
        fn main([[builtin(position)]] FragCoord : vec4<f32>) -> [[location(0)]] vec4<f32> {
            return textureSample(tex, samp, vec3<f32>(FragCoord.xy, 1.0 / 4.0));
        })");

    utils::ComboRenderPipelineDescriptor pipelineDescriptor;
    pipelineDescriptor.vertex.module = vsModule;
    pipelineDescriptor.cFragment.module = fsModule;
    pipelineDescriptor.cTargets[0].format = renderPass.colorFormat;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDescriptor);

    wgpu::SamplerDescriptor samplerDescriptor;
    wgpu::Sampler sampler = device.CreateSampler(&samplerDescriptor);

    wgpu::Extent3D copySize = {kRTSize, kRTSize, kRTSize};

    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e3D;
    descriptor.size = copySize;
    descriptor.sampleCount = 1;
    descriptor.format = kFormat;
    descriptor.mipLevelCount = 1;
    descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled;
    wgpu::Texture texture = device.CreateTexture(&descriptor);
    wgpu::TextureView textureView = texture.CreateView();

    uint32_t bytesPerRow = utils::GetMinimumBytesPerRow(kFormat, copySize.width);
    uint32_t sizeInBytes =
        utils::RequiredBytesInCopy(bytesPerRow, copySize.height, copySize, kFormat);
    const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(kFormat);
    uint32_t size = sizeInBytes / bytesPerTexel;
    std::vector<RGBA8> data = std::vector<RGBA8>(size);
    for (uint32_t z = 0; z < copySize.depthOrArrayLayers; ++z) {
        for (uint32_t y = 0; y < copySize.height; ++y) {
            for (uint32_t x = 0; x < copySize.width; ++x) {
                uint32_t i = (z * copySize.height + y) * bytesPerRow / bytesPerTexel + x;
                // Note that the data at (x, y) on each depth slice of the 3D texture will be
                // slightly different: its B channel equals to z (depth index).
                data[i] = RGBA8(0, 255, z, 255);
            }
        }
    }
    wgpu::Buffer buffer =
        utils::CreateBufferFromData(device, data.data(), sizeInBytes, wgpu::BufferUsage::CopySrc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    wgpu::ImageCopyBuffer imageCopyBuffer =
        utils::CreateImageCopyBuffer(buffer, 0, bytesPerRow, copySize.height);
    wgpu::ImageCopyTexture imageCopyTexture = utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
    encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);

    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {{0, sampler}, {1, textureView}});

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(3);
    pass.EndPass();

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    // We sample data from the 3D texture at depth = 1 (1.0 / 4.0) and render the data into color
    // attachment, so the expected color of the render triangle is (0, 255, 1, 255).
    RGBA8 filled(0, 255, 1, 255);
    RGBA8 notFilled(0, 0, 0, 0);
    uint32_t min = 0, max = kRTSize - 2;
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color, min, min);
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color, max, min);
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color, min, max);
    EXPECT_PIXEL_RGBA8_EQ(notFilled, renderPass.color, max, max);
}

DAWN_INSTANTIATE_TEST(Texture3DTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
