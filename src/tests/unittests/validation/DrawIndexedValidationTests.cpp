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
#include "tests/unittests/validation/ValidationTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class DrawIndexedValidationTest : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();

        wgpu::ShaderModule vsModule = utils::CreateShaderModuleFromWGSL(device, R"(
            [[builtin(position)]] var<out> Position : vec4<f32>;
            [[stage(vertex)]] fn main() -> void {
                Position = vec4<f32>(0.0, 0.0, 0.0, 0.0);
            })");

        wgpu::ShaderModule fsModule = utils::CreateShaderModuleFromWGSL(device, R"(
            [[location(0)]] var<out> fragColor : vec4<f32>;
            [[stage(fragment)]] fn main() -> void {
                fragColor = vec4<f32>(0.0, 0.0, 0.0, 0.0);
            })");

        // Set up render pipeline
        wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, nullptr);

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.layout = pipelineLayout;
        descriptor.vertexStage.module = vsModule;
        descriptor.cFragmentStage.module = fsModule;

        pipeline = device.CreateRenderPipeline(&descriptor);

        uint32_t indices[] = {0, 1, 2, 3, 1, 2};
        indexBuffer =
            utils::CreateBufferFromData(device, indices, sizeof(indices), wgpu::BufferUsage::Index);
    }

    wgpu::Buffer indexBuffer;

    void ValidateExpectation(wgpu::CommandEncoder encoder, utils::Expectation expectation) {
        if (expectation == utils::Expectation::Success) {
            encoder.Finish();
        } else {
            ASSERT_DEVICE_ERROR(encoder.Finish());
        }
    }

    void TestDrawIndexedIndexBound(utils::Expectation expectation,
                                   uint32_t indexCount,
                                   uint32_t firstIndex) {
        TestDrawIndexed(expectation, indexCount, 1, firstIndex, 0, 0);
    }

    void TestDrawIndexed(utils::Expectation expectation,
                         uint32_t indexCount,
                         uint32_t instanceCount,
                         uint32_t firstIndex,
                         int32_t baseVertex,
                         uint32_t firstInstance) {
        DummyRenderPass renderPass(device);
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
        pass.EndPass();

        ValidateExpectation(encoder, expectation);
    }

    wgpu::RenderPipeline pipeline;
};

// Test validation with indexCount and firstIndex go out of bounds
TEST_F(DrawIndexedValidationTest, IndexOutOfBounds) {
    // In bounds
    TestDrawIndexedIndexBound(utils::Expectation::Success, 6, 0);
    // indexCount + firstIndex out of bound
    TestDrawIndexedIndexBound(utils::Expectation::Failure, 6, 1);
    // only firstIndex out of bound
    TestDrawIndexedIndexBound(utils::Expectation::Failure, 6, 6);
    // firstIndex much larger than the bound
    TestDrawIndexedIndexBound(utils::Expectation::Failure, 6, 10000);
    // only indexCount out of bound
    TestDrawIndexedIndexBound(utils::Expectation::Failure, 7, 0);
    // indexCount much larger than the bound
    TestDrawIndexedIndexBound(utils::Expectation::Failure, 10000, 0);
    // max uint32_t indexCount and firstIndex
    TestDrawIndexedIndexBound(utils::Expectation::Failure, std::numeric_limits<uint32_t>::max(),
                              std::numeric_limits<uint32_t>::max());
    // max uint32_t indexCount and small firstIndex
    TestDrawIndexedIndexBound(utils::Expectation::Failure, std::numeric_limits<uint32_t>::max(), 2);
    // small indexCount and max uint32_t firstIndex
    TestDrawIndexedIndexBound(utils::Expectation::Failure, 2, std::numeric_limits<uint32_t>::max());
}
