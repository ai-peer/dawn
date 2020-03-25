// Copyright 2020 The Dawn Authors
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

#include "utils/WGPUHelpers.h"

#include "tests/unittests/validation/ValidationTest.h"

namespace {

    class TextureSubresourceRWTrackingTest : public ValidationTest {};

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
        descriptor.mipLevelCount = mipLevelCount;
        descriptor.usage = usage;
        descriptor.format = format;
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

    // Test different mipmap levels
    TEST_F(TextureSubresourceRWTrackingTest, MipmapLevelsTest) {
        // Create texture, texture subresource and views
        wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
            2, 1, wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment, kColorFormat);
        wgpu::Texture texture = device.CreateTexture(&descriptor);

        wgpu::TextureViewDescriptor samplerViewDesc = CreateTextureViewDescriptor(0, 0);
        wgpu::TextureViewDescriptor renderViewDesc = CreateTextureViewDescriptor(1, 0);
        wgpu::TextureView samplerView = texture.CreateView(&samplerViewDesc);
        wgpu::TextureView renderView = texture.CreateView(&renderViewDesc);

        // Create bindgroup
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});

        utils::ComboRenderPassDescriptor renderPassDesc({renderView});

        // It is valid to read and write into different subresources of the same texture
        {
            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl, {{0, samplerView}});
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetBindGroup(0, bindGroup);
            pass.EndPass();
            encoder.Finish();
        }

        // It is invalid to read and write into the same subresources
        {
            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl, {{0, renderView}});
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetBindGroup(0, bindGroup);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test different array layers
    TEST_F(TextureSubresourceRWTrackingTest, ArrayLayersTest) {
        // Create texture, texture subresource and views
        wgpu::TextureDescriptor descriptor = CreateTextureDescriptor(
            1, 2, wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment, kColorFormat);
        wgpu::Texture texture = device.CreateTexture(&descriptor);

        wgpu::TextureViewDescriptor samplerViewDesc = CreateTextureViewDescriptor(0, 0);
        wgpu::TextureViewDescriptor renderViewDesc = CreateTextureViewDescriptor(0, 1);
        wgpu::TextureView samplerView = texture.CreateView(&samplerViewDesc);
        wgpu::TextureView renderView = texture.CreateView(&renderViewDesc);

        // Create bindgroup
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});

        utils::ComboRenderPassDescriptor renderPassDesc({renderView});

        // It is valid to read and write into different subresources of the same texture
        {
            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl, {{0, samplerView}});
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetBindGroup(0, bindGroup);
            pass.EndPass();
            encoder.Finish();
        }

        // It is invalid to read and write into the same subresources
        {
            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl, {{0, renderView}});
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetBindGroup(0, bindGroup);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }
}  // anonymous namespace
