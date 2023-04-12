// Copyright 2023 The Dawn Authors
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

#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/TestUtils.h"
#include "dawn/utils/TextureUtils.h"
#include "dawn/utils/WGPUHelpers.h"

class MultithreadTests : public DawnTest {
  protected:
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> features;
        // TODO(crbug.com/dawn/1678): DawnWire doesn't support thread safe API yet.
        if (!UsesWire()) {
            features.push_back(wgpu::FeatureName::ImplicitDeviceSynchronization);
        }
        return features;
    }

    void SetUp() override {
        DawnTest::SetUp();
        // TODO(crbug.com/dawn/1678): DawnWire doesn't support thread safe API yet.
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());

        // TODO(crbug.com/dawn/1679): OpenGL backend doesn't support thread safe API yet.
        DAWN_TEST_UNSUPPORTED_IF(IsOpenGL() || IsOpenGLES());
    }

    wgpu::Buffer CreateBuffer(uint32_t size, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = size;
        descriptor.usage = usage;
        return device.CreateBuffer(&descriptor);
    }

    wgpu::Texture CreateTexture(uint32_t width,
                                uint32_t height,
                                wgpu::TextureFormat format,
                                wgpu::TextureUsage usage,
                                uint32_t mipLevelCount = 1,
                                uint32_t sampleCount = 1) {
        wgpu::TextureDescriptor texDescriptor = {};
        texDescriptor.size = {width, height, 1};
        texDescriptor.format = format;
        texDescriptor.usage = usage;
        texDescriptor.mipLevelCount = mipLevelCount;
        texDescriptor.sampleCount = sampleCount;
        return device.CreateTexture(&texDescriptor);
    }
};

// Test that encoding render passes in parallel should work
TEST_P(MultithreadTests, RenderPassEncodersInParallel) {
    constexpr uint32_t kRTSize = 16;

    wgpu::Texture msaaRenderTarget =
        CreateTexture(kRTSize, kRTSize, wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
                      /*mipLevelCount=*/1, /*sampleCount=*/4);
    wgpu::TextureView msaaRenderTargetView = msaaRenderTarget.CreateView();

    // Resolve to mip level = 1 to force render pass workarounds (there shouldn't be any deadlock
    // happening).
    wgpu::Texture resolveTarget =
        CreateTexture(kRTSize * 2, kRTSize * 2, wgpu::TextureFormat::RGBA8Unorm,
                      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
                      /*mipLevelCount=*/2, /*sampleCount=*/1);
    wgpu::TextureViewDescriptor resolveTargetViewDesc;
    resolveTargetViewDesc.baseMipLevel = 1;
    resolveTargetViewDesc.mipLevelCount = 1;
    wgpu::TextureView resolveTargetView = resolveTarget.CreateView(&resolveTargetViewDesc);

    std::vector<std::unique_ptr<std::thread>> threads(10);
    std::vector<wgpu::CommandBuffer> commandBuffers(threads.size());

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::make_unique<std::thread>([=, &commandBuffers] {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

            // Clear the renderTarget to red.
            utils::ComboRenderPassDescriptor renderPass({msaaRenderTargetView});
            renderPass.cColorAttachments[0].resolveTarget = resolveTargetView;
            renderPass.cColorAttachments[0].clearValue = {1.0f, 0.0f, 0.0f, 1.0f};

            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
            pass.End();

            commandBuffers[i] = encoder.Finish();
        });
    }

    for (auto& thread : threads) {
        thread->join();
    }

    // Verify command buffers
    for (auto& commandBuffer : commandBuffers) {
        queue.Submit(1, &commandBuffer);

        EXPECT_TEXTURE_EQ(utils::RGBA8::kRed, resolveTarget, {0, 0}, 1);
        EXPECT_TEXTURE_EQ(utils::RGBA8::kRed, resolveTarget, {kRTSize - 1, kRTSize - 1}, 1);
    }
}

// Test that encoding compute passes in parallel should work
TEST_P(MultithreadTests, ComputePassEncodersInParallel) {
    constexpr uint32_t kExpected = 0xFFFFFFFFu;
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
            @group(0) @binding(0) var<storage, read_write> output : u32;

            @compute @workgroup_size(1, 1, 1)
            fn main(@builtin(global_invocation_id) GlobalInvocationID : vec3u) {
                output = 0xFFFFFFFFu;
            })");
    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";
    auto pipeline = device.CreateComputePipeline(&csDesc);

    wgpu::Buffer dstBuffer =
        CreateBuffer(sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc |
                                           wgpu::BufferUsage::CopyDst);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {
                                                         {0, dstBuffer, 0, sizeof(uint32_t)},
                                                     });

    std::vector<std::unique_ptr<std::thread>> threads(10);
    std::vector<wgpu::CommandBuffer> commandBuffers(threads.size());

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i] = std::make_unique<std::thread>([=, &commandBuffers] {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.DispatchWorkgroups(1, 1, 1);
            pass.End();

            commandBuffers[i] = encoder.Finish();
        });
    }

    for (auto& thread : threads) {
        thread->join();
    }

    // Verify command buffers
    for (auto& commandBuffer : commandBuffers) {
        constexpr uint32_t kSentinelData = 0;
        queue.WriteBuffer(dstBuffer, 0, &kSentinelData, sizeof(kSentinelData));
        queue.Submit(1, &commandBuffer);

        EXPECT_BUFFER_U32_EQ(kExpected, dstBuffer, 0);
    }
}

DAWN_INSTANTIATE_TEST(MultithreadTests,
                      D3D12Backend(),
                      D3D12Backend({"always_resolve_into_zero_level_and_layer"}),
                      MetalBackend(),
                      MetalBackend({"always_resolve_into_zero_level_and_layer"}),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend(),
                      VulkanBackend({"always_resolve_into_zero_level_and_layer"}));
