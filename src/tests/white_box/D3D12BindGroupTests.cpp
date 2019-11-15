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

#include "tests/DawnTest.h"

#include <d3d12.h>

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr static unsigned int kRTSize = 8;

class D3D12BindGroupTests : public DawnTest {
  protected:
    wgpu::RenderPipeline MakeRenderPipeline(const wgpu::BindGroupLayout& bgl) {
        utils::ComboRenderPipelineDescriptor desc(device);

        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            void main() {
                gl_Position = vec4(0.0f, 0.0f, 0.0f, 0.0f);
            })");

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
                #version 450
                layout(set = 0, binding = 0) uniform sampler sampler0;
                layout(location = 0) out vec4 fragColor;
                void main() {
                    fragColor = vec4(0.0, 0.0, 0.0, 0.0);
                })");

        desc.vertexStage.module = vsModule;
        desc.cFragmentStage.module = fsModule;
        desc.layout = utils::MakeBasicPipelineLayout(device, &bgl);

        return device.CreateRenderPipeline(&desc);
    }
};

// Test that exercises the logic of bind group allocation spills.
TEST_P(D3D12BindGroupTests, BindGroupAllocationSpill) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

    wgpu::RenderPipeline pipeline = MakeRenderPipeline(layout);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    pass.SetPipeline(pipeline);

    // Max number of samplers in a GPU descriptor heap is 2048, the smallest limit.
    // The first 2048 draws fill up the current heap then last one causes a spill.
    const uint32_t kDrawNum = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE + 1;

    for (uint32_t drawNum = 0; drawNum < kDrawNum; drawNum++) {
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, layout, {{0, sampler}});
        pass.SetBindGroup(0, bindGroup);
        pass.Draw(3, 1, 0, 0);
    }

    pass.EndPass();

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
}

DAWN_INSTANTIATE_TEST(D3D12BindGroupTests, D3D12Backend);