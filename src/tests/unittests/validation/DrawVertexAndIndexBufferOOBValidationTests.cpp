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

namespace {
    constexpr uint32_t kRTSize = 4;
    constexpr uint32_t kFloat32x2Stride = 2 * sizeof(float);
    constexpr uint32_t kFloat32x4Stride = 4 * sizeof(float);

    class DrawVertexAndIndexBufferOOBValidationTests : public ValidationTest {
      public:
        struct VertexBufferSpec {
            uint32_t slot;
            const wgpu::Buffer& buffer;
            uint64_t offset;
            uint64_t size;
        };

        using VertexBufferListT =
            // std::initializer_list<std::tuple<uint32_t, const wgpu::Buffer&, uint64_t, uint64_t>>;
            std::vector<VertexBufferSpec>;

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
                renderPassEncoder.SetVertexBuffer(vertexBufferParam.slot, vertexBufferParam.buffer,
                                                  vertexBufferParam.offset, vertexBufferParam.size);
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
                                       wgpu::IndexFormat indexFormat,
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

            renderPassEncoder.SetIndexBuffer(indexBuffer, indexFormat);

            for (auto vertexBufferParam : vertexBufferList) {
                renderPassEncoder.SetVertexBuffer(vertexBufferParam.slot, vertexBufferParam.buffer,
                                                  vertexBufferParam.offset, vertexBufferParam.size);
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

        struct PipelineVertexBufferAttributeDesc {
            uint32_t shaderLocation;
            wgpu::VertexFormat format;
            uint64_t offset = 0;
        };

        struct PipelineVertexBufferDesc {
            uint64_t arrayStride;
            wgpu::InputStepMode stepMode;
            std::vector<PipelineVertexBufferAttributeDesc> attributes = {};
        };

        // Create a render pipeline descriptor with given buffer setting
        std::unique_ptr<utils::ComboRenderPipelineDescriptor>
        CreateBasicRenderPipelineDescriptorWithBuffer(
            wgpu::ShaderModule vertexModule,
            wgpu::ShaderModule fragmentModule,
            std::vector<PipelineVertexBufferDesc> bufferDescList) {
            auto descriptor = std::make_unique<utils::ComboRenderPipelineDescriptor>();

            descriptor->vertex.module = vertexModule;
            descriptor->cFragment.module = fragmentModule;
            descriptor->primitive.topology = wgpu::PrimitiveTopology::TriangleList;

            descriptor->vertex.bufferCount = bufferDescList.size();

            size_t attributeCount = 0;

            for (size_t bufferCount = 0; bufferCount < bufferDescList.size(); bufferCount++) {
                auto bufferDesc = bufferDescList[bufferCount];
                descriptor->cBuffers[bufferCount].arrayStride = bufferDesc.arrayStride;
                descriptor->cBuffers[bufferCount].stepMode = bufferDesc.stepMode;
                if (bufferDesc.attributes.size() > 0) {
                    descriptor->cBuffers[bufferCount].attributeCount = bufferDesc.attributes.size();
                    descriptor->cBuffers[bufferCount].attributes =
                        &descriptor->cAttributes[attributeCount];
                    for (auto attribute : bufferDesc.attributes) {
                        descriptor->cAttributes[attributeCount].shaderLocation =
                            attribute.shaderLocation;
                        descriptor->cAttributes[attributeCount].format = attribute.format;
                        descriptor->cAttributes[attributeCount].offset = attribute.offset;
                        attributeCount++;
                    }
                } else {
                    descriptor->cBuffers[bufferCount].attributeCount = 0;
                    descriptor->cBuffers[bufferCount].attributes = nullptr;
                }
            }

            descriptor->cTargets[0].format = renderPass.colorFormat;

            return descriptor;
        }

        // Create a render pipeline descriptor using only one vertex-step-mode Float32x4 buffer
        std::unique_ptr<utils::ComboRenderPipelineDescriptor> CreateBasicRenderPipelineDescriptor(
            uint32_t bufferStride = kFloat32x4Stride) {
            DAWN_ASSERT(bufferStride >= kFloat32x4Stride);
            return CreateBasicRenderPipelineDescriptorWithBuffer(
                vsModule, fsModule,
                {
                    {bufferStride,
                     wgpu::InputStepMode::Vertex,
                     {{0, wgpu::VertexFormat::Float32x4}}},
                });
        }

        // Create a render pipeline descriptor using one vertex-step-mode Float32x4 buffer and one
        // instance-step-mode Float32x2 buffer
        std::unique_ptr<utils::ComboRenderPipelineDescriptor>
        CreateBasicRenderPipelineDescriptorWithInstance(uint32_t bufferStride1 = kFloat32x4Stride,
                                                        uint32_t bufferStride2 = kFloat32x2Stride) {
            DAWN_ASSERT(bufferStride1 >= kFloat32x4Stride);
            DAWN_ASSERT(bufferStride2 >= kFloat32x2Stride);
            return CreateBasicRenderPipelineDescriptorWithBuffer(
                vsModuleForTwoBuffers, fsModule,
                {
                    {bufferStride1,
                     wgpu::InputStepMode::Vertex,
                     {{0, wgpu::VertexFormat::Float32x4}}},
                    {bufferStride2,
                     wgpu::InputStepMode::Instance,
                     {{3, wgpu::VertexFormat::Float32x2}}},
                });
        }

        wgpu::Buffer CreateBuffer(uint64_t size,
                                  wgpu::BufferUsage usage = wgpu::BufferUsage::Vertex) {
            wgpu::BufferDescriptor descriptor;
            descriptor.size = size;
            descriptor.usage = usage;

            return device.CreateBuffer(&descriptor);
        }

      private:
        wgpu::ShaderModule vsModule;
        wgpu::ShaderModule vsModuleForTwoBuffers;
        wgpu::ShaderModule fsModule;
        utils::BasicRenderPass renderPass;
    };

