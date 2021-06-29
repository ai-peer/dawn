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

#include "tests/unittests/validation/ValidationTest.h"

#include "common/Constants.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr uint32_t kRTSize = 4;

namespace {

    class RenderPassCommandValidationTest : public ValidationTest {
      public:
        using VertexBufferListT =
            std::initializer_list<std::tuple<uint32_t, const wgpu::Buffer&, uint64_t, uint64_t>>;

        void SetUp() override {
            ValidationTest::SetUp();

            renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

            vsModule = utils::CreateShaderModule(device, R"(
            [[stage(vertex)]]
            fn main([[location(0)]] pos : vec4<f32>) -> [[builtin(position)]] vec4<f32> {
                return pos;
            })");

            vsModuleForTwoBuffers = utils::CreateShaderModule(device, R"(
            [[stage(vertex)]]
            fn main([[location(0)]] pos : vec4<f32>, [[location(3)]] uv : vec2<f32>) -> [[builtin(position)]] vec4<f32> {
                return pos;
            })");

            fsModule = utils::CreateShaderModule(device, R"(
            [[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {
                return vec4<f32>(0.0, 1.0, 0.0, 1.0);
            })");
        }

        void TestRenderPassDraw(const wgpu::RenderPipeline& pipeline,
                                VertexBufferListT vertexBufferList,
                                uint32_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance,
                                bool isSuccess) {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder renderPassEncoder =
                encoder.BeginRenderPass(GetBasicRenderPassDescriptor());
            renderPassEncoder.SetPipeline(pipeline);

            for (auto vertexBufferParam : vertexBufferList) {
                uint32_t slot = std::get<0>(vertexBufferParam);
                const wgpu::Buffer& buffer = std::get<1>(vertexBufferParam);
                uint64_t offset = std::get<2>(vertexBufferParam);
                uint64_t size = std::get<3>(vertexBufferParam);

                renderPassEncoder.SetVertexBuffer(slot, buffer, offset, size);
            }
            renderPassEncoder.Draw(vertexCount, instanceCount, firstVertex, firstInstance);
            renderPassEncoder.EndPass();

            if (isSuccess) {
                encoder.Finish();
            } else {
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }
        }

        void TestRenderPassDrawIndexed(const wgpu::RenderPipeline& pipeline,
                                       const wgpu::Buffer& indexBuffer,
                                       VertexBufferListT vertexBufferList,
                                       uint32_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t baseVertex,
                                       uint32_t firstInstance,
                                       bool isSuccess) {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder renderPassEncoder =
                encoder.BeginRenderPass(GetBasicRenderPassDescriptor());
            renderPassEncoder.SetPipeline(pipeline);

            renderPassEncoder.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);

            for (auto vertexBufferParam : vertexBufferList) {
                uint32_t slot = std::get<0>(vertexBufferParam);
                const wgpu::Buffer& buffer = std::get<1>(vertexBufferParam);
                uint64_t offset = std::get<2>(vertexBufferParam);
                uint64_t size = std::get<3>(vertexBufferParam);

                renderPassEncoder.SetVertexBuffer(slot, buffer, offset, size);
            }
            renderPassEncoder.DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex,
                                          firstInstance);
            renderPassEncoder.EndPass();

            if (isSuccess) {
                encoder.Finish();
            } else {
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }
        }

        const wgpu::RenderPassDescriptor* GetBasicRenderPassDescriptor() const {
            return &renderPass.renderPassInfo;
        }

        void SetBasicRenderPipelineDescriptor(utils::ComboRenderPipelineDescriptor& descriptor) {
            descriptor.vertex.module = vsModule;
            descriptor.cFragment.module = fsModule;
            descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
            descriptor.vertex.bufferCount = 1;
            descriptor.cBuffers[0].arrayStride = 4 * sizeof(float);
            descriptor.cBuffers[0].attributeCount = 1;
            descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
            descriptor.cTargets[0].format = renderPass.colorFormat;
        }

        void SetBasicRenderPipelineDescriptorWithInstance(
            utils::ComboRenderPipelineDescriptor& descriptor) {
            descriptor.vertex.module = vsModuleForTwoBuffers;
            descriptor.cFragment.module = fsModule;
            descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

            descriptor.vertex.bufferCount = 2;

            descriptor.cBuffers[0].arrayStride = 4 * sizeof(float);
            descriptor.cBuffers[0].attributeCount = 1;
            descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;

            descriptor.cBuffers[1].arrayStride = 2 * sizeof(float);
            descriptor.cBuffers[1].stepMode = wgpu::InputStepMode::Instance;
            descriptor.cBuffers[1].attributeCount = 1;
            descriptor.cBuffers[1].attributes = &descriptor.cAttributes[1];
            descriptor.cAttributes[1].format = wgpu::VertexFormat::Float32x2;
            descriptor.cAttributes[1].shaderLocation = 3;

            descriptor.cTargets[0].format = renderPass.colorFormat;
        }

        wgpu::Buffer CreateBuffer(uint64_t size,
                                  wgpu::BufferUsage usage = wgpu::BufferUsage::Vertex) {
            wgpu::BufferDescriptor descriptor;
            descriptor.size = size;
            descriptor.usage = usage;
            wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

            return buffer;
        }

      private:
        wgpu::ShaderModule vsModule;
        wgpu::ShaderModule vsModuleForTwoBuffers;
        wgpu::ShaderModule fsModule;
        utils::BasicRenderPass renderPass;
    };

