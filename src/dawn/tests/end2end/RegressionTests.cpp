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

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <vector>

#include "dawn/common/Math.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {

// Create a compute pipeline with all buffer in bufferList binded in order starting from slot 0, and
// run the given shader.
void RunComputeShaderWithBuffers(const wgpu::Device& device,
                                 const wgpu::Queue& queue,
                                 const std::string& shader,
                                 std::initializer_list<wgpu::Buffer> bufferList) {
    // Set up shader and pipeline
    auto module = utils::CreateShaderModule(device, shader.c_str());

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";

    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    // Set up bind group and issue dispatch
    std::vector<wgpu::BindGroupEntry> entries;
    uint32_t bufferSlot = 0;
    for (const wgpu::Buffer& buffer : bufferList) {
        wgpu::BindGroupEntry entry;
        entry.binding = bufferSlot++;
        entry.buffer = buffer;
        entry.offset = 0;
        entry.size = wgpu::kWholeSize;
        entries.push_back(entry);
    }

    wgpu::BindGroupDescriptor descriptor;
    descriptor.layout = pipeline.GetBindGroupLayout(0);
    descriptor.entryCount = static_cast<uint32_t>(entries.size());
    descriptor.entries = entries.data();

    wgpu::BindGroup bindGroup = device.CreateBindGroup(&descriptor);

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.DispatchWorkgroups(1);
        pass.End();

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);
}

class TintBug1753 : public DawnTest {
    void SetUp() override { DawnTestBase::SetUp(); }
};

TEST_P(TintBug1753, Test) {
    std::string shader = R"(
@group(0) @binding(0) var<storage, read_write> outputs : array<u32, 3>;

const values = array<u32, 3>(0xffbfffca, 0x09909909, 1);

@compute @workgroup_size(1,1,1)
fn main() {
    for (var i = 0u; i < 3; i++) {
        outputs[i] = values[i];
    }
})";

    std::vector<uint32_t> initData{0, 0, 0, 0};

    // Set up output storage buffer
    wgpu::Buffer outputBuf = utils::CreateBufferFromData(
        device, initData.data(), initData.size() * 4,
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

    RunComputeShaderWithBuffers(device, queue, shader, {outputBuf});

    // Check the data
    EXPECT_BUFFER_U32_EQ(0xffbfffca, outputBuf, 0);
    EXPECT_BUFFER_U32_EQ(0x09909909, outputBuf, 4);
    EXPECT_BUFFER_U32_EQ(1, outputBuf, 8);
}

DAWN_INSTANTIATE_TEST(TintBug1753, D3D12Backend({}));

}  // namespace
