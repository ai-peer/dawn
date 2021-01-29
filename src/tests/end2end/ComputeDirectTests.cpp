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

#include "tests/DawnTest.h"

#include "utils/WGPUHelpers.h"

#include <initializer_list>

class ComputeDirectTests : public DawnTest {
  public:
    void BasicTest(uint32_t x, uint32_t y, uint32_t z);
};

void ComputeDirectTests::BasicTest(uint32_t x, uint32_t y, uint32_t z) {
    // Set up shader and pipeline

    // Write into the output buffer if we saw the zero dispatch or biggest dispatch
    // This is a workaround since D3D12 doesn't have gl_NumWorkGroups
    wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, R"(
        [[block]] struct InputBuf {
            [[offset(0)]] expectedDispatch : vec3<u32>;
        };
        [[block]] struct OutputBuf {
            [[offset(0)]] workGroups : vec3<u32>;
        };

        [[group(0), binding(0)]] var<uniform> input : InputBuf;
        [[group(0), binding(1)]] var<storage_buffer> output : OutputBuf;

        [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

        [[stage(compute), workgroup_size(1, 1, 1)]]
        fn main() -> void {
            const dispatch : vec3<u32> = input.expectedDispatch;
            if (dispatch.x * dispatch.y * dispatch.z == 0 ||
                all(GlobalInvocationID == dispatch - vec3<u32>(1u, 1u, 1u))) {
                output.workGroups = dispatch;
            }
        })");

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.computeStage.module = module;
    csDesc.computeStage.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    constexpr static std::initializer_list<uint32_t> kSentinelData{~uint32_t(0), ~uint32_t(0),
                                                                   ~uint32_t(0)};

    // Set up dst storage buffer to contain dispatch x, y, z
    wgpu::Buffer dst = utils::CreateBufferFromData<uint32_t>(
        device,
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst,
        kSentinelData);

    std::initializer_list<uint32_t> expectedBufferData{x, y, z};
    wgpu::Buffer expectedBuffer = utils::CreateBufferFromData<uint32_t>(
        device, wgpu::BufferUsage::Uniform, expectedBufferData);

    // Set up bind group and issue dispatch
    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                             {
                                 {0, expectedBuffer, 0, 3 * sizeof(uint32_t)},
                                 {1, dst, 0, 3 * sizeof(uint32_t)},
                             });

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.Dispatch(x, y, z);
        pass.EndPass();

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);

    std::vector<uint32_t> expected = x * y * z == 0 ? kSentinelData : expectedBufferData;

    // Verify the dispatch got called if all group counts are not zero
    EXPECT_BUFFER_U32_RANGE_EQ(&expected[0], dst, 0, 3);
}

// Test basic dispatch
TEST_P(ComputeDirectTests, Basic) {
    BasicTest(2, 3, 4);
}

// Test noop dispatch
TEST_P(ComputeDirectTests, Noop) {
    // All dimensions are 0s
    BasicTest(0, 0, 0);

    // Only x dimension is 0
    BasicTest(0, 3, 4);

    // Only y dimension is 0
    BasicTest(2, 0, 4);

    // Only z dimension is 0
    BasicTest(2, 3, 0);
}

DAWN_INSTANTIATE_TEST(ComputeDirectTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
