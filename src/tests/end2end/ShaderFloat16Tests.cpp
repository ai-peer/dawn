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

#include "tests/DawnTest.h"

#include "utils/WGPUHelpers.h"

class ShaderFloat16Tests : public DawnTest {
  protected:
    std::vector<const char*> GetRequiredExtensions() override {
        mIsShaderFloat16Supported = SupportsExtensions({"shader_float16"});
        if (!mIsShaderFloat16Supported) {
            return {};
        }

        return {"shader_float16"};
    }

    bool IsShaderFloat16Supported() const {
        return mIsShaderFloat16Supported;
    }

    bool mIsShaderFloat16Supported = false;
};

// Test basic 16bit float arithmetic and 16bit storage features.
TEST_P(ShaderFloat16Tests, VendorIdFilter) {
    DAWN_SKIP_TEST_IF(!IsShaderFloat16Supported());

    uint16_t uniformData[] = {15596, 0};  // to float16 = {1.23, 0}, 0 is a padding.
    wgpu::Buffer uniformBuffer = utils::CreateBufferFromData(
        device, &uniformData, sizeof(uniformData), wgpu::BufferUsage::Uniform);

    uint16_t bufferInData[] = {16558, 0};  // to float16 = {2.34, 0}, 0 is a padding.
    wgpu::Buffer bufferIn = utils::CreateBufferFromData(device, &bufferInData, sizeof(bufferInData),
                                                        wgpu::BufferUsage::Storage);

    uint16_t bufferOutData[] = {0, 0};
    wgpu::Buffer bufferOut =
        utils::CreateBufferFromData(device, &bufferOutData, sizeof(bufferOutData),
                                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    wgpu::ShaderModule module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
        #version 450

        #extension GL_AMD_gpu_shader_half_float : require

        struct S {
            float16_t f;
            float16_t padding;
        };
        layout(std140, set = 0, binding = 0) uniform uniformBuf {
            S c;
        };

        layout(std140, set = 0, binding = 1) buffer bufA {
            S a;
        } ;

        layout(std140, set = 0, binding = 2) buffer bufB {
            S b;
        } ;

        void main() {
            b.f = a.f + c.f;
        }

        )");

    auto bgl = utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer},
                    {1, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer},
                    {2, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer},
                });

    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.layout = pl;
    csDesc.computeStage.module = module;
    csDesc.computeStage.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl,
                                                     {
                                                         {0, uniformBuffer, 0, sizeof(uniformData)},
                                                         {1, bufferIn, 0, sizeof(bufferInData)},
                                                         {2, bufferOut, 0, sizeof(bufferOutData)},
                                                     });

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Dispatch(1);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    uint16_t expected[] = {17188, 0};  // to float16 = {3.57, 0}, 0 is a padding.

    EXPECT_BUFFER_U16_RANGE_EQ(expected, bufferOut, 0, 2);
}

DAWN_INSTANTIATE_TEST(ShaderFloat16Tests, VulkanBackend());