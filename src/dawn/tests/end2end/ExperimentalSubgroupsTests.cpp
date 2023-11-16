// Copyright 2022 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <vector>

#include "dawn/tests/DawnTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

using RequestSubgroupsExtension = bool;
DAWN_TEST_PARAM_STRUCT(ExperimentalSubgroupsTestsParams, RequestSubgroupsExtension);

class ExperimentalSubgroupsTests : public DawnTestWithParams<ExperimentalSubgroupsTestsParams> {
  protected:
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        mIsSubgroupsSupportedOnAdapter =
            SupportsFeatures({wgpu::FeatureName::ChromiumExperimentalSubgroups});
        if (!mIsSubgroupsSupportedOnAdapter) {
            return {};
        }

        if (!IsD3D12()) {
            mUseDxcEnabledOrNonD3D12 = true;
        } else {
            bool useDxcDisabled = false;
            for (auto* disabledToggle : GetParam().forceDisabledWorkarounds) {
                if (strncmp(disabledToggle, "use_dxc", strlen("use_dxc")) == 0) {
                    useDxcDisabled = true;
                    break;
                }
            }
            mUseDxcEnabledOrNonD3D12 = !useDxcDisabled;
        }

        if (GetParam().mRequestSubgroupsExtension && mUseDxcEnabledOrNonD3D12) {
            return {wgpu::FeatureName::ChromiumExperimentalSubgroups};
        }

        return {};
    }

    // Helper function that validate the device support (or not) ChromiumExperimentalSubgroups
    // feature as expected, and return whether supported.
    bool GetFeatureSupportStatus() {
        const bool shouldSubgroupsFeatureSupportedByDevice =
            // Required when creating device
            GetParam().mRequestSubgroupsExtension &&
            // Adapter support the feature
            IsSubgroupsSupportedOnAdapter() &&
            // Proper toggle, allow_unsafe_apis and use_dxc if d3d12
            // Note that "allow_unsafe_apis" is always enabled in
            // DawnTestEnvironment::CreateInstance.
            HasToggleEnabled("allow_unsafe_apis") && UseDxcEnabledOrNonD3D12();
        const bool deviceSupportSubgroupsFeature =
            device.HasFeature(wgpu::FeatureName::ChromiumExperimentalSubgroups);
        EXPECT_EQ(deviceSupportSubgroupsFeature, shouldSubgroupsFeatureSupportedByDevice);
        return deviceSupportSubgroupsFeature;
    }

    // Helper function that create shader module with subgroups extension required and a empty
    // compute entry point, named main, of given workgroup size
    wgpu::ShaderModule CreateShaderModuleWithSubgroupsRequired(  //
        WGPUExtent3D workgroupSize = {1, 1, 1}) {
        std::stringstream code;
        code << R"(
        enable chromium_experimental_subgroups;

        @compute @workgroup_size()"
             << workgroupSize.width << ", " << workgroupSize.height << ", "
             << workgroupSize.depthOrArrayLayers << R"()
        fn main() {}
)";
        return utils::CreateShaderModule(device, code.str().c_str());
    }

    // Helper function that create shader module with subgroups extension required and a empty
    // compute entry point, named main, of workgroup size that are override constants.
    wgpu::ShaderModule CreateShaderModuleWithOverrideWorkgroupSize() {
        std::stringstream code;
        code << R"(
        enable chromium_experimental_subgroups;

        override wgs_x: u32;
        override wgs_y: u32;
        override wgs_z: u32;

        @compute @workgroup_size(wgs_x, wgs_y, wgs_z)
        fn main() {}
)";
        return utils::CreateShaderModule(device, code.str().c_str());
    }

    // Return a ErrorCallBack that expect that no error generated within the error scope
    auto ExpectNoErrorInScope() {
        return [](WGPUErrorType type, char const* msg, void*) {
            // Expect that no error generated within the error scope
            EXPECT_EQ(type, WGPUErrorType::WGPUErrorType_NoError);
            if (type != WGPUErrorType::WGPUErrorType_NoError) {
                printf("ExpectNoErrorInScope get error: %s\n", msg);
            }
        };
    }

    // Return a ErrorCallBack that expect that no error generated within the error scope
    auto ExpectValidationErrorInScope() {
        return [](WGPUErrorType type, char const*, void*) {
            // Expect that no error generated within the error scope
            // EXPECT_EQ(type, WGPUErrorType::WGPUErrorType_Validation);
            EXPECT_EQ(type, WGPUErrorType::WGPUErrorType_Validation);
        };
    }

    bool IsSubgroupsSupportedOnAdapter() const { return mIsSubgroupsSupportedOnAdapter; }
    bool UseDxcEnabledOrNonD3D12() const { return mUseDxcEnabledOrNonD3D12; }

  private:
    bool mIsSubgroupsSupportedOnAdapter = false;
    bool mUseDxcEnabledOrNonD3D12 = false;
};

