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

#include <initializer_list>
#include <limits>
#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

class DrawIndirectValidationTest : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();

        wgpu::ShaderModule vsModule = dawn::utils::CreateShaderModule(device, R"(
            @vertex fn main() -> @builtin(position) vec4f {
                return vec4f(0.0, 0.0, 0.0, 0.0);
            })");

        wgpu::ShaderModule fsModule = dawn::utils::CreateShaderModule(device, R"(
            @fragment fn main() -> @location(0) vec4f{
                return vec4f(0.0, 0.0, 0.0, 0.0);
            })");

        // Set up render pipeline
        wgpu::PipelineLayout pipelineLayout = dawn::utils::MakeBasicPipelineLayout(device, nullptr);

        dawn::utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.layout = pipelineLayout;
        descriptor.vertex.module = vsModule;
        descriptor.cFragment.module = fsModule;

        pipeline = device.CreateRenderPipeline(&descriptor);
    }

    void ValidateExpectation(wgpu::CommandEncoder encoder, dawn::utils::Expectation expectation) {
        if (expectation == dawn::utils::Expectation::Success) {
            encoder.Finish();
        } else {
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    void TestIndirectOffsetDrawIndexed(dawn::utils::Expectation expectation,
                                       std::initializer_list<uint32_t> bufferList,
                                       uint64_t indirectOffset) {
        TestIndirectOffset(expectation, bufferList, indirectOffset, true);
    }

    void TestIndirectOffsetDraw(dawn::utils::Expectation expectation,
                                std::initializer_list<uint32_t> bufferList,
                                uint64_t indirectOffset) {
        TestIndirectOffset(expectation, bufferList, indirectOffset, false);
    }

    void TestIndirectOffset(dawn::utils::Expectation expectation,
                            std::initializer_list<uint32_t> bufferList,
                            uint64_t indirectOffset,
                            bool indexed,
                            wgpu::BufferUsage usage = wgpu::BufferUsage::Indirect) {
        wgpu::Buffer indirectBuffer =
            dawn::utils::CreateBufferFromData<uint32_t>(device, usage, bufferList);

        PlaceholderRenderPass renderPass(device);
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        if (indexed) {
            uint32_t zeros[100] = {};
            wgpu::Buffer indexBuffer = dawn::utils::CreateBufferFromData(
                device, zeros, sizeof(zeros), wgpu::BufferUsage::Index);
            pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
            pass.DrawIndexedIndirect(indirectBuffer, indirectOffset);
        } else {
            pass.DrawIndirect(indirectBuffer, indirectOffset);
        }
        pass.End();

        ValidateExpectation(encoder, expectation);
    }

    wgpu::RenderPipeline pipeline;
};

// Verify out of bounds indirect draw calls are caught early
TEST_F(DrawIndirectValidationTest, DrawIndirectOffsetBounds) {
    // In bounds
    TestIndirectOffsetDraw(dawn::utils::Expectation::Success, {1, 2, 3, 4}, 0);
    // In bounds, bigger buffer
    TestIndirectOffsetDraw(dawn::utils::Expectation::Success, {1, 2, 3, 4, 5, 6, 7}, 0);
    // In bounds, bigger buffer, positive offset
    TestIndirectOffsetDraw(dawn::utils::Expectation::Success, {1, 2, 3, 4, 5, 6, 7, 8},
                           4 * sizeof(uint32_t));

    // In bounds, non-multiple of 4 offsets
    TestIndirectOffsetDraw(dawn::utils::Expectation::Failure, {1, 2, 3, 4, 5}, 1);
    TestIndirectOffsetDraw(dawn::utils::Expectation::Failure, {1, 2, 3, 4, 5}, 2);

    // Out of bounds, buffer too small
    TestIndirectOffsetDraw(dawn::utils::Expectation::Failure, {1, 2, 3}, 0);
    // Out of bounds, index too big
    TestIndirectOffsetDraw(dawn::utils::Expectation::Failure, {1, 2, 3, 4}, 1 * sizeof(uint32_t));
    // Out of bounds, index past buffer
    TestIndirectOffsetDraw(dawn::utils::Expectation::Failure, {1, 2, 3, 4}, 5 * sizeof(uint32_t));
    // Out of bounds, index + size of command overflows
    uint64_t offset = std::numeric_limits<uint64_t>::max();
    TestIndirectOffsetDraw(dawn::utils::Expectation::Failure, {1, 2, 3, 4, 5, 6, 7}, offset);
}

// Verify out of bounds indirect draw indexed calls are caught early
TEST_F(DrawIndirectValidationTest, DrawIndexedIndirectOffsetBounds) {
    // In bounds
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Success, {1, 2, 3, 4, 5}, 0);
    // In bounds, bigger buffer
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Success, {1, 2, 3, 4, 5, 6, 7, 8, 9},
                                  0);
    // In bounds, bigger buffer, positive offset
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Success,
                                  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, 5 * sizeof(uint32_t));

    // In bounds, non-multiple of 4 offsets
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Failure, {1, 2, 3, 4, 5, 6}, 1);
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Failure, {1, 2, 3, 4, 5, 6}, 2);

    // Out of bounds, buffer too small
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Failure, {1, 2, 3, 4}, 0);
    // Out of bounds, index too big
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Failure, {1, 2, 3, 4, 5},
                                  1 * sizeof(uint32_t));
    // Out of bounds, index past buffer
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Failure, {1, 2, 3, 4, 5},
                                  5 * sizeof(uint32_t));
    // Out of bounds, index + size of command overflows
    uint64_t offset = std::numeric_limits<uint64_t>::max();
    TestIndirectOffsetDrawIndexed(dawn::utils::Expectation::Failure,
                                  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, offset);
}

// Check that the buffer must have the indirect usage
TEST_F(DrawIndirectValidationTest, IndirectUsage) {
    // Control cases: using a buffer with the indirect usage is valid.
    TestIndirectOffset(dawn::utils::Expectation::Success, {1, 2, 3, 4}, 0, false,
                       wgpu::BufferUsage::Indirect);
    TestIndirectOffset(dawn::utils::Expectation::Success, {1, 2, 3, 4, 5}, 0, true,
                       wgpu::BufferUsage::Indirect);

    // Error cases: using a buffer with the vertex usage is an error.
    TestIndirectOffset(dawn::utils::Expectation::Failure, {1, 2, 3, 4}, 0, false,
                       wgpu::BufferUsage::Vertex);
    TestIndirectOffset(dawn::utils::Expectation::Failure, {1, 2, 3, 4, 5}, 0, true,
                       wgpu::BufferUsage::Vertex);
}
