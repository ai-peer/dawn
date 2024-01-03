// Copyright 2024 The Dawn & Tint Authors
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

#include <chrono>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "dawn/tests/DawnTest.h"

#include "dawn/utils/WGPUHelpers.h"

using ::testing::HasSubstr;

namespace dawn {
namespace {

enum class Control : uint32_t {
    kSkipLoop = 0,
    kExecuteLoop = 1,
};

using LoopStr = const char*;
DAWN_TEST_PARAM_STRUCT(Params, LoopStr);

// Tests WGSL shaders that either terminate quickly, or run an infinite loop.
class ComputeInfiniteLoopDeathTest : public DawnTestWithParams<Params> {
  public:
    // Runs a compute shader pipeline that conditionally executes a infinite loop,
    // and then either terminates quickly, or is forcibly killed.
    // This is the body of a death test, and will terminate the process.
    // "status:ready" is emitted to stderr if the pipeline ended quickly,
    // and "status:timeout" is emitted to stderr if the pipeline had to be killed.
    void RunTest(const std::string& loop, Control control);
    // Runs a compute pipeline with an input buffer, and an buffer of expected outputs
    // that are checked if the pipeline terminates. The control input specifies
    // whether the given infinite loop in the shader should be executed.
    void RunPipeline(const std::string& shader,
                     Control control,
                     const std::vector<uint32_t>& expected);

    // Returns a shader string with a given loop statement.
    // The shader behaves as follows:
    //   If the 'control' buffer contains 0, then the loop statement is never
    //   executed. Otherwise, the loop statement is executed.
    //   The shader will write sentinel values 1, 2, 3 to the output buffer locations
    //   0, 1, 2 as it reaches key points in its control flow.
    std::string MakeShader(const std::string& loop) const;

    // Returns the loop string parameter for the test.
    const char* GetLoopStr() { return GetParam().mLoopStr; }
};

void ComputeInfiniteLoopDeathTest::RunPipeline(const std::string& shader,
                                               Control control,
                                               const std::vector<uint32_t>& expected) {
    // Set up shader and pipeline
    auto module = utils::CreateShaderModule(device, shader.c_str());

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";

    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    // Set up src storage buffer
    wgpu::Buffer src = utils::CreateBufferFromData(
        device, &control, sizeof(control),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

    // Set up dst storage buffer
    std::vector<uint32_t> dst_init_values(expected.size(), 0xCAFEBEBE);

    wgpu::Buffer dst = utils::CreateBufferFromData(
        device, dst_init_values.data(), dst_init_values.size() * sizeof(dst_init_values[0]),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

    // Set up bind group and issue dispatch
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {
                                                         {0, src},
                                                         {1, dst},
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
    ResolveDeferredExpectationsNow();
    WaitForAllOperations();
}

void ComputeInfiniteLoopDeathTest::RunTest(const std::string& loop, Control control) {
    // These are the expected output values when the pipeline terminates.
    const auto shader = MakeShader(loop);
    const std::vector<uint32_t> expected = {1, 2, 3};

    // Time a dry run of the pipeline.
    const auto start = std::chrono::steady_clock::now();
    RunPipeline(shader, Control::kSkipLoop, expected);
    const auto end = std::chrono::steady_clock::now();
    const std::chrono::duration<double> dry_run_duration = end - start;

    // Run the pipeline, with the specified control, with a reasonable timeout.
    // When control is kSkipLoop, it should take roughly the same time as the dry run.
    // When control is kExecuteLoop, it would run forver without the timeout. Kill it after
    // an additional second, roughly.
    auto after_reasonable_delay =
        std::chrono::steady_clock::now() + dry_run_duration + std::chrono::seconds(1);

    auto future = std::async(
        std::launch::async,
        [&](const std::string& shader) { RunPipeline(shader, control, expected); }, shader);

    // Determine if the pipeline was forcibly killed, and signal the result on the
    // stderr output stream.
    const auto future_result = future.wait_until(after_reasonable_delay);
    switch (future_result) {
        case std::future_status::ready:
            std::cerr << "status:ready " << std::endl;
            break;
        case std::future_status::timeout:
            std::cerr << "status:timeout " << std::endl;
            break;
        default:
            ASSERT_TRUE(false) << "unexpected future state from async function launch";
            break;
    }

    // Terminate the process, as required by a death test.
    std::terminate();
}

// Returns a shader string with a given loop statement.
// The shader behaves as follows:
//   If the 'control' buffer contains 0, then the loop statement is never
//   executed. Otherwise, the loop statement is executed.
//   The shader will write sentinel values 1, 2, 3 to the output buffer locations
//   0, 1, 2 as it reaches key points in its control flow.
std::string ComputeInfiniteLoopDeathTest::MakeShader(const std::string& loop) const {
    std::ostringstream out;
    out << R"(
@group(0) @binding(0) var<storage, read>       control: u32;
@group(0) @binding(1) var<storage, read_write> output: array<u32>;

@compute @workgroup_size(1)
fn main() {
  output[0] = 1;
  if (control == )"
        << uint32_t(Control::kExecuteLoop) << R"() {
    )" << loop
        << R"(
    output[1] = 2;
  }
  output[2] = 3;
}
)";
    return out.str();
}

// Returns a list of infinite WGSL loops that satisfy behaviour analysis.
std::vector<const char*> ValidInfiniteLoops() {
    return {
        // Due to behaviour analysis, infinite loops must use a condition
        // even the value can be trivially evaluated.
        // See https://gpuweb.github.io/gpuweb/wgsl/#behaviors
        "while true { }",
        "loop { continuing { break if false; } }",
        "loop { if false { break; } }",
        "loop { if false { break; } continuing {} }",
        "for ( ; true ; ) { }",
    };
}

TEST_P(ComputeInfiniteLoopDeathTest, SkipLoop) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH_IF_SUPPORTED(RunTest(GetLoopStr(), Control::kSkipLoop), HasSubstr("status:ready"));
}

TEST_P(ComputeInfiniteLoopDeathTest, ExecuteLoop) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    const auto shader = MakeShader(GetLoopStr());
    EXPECT_DEATH_IF_SUPPORTED(RunTest(GetLoopStr(), Control::kExecuteLoop),
                              HasSubstr("status:timeout"));
}

DAWN_INSTANTIATE_TEST_B(ComputeInfiniteLoopDeathTest, {MetalBackend()}, ValidInfiniteLoops());

// TODO(tint:2125): test the following backends
//  D3D11Backend(),
//  D3D12Backend(),
//  OpenGLBackend(),
//  OpenGLESBackend(),
//  VulkanBackend());

}  // anonymous namespace
}  // namespace dawn
