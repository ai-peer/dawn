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

// Test that the creation of the compute pipeline object should fail when the shader module is null,
// DAWN_ENABLE_ASSERTS is defined and Toggle::ReportErrorOnNullptrObjectInDeviceValidateObject is
// set.
#if defined(DAWN_ENABLE_ASSERTS)
TEST_F(ComputeValidationTest, UseNullShaderModule) {
    std::vector<const char*> forceEnabledToggles(
        {"report_error_on_nullptr_object_in_device_validate_object"});
    dawn::Device deviceWithToggle = CreateDeviceFromAdapter(adapter, {}, forceEnabledToggles);

    dawn::ComputePipelineDescriptor csDesc;
    csDesc.layout = utils::MakeBasicPipelineLayout(deviceWithToggle, nullptr);
    csDesc.computeStage.module = nullptr;
    csDesc.computeStage.entryPoint = "main";
    // dawn::ComputePipeline pipeline = deviceWithToggle.CreateComputePipeline(&csDesc);
    ASSERT_DEVICE_ERROR(deviceWithToggle.CreateComputePipeline(&csDesc));
}
#endif

// TODO(cwallez@chromium.org): Add a regression test for Disptach validation trying to acces the
// input state.
