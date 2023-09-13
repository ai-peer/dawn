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

#include <string>

#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

constexpr unsigned int kNumDispatches = 100;

enum class Type {
    Float,
    Integer,
};

enum class Op {
    AccMul,
    AccAdd,
};

enum class Loop {
    Loop,
    Unroll,
};

std::ostream& operator<<(std::ostream& ostream, const Type& type) {
    switch (type) {
        case Type::Float:
            ostream << "Float";
            break;
        case Type::Integer:
            ostream << "Integer";
            break;
    }
    return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const Op& op) {
    switch (op) {
        case Op::AccMul:
            ostream << "AccMul";
            break;
        case Op::AccAdd:
            ostream << "AccAdd";
            break;
    }
    return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const Loop& loop) {
    switch (loop) {
        case Loop::Loop:
            ostream << "Loop";
            break;
        case Loop::Unroll:
            ostream << "Unroll";
            break;
    }
    return ostream;
}

using Count = uint32_t;
DAWN_TEST_PARAM_STRUCT(ShaderMathParams, Type, Op, Loop, Count);

class ShaderMathPerf : public DawnPerfTestWithParams<ShaderMathParams> {
  public:
    ShaderMathPerf()
        : DawnPerfTestWithParams(/*iterationsPerStep=*/kNumDispatches, /*maxStepsInFlight=*/3) {}
    ~ShaderMathPerf() override = default;

    void SetUp() override;

  private:
    void Step() override;

    wgpu::ComputePipeline mPipeline;
    wgpu::BindGroup mBindGroup;
};

void ShaderMathPerf::SetUp() {
    DawnPerfTestWithParams<ShaderMathParams>::SetUp();

    std::string shader = "";

    switch (GetParam().mType) {
        case Type::Float:
            shader += "alias TestType = vec4<f32>;\n";
            break;
        case Type::Integer:
            shader += "alias TestType = vec4<i32>;\n";
            break;
    }

    std::string op;
    switch (GetParam().mOp) {
        case Op::AccAdd:
            op = "+=";
            break;
        case Op::AccMul:
            op = "*=";
            break;
    }

    shader += R"(
    struct Dst {
        values: array<vec4f>
    }
    @group(0) @binding(0) var<storage, read_write> dst : Dst;

    @compute @workgroup_size(64, 1, 1)
    fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
      var acc : TestType = TestType(1, 2, 3, 4);
    )";

    std::string testOp = "acc " + op + "acc;\n";
    switch (GetParam().mLoop) {
        case Loop::Loop:
            shader += "const kCount = " + std::to_string(GetParam().mCount) + ";\n";
            shader += "for (var i : i32 = 0; i < kCount; i++) { " + testOp + " }";
            break;
        case Loop::Unroll:
            for (uint32_t i = 0; i < GetParam().mCount; i++) {
                shader += testOp;
            }
            break;
    }
    shader += R"(
      dst.values[gid.x] = vec4f(acc);
    }
    )";

    wgpu::ShaderModule module = utils::CreateShaderModule(device, shader.c_str());

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    csDesc.compute.entryPoint = "main";
    mPipeline = device.CreateComputePipeline(&csDesc);

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = 16 * 4096;
    bufferDesc.usage = wgpu::BufferUsage::Storage;

    wgpu::Buffer dst = device.CreateBuffer(&bufferDesc);
    mBindGroup = utils::MakeBindGroup(device, mPipeline.GetBindGroupLayout(0), {{0, dst}});
}

void ShaderMathPerf::Step() {
    wgpu::CommandEncoder commands = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = commands.BeginComputePass();

    pass.SetPipeline(mPipeline);
    pass.SetBindGroup(0, mBindGroup);
    for (uint32_t i = 0; i < kNumDispatches; ++i) {
        pass.DispatchWorkgroups(4096 / 64, 1, 1);
    }

    pass.End();
    wgpu::CommandBuffer commandBuffer = commands.Finish();
    queue.Submit(1, &commandBuffer);
}

TEST_P(ShaderMathPerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_TEST_P(ShaderMathPerf,
                        {D3D12Backend(), MetalBackend(), OpenGLBackend(), VulkanBackend()},
                        {Type::Float, Type::Integer},
                        {Op::AccMul, Op::AccAdd},
                        {Loop::Loop, Loop::Unroll},
                        {100u, 1000u});

}  // anonymous namespace
}  // namespace dawn
