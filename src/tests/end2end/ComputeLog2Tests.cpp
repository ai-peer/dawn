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

#include <array>

class ComputeLog2Tests : public DawnTest {
  public:
    static constexpr int kSteps = 10;
};

// Test that logs are being properly calculated
TEST_P(ComputeLog2Tests, chromium104662) {
    std::vector<uint32_t> data(kSteps, 0);
    std::vector<uint32_t> expected{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint64_t bufferSize = static_cast<uint64_t>(data.size() * sizeof(uint32_t));
    wgpu::Buffer buffer = utils::CreateBufferFromData(
        device, data.data(), bufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    std::string shader = R"(
[[block]] struct Buf {
  [[offset(0)]] data : [[stride(4)]] array<u32, 10>;
};

[[group(0), binding(0)]] var<storage_buffer> buf : [[access(read_write)]] Buf;

[[stage(compute)]] fn main() -> void {
  buf.data[0] = u32(log2(1.0));
  buf.data[1] = u32(log2(2.0));
  buf.data[2] = u32(log2(4.0));
  buf.data[3] = u32(log2(8.0));
  buf.data[4] = u32(log2(16.0));
  buf.data[5] = u32(log2(32.0));
  buf.data[6] = u32(log2(64.0));
  buf.data[7] = u32(log2(128.0));
  buf.data[8] = u32(log2(256.0));
  buf.data[9] = u32(log2(512.0));
})";

    // Set up shader and pipeline
    auto module = utils::CreateShaderModuleFromWGSL(device, shader.c_str());

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.computeStage.module = module;
    csDesc.computeStage.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0), {{0, buffer, 0, bufferSize}});

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.Dispatch(1);
        pass.EndPass();

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_RANGE_EQ(expected.data(), buffer, 0, kSteps);
}

DAWN_INSTANTIATE_TEST(ComputeLog2Tests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
