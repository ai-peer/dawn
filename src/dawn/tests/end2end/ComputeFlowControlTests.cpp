// Copyright 2023 The Dawn Authors
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

#include "dawn/tests/DawnTest.h"

#include "dawn/utils/WGPUHelpers.h"

class ComputeFlowControlTests : public DawnTest {
  public:
    static constexpr int kUintsPerInstance = 4;
    static constexpr int kNumUints = kUintsPerInstance;

    void RunTest(const char* shader,
                 const std::vector<uint32_t>& inputs,
                 const std::vector<uint32_t>& expected);
};

void ComputeFlowControlTests::RunTest(const char* shader,
                                      const std::vector<uint32_t>& inputs,
                                      const std::vector<uint32_t>& expected) {
    // Set up shader and pipeline
    auto module = utils::CreateShaderModule(device, shader);

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";

    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    // Set up src storage buffer
    wgpu::BufferDescriptor srcDesc;
    srcDesc.size = inputs.size() * sizeof(uint32_t);
    srcDesc.usage =
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer src = device.CreateBuffer(&srcDesc);

    queue.WriteBuffer(src, 0, inputs.data(), inputs.size() * sizeof(uint32_t));
    EXPECT_BUFFER_U32_RANGE_EQ(inputs.data(), src, 0, inputs.size());

    // Set up dst storage buffer
    wgpu::BufferDescriptor dstDesc;
    dstDesc.size = expected.size() * sizeof(uint32_t);
    dstDesc.usage =
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer dst = device.CreateBuffer(&dstDesc);

    std::vector<uint32_t> zero(expected.size());
    queue.WriteBuffer(dst, 0, zero.data(), zero.size() * sizeof(uint32_t));

    // Set up bind group and issue dispatch
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {
                                                         {0, src, 0, srcDesc.size},
                                                         {1, dst, 0, dstDesc.size},
                                                     });

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
    EXPECT_BUFFER_U32_RANGE_EQ(expected.data(), dst, 0, expected.size());
}

TEST_P(ComputeFlowControlTests, IfFalse) {
    const char* shader = R"(
struct Outputs {
  count : u32,
  data  : array<u32>,
};
@group(0) @binding(0) var<storage, read>       inputs  : array<u32>;
@group(0) @binding(1) var<storage, read_write> outputs : Outputs;

fn push_output(value : u32) {
  let i = outputs.count;
  outputs.data[i] = value;
  outputs.count++;
}

@compute @workgroup_size(1)
fn main() {
  _ = &inputs;
  _ = &outputs;
  
  push_output(0);
  if (inputs[0] != 0) {
    push_output(1);
  } else {
    push_output(2);
  }
  push_output(3);
})";

    auto inputs = std::vector<uint32_t>{
        0  // take false branch
    };
    auto expected = std::vector<uint32_t>{3,   // count
                                          0,   // before if-else
                                          2,   // false branch
                                          3};  // after if-else
    RunTest(shader, inputs, expected);
}

DAWN_INSTANTIATE_TEST(ComputeFlowControlTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
