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
protected:
    dawn::CommandBuffer CreateSimpleComputeCommandBuffer(
            const dawn::ComputePipeline& pipeline, const dawn::BindGroup& bindGroup) {
        dawn::CommandBufferBuilder builder = device.CreateCommandBufferBuilder();
        dawn::ComputePassEncoder pass = builder.BeginComputePass();
        pass.SetComputePipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.Dispatch(1, 1, 1);
        pass.EndPass();
        return builder.GetResult();
    }
};

// Test a bindgroup reused in two command buffers in the same call to queue.Submit().
// This test passes by not asserting or crashing.
TEST_P(BindGroupTests, ReusedBindGroupSingleSubmit) {
    dawn::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device,
        {
            {0, dawn::ShaderStageBit::Vertex | dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer },
        }
    );
    dawn::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

    const char* shader = R"(
        #version 450
        layout(std140, set = 0, binding = 0) uniform Contents {
            float f;
        } contents;
        void main() {
        }
    )";

    dawn::ShaderModule module =
        utils::CreateShaderModule(device, dawn::ShaderStage::Compute, shader);
    dawn::ComputePipelineDescriptor cpDesc;
    cpDesc.module = module;
    cpDesc.entryPoint = "main";
    cpDesc.layout = pl;
    dawn::ComputePipeline cp = device.CreateComputePipeline(&cpDesc);

    dawn::BufferDescriptor bufferDesc;
    bufferDesc.size = sizeof(float);
    bufferDesc.usage = dawn::BufferUsageBit::TransferDst |
                       dawn::BufferUsageBit::Uniform;
    dawn::Buffer buffer = device.CreateBuffer(&bufferDesc);
    dawn::BufferView bufferView =
        buffer.CreateBufferViewBuilder().SetExtent(0, sizeof(float)).GetResult();
    dawn::BindGroup bindGroup = device.CreateBindGroupBuilder()
        .SetLayout(bgl)
        .SetBufferViews(0, 1, &bufferView)
        .GetResult();

    dawn::CommandBuffer cb[2];
    cb[0] = CreateSimpleComputeCommandBuffer(cp, bindGroup);
    cb[1] = CreateSimpleComputeCommandBuffer(cp, bindGroup);
    queue.Submit(2, cb);
}

constexpr static unsigned int kRTSize = 8;

TEST_P(BindGroupTests, ReusedUBO) {
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    dawn::ShaderModule vsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
        #version 450
        layout (set = 0, binding = 0) uniform vertexUniformBuffer {
            mat2 transform;
        };
        void main() {
            const vec2 pos[3] = vec2[3](vec2(-1.f, -1.f), vec2(1.f, -1.f), vec2(-1.f, 1.f));
            gl_Position = vec4(transform * pos[gl_VertexIndex], 0.f, 1.f);
        })"
    );

    dawn::ShaderModule fsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
        #version 450
        layout (set = 0, binding = 1) uniform fragmentUniformBuffer {
            vec4 color;
        };
        layout(location = 0) out vec4 fragColor;
        void main() {
            fragColor = color;
        })"
    );

    dawn::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device,
        {
            {0, dawn::ShaderStageBit::Vertex, dawn::BindingType::UniformBuffer },
            {1, dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer },
        }
    );
    dawn::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bgl);

    dawn::RenderPipeline pipeline = device.CreateRenderPipelineBuilder()
        .SetColorAttachmentFormat(0, renderPass.colorFormat)
	.SetLayout(pipelineLayout)
        .SetPrimitiveTopology(dawn::PrimitiveTopology::TriangleList)
        .SetStage(dawn::ShaderStage::Vertex, vsModule, "main")
        .SetStage(dawn::ShaderStage::Fragment, fsModule, "main")
        .GetResult();
    constexpr float transform[] = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f };
    constexpr float color[] = { 0.f, 1.f, 0.f, 1.f };
    dawn::BufferDescriptor bufferDesc;
    bufferDesc.size = 512;
    bufferDesc.usage = dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform;
    dawn::Buffer buffer = device.CreateBuffer(&bufferDesc);
    buffer.SetSubData(0, sizeof(transform), reinterpret_cast<const uint8_t *>(transform));
    buffer.SetSubData(256, sizeof(color), reinterpret_cast<const uint8_t *>(color));
    dawn::BufferView vertUBOBufferView =
        buffer.CreateBufferViewBuilder().SetExtent(0, sizeof(transform)).GetResult();
    dawn::BufferView fragUBOBufferView =
        buffer.CreateBufferViewBuilder().SetExtent(256, sizeof(color)).GetResult();
    dawn::BindGroup bindGroup = device.CreateBindGroupBuilder()
        .SetLayout(bgl)
        .SetBufferViews(0, 1, &vertUBOBufferView)
        .SetBufferViews(1, 1, &fragUBOBufferView)
        .GetResult();

    dawn::CommandBufferBuilder builder = device.CreateCommandBufferBuilder();
    dawn::RenderPassEncoder pass = builder.BeginRenderPass(renderPass.renderPassInfo);
    pass.SetRenderPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.DrawArrays(3, 1, 0, 0);
    pass.EndPass();

    dawn::CommandBuffer commands = builder.GetResult();
    queue.Submit(1, &commands);

    RGBA8 filled(0, 255, 0, 255);
    RGBA8 notFilled(0, 0, 0, 0);
    int min = 1, max = kRTSize - 3;
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color,    min, min);
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color,    max, min);
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color,    min, max);
    EXPECT_PIXEL_RGBA8_EQ(notFilled, renderPass.color, max, max);
}

DAWN_INSTANTIATE_TEST(BindGroupTests, D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend);
