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

#include "tests/perf_tests/DawnPerfTest.h"

#include <math.h>
#include "tests/ParamGenerator.h"
#include "utils/WGPUHelpers.h"

namespace {
    constexpr char kComputeShader[] = R"(
        [[block]] struct Uniforms {
            aShape : vec2<u32>;
            bShape : vec2<u32>;
            outShape : vec2<u32>;
        };
        [[block]] struct Matrix {
          numbers: array<u32>;
        };

        [[group(0), binding(0)]] var<storage> firstMatrix : [[access(read)]] Matrix;
        [[group(0), binding(1)]] var<storage> secondMatrix : [[access(read)]] Matrix;
        [[group(0), binding(2)]] var<storage> resultMatrix : [[access(write)]] Matrix;
        [[group(0), binding(3)]] var<uniform> uniforms : Uniforms;

        [[stage(compute), workgroup_size(2,2,1)]]
        fn main([[builtin(global_invocation_id)]] global_id  : vec3<u32>) {
            let resultCell : vec2<u32> = vec2<u32>(global_id.y, global_id.x);
            let dimInner : u32 = uniforms.aShape.y;
            let dimOutter: u32 = uniforms.outShape.y;
            var result : u32 = 0u;
            for (var i : u32 = 0u; i < dimInner; i = i + 1u) {
                let a : u32 = i + resultCell.x * dimInner;
                let b : u32 = resultCell.y + i * dimOutter;
                result = result + firstMatrix.numbers[a] * secondMatrix.numbers[b];
            }

            let index : u32 = resultCell.y + resultCell.x * dimOutter;
            resultMatrix.numbers[index] = result;
        })";
    constexpr unsigned int kNumIterations = 50;

    struct RobustnessParams : AdapterTestParam {
        RobustnessParams(const AdapterTestParam& param,
                         uint32_t dimAOuter,
                         uint32_t dimInner,
                         uint32_t dimBOuter)
            : AdapterTestParam(param),
              dimAOuter(dimAOuter),
              dimInner(dimInner),
              dimBOuter(dimBOuter) {
        }

        uint32_t dimAOuter;
        uint32_t dimInner;
        uint32_t dimBOuter;
    };

    std::ostream& operator<<(std::ostream& ostream, const RobustnessParams& param) {
        ostream << static_cast<const AdapterTestParam&>(param);

        ostream << "_" << param.dimAOuter << "_" << param.dimInner << "_" << param.dimBOuter;

        return ostream;
    }

}  // namespace

// Test matrix multiplication with A [dimAOuter, dimInner] * B [dimInner, dimBOuter]
// |kNumIterations| times.
class RobustnessPerf : public DawnPerfTestWithParams<RobustnessParams> {
  public:
    RobustnessPerf()
        : DawnPerfTestWithParams(kNumIterations, 1),
          mDimAOuter(GetParam().dimAOuter),
          mDimInner(GetParam().dimInner),
          mDimBOuter(GetParam().dimBOuter) {
    }
    ~RobustnessPerf() override = default;

    void SetUp() override;

  private:
    void Step() override;

    wgpu::Buffer dst;
    wgpu::BindGroup mBindGroup;
    wgpu::ComputePipeline mPipeline;
    uint32_t mDimAOuter;
    uint32_t mDimInner;
    uint32_t mDimBOuter;
};

void RobustnessPerf::SetUp() {
    DawnPerfTestWithParams<RobustnessParams>::SetUp();
    const size_t dataASize = mDimAOuter * mDimInner;
    std::vector<uint32_t> dataA(dataASize);
    for (uint32_t i = 0; i < static_cast<uint32_t>(dataASize); i++) {
        dataA[i] = i + 1u;
    }

    uint64_t byteASize = sizeof(uint32_t) * dataA.size();
    wgpu::BufferDescriptor desc = {};
    desc.size = byteASize;
    desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer bufA = device.CreateBuffer(&desc);
    queue.WriteBuffer(bufA, 0, dataA.data(), byteASize);

    const size_t dataBSize = mDimInner * mDimBOuter;
    std::vector<uint32_t> dataB(dataBSize);
    for (uint32_t i = 0; i < static_cast<uint32_t>(dataBSize); i++) {
        dataB[i] = i + 1u;
    }
    uint64_t byteBSize = sizeof(uint32_t) * dataB.size();
    desc.size = byteBSize;
    wgpu::Buffer bufB = device.CreateBuffer(&desc);
    queue.WriteBuffer(bufB, 0, dataB.data(), byteBSize);

    uint64_t byteDstSize = sizeof(uint32_t) * mDimAOuter * mDimBOuter;
    desc.size = byteDstSize;
    desc.usage =
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    dst = device.CreateBuffer(&desc);

    uint32_t uniformData[] = {mDimAOuter, mDimInner, mDimInner, mDimBOuter, mDimAOuter, mDimBOuter};
    wgpu::Buffer uniformBuffer = utils::CreateBufferFromData(
        device, uniformData, sizeof(uniformData), wgpu::BufferUsage::Uniform);

    // Set up shader and pipeline
    auto module = utils::CreateShaderModule(device, kComputeShader);

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.computeStage.module = module;
    csDesc.computeStage.entryPoint = "main";
    mPipeline = device.CreateComputePipeline(&csDesc);

    // Set up bind group and issue dispatch
    mBindGroup = utils::MakeBindGroup(device, mPipeline.GetBindGroupLayout(0),
                                      {
                                          {0, bufA, 0, byteASize},
                                          {1, bufB, 0, byteBSize},
                                          {2, dst, 0, byteDstSize},
                                          {3, uniformBuffer, 0, sizeof(uniformData)},
                                      });
}

void RobustnessPerf::Step() {
    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(mPipeline);
        pass.SetBindGroup(0, mBindGroup);
        for (unsigned int i = 0; i < kNumIterations; ++i) {
            pass.Dispatch(ceil(mDimBOuter / 2), ceil(mDimAOuter / 2), 1);
        }
        pass.EndPass();

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);
}

TEST_P(RobustnessPerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_PERF_TEST_SUITE_P(RobustnessPerf, {D3D12Backend()}, {2}, {3}, {2});