    TEST_F(RenderPassCommandValidationTest, DrawBasic) {
        utils::ComboRenderPipelineDescriptor descriptor;
        SetBasicRenderPipelineDescriptor(descriptor);
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

        wgpu::Buffer vertexBuffer = CreateBuffer(3 * 4 * sizeof(float));

        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}};
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 0, 0, true);
    }

    TEST_F(RenderPassCommandValidationTest, DrawVertexBufferOutOfBoundWithoutInstance) {
        utils::ComboRenderPipelineDescriptor descriptor;
        SetBasicRenderPipelineDescriptor(descriptor);
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

        // Build vertex buffer for 3 vertices
        wgpu::Buffer vertexBuffer = CreateBuffer(3 * 4 * sizeof(float));
        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}};

        // It is ok to draw 3 vertices with vertex buffer
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 0, 0, true);
        // It is ok to draw 2 vertices with offset 1
        TestRenderPassDraw(pipeline, vertexBufferList, 2, 1, 1, 0, true);
        // Drawing more vertices will cause OOB
        TestRenderPassDraw(pipeline, vertexBufferList, 4, 1, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 6, 1, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 1000, 1, 0, 0, false);
        // Drawing 3 vertices will non-zero offset will cause OOB
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 1, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 1000, 0, false);
        // It is ok to draw more instances, as we have no instance-mode buffer
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 0, true);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 5, true);
    }

    TEST_F(RenderPassCommandValidationTest, DrawVertexBufferOutOfBoundWithInstance) {
        utils::ComboRenderPipelineDescriptor descriptor;
        SetBasicRenderPipelineDescriptorWithInstance(descriptor);
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

        // Build vertex buffer for 3 vertices
        wgpu::Buffer vertexBuffer = CreateBuffer(3 * 4 * sizeof(float));
        // Build vertex buffer for 5 instances
        wgpu::Buffer instanceBuffer = CreateBuffer(5 * 2 * sizeof(float));

        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}, {1, instanceBuffer, 0, 0}};

        // It is ok to draw 3 vertices
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 0, 0, true);
        TestRenderPassDraw(pipeline, vertexBufferList, 2, 1, 1, 0, true);
        // It is ok to draw 3 vertices and 5 instences
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 0, true);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 4, 0, 1, true);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 0, 4, true);
        // 4 or more vertices casuing OOB
        TestRenderPassDraw(pipeline, vertexBufferList, 4, 1, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 6, 1, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 1, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 4, 5, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 600, 5, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 1, 0, false);
        // 6 or more instances causing OOB
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 6, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 1, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1000, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 1000, false);
        // Both OOB
        TestRenderPassDraw(pipeline, vertexBufferList, 4, 6, 0, 0, false);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 1, 1, false);
    }

    TEST_F(RenderPassCommandValidationTest, DrawIndexedBasic) {
        utils::ComboRenderPipelineDescriptor descriptor;
        SetBasicRenderPipelineDescriptor(descriptor);
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

        // Build index buffer
        wgpu::Buffer indexBuffer = CreateBuffer(4 * 3 * sizeof(uint32_t), wgpu::BufferUsage::Index);

        // Build vertex buffer for 3 vertices
        wgpu::Buffer vertexBuffer = CreateBuffer(3 * 4 * sizeof(float));
        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}};

        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 1, 0, 0, 0, true);
    }

    TEST_F(RenderPassCommandValidationTest, DrawIndexedIndexBufferOOB) {
        utils::ComboRenderPipelineDescriptor descriptor;
        SetBasicRenderPipelineDescriptorWithInstance(descriptor);
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

        // Build index buffer
        wgpu::Buffer indexBuffer = CreateBuffer(4 * 3 * sizeof(uint32_t), wgpu::BufferUsage::Index);
        // Build vertex buffer for 3 vertices
        wgpu::Buffer vertexBuffer = CreateBuffer(3 * 4 * sizeof(float));
        // Build vertex buffer for 5 instances
        wgpu::Buffer instanceBuffer = CreateBuffer(5 * 2 * sizeof(float));

        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}, {1, instanceBuffer, 0, 0}};

        // Control case
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 5, 0, 0, 0, true);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 9, 5, 3, 0, 0, true);
        // Index buffer OOB
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 13, 5, 0, 0, 0, false);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 1200, 5, 0, 0, 0, false);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 5, 1, 0, 0, false);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 9, 5, 4, 0, 0, false);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 5, 1000, 0, 0,
                                  false);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 15, 5, 0, 0, 0, false);
    }

    TEST_F(RenderPassCommandValidationTest, DrawIndexedVertexBufferOOB) {
        utils::ComboRenderPipelineDescriptor descriptor;
        SetBasicRenderPipelineDescriptorWithInstance(descriptor);
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

        // Build index buffer
        wgpu::Buffer indexBuffer = CreateBuffer(4 * 3 * sizeof(uint32_t), wgpu::BufferUsage::Index);
        // Build vertex buffer for 3 vertices
        wgpu::Buffer vertexBuffer = CreateBuffer(3 * 4 * sizeof(float));
        // Build vertex buffer for 5 instances
        wgpu::Buffer instanceBuffer = CreateBuffer(5 * 2 * sizeof(float));

        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}, {1, instanceBuffer, 0, 0}};

        // Control case
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 5, 0, 0, 0, true);
        // Vertex buffer (stepMode = instance) OOB
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 6, 0, 0, 0, false);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 5, 0, 0, 1, false);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 600, 0, 0, 0, false);
        TestRenderPassDrawIndexed(pipeline, indexBuffer, vertexBufferList, 12, 5, 0, 0, 100, false);
        // OOB of vertex buffer that stepMode=vertex can not be validated in CPU.
    }

}  // anonymous namespace