    // Control case for Draw
    TEST_F(DrawVertexAndIndexBufferOOBValidationTests, DrawBasic) {
        std::unique_ptr<utils::ComboRenderPipelineDescriptor> descriptor =
            CreateBasicRenderPipelineDescriptor();
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(descriptor.get());

        wgpu::Buffer vertexBuffer = CreateBuffer(3 * kFloat32x4Stride);

        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}};
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 0, 0, true);
    }

    // Verify vertex buffer OOB for non-instanced Draw are caught in command encoder
    TEST_F(DrawVertexAndIndexBufferOOBValidationTests, DrawVertexBufferOutOfBoundWithoutInstance) {
        // Create a render pipeline without instance step mode buffer
        std::unique_ptr<utils::ComboRenderPipelineDescriptor> descriptor =
            CreateBasicRenderPipelineDescriptor();
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(descriptor.get());

        // Build vertex buffer for 3 vertices
        wgpu::Buffer vertexBuffer = CreateBuffer(3 * kFloat32x4Stride);
        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}};

        // It is ok to draw 3 vertices with vertex buffer
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 0, 0, true);
        // It is ok to draw 2 vertices with offset 1
        TestRenderPassDraw(pipeline, vertexBufferList, 2, 1, 1, 0, true);
        // Drawing more vertices will cause OOB, even if not enough for another primitive
        TestRenderPassDraw(pipeline, vertexBufferList, 4, 1, 0, 0, false);
        // Drawing 3 vertices will non-zero offset will cause OOB
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 1, 0, false);
        // It is ok to draw any number of instances, as we have no instance-mode buffer
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 0, true);
        TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 5, true);
    }

    // Verify vertex buffer OOB for instanced Draw are caught in command encoder
    TEST_F(DrawVertexAndIndexBufferOOBValidationTests, DrawVertexBufferOutOfBoundWithInstance) {
        // Test for different buffer strides, making sure that stride is taken into account
        for (auto bufferStrides : std::initializer_list<std::pair<uint32_t, uint32_t>>{
                 {kFloat32x4Stride, kFloat32x2Stride},
                 {2 * kFloat32x4Stride, 3 * kFloat32x2Stride}}) {
            // Create pipeline with given buffer stride
            std::unique_ptr<utils::ComboRenderPipelineDescriptor> descriptor =
                CreateBasicRenderPipelineDescriptorWithInstance(bufferStrides.first,
                                                                bufferStrides.second);
            wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(descriptor.get());

            // Build vertex buffer for 3 vertices
            wgpu::Buffer vertexBuffer = CreateBuffer(3 * bufferStrides.first);
            // Build vertex buffer for 5 instances
            wgpu::Buffer instanceBuffer = CreateBuffer(5 * bufferStrides.second);

            VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0},
                                                  {1, instanceBuffer, 0, 0}};

            // It is ok to draw 3 vertices
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 0, 0, true);
            TestRenderPassDraw(pipeline, vertexBufferList, 2, 1, 1, 0, true);
            // It is ok to draw 3 vertices and 5 instences
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 0, true);
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 4, 0, 1, true);
            // 4 or more vertices causing OOB
            TestRenderPassDraw(pipeline, vertexBufferList, 4, 1, 0, 0, false);
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 1, 1, 0, false);
            TestRenderPassDraw(pipeline, vertexBufferList, 4, 5, 0, 0, false);
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 1, 0, false);
            // 6 or more instances causing OOB
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 6, 0, 0, false);
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 1, false);
            // Both OOB
            TestRenderPassDraw(pipeline, vertexBufferList, 4, 6, 0, 0, false);
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 1, 1, false);
        }
    }

    // Control case for DrawIndexed
    TEST_F(DrawVertexAndIndexBufferOOBValidationTests, DrawIndexedBasic) {
        std::unique_ptr<utils::ComboRenderPipelineDescriptor> descriptor =
            CreateBasicRenderPipelineDescriptor();
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(descriptor.get());

        // Build index buffer for 12 indexes
        wgpu::Buffer indexBuffer = CreateBuffer(12 * sizeof(uint32_t), wgpu::BufferUsage::Index);

        // Build vertex buffer for 3 vertices
        wgpu::Buffer vertexBuffer = CreateBuffer(3 * kFloat32x4Stride);
        VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0}};

        TestRenderPassDrawIndexed(pipeline, indexBuffer, wgpu::IndexFormat::Uint32,
                                  vertexBufferList, 12, 1, 0, 0, 0, true);
    }

    // Verify index buffer OOB for DrawIndexed are caught in command encoder
    TEST_F(DrawVertexAndIndexBufferOOBValidationTests, DrawIndexedIndexBufferOOB) {
        std::unique_ptr<utils::ComboRenderPipelineDescriptor> descriptor =
            CreateBasicRenderPipelineDescriptorWithInstance();
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(descriptor.get());

        // Test both index formats
        for (auto indexFormatAndSize :
             std::initializer_list<std::pair<wgpu::IndexFormat, uint32_t>>{
                 {wgpu::IndexFormat::Uint32, sizeof(uint32_t)},
                 {wgpu::IndexFormat::Uint16, sizeof(uint16_t)}}) {
            wgpu::IndexFormat indexFormat = indexFormatAndSize.first;
            uint32_t indexStride = indexFormatAndSize.second;

            // Build index buffer for 12 indexes
            wgpu::Buffer indexBuffer = CreateBuffer(12 * indexStride, wgpu::BufferUsage::Index);
            // Build vertex buffer for 3 vertices
            wgpu::Buffer vertexBuffer = CreateBuffer(3 * kFloat32x4Stride);
            // Build vertex buffer for 5 instances
            wgpu::Buffer instanceBuffer = CreateBuffer(5 * kFloat32x2Stride);

            VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0},
                                                  {1, instanceBuffer, 0, 0}};

            // Control case
            TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 12, 5,
                                      0, 0, 0, true);
            TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 9, 5, 3,
                                      0, 0, true);
            // Index buffer OOB, indexCount too large
            TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 13, 5,
                                      0, 0, 0, false);
            // Index buffer OOB, indexCount + firstIndex too large
            TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 12, 5,
                                      1, 0, 0, false);
            // Index buffer OOB, indexCount + firstIndex too large
            TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 9, 5, 4,
                                      0, 0, false);

            if (!HasToggleEnabled("disable_base_vertex")) {
                // baseVertex is not considered in CPU validation and has no effect on validation
                // Althought baseVertex is too large, it will still pass
                TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 12,
                                          5, 0, 100, 0, true);
                // Index buffer OOB, indexCount too large
                TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 13,
                                          5, 0, 100, 0, false);
            }
        }
    }

    // Verify instance mode vertex buffer OOB for DrawIndexed are caught in command encoder
    TEST_F(DrawVertexAndIndexBufferOOBValidationTests, DrawIndexedVertexBufferOOB) {
        // Test for different buffer strides, making sure that stride is taken into account
        for (auto bufferStrides : std::initializer_list<std::pair<uint32_t, uint32_t>>{
                 {kFloat32x4Stride, kFloat32x2Stride},
                 {2 * kFloat32x4Stride, 3 * kFloat32x2Stride}}) {
            // Create pipeline with given buffer stride
            std::unique_ptr<utils::ComboRenderPipelineDescriptor> descriptor =
                CreateBasicRenderPipelineDescriptorWithInstance(bufferStrides.first,
                                                                bufferStrides.second);
            wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(descriptor.get());

            auto indexFormat = wgpu::IndexFormat::Uint32;
            auto indexStride = sizeof(uint32_t);

            // Build index buffer for 12 indexes
            wgpu::Buffer indexBuffer = CreateBuffer(12 * indexStride, wgpu::BufferUsage::Index);
            // Build vertex buffer for 3 vertices
            wgpu::Buffer vertexBuffer = CreateBuffer(3 * bufferStrides.first);
            // Build vertex buffer for 5 instances
            wgpu::Buffer instanceBuffer = CreateBuffer(5 * bufferStrides.second);

            VertexBufferListT vertexBufferList = {{0, vertexBuffer, 0, 0},
                                                  {1, instanceBuffer, 0, 0}};

            // Control case
            TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 12, 5,
                                      0, 0, 0, true);
            // Vertex buffer (stepMode = instance) OOB, instanceCount too large
            TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 12, 6,
                                      0, 0, 0, false);

            if (!HasToggleEnabled("disable_base_instance")) {
                // firstInstance is considered in CPU validation
                // Vertex buffer (stepMode = instance) in bound
                TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 12,
                                          4, 0, 0, 1, true);
                // Vertex buffer (stepMode = instance) OOB, instanceCount + firstInstance too large
                TestRenderPassDrawIndexed(pipeline, indexBuffer, indexFormat, vertexBufferList, 12,
                                          5, 0, 0, 1, false);
            }

            // OOB of vertex buffer that stepMode=vertex can not be validated in CPU.
        }
    }

    // Verify that if setVertexBuffer and/or setIndexBuffer for multiple times, only the last one is
    // taken into account
    TEST_F(DrawVertexAndIndexBufferOOBValidationTests, SetBufferMultipleTime) {
        wgpu::IndexFormat indexFormat = wgpu::IndexFormat::Uint32;
        uint32_t indexStride = sizeof(uint32_t);

        // Build index buffer for 11 indexes
        wgpu::Buffer indexBuffer11 = CreateBuffer(11 * indexStride, wgpu::BufferUsage::Index);
        // Build index buffer for 12 indexes
        wgpu::Buffer indexBuffer12 = CreateBuffer(12 * indexStride, wgpu::BufferUsage::Index);
        // Build vertex buffer for 2 vertices
        wgpu::Buffer vertexBuffer2 = CreateBuffer(2 * kFloat32x4Stride);
        // Build vertex buffer for 3 vertices
        wgpu::Buffer vertexBuffer3 = CreateBuffer(3 * kFloat32x4Stride);
        // Build vertex buffer for 4 instances
        wgpu::Buffer instanceBuffer4 = CreateBuffer(4 * kFloat32x2Stride);
        // Build vertex buffer for 5 instances
        wgpu::Buffer instanceBuffer5 = CreateBuffer(5 * kFloat32x2Stride);

        // Test for setting vertex buffer for multiple times
        {
            std::unique_ptr<utils::ComboRenderPipelineDescriptor> descriptor =
                CreateBasicRenderPipelineDescriptorWithInstance();
            wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(descriptor.get());

            // Set to vertexBuffer3 and instanceBuffer5 at last
            VertexBufferListT vertexBufferList = {{0, vertexBuffer2, 0, 0},
                                                  {1, instanceBuffer4, 0, 0},
                                                  {1, instanceBuffer5, 0, 0},
                                                  {0, vertexBuffer3, 0, 0}};

            // For Draw, the max vertexCount is 3 and the max instanceCount is 5
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 5, 0, 0, true);
            TestRenderPassDraw(pipeline, vertexBufferList, 4, 5, 0, 0, false);
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 6, 0, 0, false);
            // For DrawIndex, the max instanceCount is 5
            TestRenderPassDrawIndexed(pipeline, indexBuffer12, indexFormat, vertexBufferList, 12, 5,
                                      0, 0, 0, true);
            TestRenderPassDrawIndexed(pipeline, indexBuffer12, indexFormat, vertexBufferList, 12, 6,
                                      0, 0, 0, false);

            // Set to vertexBuffer2 and instanceBuffer4 at last
            vertexBufferList = VertexBufferListT{{0, vertexBuffer3, 0, 0},
                                                 {1, instanceBuffer5, 0, 0},
                                                 {0, vertexBuffer2, 0, 0},
                                                 {1, instanceBuffer4, 0, 0}};

            // For Draw, the max vertexCount is 2 and the max instanceCount is 4
            TestRenderPassDraw(pipeline, vertexBufferList, 2, 4, 0, 0, true);
            TestRenderPassDraw(pipeline, vertexBufferList, 3, 4, 0, 0, false);
            TestRenderPassDraw(pipeline, vertexBufferList, 2, 5, 0, 0, false);
            // For DrawIndex, the max instanceCount is 4
            TestRenderPassDrawIndexed(pipeline, indexBuffer12, indexFormat, vertexBufferList, 12, 4,
                                      0, 0, 0, true);
            TestRenderPassDrawIndexed(pipeline, indexBuffer12, indexFormat, vertexBufferList, 12, 5,
                                      0, 0, 0, false);
        }

        // Test for setIndexBuffer multiple times
        {
            std::unique_ptr<utils::ComboRenderPipelineDescriptor> descriptor =
                CreateBasicRenderPipelineDescriptor();
            wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(descriptor.get());

            VertexBufferListT vertexBufferList = {{0, vertexBuffer3, 0, 0}};

            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::RenderPassEncoder renderPassEncoder =
                    encoder.BeginRenderPass(GetBasicRenderPassDescriptor());
                renderPassEncoder.SetPipeline(pipeline);

                // Index buffer is set to indexBuffer12 at last
                renderPassEncoder.SetIndexBuffer(indexBuffer11, indexFormat);
                renderPassEncoder.SetIndexBuffer(indexBuffer12, indexFormat);

                renderPassEncoder.SetVertexBuffer(0, vertexBuffer3, 0, 0);
                // It should be ok to draw 12 index
                renderPassEncoder.DrawIndexed(12, 1, 0, 0, 0);
                renderPassEncoder.EndPass();

                // Expect success
                encoder.Finish();
            }

            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::RenderPassEncoder renderPassEncoder =
                    encoder.BeginRenderPass(GetBasicRenderPassDescriptor());
                renderPassEncoder.SetPipeline(pipeline);

                // Index buffer is set to indexBuffer12 at last
                renderPassEncoder.SetIndexBuffer(indexBuffer11, indexFormat);
                renderPassEncoder.SetIndexBuffer(indexBuffer12, indexFormat);

                renderPassEncoder.SetVertexBuffer(0, vertexBuffer3, 0, 0);
                // It should be index buffer OOB to draw 13 index
                renderPassEncoder.DrawIndexed(13, 1, 0, 0, 0);
                renderPassEncoder.EndPass();

                // Expect failure
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }

            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::RenderPassEncoder renderPassEncoder =
                    encoder.BeginRenderPass(GetBasicRenderPassDescriptor());
                renderPassEncoder.SetPipeline(pipeline);

                // Index buffer is set to indexBuffer11 at last
                renderPassEncoder.SetIndexBuffer(indexBuffer12, indexFormat);
                renderPassEncoder.SetIndexBuffer(indexBuffer11, indexFormat);

                renderPassEncoder.SetVertexBuffer(0, vertexBuffer3, 0, 0);
                // It should be ok to draw 11 index
                renderPassEncoder.DrawIndexed(11, 1, 0, 0, 0);
                renderPassEncoder.EndPass();

                // Expect success
                encoder.Finish();
            }

            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::RenderPassEncoder renderPassEncoder =
                    encoder.BeginRenderPass(GetBasicRenderPassDescriptor());
                renderPassEncoder.SetPipeline(pipeline);

                // Index buffer is set to indexBuffer11 at last
                renderPassEncoder.SetIndexBuffer(indexBuffer12, indexFormat);
                renderPassEncoder.SetIndexBuffer(indexBuffer11, indexFormat);

                renderPassEncoder.SetVertexBuffer(0, vertexBuffer3, 0, 0);
                // It should be index buffer OOB to draw 12 index
                renderPassEncoder.DrawIndexed(12, 1, 0, 0, 0);
                renderPassEncoder.EndPass();

                // Expect failure
                ASSERT_DEVICE_ERROR(encoder.Finish());
            }
        }
    }

}  // anonymous namespace
