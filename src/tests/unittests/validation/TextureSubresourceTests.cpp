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

    class TextureSubresourceTest : public ValidationTest {
      public:
        static constexpr uint32_t kSize = 32u;
        static constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;

        wgpu::Texture CreateTexture(uint32_t mipLevelCount,
                                    uint32_t arrayLayerCount,
                                    wgpu::TextureUsage usage) {
            wgpu::TextureDescriptor descriptor;
            descriptor.dimension = wgpu::TextureDimension::e2D;
            descriptor.size.width = kSize;
            descriptor.size.height = kSize;
            descriptor.size.depth = 1;
            descriptor.arrayLayerCount = arrayLayerCount;
            descriptor.sampleCount = 1;
            descriptor.mipLevelCount = mipLevelCount;
            descriptor.usage = usage;
            descriptor.format = kFormat;
            return device.CreateTexture(&descriptor);
        }

        wgpu::TextureView CreateTextureView(wgpu::Texture texture,
                                            uint32_t baseMipLevel,
                                            uint32_t baseArrayLayer) {
            wgpu::TextureViewDescriptor viewDesc = {
                .format = kFormat,
                .dimension = wgpu::TextureViewDimension::e2D,
                .baseMipLevel = baseMipLevel,
                .mipLevelCount = 1,
                .baseArrayLayer = baseArrayLayer,
                .arrayLayerCount = 1,
            };
            return texture.CreateView(&viewDesc);
        }
    };

    // Test different mipmap levels
    TEST_F(TextureSubresourceTest, MipmapLevelsTest) {
        // Create texture, texture subresource and views
        wgpu::Texture texture =
            CreateTexture(2, 1,
                          wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment |
                              wgpu::TextureUsage::Storage);

        wgpu::TextureView samplerView = CreateTextureView(texture, 0, 0);
        wgpu::TextureView renderView = CreateTextureView(texture, 1, 0);

        // Create bind group
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

        // It is valid to write into and then read from the same level of a texture in different
        // render passes
        {
            wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
                device, {{.binding = 0,
                          .visibility = wgpu::ShaderStage::Fragment,
                          .type = wgpu::BindingType::WriteonlyStorageTexture,
                          .storageTextureFormat = kFormat}});
            wgpu::BindGroup bindGroup1 = utils::MakeBindGroup(device, bgl1, {{0, samplerView}});

            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl, {{0, samplerView}});

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass1 = encoder.BeginRenderPass(&renderPassDesc);
            pass1.SetBindGroup(0, bindGroup1);
            pass1.EndPass();

            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetBindGroup(0, bindGroup);
            pass.EndPass();

            encoder.Finish();
        }
    }

    // Test different array layers
    TEST_F(TextureSubresourceTest, ArrayLayersTest) {
        // Create texture, texture subresource and views
        wgpu::Texture texture =
            CreateTexture(1, 2,
                          wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment |
                              wgpu::TextureUsage::Storage);

        wgpu::TextureView samplerView = CreateTextureView(texture, 0, 0);
        wgpu::TextureView renderView = CreateTextureView(texture, 0, 1);

        // Create bind group
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});

        utils::ComboRenderPassDescriptor renderPassDesc({renderView});

        // It is valid to read and write into different layers
        {
            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl, {{0, samplerView}});
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetBindGroup(0, bindGroup);
            pass.EndPass();
            encoder.Finish();
        }

        // It is invalid to read and write into the same layer in one render pass
        {
            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl, {{0, renderView}});
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetBindGroup(0, bindGroup);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // It is valid to write into and then read from the same layer of a texture in different
        // render passes
        {
            wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
                device, {{.binding = 0,
                          .visibility = wgpu::ShaderStage::Fragment,
                          .type = wgpu::BindingType::WriteonlyStorageTexture,
                          .storageTextureFormat = kFormat}});
            wgpu::BindGroup bindGroup1 = utils::MakeBindGroup(device, bgl1, {{0, samplerView}});

            wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl, {{0, samplerView}});
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

            wgpu::RenderPassEncoder pass1 = encoder.BeginRenderPass(&renderPassDesc);
            pass1.SetBindGroup(0, bindGroup1);
            pass1.EndPass();

            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetBindGroup(0, bindGroup);
            pass.EndPass();

            encoder.Finish();
        }
    }

    // TODO (yunchao.he@intel.com):
    //	* Add tests for compute, in which texture subresource is traced per dispatch.
    //
    //	* Add tests for multiple threading, in which we can have multiple encoders upon the same
    //	texture subresource simultaneously. Note that this is a long-term task because we have no
    //	multiple threading support yet.

}  // anonymous namespace
