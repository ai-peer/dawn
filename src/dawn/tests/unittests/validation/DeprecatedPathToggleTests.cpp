// Copyright 2022 The Dawn Authors
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

#include "dawn/tests/unittests/validation/ValidationTest.h"

#include "dawn/native/null/DeviceNull.h"

#include "dawn/utils/WGPUHelpers.h"

// Tests that for various deprecated path, with Toggle::DisallowDeprecatedPath off, it emits
// deprecation warning. Otherwise with the toggle on, it emits validation error.
class DeprecatedPathToggleTests : public ValidationTest {
    void SetUp() override {
        ValidationTest::SetUp();
        DAWN_SKIP_TEST_IF(UsesWire());
    }

  public:
    wgpu::Device CreateDeviceWithDeprecatedPathDisallowed() {
        const char* toggleName = "disallow_deprecated_path";
        wgpu::DeviceDescriptor descriptor;
        wgpu::DawnTogglesDeviceDescriptor togglesDesc;
        descriptor.nextInChain = &togglesDesc;
        togglesDesc.forceEnabledToggles = &toggleName;
        togglesDesc.forceEnabledTogglesCount = 1;

        wgpu::Device result = wgpu::Device::Acquire(GetBackendAdapter().CreateDevice(&descriptor));

        result.SetUncapturedErrorCallback(ValidationTest::OnDeviceError, this);

        return result;
    }
};

// Now that we only test the disallow_deprecated_path toggle against MultisampledTextureSampleType
TEST_F(DeprecatedPathToggleTests, MultisampledTextureSampleType) {
    {
        EXPECT_DEPRECATION_WARNING(utils::MakeBindGroupLayout(
            device, {
                        {0, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::Float,
                         wgpu::TextureViewDimension::e2D, true},
                    }));
    }

    {
        wgpu::Device deviceDisallowDeprecatedPath = CreateDeviceWithDeprecatedPathDisallowed();

        ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
            deviceDisallowDeprecatedPath,
            {
                {0, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::Float,
                 wgpu::TextureViewDimension::e2D, true},
            }));
    }
}
