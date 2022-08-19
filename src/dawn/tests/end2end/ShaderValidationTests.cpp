// Copyright 2021 The Dawn Authors
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

#include <numeric>
#include <string>
#include <vector>

#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

// The compute shader workgroup size is settled at compute pipeline creation time.
// The validation code in dawn is in each backend thus this test needs to be as part of a
// dawn_end2end_tests instead of the dawn_unittests
class WorkgroupSizeValidationTest : public DawnTest {
  public:
    void SetUpShadersWithInvalidFixedValues() {
        computeModule = utils::CreateShaderModule(device, R"(
@compute @workgroup_size(1, 1, 9999) fn main() {
    _ = 0u;
})");
    }

    void SetUpShadersWithValidFixedValues() {
        computeModule = utils::CreateShaderModule(device, R"(
@compute @workgroup_size(1, 1, 1) fn main() {
    _ = 0u;
})");
    }

    void SetUpShadersWithValidDefaultValueConstants() {
        computeModule = utils::CreateShaderModule(device, R"(
override x: u32 = 1u;
override y: u32 = 1u;
override z: u32 = 1u;

@compute @workgroup_size(x, y, z) fn main() {
    _ = 0u;
})");
    }

    void SetUpShadersWithZeroDefaultValueConstants() {
        computeModule = utils::CreateShaderModule(device, R"(
override x: u32 = 0u;
override y: u32 = 0u;
override z: u32 = 0u;

@compute @workgroup_size(x, y, z) fn main() {
    _ = 0u;
})");
    }

    void SetUpShadersWithOutOfLimitsDefaultValueConstants() {
        computeModule = utils::CreateShaderModule(device, R"(
override x: u32 = 1u;
override y: u32 = 1u;
override z: u32 = 9999u;

@compute @workgroup_size(x, y, z) fn main() {
    _ = 0u;
})");
    }

    void SetUpShadersWithUninitializedConstants() {
        computeModule = utils::CreateShaderModule(device, R"(
override x: u32;
override y: u32;
override z: u32;

@compute @workgroup_size(x, y, z) fn main() {
    _ = 0u;
})");
    }

    void SetUpShadersWithPartialConstants() {
        computeModule = utils::CreateShaderModule(device, R"(
override x: u32;

@compute @workgroup_size(x, 1, 1) fn main() {
    _ = 0u;
})");
    }

    void TestCreatePipeline() {
        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = computeModule;
        csDesc.compute.entryPoint = "main";
        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);
    }

    void TestCreatePipeline(const std::vector<wgpu::ConstantEntry>& constants) {
        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = computeModule;
        csDesc.compute.entryPoint = "main";
        csDesc.compute.constants = constants.data();
        csDesc.compute.constantCount = constants.size();
        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);
    }

    void TestInitializedWithZero() {
        std::vector<wgpu::ConstantEntry> constants{
            {nullptr, "x", 0}, {nullptr, "y", 0}, {nullptr, "z", 0}};
        TestCreatePipeline(constants);
    }

    void TestInitializedWithOutOfLimitValue() {
        std::vector<wgpu::ConstantEntry> constants{
            {nullptr, "x", 9999}, {nullptr, "y", 8888}, {nullptr, "z", 7777}};
        TestCreatePipeline(constants);
    }

    void TestInitializedWithValidValue() {
        std::vector<wgpu::ConstantEntry> constants{
            {nullptr, "x", 1}, {nullptr, "y", 1}, {nullptr, "z", 1}};
        TestCreatePipeline(constants);
    }

    void TestInitializedPartially() {
        std::vector<wgpu::ConstantEntry> constants{{nullptr, "y", 1}};
        TestCreatePipeline(constants);
    }

    wgpu::ShaderModule computeModule;
    wgpu::Buffer buffer;
};

// Test workgroup size validation with valid fixed values.
TEST_P(WorkgroupSizeValidationTest, WithValidFixedValues) {
    SetUpShadersWithValidFixedValues();
    TestCreatePipeline();
}

