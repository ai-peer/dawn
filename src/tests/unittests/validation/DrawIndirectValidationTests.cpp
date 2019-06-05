// Copyright 2017 The Dawn Authors
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

#include <initializer_list>
#include "tests/unittests/validation/ValidationTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

class DrawIndirectValidationTest : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();

        vsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
            #version 450
            layout (set = 0, binding = 0) uniform vertexUniformBuffer {
                mat2 transform;
            };
            void main() {
                const vec2 pos[3] = vec2[3](vec2(-1.f, -1.f), vec2(1.f, -1.f), vec2(-1.f, 1.f));
                gl_Position = vec4(transform * pos[gl_VertexIndex], 0.f, 1.f);
            })");

        fsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
            #version 450
            layout (set = 1, binding = 0) uniform fragmentUniformBuffer {
                vec4 color;
            };
            layout(location = 0) out vec4 fragColor;
            void main() {
                fragColor = color;
            })");
    }

    void ValidateExpectation(dawn::CommandEncoder encoder, utils::Expectation expectation) {
        if (expectation == utils::Expectation::Success) {
            encoder.Finish();
        } else {
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    void TestIndirectOffsetDrawIndexed(utils::Expectation expectation,
                                       std::initializer_list<uint32_t> bufferList,
                                       uint64_t indirectOffset) {
        TestIndirectOffset(expectation, bufferList, indirectOffset, true);
    }

    void TestIndirectOffsetDraw(utils::Expectation expectation,
                                std::initializer_list<uint32_t> bufferList,
                                uint64_t indirectOffset) {
        TestIndirectOffset(expectation, bufferList, indirectOffset, false);
    }

    void TestIndirectOffset(utils::Expectation expectation,
                            std::initializer_list<uint32_t> bufferList,
                            uint64_t indirectOffset,
                            bool indexed) {
        dawn::BindGroupLayout bgls[] = {
            utils::MakeBindGroupLayout(
                device, {{0, dawn::ShaderStageBit::Vertex, dawn::BindingType::UniformBuffer}}),
            utils::MakeBindGroupLayout(
                device, {{0, dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer}})};

        dawn::PipelineLayoutDescriptor pipelineLayoutDesc;
        pipelineLayoutDesc.bindGroupLayoutCount = 2;
        pipelineLayoutDesc.bindGroupLayouts = bgls;

        dawn::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.layout = pipelineLayout;
        descriptor.cVertexStage.module = vsModule;
        descriptor.cFragmentStage.module = fsModule;

        dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

        float data[4];
        dawn::Buffer buffer = utils::CreateBufferFromData(device, data, 4 * sizeof(float),
                                                          dawn::BufferUsageBit::Uniform);

        dawn::BindGroup bg1 =
            utils::MakeBindGroup(device, bgls[0], {{0, buffer, 0, 4 * sizeof(float)}});
        dawn::BindGroup bg2 =
            utils::MakeBindGroup(device, bgls[1], {{0, buffer, 0, 4 * sizeof(float)}});

        dawn::Buffer indirectBuffer = utils::CreateBufferFromData<uint32_t>(
            device, dawn::BufferUsageBit::Indirect, bufferList);

        DummyRenderPass renderPass(device);
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bg1, 0, nullptr);
        pass.SetBindGroup(1, bg2, 0, nullptr);
        if (indexed) {
            dawn::Buffer indexBuffer =
                utils::CreateBufferFromData<uint32_t>(device, dawn::BufferUsageBit::Index, {});
            pass.SetIndexBuffer(indexBuffer, 0);
            pass.DrawIndexedIndirect(indirectBuffer, indirectOffset);
        } else {
            pass.DrawIndirect(indirectBuffer, indirectOffset);
        }
        pass.EndPass();

        ValidateExpectation(encoder, expectation);
    }

    dawn::ShaderModule vsModule;
    dawn::ShaderModule fsModule;
};

// Verify out of bounds indirect draw calls are caught early
TEST_F(DrawIndirectValidationTest, DrawIndirectOffsetBounds) {
    // In bounds
    TestIndirectOffsetDraw(utils::Expectation::Success, {1, 2, 3, 4}, 0);
    // In bounds, bigger buffer
    TestIndirectOffsetDraw(utils::Expectation::Success, {1, 2, 3, 4, 5, 6, 7}, 0);
    // In bounds, bigger buffer, positive offset
    TestIndirectOffsetDraw(utils::Expectation::Success, {1, 2, 3, 4, 5, 6, 7, 8},
                           4 * sizeof(uint32_t));

    // Out of bounds, buffer too small
    TestIndirectOffsetDraw(utils::Expectation::Failure, {1, 2, 3}, 0);
    // Out of bounds, index too big
    TestIndirectOffsetDraw(utils::Expectation::Failure, {1, 2, 3, 4}, 1 * sizeof(uint32_t));
    // Out of bounds, index past buffer
    TestIndirectOffsetDraw(utils::Expectation::Failure, {1, 2, 3, 4}, 5 * sizeof(uint32_t));
    // Out of bounds, index + size of command overflows
    uint64_t offset = 0;
    offset -= 1;
    TestIndirectOffsetDraw(utils::Expectation::Failure, {1, 2, 3, 4, 5, 6, 7}, offset);
}

// Verify out of bounds indirect draw indexed calls are caught early
TEST_F(DrawIndirectValidationTest, DrawIndexedIndirectOffsetBounds) {
    // In bounds
    TestIndirectOffsetDrawIndexed(utils::Expectation::Success, {1, 2, 3, 4, 5}, 0);
    // In bounds, bigger buffer
    TestIndirectOffsetDrawIndexed(utils::Expectation::Success, {1, 2, 3, 4, 5, 6, 7, 8, 9}, 0);
    // In bounds, bigger buffer, positive offset
    TestIndirectOffsetDrawIndexed(utils::Expectation::Success, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                                  5 * sizeof(uint32_t));

    // Out of bounds, buffer too small
    TestIndirectOffsetDrawIndexed(utils::Expectation::Failure, {1, 2, 3, 4}, 0);
    // Out of bounds, index too big
    TestIndirectOffsetDrawIndexed(utils::Expectation::Failure, {1, 2, 3, 4, 5},
                                  1 * sizeof(uint32_t));
    // Out of bounds, index past buffer
    TestIndirectOffsetDrawIndexed(utils::Expectation::Failure, {1, 2, 3, 4, 5},
                                  5 * sizeof(uint32_t));
    // Out of bounds, index + size of command overflows
    uint64_t offset = 0;
    offset -= 1;
    TestIndirectOffsetDrawIndexed(utils::Expectation::Failure, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                                  offset);
}
