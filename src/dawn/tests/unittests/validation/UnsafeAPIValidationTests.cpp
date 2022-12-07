// Copyright 2020 The Dawn Authors
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

#include <memory>
#include <vector>

#include "dawn/tests/MockCallback.h"
#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/ComboRenderBundleEncoderDescriptor.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {
using testing::HasSubstr;
}  // anonymous namespace

// UnsafeAPIValidationTest create the instance with toggle DisallowUnsafeApis explicitly enabled,
// and assert that creating adapter and device will succeed.
class UnsafeAPIValidationTest : public ValidationTest {
  protected:
    std::unique_ptr<dawn::native::Instance> CreateTestInstance() override {
        // Create an instance with toggle DisallowUnsafeApis explicitly enabled, which would be
        // inherited to adapter and device toggles and disallow experimental features.
        const char* disallowUnsafeApisToggle = "disallow_unsafe_apis";
        WGPUDawnTogglesDescriptor instanceToggles = {};
        instanceToggles.chain.sType = WGPUSType::WGPUSType_DawnTogglesDescriptor;
        instanceToggles.enabledTogglesCount = 1;
        instanceToggles.enabledToggles = &disallowUnsafeApisToggle;

        WGPUInstanceDescriptor instanceDesc = {};
        instanceDesc.nextInChain = &instanceToggles.chain;

        return std::make_unique<dawn::native::Instance>(&instanceDesc);
    }
};

// Check chromium_disable_uniformity_analysis is an unsafe API.
TEST_F(UnsafeAPIValidationTest, chromium_disable_uniformity_analysis) {
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        enable chromium_disable_uniformity_analysis;

        @compute @workgroup_size(8) fn uniformity_error(
            @builtin(local_invocation_id) local_invocation_id : vec3<u32>
        ) {
            if (local_invocation_id.x == 0u) {
                workgroupBarrier();
            }
        }
    )"));
}