// Test workgroup size validation with invalid fixed values.
TEST_P(WorkgroupSizeValidationTest, WithInvalidFixedValues) {
    SetUpShadersWithInvalidFixedValues();
    ASSERT_DEVICE_ERROR(TestCreatePipeline());
}

// Test workgroup size validation with valid overrides default values.
TEST_P(WorkgroupSizeValidationTest, WithValidDefault) {
    SetUpShadersWithValidDefaultValueConstants();
    {
        // Valid default
        TestCreatePipeline();
    }
    {
        // Error: invalid value (zero)
        ASSERT_DEVICE_ERROR(TestInitializedWithZero());
    }
    {
        // Error: invalid value (out of device limits)
        ASSERT_DEVICE_ERROR(TestInitializedWithOutOfLimitValue());
    }
    {
        // Valid: initialized partially
        TestInitializedPartially();
    }
    {
        // Valid
        TestInitializedWithValidValue();
    }
}

// Test workgroup size validation with zero as the overrides default values.
TEST_P(WorkgroupSizeValidationTest, WithZeroDefault) {
    // Error: zero is detected as invalid at shader creation time
    ASSERT_DEVICE_ERROR(SetUpShadersWithZeroDefaultValueConstants());
}

// Test workgroup size validation with out-of-limits overrides default values.
TEST_P(WorkgroupSizeValidationTest, WithOutOfLimitsDefault) {
    SetUpShadersWithOutOfLimitsDefaultValueConstants();
    {
        // Error: invalid default
        ASSERT_DEVICE_ERROR(TestCreatePipeline());
    }
    {
        // Error: invalid value (zero)
        ASSERT_DEVICE_ERROR(TestInitializedWithZero());
    }
    {
        // Error: invalid value (out of device limits)
        ASSERT_DEVICE_ERROR(TestInitializedWithOutOfLimitValue());
    }
    {
        // Error: initialized partially
        ASSERT_DEVICE_ERROR(TestInitializedPartially());
    }
    {
        // Valid
        TestInitializedWithValidValue();
    }
}

// Test workgroup size validation without overrides default values specified.
TEST_P(WorkgroupSizeValidationTest, WithUninitialized) {
    SetUpShadersWithUninitializedConstants();
    {
        // Error: uninitialized
        ASSERT_DEVICE_ERROR(TestCreatePipeline());
    }
    {
        // Error: invalid value (zero)
        ASSERT_DEVICE_ERROR(TestInitializedWithZero());
    }
    {
        // Error: invalid value (out of device limits)
        ASSERT_DEVICE_ERROR(TestInitializedWithOutOfLimitValue());
    }
    {
        // Error: initialized partially
        ASSERT_DEVICE_ERROR(TestInitializedPartially());
    }
    {
        // Valid
        TestInitializedWithValidValue();
    }
}

// Test workgroup size validation after being overrided with invalid values.
TEST_P(WorkgroupSizeValidationTest, ValidationAfterOverride) {
    SetUpShadersWithUninitializedConstants();
    {
        // Error: exceed maxComputeWorkgroupSizeZ
        std::vector<wgpu::ConstantEntry> constants{
            {nullptr, "x", 1}, {nullptr, "y", 1}, {nullptr, "z", 9999}};
        ASSERT_DEVICE_ERROR(TestCreatePipeline(constants));
    }
    {
        // Error: exceed maxComputeInvocations
        std::vector<wgpu::ConstantEntry> constants{
            {nullptr, "x", 128}, {nullptr, "y", 128}, {nullptr, "z", 1}};
        ASSERT_DEVICE_ERROR(TestCreatePipeline(constants));
    }
}

DAWN_INSTANTIATE_TEST(WorkgroupSizeValidationTest, D3D12Backend(), MetalBackend(), VulkanBackend());
