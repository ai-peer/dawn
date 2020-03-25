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

#include "tests/unittests/validation/ValidationTest.h"

#include "utils/WGPUHelpers.h"

namespace {

    class ResourceRWTrackingTest : public ValidationTest {};

    wgpu::TextureDescriptor CreateTextureDescriptor(wgpu::TextureUsage usage,
                                                    wgpu::TextureFormat format) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {1, 1, 1};
        descriptor.arrayLayerCount = 1;
        descriptor.sampleCount = 1;
        descriptor.mipLevelCount = 1;
        descriptor.usage = usage;
        descriptor.format = format;
        return descriptor;
    }

    // Test that using a single buffer in multiple read usages in the same pass is allowed.
    TEST_F(ResourceRWTrackingTest, BufferWithMultipleReadUsage) {
        // Create a buffer used as both vertex and index buffer.
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Index;
        bufferDescriptor.size = 4;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

        // Use the buffer as both index and vertex in the same pass
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        DummyRenderPass dummyRenderPass(device);
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
        pass.SetIndexBuffer(buffer);
        pass.SetVertexBuffer(0, buffer);
        pass.EndPass();
        encoder.Finish();
    }

    // Test that using the same buffer as both readable and writable in the same pass is disallowed
    TEST_F(ResourceRWTrackingTest, BufferWithReadAndWriteUsage) {
        // Create a buffer that will be used as an index buffer and as a storage buffer
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index;
        bufferDescriptor.size = 4;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

        // Create the bind group to use the buffer as storage
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
        wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, buffer, 0, 4}});

        // Use the buffer as both index and storage in the same pass
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        DummyRenderPass dummyRenderPass(device);
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
        pass.SetIndexBuffer(buffer);
        pass.SetBindGroup(0, bg);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Test that using the same buffer as copy src/dst and write usage in a render pass is allowed.
    TEST_F(ResourceRWTrackingTest, BufferCopyAndBufferWriteUsage) {
        // Create buffers that will be used as an copy src/dst buffer and as a storage buffer
        wgpu::BufferDescriptor bufferSrcDescriptor;
        bufferSrcDescriptor.usage = wgpu::BufferUsage::CopySrc;
        bufferSrcDescriptor.size = 4;
        wgpu::Buffer bufferSrc = device.CreateBuffer(&bufferSrcDescriptor);

        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        bufferDescriptor.size = 4;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

        // Create the bind group to use the buffer as storage
        wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
        wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl0, {{0, buffer, 0, 4}});
        wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer}});
        wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl1, {{0, buffer, 0, 4}});

        // Use the buffer as both copy dst and storage in render pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            encoder.CopyBufferToBuffer(bufferSrc, 0, buffer, 0, 4);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetBindGroup(0, bg0);
            pass.EndPass();
            encoder.Finish();
        }

        // Use the buffer as both copy dst and readonly storage in compute pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            encoder.CopyBufferToBuffer(bufferSrc, 0, buffer, 0, 4);
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg1);
            pass.EndPass();
            encoder.Finish();
        }
    }

    // Test that using a single buffer in multiple read usages which include readonly storage usage
    // in the same pass is allowed.
    TEST_F(ResourceRWTrackingTest, BufferWithMultipleReadAndReadOnlyStorageUsage) {
        // Create a buffer that will be used as an index buffer and as a storage buffer
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index;
        bufferDescriptor.size = 4;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

        // Create the bind group to use the buffer as storage
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::ReadonlyStorageBuffer}});
        wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, buffer, 0, 4}});

        // Use the buffer as both index and storage in the same pass
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        DummyRenderPass dummyRenderPass(device);
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
        pass.SetIndexBuffer(buffer);
        pass.SetBindGroup(0, bg);
        pass.EndPass();
        encoder.Finish();
    }

    // Test that using the same storage buffer as both readable and writable in the same pass is
    // disallowed
    TEST_F(ResourceRWTrackingTest, BufferWithReadAndWriteStorageBufferUsage) {
        // Create a buffer that will be used as an storage buffer and as a readonly storage buffer
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.usage = wgpu::BufferUsage::Storage;
        bufferDescriptor.size = 512;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

        // Create the bind group to use the buffer as storage
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer},
                     {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer}});
        wgpu::BindGroup bg =
            utils::MakeBindGroup(device, bgl, {{0, buffer, 0, 4}, {1, buffer, 256, 4}});

        // Use the buffer as both index and storage in the same pass
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        DummyRenderPass dummyRenderPass(device);
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
        pass.SetBindGroup(0, bg);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Test that only the last SetBindGroup takes effect if there are consecutive SetBindGroups
    TEST_F(ResourceRWTrackingTest, BufferWithConsecutiveSetBindGroups) {
        // Create a buffer that will be used as an index buffer and as a storage buffer
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index;
        bufferDescriptor.size = 4;
        wgpu::Buffer buffer0 = device.CreateBuffer(&bufferDescriptor);
        wgpu::Buffer buffer1 = device.CreateBuffer(&bufferDescriptor);

        // Create the bind group to use the buffer as storage
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
        wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl, {{0, buffer0, 0, 4}});
        wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, buffer1, 0, 4}});

        DummyRenderPass dummyRenderPass(device);

        // Set bind group twice. The second one overwrites the first one. Then no buffer is used
        // as both read and write in the same pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetIndexBuffer(buffer0);
            pass.SetBindGroup(0, bg0);
            pass.SetBindGroup(0, bg1);
            pass.EndPass();
            encoder.Finish();
        }

        // Set bind group twice. The second one overwrites the first one. Then buffer0 is used
        // as both read and write in the same pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetIndexBuffer(buffer0);
            pass.SetBindGroup(0, bg1);
            pass.SetBindGroup(0, bg0);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test that using the same buffer as both readable and writable in different passes is allowed
    TEST_F(ResourceRWTrackingTest, BufferWithReadAndWriteUsageOnDifferentPasses) {
        // Create a buffer that will be used as an index buffer and as a storage buffer
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index;
        bufferDescriptor.size = 4;
        wgpu::Buffer buffer0 = device.CreateBuffer(&bufferDescriptor);
        wgpu::Buffer buffer1 = device.CreateBuffer(&bufferDescriptor);

        // Create the bind group to use the buffer as storage
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
        wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl, {{0, buffer0, 0, 4}});
        wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, buffer1, 0, 4}});

        // Use these two buffers as both index and storage in the different passes
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        DummyRenderPass dummyRenderPass(device);
        wgpu::RenderPassEncoder pass0 = encoder.BeginRenderPass(&dummyRenderPass);
        pass0.SetIndexBuffer(buffer0);
        pass0.SetBindGroup(0, bg1);
        pass0.EndPass();
        wgpu::RenderPassEncoder pass1 = encoder.BeginRenderPass(&dummyRenderPass);
        pass1.SetIndexBuffer(buffer1);
        pass1.SetBindGroup(0, bg0);
        pass1.EndPass();
        encoder.Finish();
    }

    // Test that using the same buffer as both readable and writable in the different draws
    TEST_F(ResourceRWTrackingTest, BufferWithReadAndWriteUsageOnDifferentDraws) {
        // Create a buffer that will be used as a storage buffer
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.usage = wgpu::BufferUsage::Storage;
        bufferDescriptor.size = 4;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDescriptor);

        // Create the bind group to use the buffer as both readonly storage and writable storage
        // bindings
        wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer}});
        wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
        wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl0, {{0, buffer, 0, 4}});
        wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl1, {{0, buffer, 0, 4}});

        // It is not allowed to use the same buffer as both readable and writable in different draws
        // within the same render pass.
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetBindGroup(0, bg0);
            pass.Draw(3, 1, 0, 0);
            pass.SetBindGroup(1, bg1);
            pass.Draw(3, 1, 0, 0);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // It is allowed to use the same buffer as both readable and writable in different draws
        // within the same compute pass.
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg0);
            pass.Dispatch(1, 1, 1);
            pass.SetBindGroup(1, bg1);
            pass.Dispatch(1, 1, 1);
            pass.EndPass();
            encoder.Finish();
        }
    }

    // Test that using a single texture in multiple read usages in the same pass is allowed.
    TEST_F(ResourceRWTrackingTest, TextureWithMultipleReadUsages) {
        // Create a texture that will be used both as a sampled texture and a readonly storage
        // texture
        wgpu::TextureDescriptor textureDescriptor =
            CreateTextureDescriptor(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::Storage,
                                    wgpu::TextureFormat::RGBA8Unorm);
        wgpu::Texture texture = device.CreateTexture(&textureDescriptor);
        wgpu::TextureView view = texture.CreateView();

        // Create the bind group to use the texture as sampled textire amd readonly storage texture
        // bindings
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture},
                     {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageTexture}});
        wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view}, {1, view}});

        DummyRenderPass dummyRenderPass(device);

        // Use the texture as both sampeld and readonly storage in the same pass
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
        pass.SetBindGroup(0, bg);
        pass.EndPass();
        encoder.Finish();
    }

    // Test that using the same texture as both readable and writable in the same pass is disallowed
    TEST_F(ResourceRWTrackingTest, TextureWithReadAndWriteUsage) {
        // Create a texture that will be used both as a sampled texture and a render target
        wgpu::TextureDescriptor textureDescriptor = CreateTextureDescriptor(
            wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment,
            wgpu::TextureFormat::RGBA8Unorm);
        wgpu::Texture texture = device.CreateTexture(&textureDescriptor);
        wgpu::TextureView view = texture.CreateView();

        // Create the bind group to use the texture as sampled
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});
        wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view}});

        // Create the render pass that will use the texture as an output attachment
        utils::ComboRenderPassDescriptor renderPass({view});

        // Use the texture as both sampeld and output attachment in the same pass
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetBindGroup(0, bg);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Test that using a single texture as copy src/dst and writable/readable usage in pass is
    // allowed.
    TEST_F(ResourceRWTrackingTest, TextureCopyAndTextureUsageInPass) {
        // Create a texture that will be used both as a sampled texture and a render target
        wgpu::TextureDescriptor textureDescriptor0 =
            CreateTextureDescriptor(wgpu::TextureUsage::CopySrc, wgpu::TextureFormat::RGBA8Unorm);
        wgpu::TextureDescriptor textureDescriptor1 =
            CreateTextureDescriptor(wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled |
                                        wgpu::TextureUsage::OutputAttachment,
                                    wgpu::TextureFormat::RGBA8Unorm);
        wgpu::Texture texture0 = device.CreateTexture(&textureDescriptor0);
        wgpu::TextureView view0 = texture0.CreateView();
        wgpu::Texture texture1 = device.CreateTexture(&textureDescriptor1);
        wgpu::TextureView view1 = texture1.CreateView();

        wgpu::TextureCopyView srcView = utils::CreateTextureCopyView(texture0, 0, 0, {0, 0, 0});
        wgpu::TextureCopyView dstView = utils::CreateTextureCopyView(texture1, 0, 0, {0, 0, 0});
        wgpu::Extent3D copySize = {1, 1, 1};

        // Create the bind group to use the texture as sampled
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture}});
        wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view1}});

        // Create the render pass that will use the texture as an output attachment
        utils::ComboRenderPassDescriptor renderPass({view1});

        // Use the texture as both copy dst and output attachment in render pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            encoder.CopyTextureToTexture(&srcView, &dstView, &copySize);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
            pass.EndPass();
            encoder.Finish();
        }

        // Use the texture as both copy dst and readable usage in compute pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            encoder.CopyTextureToTexture(&srcView, &dstView, &copySize);
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            encoder.Finish();
        }
    }

    // Test that only the last SetBindGroup takes effect if there are consecutive SetBindGroups
    TEST_F(ResourceRWTrackingTest, TextureWithConsecutiveSetBindGroups) {
        // Create a texture that will be used both as a sampled texture and a render target
        wgpu::TextureDescriptor textureDescriptor = CreateTextureDescriptor(
            wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment,
            wgpu::TextureFormat::RGBA8Unorm);
        wgpu::Texture texture0 = device.CreateTexture(&textureDescriptor);
        wgpu::TextureView view0 = texture0.CreateView();
        wgpu::Texture texture1 = device.CreateTexture(&textureDescriptor);
        wgpu::TextureView view1 = texture1.CreateView();

        // Create the bind group to use the texture as sampled
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});
        wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl, {{0, view0}});
        wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, view1}});

        // Create the render pass that will use the texture as an output attachment
        utils::ComboRenderPassDescriptor renderPass({view0});

        // Set bind group twice. The second one overwrites the first one. Then texture is not used
        // as both sampeld and output attachment in the same pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
            pass.SetBindGroup(0, bg0);
            pass.SetBindGroup(0, bg1);
            pass.EndPass();
            encoder.Finish();
        }

        // Set bind group twice. The second one overwrites the first one. Then texture is used as
        // both sampeld and output attachment in the same pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
            pass.SetBindGroup(0, bg1);
            pass.SetBindGroup(0, bg0);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test that using the same texture as both readable and writable in the different passes is
    // allowed
    TEST_F(ResourceRWTrackingTest, TextureWithReadAndWriteUsageOnDifferentPasses) {
        // Create a texture that will be used both as a sampled texture and a render target
        wgpu::TextureDescriptor textureDescriptor = CreateTextureDescriptor(
            wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment,
            wgpu::TextureFormat::RGBA8Unorm);
        wgpu::Texture t0 = device.CreateTexture(&textureDescriptor);
        wgpu::TextureView v0 = t0.CreateView();
        wgpu::Texture t1 = device.CreateTexture(&textureDescriptor);
        wgpu::TextureView v1 = t1.CreateView();

        // Create the bind group to use the texture as sampled
        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});
        wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl, {{0, v0}});
        wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, v1}});

        // Create the render pass that will use the texture as an output attachment
        utils::ComboRenderPassDescriptor renderPass0({v1});
        utils::ComboRenderPassDescriptor renderPass1({v0});

        // Use the texture as both sampeld and output attachment in the same pass
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass0 = encoder.BeginRenderPass(&renderPass0);
        pass0.SetBindGroup(0, bg0);
        pass0.EndPass();
        wgpu::RenderPassEncoder pass1 = encoder.BeginRenderPass(&renderPass1);
        pass1.SetBindGroup(0, bg1);
        pass1.EndPass();
        encoder.Finish();
    }

    // Test that using the same texture as both readable and writable in the different passes is
    // allowed
    TEST_F(ResourceRWTrackingTest, TextureWithReadAndWriteUsageOnDifferentDrawsOrDispatches) {
        // Create a texture that will be used both as a sampled texture and a storage texture
        wgpu::TextureDescriptor textureDescriptor =
            CreateTextureDescriptor(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::Storage,
                                    wgpu::TextureFormat::RGBA8Unorm);
        wgpu::Texture t0 = device.CreateTexture(&textureDescriptor);
        wgpu::TextureView v0 = t0.CreateView();
        wgpu::Texture t1 = device.CreateTexture(&textureDescriptor);
        wgpu::TextureView v1 = t1.CreateView();

        // Create the bind group to use the texture as sampled
        wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});
        wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::WriteonlyStorageTexture}});
        wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl0, {{0, v0}});
        wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl1, {{0, v1}});

        // Use the texture as both sampeld and writable storage in different draws in the same
        // render pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetBindGroup(0, bg0);
            pass.Draw(3, 1, 0, 0);
            pass.SetBindGroup(1, bg1);
            pass.Draw(3, 1, 0, 0);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Use the texture as both sampeld and writable storage in different dispatches in the same
        // compute pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg0);
            pass.Dispatch(1, 1, 1);
            pass.SetBindGroup(1, bg1);
            pass.Dispatch(1, 1, 1);
            pass.EndPass();
            encoder.Finish();
        }
    }

}  // anonymous namespace