// Test that dispatching a shader module requiring chromium_experimental_subgroups can success if
// device support ChromiumExperimentalSubgroups feature, otherwise shader module creation should
// fail.
TEST_P(ExperimentalSubgroupsTests, DispatchShaderWithSubgroupsRequired) {
    if (!GetFeatureSupportStatus()) {
        ASSERT_DEVICE_ERROR(CreateShaderModuleWithSubgroupsRequired());
        return;
    }

    device.PushErrorScope(wgpu::ErrorFilter::Validation);

    auto shaderModule = CreateShaderModuleWithSubgroupsRequired();

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = shaderModule;
    csDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);
    pass.DispatchWorkgroups(1);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    device.PopErrorScope(ExpectNoErrorInScope(), nullptr);
}

// Test that creating compute pipeline with full subgroups required will validate the workgroup size
// as expected.
TEST_P(ExperimentalSubgroupsTests, ComputePipelineRequiringFullSubgroups) {
    if (!GetFeatureSupportStatus()) {
        return;
    }

    struct TestCase {
        WGPUExtent3D workgroupSize;
        bool isFullSubgroups;
    };

    std::initializer_list<TestCase> casesList = {
        // Maximium possible subgroup size is 128, so 128 can fullfill subgroups of any size.
        {{128, 1, 1}, true},
        // 127 can not fullfill any subgroup size.
        {{127, 1, 1}, false},
        // Test that workgroup size validation check the X dimension, instead of whole threads
        // number.
        {{1, 128, 1}, false},
    };

    auto testConstantWorkgroupSizeShader = [&](const TestCase& c, bool requireFullSubgroups) {
        device.PushErrorScope(wgpu::ErrorFilter::Validation);
        auto shaderModule = CreateShaderModuleWithSubgroupsRequired(c.workgroupSize);
        device.PopErrorScope(ExpectNoErrorInScope(), nullptr);

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = shaderModule;
        csDesc.compute.entryPoint = "main";

        wgpu::DawnComputePipelineDescriptorExperimentalSubgroupOptions subgroupOptions;
        subgroupOptions.requireFullSubgroups = requireFullSubgroups;
        csDesc.nextInChain = &subgroupOptions;

        device.PushErrorScope(wgpu::ErrorFilter::Validation);

        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

        if (requireFullSubgroups && !c.isFullSubgroups) {
            device.PopErrorScope(ExpectValidationErrorInScope(), nullptr);
            return;
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.DispatchWorkgroups(1);
        pass.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        device.PopErrorScope(ExpectNoErrorInScope(), nullptr);
    };

    auto testOverrideWorkgroupSizeShader = [&](const TestCase& c, bool requireFullSubgroups) {
        device.PushErrorScope(wgpu::ErrorFilter::Validation);
        auto shaderModule = CreateShaderModuleWithOverrideWorkgroupSize();
        device.PopErrorScope(ExpectNoErrorInScope(), nullptr);

        std::vector<wgpu::ConstantEntry> constants{
            {nullptr, "wgs_x", static_cast<double>(c.workgroupSize.width)},
            {nullptr, "wgs_y", static_cast<double>(c.workgroupSize.height)},
            {nullptr, "wgs_z", static_cast<double>(c.workgroupSize.depthOrArrayLayers)},
        };

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = shaderModule;
        csDesc.compute.entryPoint = "main";
        csDesc.compute.constants = constants.data();
        csDesc.compute.constantCount = constants.size();

        wgpu::DawnComputePipelineDescriptorExperimentalSubgroupOptions subgroupOptions;
        subgroupOptions.requireFullSubgroups = requireFullSubgroups;
        csDesc.nextInChain = &subgroupOptions;

        device.PushErrorScope(wgpu::ErrorFilter::Validation);

        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

        if (requireFullSubgroups && !c.isFullSubgroups) {
            device.PopErrorScope(ExpectValidationErrorInScope(), nullptr);
            return;
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.DispatchWorkgroups(1);
        pass.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        device.PopErrorScope(ExpectNoErrorInScope(), nullptr);
    };

    for (bool useOverrideSize : {false, true}) {
        for (const TestCase& c : casesList) {
            for (bool requireFullSubgroups : {false, true}) {
                printf("%s, wgs: (%d, %d, %d), %s\n", (useOverrideSize ? "Override" : "Constant"),
                       c.workgroupSize.width, c.workgroupSize.height,
                       c.workgroupSize.depthOrArrayLayers,
                       (requireFullSubgroups ? "required" : "not required"));
                if (useOverrideSize) {
                    testOverrideWorkgroupSizeShader(c, requireFullSubgroups);
                } else {
                    testConstantWorkgroupSizeShader(c, requireFullSubgroups);
                }
            }
        }
    }
}

// DawnTestBase::CreateDeviceImpl always enables allow_unsafe_apis toggle.
DAWN_INSTANTIATE_TEST_P(ExperimentalSubgroupsTests,
                        {
                            D3D12Backend(),
                            D3D12Backend({}, {"use_dxc"}),
                            MetalBackend(),
                            VulkanBackend(),
                        },
                        {true, false});

}  // anonymous namespace
}  // namespace dawn
