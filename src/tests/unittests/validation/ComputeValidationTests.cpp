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

#include "tests/unittests/validation/ValidationTest.h"
#include "utils/DawnHelpers.h"

class ComputeValidationTest : public ValidationTest {};

// Test that the creation of the compute pipeline object should fail when the shader module is null.
TEST_F(ComputeValidationTest, UseNullShaderModule) {
    // Not setting the compute shader module in the compute pipeline object should cause an error.
    {
        dawn::ComputePipelineDescriptor csDesc;
        csDesc.layout = utils::MakeBasicPipelineLayout(device, nullptr);
        csDesc.computeStage.module = nullptr;
        csDesc.computeStage.entryPoint = "main";
        ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&csDesc));
    }

    // Using a shader module that is not successfully built as the compute stage module should
    // cause an error.
    {
        dawn::ShaderModule failedComputeModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
                #version 450
                layout(local_size_x0 = 1) in;
                void main() {
                })");
        ASSERT_FALSE(failedComputeModule);

        dawn::ComputePipelineDescriptor csDesc;
        csDesc.layout = utils::MakeBasicPipelineLayout(device, nullptr);
        csDesc.computeStage.module = failedComputeModule;
        csDesc.computeStage.entryPoint = "main";
        ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&csDesc));
    }
}

// TODO(cwallez@chromium.org): Add a regression test for Disptach validation trying to acces the
// input state.
