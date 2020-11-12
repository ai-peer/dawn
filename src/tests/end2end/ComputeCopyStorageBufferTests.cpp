// Copyright 2018 The Dawn Authors
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

class ComputeCopyStorageBufferTests : public DawnTest {
  public:
    static constexpr int kInstances = 4;
    static constexpr int kUintsPerInstance = 4;
    static constexpr int kNumUints = kInstances * kUintsPerInstance;

    void BasicTest(const char* shader);
};

void ComputeCopyStorageBufferTests::BasicTest(const char* shader) {
    // Set up shader and pipeline
    auto module = utils::CreateShaderModuleFromWGSL(device, shader);

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.computeStage.module = module;
    csDesc.computeStage.entryPoint = "main";

    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    // Set up src storage buffer
    wgpu::BufferDescriptor srcDesc;
    srcDesc.size = kNumUints * sizeof(uint32_t);
    srcDesc.usage =
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer src = device.CreateBuffer(&srcDesc);

    std::array<uint32_t, kNumUints> expected;
    for (uint32_t i = 0; i < kNumUints; ++i) {
        expected[i] = (i + 1u) * 0x11111111u;
    }
    queue.WriteBuffer(src, 0, expected.data(), sizeof(expected));
    EXPECT_BUFFER_U32_RANGE_EQ(expected.data(), src, 0, kNumUints);

    // Set up dst storage buffer
    wgpu::BufferDescriptor dstDesc;
    dstDesc.size = kNumUints * sizeof(uint32_t);
    dstDesc.usage =
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer dst = device.CreateBuffer(&dstDesc);

    std::array<uint32_t, kNumUints> zero{};
    queue.WriteBuffer(dst, 0, zero.data(), sizeof(zero));

    // Set up bind group and issue dispatch
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {
                                                         {0, src, 0, kNumUints * sizeof(uint32_t)},
                                                         {1, dst, 0, kNumUints * sizeof(uint32_t)},
                                                     });

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.Dispatch(kInstances);
        pass.EndPass();

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_RANGE_EQ(expected.data(), dst, 0, kNumUints);
}

// Test that a trivial compute-shader memcpy implementation works.
TEST_P(ComputeCopyStorageBufferTests, SizedArrayOfBasic) {
    BasicTest(R"(
        [[block]] struct Buf {
            [[offset(0)]] s : [[stride(16)]] array<vec4<u32>, 4>;
        };

        [[set(0), binding(0)]] var<storage_buffer> src : Buf;
        [[set(0), binding(1)]] var<storage_buffer> dst : Buf;

        [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

        [[stage(compute)]]
        fn main() -> void {
            var index : u32 = GlobalInvocationID.x;
            if (index >= 4) { return; }
            dst.s[index] = src.s[index];
            return;
        })");
}

// Test that a slightly-less-trivial compute-shader memcpy implementation works.
TEST_P(ComputeCopyStorageBufferTests, SizedArrayOfStruct) {
    BasicTest(R"(
        [[block]] struct S {
            [[offset(0)]] a : vec2<u32>;
            [[offset(8)]] b : vec2<u32>;
        };

        [[block]] struct Buf {
            [[offset(0)]] s : [[stride(16)]] array<S, 4>;
        };

        [[set(0), binding(0)]] var<storage_buffer> src : Buf;
        [[set(0), binding(1)]] var<storage_buffer> dst : Buf;

        [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

        [[stage(compute)]]
        fn main() -> void {
            var index : u32 = GlobalInvocationID.x;
            if (index >= 4) { return; }
            dst.s[index] = src.s[index];
            return;
        })");
}

// Test that a trivial compute-shader memcpy implementation works.
TEST_P(ComputeCopyStorageBufferTests, UnsizedArrayOfBasic) {
    BasicTest(R"(
        [[block]] struct Buf {
            [[offset(0)]] s : [[stride(16)]] array<vec4<u32>>;
        };

        [[set(0), binding(0)]] var<storage_buffer> src : Buf;
        [[set(0), binding(1)]] var<storage_buffer> dst : Buf;

        [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

        [[stage(compute)]]
        fn main() -> void {
            var index : u32 = GlobalInvocationID.x;
            if (index >= 4) { return; }
            dst.s[index] = src.s[index];
            return;
        })");
}

// Test binding a sized array of SSBO descriptors.
//
// This is disabled because WebGPU doesn't currently have binding arrays (equivalent to
// VkDescriptorSetLayoutBinding::descriptorCount). https://github.com/gpuweb/gpuweb/pull/61
TEST_P(ComputeCopyStorageBufferTests, DISABLED_SizedDescriptorArray) {
    BasicTest(R"(
        [[block]] struct S {
            [[offset(0)]] a : vec2<u32>;
            [[offset(8)]] b : vec2<u32>;
        };

        [[block]] struct Buf {
            [[offset(0)]] s : S;
        };

        [[set(0), binding(0)]] var<storage_buffer> src : Buf[4];
        [[set(0), binding(1)]] var<storage_buffer> dst : Buf[4];

        [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

        [[stage(compute)]]
        fn main() -> void {
            var index : u32 = GlobalInvocationID.x;
            if (index >= 4) { return; }
            dst[index].s = src[index].s;
            return;
        })");
}

// Test binding an unsized array of SSBO descriptors.
//
// TODO(kainino@chromium.org): This test may be somewhat wrong. I'm not sure whether this is
// supposed to be possible on the various native APIs.
// Linking on OpenGL fails with "OpenGL requires constant indexes for unsized array access(dst)".
TEST_P(ComputeCopyStorageBufferTests, DISABLED_UnsizedDescriptorArray) {
    BasicTest(R"(
        [[block]] struct S {
            [[offset(0)]] a : vec2<u32>;
            [[offset(8)]] b : vec2<u32>;
        };

        [[block]] struct Buf {
            [[offset(0)]] s : S;
        };

        [[set(0), binding(0)]] var<storage_buffer> src : Buf[];
        [[set(0), binding(1)]] var<storage_buffer> dst : Buf[];

        [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

        [[stage(compute)]]
        fn main() -> void {
            var index : u32 = GlobalInvocationID.x;
            if (index >= 4) { return; }
            dst[index].s = src[index].s;
            return;
        })");
}

DAWN_INSTANTIATE_TEST(ComputeCopyStorageBufferTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
