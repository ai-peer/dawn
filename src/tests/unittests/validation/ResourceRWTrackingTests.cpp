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

    class ResourceRWTrackingTest : public ValidationTest {
      protected:
        wgpu::Buffer CreateBuffer(uint64_t size, wgpu::BufferUsage usage) {
            wgpu::BufferDescriptor descriptor;
            descriptor.size = size;
            descriptor.usage = usage;

            return device.CreateBuffer(&descriptor);
        }

        wgpu::Texture CreateTexture(wgpu::TextureUsage usage, wgpu::TextureFormat format) {
            wgpu::TextureDescriptor descriptor;
            descriptor.dimension = wgpu::TextureDimension::e2D;
            descriptor.size = {1, 1, 1};
            descriptor.arrayLayerCount = 1;
            descriptor.sampleCount = 1;
            descriptor.mipLevelCount = 1;
            descriptor.usage = usage;
            descriptor.format = format;

            return device.CreateTexture(&descriptor);
        }
    };

    // Test that using a single buffer in multiple read usages in the same pass is allowed.
    TEST_F(ResourceRWTrackingTest, BufferWithMultipleReadUsage) {
        // Test render pass
        {
            // Create a buffer, and use the buffer as both vertex and index buffer.
            wgpu::Buffer buffer =
                CreateBuffer(4, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Index);

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetIndexBuffer(buffer);
            pass.SetVertexBuffer(0, buffer);
            pass.EndPass();
            encoder.Finish();
        }

        // Test compute pass
        {
            // Create buffer and bind group
            wgpu::Buffer buffer =
                CreateBuffer(4, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage);

            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer}});
            wgpu::BindGroup bg =
                utils::MakeBindGroup(device, bgl, {{0, buffer, 0, 4}, {1, buffer, 0, 4}});

            // Use the buffer as both uniform and readonly storage buffer in compute pass.
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            encoder.Finish();
        }
    }

    // Test that using the same buffer as both readable and writable in the same pass is disallowed
    TEST_F(ResourceRWTrackingTest, BufferWithReadAndWriteUsage) {
        // test render pass for index buffer and storage buffer
        {
            // Create buffer and bind group
            wgpu::Buffer buffer =
                CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index);

            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
            wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, buffer, 0, 4}});

            // Use the buffer as both index and storage in render pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetIndexBuffer(buffer);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // test compute pass
        {
            // Create buffer and bind group
            wgpu::Buffer buffer = CreateBuffer(512, wgpu::BufferUsage::Storage);

            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer}});
            wgpu::BindGroup bg =
                utils::MakeBindGroup(device, bgl, {{0, buffer, 0, 4}, {1, buffer, 256, 4}});

            // Use the buffer as both storage and readonly storage in compute pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test that using the same buffer as copy src/dst and writeable/readadble usage is allowed.
    TEST_F(ResourceRWTrackingTest, BufferCopyAndBufferUsageInPass) {
        // Create buffers that will be used as an copy src/dst buffer and as a storage buffer
        wgpu::Buffer bufferSrc =
            CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
        wgpu::Buffer bufferDst =
            CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);

        // Create the bind group to use the buffer as storage
        wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
        wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl0, {{0, bufferSrc, 0, 4}});
        wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer}});
        wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl1, {{0, bufferDst, 0, 4}});

        // Use the buffer as both copy src and storage in render pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            encoder.CopyBufferToBuffer(bufferSrc, 0, bufferDst, 0, 4);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetBindGroup(0, bg0);
            pass.EndPass();
            encoder.Finish();
        }

        // Use the buffer as both copy dst and readonly storage in compute pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            encoder.CopyBufferToBuffer(bufferSrc, 0, bufferDst, 0, 4);
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg1);
            pass.EndPass();
            encoder.Finish();
        }
    }

    // Test that all index buffer and vertex buffer take effect even though some buffers are
    // not used because they are overwritten by a consecutive call.
    TEST_F(ResourceRWTrackingTest, BufferWithMultipleSetIndexOrVertexBuffer) {
        // Create a buffer, and use the buffer as both vertex and index buffer.
        wgpu::Buffer buffer0 = CreateBuffer(
            4, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Index | wgpu::BufferUsage::Storage);
        wgpu::Buffer buffer1 =
            CreateBuffer(4, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Index);

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
        wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, buffer0, 0, 4}});

        DummyRenderPass dummyRenderPass(device);
        // Set multiple index buffer. buffer0 usded by index buffer conflicts with buffer binding
        // in bind group. But buffer0 is overwritten by another SetIndexBuffer
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetIndexBuffer(buffer0);
            pass.SetIndexBuffer(buffer1);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Set multiple index buffer. buffer0 usded by index buffer conflicts with buffer binding
        // in bind group. buffer0 is not overwritten by another SetIndexBuffer
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetIndexBuffer(buffer1);
            pass.SetIndexBuffer(buffer0);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Set multiple vertex buffer on the same index. buffer0 usded by vertex buffer conflicts
        // with buffer binding in bind group. But buffer0 is overwritten by another SetVertexBuffer.
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetVertexBuffer(0, buffer0);
            pass.SetVertexBuffer(0, buffer1);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Set multiple vertex buffer on the same index. buffer0 usded by vertex buffer conflicts
        // with buffer binding in bind group. buffer0 is not overwritten by another SetVertexBuffer.
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetVertexBuffer(0, buffer1);
            pass.SetVertexBuffer(0, buffer0);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test that all consecutive SetBindGroup()s take effect even though some bind groups are not
    // used because they are overwritten by a consecutive call.
    TEST_F(ResourceRWTrackingTest, BufferWithMultipleSetBindGroupsOnSameIndex) {
        // test render pass
        {
            // Create buffers that will be used as index and storage buffers
            wgpu::Buffer buffer0 =
                CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index);
            wgpu::Buffer buffer1 =
                CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index);

            // Create the bind group to use the buffer as storage
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl, {{0, buffer0, 0, 4}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, buffer1, 0, 4}});

            DummyRenderPass dummyRenderPass(device);

            // Set bind group against the same index twice. The second one overwrites the first one.
            // Then no buffer is used as both read and write in the same pass. But the overwritten
            // bind group still take effect.
            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
                pass.SetIndexBuffer(buffer0);
                pass.SetBindGroup(0, bg0);
                pass.SetBindGroup(0, bg1);
                pass.EndPass();
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }

            // Set bind group against the same index twice. The second one overwrites the first one.
            // Then buffer0 is used as both read and write in the same pass
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

        // test compute pass
        {
            // Create buffers that will be used as index and storage buffers
            wgpu::Buffer buffer0 = CreateBuffer(512, wgpu::BufferUsage::Storage);
            wgpu::Buffer buffer1 = CreateBuffer(4, wgpu::BufferUsage::Storage);

            // Create the bind group to use the buffer as storage
            wgpu::BindGroupLayout baseBGL = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer}});
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer}});
            wgpu::BindGroup base = utils::MakeBindGroup(device, baseBGL, {{0, buffer0, 0, 4}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, baseBGL, {{0, buffer0, 256, 4}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, buffer1, 0, 4}});

            DummyRenderPass dummyRenderPass(device);

            // Set bind group against the same index twice. The second one overwrites the first one.
            // Then no buffer is used as both read and write in the same pass. But the overwritten
            // bind group still take effect.
            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
                pass.SetBindGroup(0, base);
                pass.SetBindGroup(1, bg0);
                pass.SetBindGroup(1, bg1);
                pass.EndPass();
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }

            // Set bind group against the same index twice. The second one overwrites the first one.
            // Then buffer0 is used as both read and write in the same pass
            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
                pass.SetBindGroup(0, base);
                pass.SetBindGroup(1, bg1);
                pass.SetBindGroup(1, bg0);
                pass.EndPass();
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }
        }
    }

    // Test that all unused bindings bindGroup still take effect for resource tracking
    TEST_F(ResourceRWTrackingTest, BufferWithUnusedBindings) {
        // Create buffers
        wgpu::Buffer buffer0 =
            CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index);
        wgpu::Buffer buffer1 = CreateBuffer(4, wgpu::BufferUsage::Storage);

        DummyRenderPass dummyRenderPass(device);

        // Test render pass for bind group, the conflict resides in compute stage only
        {
            // Create the bind group which contains both fragment and compute stages in a single
            // bind group
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer},
                 {2, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer}});
            wgpu::BindGroup bg = utils::MakeBindGroup(
                device, bgl, {{0, buffer0, 0, 4}, {1, buffer1, 0, 4}, {2, buffer1, 0, 4}});

            // Resource in compute stage is incorrect, but it is not used in render pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Test render pass for bind group and index buffer
        {
            // Create the bind group which contains compute stage
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer}});
            wgpu::BindGroup bg =
                utils::MakeBindGroup(device, bgl, {{0, buffer0, 0, 4}, {1, buffer1, 0, 4}});

            // Resource in compute stage in bind group conflicts with index buffer, but bindings for
            // compute stage is not used in render pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetIndexBuffer(buffer0);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Test compute pass for bind group, the conflict resides in fragment stage only
        {
            // Create the bind group which contains both fragment and compute stages in a single
            // bind group
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer},
                 {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer},
                 {2, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer}});
            wgpu::BindGroup bg = utils::MakeBindGroup(
                device, bgl, {{0, buffer0, 0, 4}, {1, buffer0, 0, 4}, {2, buffer1, 0, 4}});

            // Resource in fragment stage is incorrect, but it is not used in compute pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Test compute pass for bind group, the conflict resides between compute stage and fragment
        // stage
        {
            // Create the bind group which contains both fragment and compute stages in a single
            // bind group
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer},
                         {1, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer}});
            wgpu::BindGroup bg =
                utils::MakeBindGroup(device, bgl, {{0, buffer0, 0, 4}, {1, buffer0, 0, 4}});

            // Resource in fragment stage conflits with resource in compute stage, but fragment
            // stage is not used in compute pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test that using the same buffer as both readable and writable in different passes is allowed
    TEST_F(ResourceRWTrackingTest, BufferWithReadAndWriteUsageOnDifferentPasses) {
        // Test render pass
        {
            // Create buffers that will be used as index and storage buffers
            wgpu::Buffer buffer0 =
                CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index);
            wgpu::Buffer buffer1 =
                CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Index);

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

        // Test compute pass
        {
            // Create buffer and bind groups that will be used as storage and uniform bindings
            wgpu::Buffer buffer =
                CreateBuffer(4, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Uniform);

            wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer}});
            wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl0, {{0, buffer, 0, 4}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl1, {{0, buffer, 0, 4}});

            // Use these two buffers as both storage and uniform in the different passes
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass0 = encoder.BeginComputePass();
            pass0.SetBindGroup(0, bg0);
            pass0.EndPass();
            wgpu::ComputePassEncoder pass1 = encoder.BeginComputePass();
            pass1.SetBindGroup(1, bg1);
            pass1.EndPass();
            encoder.Finish();
        }
    }

    // Test that using the same buffer as both readable and writable in the different draws
    TEST_F(ResourceRWTrackingTest, BufferWithReadAndWriteUsageOnDifferentDrawsOrDispatches) {
        // Create a buffer that will be used as a storage buffer
        wgpu::Buffer buffer = CreateBuffer(4, wgpu::BufferUsage::Storage);

        // Test render pass
        {
            // Create the bind group to use the buffer as both readonly storage and writable storage
            // bindings
            wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer}});
            wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl0, {{0, buffer, 0, 4}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl1, {{0, buffer, 0, 4}});

            // It is not allowed to use the same buffer as both readable and writable in different
            // draws within the same render pass.
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetBindGroup(0, bg0);
            pass.Draw(3, 1, 0, 0);
            pass.SetBindGroup(0, bg1);
            pass.Draw(3, 1, 0, 0);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // test compute pass
        {
            // Create the bind group to use the buffer as both readonly storage and writable storage
            // bindings
            wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer}});
            wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl0, {{0, buffer, 0, 4}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl1, {{0, buffer, 0, 4}});

            // It is not allowed to use the same buffer as both readable and writable in different
            // dispatches within the same compute pass.
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg0);
            pass.Dispatch(1, 1, 1);
            pass.SetBindGroup(0, bg1);
            pass.Dispatch(1, 1, 1);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test that using a single texture in multiple read usages in the same pass is allowed.
    TEST_F(ResourceRWTrackingTest, TextureWithMultipleReadUsages) {
        // Create a texture that will be used both as sampled and readonly storage texture
        wgpu::Texture texture =
            CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::Storage,
                          wgpu::TextureFormat::RGBA8Unorm);
        wgpu::TextureView view = texture.CreateView();

        {
            // Create the bind group to use the texture as sampled texture and readonly storage
            // texture bindings
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture},
                 {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageTexture}});
            wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view}, {1, view}});

            // Use the texture as both sampeld and readonly storage in the same render pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            encoder.Finish();
        }

        {
            // Create the bind group to use the texture as sampled texture and readonly storage
            // texture bindings
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageTexture}});
            wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view}, {1, view}});

            // Use the texture as both sampeld and readonly storage in the same compute pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            encoder.Finish();
        }
    }

    // Test that using the same texture as both readable and writable in the same pass is disallowed
    TEST_F(ResourceRWTrackingTest, TextureWithReadAndWriteUsage) {
        // Test render pass
        {
            // Create a texture that will be used both as a sampled texture and a render target
            wgpu::Texture texture =
                CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment,
                              wgpu::TextureFormat::RGBA8Unorm);
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

        // test compute pass
        {
            // Create a texture that will be used both as sampled and writeonly storage texture
            wgpu::Texture texture =
                CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::Storage,
                              wgpu::TextureFormat::RGBA8Unorm);
            wgpu::TextureView view = texture.CreateView();

            // Create the bind group to use the texture as sampled
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::WriteonlyStorageTexture}});
            wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view}, {1, view}});

            // Use the texture as both sampeld and writeonly storage in the same pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test that using a single texture as copy src/dst and writable/readable usage in pass is
    // allowed.
    TEST_F(ResourceRWTrackingTest, TextureCopyAndTextureUsageInPass) {
        // Create a texture that will be used both as a sampled texture and a render target
        wgpu::Texture texture0 =
            CreateTexture(wgpu::TextureUsage::CopySrc, wgpu::TextureFormat::RGBA8Unorm);
        wgpu::Texture texture1 =
            CreateTexture(wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled |
                              wgpu::TextureUsage::OutputAttachment,
                          wgpu::TextureFormat::RGBA8Unorm);
        wgpu::TextureView view0 = texture0.CreateView();
        wgpu::TextureView view1 = texture1.CreateView();

        wgpu::TextureCopyView srcView = utils::CreateTextureCopyView(texture0, 0, 0, {0, 0, 0});
        wgpu::TextureCopyView dstView = utils::CreateTextureCopyView(texture1, 0, 0, {0, 0, 0});
        wgpu::Extent3D copySize = {1, 1, 1};

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
            // Create the bind group to use the texture as sampled
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture}});
            wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view1}});

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            encoder.CopyTextureToTexture(&srcView, &dstView, &copySize);
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            encoder.Finish();
        }
    }

    // Test that all consecutive SetBindGroup()s take effect even though some bind groups are not
    // used because they are overwritten by a consecutive call.
    TEST_F(ResourceRWTrackingTest, TextureWithMultipleSetBindGroupsOnSameIndex) {
        // Test render pass
        {
            // Create a texture that will be used both as a sampled texture and a render target
            wgpu::Texture texture0 =
                CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment,
                              wgpu::TextureFormat::RGBA8Unorm);
            wgpu::TextureView view0 = texture0.CreateView();
            wgpu::Texture texture1 =
                CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment,
                              wgpu::TextureFormat::RGBA8Unorm);
            wgpu::TextureView view1 = texture1.CreateView();

            // Create the bind group to use the texture as sampled
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl, {{0, view0}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, view1}});

            // Create the render pass that will use the texture as an output attachment
            utils::ComboRenderPassDescriptor renderPass({view0});

            // Set bind group against the same index twice. The second one overwrites the first one.
            // Then texture is not used as both sampeld and output attachment in the same pass
            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
                pass.SetBindGroup(0, bg0);
                pass.SetBindGroup(0, bg1);
                pass.EndPass();
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }

            // Set bind group against the same index twice. The second one overwrites the first one.
            // Then texture is used as both sampeld and output attachment in the same pass
            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
                pass.SetBindGroup(0, bg1);
                pass.SetBindGroup(0, bg0);
                pass.EndPass();
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }
        }

        // Test compute pass
        {
            // Create a texture that will be used both as a sampled texture and a render target
            wgpu::Texture texture0 =
                CreateTexture(wgpu::TextureUsage::Storage, wgpu::TextureFormat::RGBA8Unorm);
            wgpu::TextureView view0 = texture0.CreateView();
            wgpu::Texture texture1 =
                CreateTexture(wgpu::TextureUsage::Storage, wgpu::TextureFormat::RGBA8Unorm);
            wgpu::TextureView view1 = texture1.CreateView();

            // Create the bind group to use the texture as sampled
            wgpu::BindGroupLayout baseBGL = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::WriteonlyStorageTexture}});
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageTexture}});
            wgpu::BindGroup base = utils::MakeBindGroup(device, baseBGL, {{0, view0}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl, {{0, view0}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, view1}});

            // Set bind group against the same index twice. The second one overwrites the first one.
            // Then texture is not used as both sampeld and output attachment in the same pass
            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
                pass.SetBindGroup(0, base);
                pass.SetBindGroup(1, bg0);
                pass.SetBindGroup(1, bg1);
                pass.EndPass();
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }

            // Set bind group against the same index twice. The second one overwrites the first one.
            // Then texture is used as both sampeld and output attachment in the same pass
            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
                pass.SetBindGroup(0, base);
                pass.SetBindGroup(1, bg1);
                pass.SetBindGroup(1, bg0);
                pass.EndPass();
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }
        }
    }

    // Test that all unused bindings bindGroup still take effect for resource tracking
    TEST_F(ResourceRWTrackingTest, TextureWithUnusedBindings) {
        // Create textures
        wgpu::Texture texture0 =
            CreateTexture(wgpu::TextureUsage::Storage | wgpu::TextureUsage::OutputAttachment,
                          wgpu::TextureFormat::RGBA8Unorm);
        wgpu::TextureView view0 = texture0.CreateView();
        wgpu::Texture texture1 =
            CreateTexture(wgpu::TextureUsage::Storage, wgpu::TextureFormat::RGBA8Unorm);
        wgpu::TextureView view1 = texture1.CreateView();

        // Test render pass for bind group, the conflict resides in compute stage only
        {
            // Create the bind group which contains both fragment and compute stages in a single
            // bind group
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageTexture},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::WriteonlyStorageTexture},
                 {2, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageTexture}});
            wgpu::BindGroup bg =
                utils::MakeBindGroup(device, bgl, {{0, view0}, {1, view1}, {2, view1}});

            // Resource in compute stage is incorrect, but it is not used in render pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            DummyRenderPass dummyRenderPass(device);
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&dummyRenderPass);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Test render pass for bind group and output attachment
        {
            // Create the render pass that will use the texture as an output attachment
            utils::ComboRenderPassDescriptor renderPass({view0});

            // Create the bind group which contains compute stage
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageTexture},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::WriteonlyStorageTexture}});
            wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view0}, {1, view1}});

            // Resource in compute stage in bind group conflicts with index buffer, but bindings for
            // compute stage is not used in render pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Test compute pass for bind group, the conflict resides in fragment stage only
        {
            // Create the bind group which contains both fragment and compute stages in a single
            // bind group
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageTexture},
                 {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::WriteonlyStorageTexture},
                 {2, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageTexture}});
            wgpu::BindGroup bg =
                utils::MakeBindGroup(device, bgl, {{0, view0}, {1, view0}, {2, view1}});

            // Resource in fragment stage is incorrect, but it is not used in compute pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }

        // Test compute pass for bind group, the conflict resides between compute stage and fragment
        // stage
        {
            // Create the bind group which contains both fragment and compute stages in a single
            // bind group
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageTexture},
                 {1, wgpu::ShaderStage::Compute, wgpu::BindingType::WriteonlyStorageTexture}});
            wgpu::BindGroup bg = utils::MakeBindGroup(device, bgl, {{0, view0}, {1, view0}});

            // Resource in fragment stage conflits with resource in compute stage, but fragment
            // stage is not used in compute pass
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetBindGroup(0, bg);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    // Test that using the same texture as both readable and writable in the different passes is
    // allowed
    TEST_F(ResourceRWTrackingTest, TextureWithReadAndWriteUsageInDifferentPasses) {
        // Test render pass
        {
            // Create a texture that will be used both as a sampled texture and a render target
            wgpu::Texture t0 =
                CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment,
                              wgpu::TextureFormat::RGBA8Unorm);
            wgpu::TextureView v0 = t0.CreateView();
            wgpu::Texture t1 =
                CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment,
                              wgpu::TextureFormat::RGBA8Unorm);
            wgpu::TextureView v1 = t1.CreateView();

            // Create the bind group to use the texture as sampled
            wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
                device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::SampledTexture}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl, {{0, v0}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl, {{0, v1}});

            // Create the render pass that will use the texture as an output attachment
            utils::ComboRenderPassDescriptor renderPass0({v1});
            utils::ComboRenderPassDescriptor renderPass1({v0});

            // Use the texture as both sampeld and output attachment in the different passes
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass0 = encoder.BeginRenderPass(&renderPass0);
            pass0.SetBindGroup(0, bg0);
            pass0.EndPass();
            wgpu::RenderPassEncoder pass1 = encoder.BeginRenderPass(&renderPass1);
            pass1.SetBindGroup(0, bg1);
            pass1.EndPass();
            encoder.Finish();
        }

        // Test compute pass
        {
            // Create a texture that will be used both as a sampled texture and a render target
            wgpu::Texture texture =
                CreateTexture(wgpu::TextureUsage::Storage, wgpu::TextureFormat::RGBA8Unorm);
            wgpu::TextureView view = texture.CreateView();

            // Create the bind group to use the texture as sampled
            wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageTexture}});
            wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
                device,
                {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::WriteonlyStorageTexture}});
            wgpu::BindGroup bg0 = utils::MakeBindGroup(device, bgl0, {{0, view}});
            wgpu::BindGroup bg1 = utils::MakeBindGroup(device, bgl1, {{0, view}});

            // Use the texture as both readonly and writeonly storage in the different passes
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass0 = encoder.BeginComputePass();
            pass0.SetBindGroup(0, bg0);
            pass0.EndPass();
            wgpu::ComputePassEncoder pass1 = encoder.BeginComputePass();
            pass1.SetBindGroup(0, bg1);
            pass1.EndPass();
            encoder.Finish();
        }
    }

    // Test that using the same texture as both readable and writable in the different passes is
    // allowed
    TEST_F(ResourceRWTrackingTest, TextureWithReadAndWriteUsageOnDifferentDrawsOrDispatches) {
        // Create a texture that will be used both as a sampled texture and a storage texture
        wgpu::Texture t0 = CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::Storage,
                                         wgpu::TextureFormat::RGBA8Unorm);
        wgpu::TextureView v0 = t0.CreateView();
        wgpu::Texture t1 = CreateTexture(wgpu::TextureUsage::Sampled | wgpu::TextureUsage::Storage,
                                         wgpu::TextureFormat::RGBA8Unorm);
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
