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

#include "common/Constants.h"
#include "tests/unittests/validation/ValidationTest.h"
#include "utils/WGPUHelpers.h"

// TODO(cwallez@chromium.org): Add a regression test for Disptach validation trying to acces the
// input state.

class ComputeValidationTest : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();

        wgpu::ShaderModule computeModule = utils::CreateShaderModule(device, R"(
            [[stage(compute), workgroup_size(1)]] fn main() {
            })");

        // Set up compute pipeline
        wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, nullptr);

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.layout = pl;
        csDesc.compute.module = computeModule;
        csDesc.compute.entryPoint = "main";
        pipeline = device.CreateComputePipeline(&csDesc);
    }

    void TestDispatch(uint32_t x, uint32_t y, uint32_t z) {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.Dispatch(x, y, z);
        pass.EndPass();
        encoder.Finish();
    }

    wgpu::ComputePipeline pipeline;
};

// Check that per-dimension dispatch size limits are enforced on direct dispatch calls.
TEST_F(ComputeValidationTest, PerDimensionDispatchSizeLimits) {
    constexpr uint32_t kMax = kMaxComputePerDimensionDispatchSize;
    TestDispatch(1, 1, 1);
    TestDispatch(kMax, kMax, kMax);
    ASSERT_DEVICE_ERROR(TestDispatch(kMax + 1, 1, 1));
    ASSERT_DEVICE_ERROR(TestDispatch(1, kMax + 1, 1));
    ASSERT_DEVICE_ERROR(TestDispatch(1, 1, kMax + 1));
    ASSERT_DEVICE_ERROR(TestDispatch(kMax + 1, kMax + 1, kMax + 1));
}
