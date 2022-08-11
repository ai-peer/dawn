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

#include <vector>

#include "dawn/tests/DawnTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {
using RequireShaderF16Feature = bool;
DAWN_TEST_PARAM_STRUCT(ShaderF16TestsParams, RequireShaderF16Feature);

}  // anonymous namespace

class ShaderF16Tests : public DawnTestWithParams<ShaderF16TestsParams> {
  protected:
    void SetUp() override {
        DawnTestWithParams<ShaderF16TestsParams>::SetUp();
        mIsShaderF16SupportedOnAdapter = SupportsFeatures({wgpu::FeatureName::ShaderF16});
    }

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        if (mIsShaderF16SupportedOnAdapter && GetParam().mRequireShaderF16Feature) {
            mIsShaderF16FeatureRequired = true;
            return {wgpu::FeatureName::ShaderF16};
        }

        return {};
    }

    bool IsShaderF16SupportedOnAdapter() const { return mIsShaderF16SupportedOnAdapter; }

  private:
    bool mIsShaderF16SupportedOnAdapter = false;
    bool mIsShaderF16FeatureRequired = false;
};

/*
// Test that adapter don't support shader-f16 feature if enable_shader_f16 toggle is not enabled.
TEST_P(ShaderF16Tests, ShaderF16FeatureGuardedByToggle) {
    EXPECT_TRUE((!IsShaderF16SupportedOnAdapter()) || HasToggleEnabled("enable_shader_f16"));
}

TEST_P(ShaderF16Tests, RequireShaderF16FeatureTest) {

    const char* computeShader = R"(
        enable chromium_experimental_dp4a;

        struct Buf {
            data1 : i32,
            data2 : u32,
            data3 : i32,
            data4 : u32,
        }
        @group(0) @binding(0) var<storage, read_write> buf : Buf;

        @compute @workgroup_size(1)
        fn main() {
            var a = 0xFFFFFFFFu;
            var b = 0xFFFFFFFEu;
            var c = 0x01020304u;
            buf.data1 = dot4I8Packed(a, b);
            buf.data2 = dot4U8Packed(a, b);
            buf.data3 = dot4I8Packed(a, c);
            buf.data4 = dot4U8Packed(a, c);
        }
)";
    if (!GetParam().mRequireShaderF16Feature || !IsShaderF16SupportedOnAdapter() ||
        (IsD3D12() && !HasToggleEnabled("use_dxc"))) {
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, computeShader));
        return;
    }

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = 4 * sizeof(uint32_t);
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer bufferOut = device.CreateBuffer(&bufferDesc);

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = utils::CreateShaderModule(device, computeShader);
    csDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {
                                                         {0, bufferOut},
                                                     });

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.DispatchWorkgroups(1);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    uint32_t expected[] = {5, 259845, static_cast<uint32_t>(-10), 2550};
    EXPECT_BUFFER_U32_RANGE_EQ(expected, bufferOut, 0, 4);
}
*/

DAWN_INSTANTIATE_TEST_P(ShaderF16Tests,
                        {
                            D3D12Backend(),
                            VulkanBackend(),
                            MetalBackend(),
                            OpenGLBackend(),
                            OpenGLESBackend(),
                            NullBackend(),
                            D3D12Backend({}, {"disallow_unsafe_apis"}),
                            D3D12Backend({"use_dxc"}, {"disallow_unsafe_apis"}),
                            VulkanBackend({}, {"disallow_unsafe_apis"}),
                            MetalBackend({}, {"disallow_unsafe_apis"}),
                            OpenGLBackend({}, {"disallow_unsafe_apis"}),
                            OpenGLESBackend({}, {"disallow_unsafe_apis"}),
                            NullBackend({}, {"disallow_unsafe_apis"}),
                        },
                        {true, false});
