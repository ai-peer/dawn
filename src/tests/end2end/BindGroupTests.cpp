// Copyright 2018 The Dawn Authors
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
#include "utils/DawnHelpers.h"

class BindGroupTests : public DawnTest {
};

TEST_P(BindGroupTests, MultipleBindGroupsPerSubmit) {
    // Test a bindgroup reused in two command buffers in the same call to queue.Submit().
    // This test passes by not asserting or crashing.
    dawn::PipelineLayout layout;
    typedef float Contents;
    auto bgl = utils::MakeBindGroupLayout(
        device,
        {
            {0, dawn::ShaderStageBit::Compute, dawn::BindingType::UniformBuffer },
        }
    );
    dawn::BufferDescriptor bufferDesc;
    bufferDesc.size = sizeof(Contents);
    bufferDesc.usage = dawn::BufferUsageBit::TransferSrc | dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform;
    dawn::Buffer buffer = device.CreateBuffer(&bufferDesc);
    Contents contents = {0};
    buffer.SetSubData(0, sizeof(Contents), reinterpret_cast<uint8_t*>(&contents));
    auto bufferView = buffer.CreateBufferViewBuilder().SetExtent(0, sizeof(Contents)).GetResult();
    dawn::BindGroup bindGroup = device.CreateBindGroupBuilder()
        .SetLayout(bgl)
        .SetBufferViews(0, 1, &bufferView)
        .GetResult();
    dawn::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

    const char* shader = R"(
        #version 450
        layout(std140, set = 0, binding = 0) uniform Contents {
            float f;
        } contents;
        void main() {
        }
    )";

    auto module = utils::CreateShaderModule(device, dawn::ShaderStage::Compute, shader);

    dawn::ComputePipelineDescriptor cpDesc;
    cpDesc.module = module.Clone();
    cpDesc.entryPoint = "main";
    cpDesc.layout = pl.Clone();
    dawn::ComputePipeline cp = device.CreateComputePipeline(&cpDesc);

    dawn::CommandBuffer cb[2];
    auto builder1 = device.CreateCommandBufferBuilder();
    auto pass1 = builder1.BeginComputePass();
    pass1.SetComputePipeline(cp);
    pass1.SetBindGroup(0, bindGroup);
    pass1.Dispatch(1, 1, 1);
    pass1.EndPass();
    cb[0] = builder1.GetResult();

    auto builder2 = device.CreateCommandBufferBuilder();
    auto pass2 = builder2.BeginComputePass();
    pass2.SetComputePipeline(cp);
    pass2.SetBindGroup(0, bindGroup);
    pass2.Dispatch(1, 1, 1);
    pass2.EndPass();
    cb[1] = builder2.GetResult();

    queue.Submit(2, cb);
}

DAWN_INSTANTIATE_TEST(BindGroupTests, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend);
