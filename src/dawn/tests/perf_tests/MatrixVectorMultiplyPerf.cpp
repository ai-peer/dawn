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

#include <sstream>
#include <string>
#include <vector>

#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

constexpr unsigned int kNumIterations = 10;

enum class DataType {
    F32,
    F16,
    U8,
};

std::ostream& operator<<(std::ostream& ostream, const DataType& v) {
    switch (v) {
        case DataType::F32:
            ostream << "F32";
            break;
        case DataType::F16:
            ostream << "F16";
            break;
        case DataType::U8:
            ostream << "U8";
            break;
    }
    return ostream;
}

using Rows = uint32_t;
using Cols = uint32_t;
using Subgroups = bool;
using StoreType = DataType;
using AccType = DataType;
DAWN_TEST_PARAM_STRUCT(MatrixVectorMultiplyParams, Rows, Cols, StoreType, AccType, Subgroups);

class MatrixVectorMultiplyPerf : public DawnPerfTestWithParams<MatrixVectorMultiplyParams> {
  public:
    MatrixVectorMultiplyPerf() : DawnPerfTestWithParams(kNumIterations, 1) {}
    ~MatrixVectorMultiplyPerf() override = default;

    void SetUp() override;

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        auto requirements =
            DawnPerfTestWithParams<MatrixVectorMultiplyParams>::GetRequiredFeatures();
        if ((GetParam().mStoreType == StoreType::F16 || GetParam().mAccType == AccType::F16) &&
            SupportsFeatures({wgpu::FeatureName::ShaderF16})) {
            requirements.push_back(wgpu::FeatureName::ShaderF16);
        }
        if (GetParam().mSubgroups &&
            SupportsFeatures({wgpu::FeatureName::ChromiumExperimentalSubgroups})) {
            requirements.push_back(wgpu::FeatureName::ChromiumExperimentalSubgroups);
        }
        return requirements;
    }

    wgpu::RequiredLimits GetRequiredLimits(const wgpu::SupportedLimits& supported) override {
        wgpu::RequiredLimits required = {};
        required.limits.maxStorageBufferBindingSize =
            BytesPerElement() * GetParam().mRows * GetParam().mCols;
        return required;
    }

  private:
    void Step() override;

    uint32_t BytesPerElement() const {
        switch (GetParam().mStoreType) {
            case StoreType::F32:
                return 4;
            case StoreType::F16:
                return 2;
            case StoreType::U8:
                return 1;
        }
    }

    std::string GenerateShader() const;

    wgpu::BindGroup mBindGroup;
    wgpu::ComputePipeline mPipeline;
};

void MatrixVectorMultiplyPerf::SetUp() {
    DawnPerfTestWithParams<MatrixVectorMultiplyParams>::SetUp();

    if (GetParam().mStoreType != StoreType::U8) {
        // Don't care about testing this case.
        DAWN_TEST_UNSUPPORTED_IF(GetParam().mStoreType != GetParam().mAccType);
    }

    DAWN_TEST_UNSUPPORTED_IF(
        (GetParam().mStoreType == StoreType::F16 || GetParam().mAccType == AccType::F16) &&
        !SupportsFeatures({wgpu::FeatureName::ShaderF16}));

    DAWN_TEST_UNSUPPORTED_IF(GetParam().mSubgroups &&
                             !SupportsFeatures({wgpu::FeatureName::ChromiumExperimentalSubgroups}));

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.usage = wgpu::BufferUsage::Storage;
    bufferDesc.size = BytesPerElement() * GetParam().mRows * GetParam().mCols;
    wgpu::Buffer matrix = device.CreateBuffer(&bufferDesc);

    bufferDesc.size = BytesPerElement() * GetParam().mCols;
    wgpu::Buffer vector = device.CreateBuffer(&bufferDesc);

    bufferDesc.size = BytesPerElement() * GetParam().mRows;
    wgpu::Buffer result = device.CreateBuffer(&bufferDesc);

    uint32_t uniformData[] = {GetParam().mRows, GetParam().mCols};
    wgpu::Buffer uniformBuffer = utils::CreateBufferFromData(
        device, uniformData, sizeof(uniformData), wgpu::BufferUsage::Uniform);

    std::string code = GenerateShader();
    wgpu::ShaderModule module = utils::CreateShaderModule(device, code.c_str());

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = module;
    mPipeline = device.CreateComputePipeline(&csDesc);

    mBindGroup = utils::MakeBindGroup(device, mPipeline.GetBindGroupLayout(0),
                                      {
                                          {0, matrix},
                                          {1, vector},
                                          {2, result},
                                          {3, uniformBuffer},
                                      });
}

std::string MatrixVectorMultiplyPerf::GenerateShader() const {
    std::stringstream code;
    if (GetParam().mStoreType == StoreType::F16 || GetParam().mAccType == AccType::F16) {
        code << "enable f16;\n";
    }
    if (GetParam().mSubgroups) {
        code << "enable chromium_experimental_subgroups;\n";
    }
    switch (GetParam().mStoreType) {
        case StoreType::F32:
            code << "alias StoreType = vec4<f32>;\n";
            break;
        case StoreType::F16:
            code << "alias StoreType = vec4<f16>;\n";
            break;
        case StoreType::U8:
            code << "alias StoreType = u32;\n";
            break;
    }
    switch (GetParam().mAccType) {
        case AccType::F32:
            code << "alias AccType = f32;\n";
            break;
        case AccType::F16:
            code << "alias AccType = f16;\n";
            break;
        case AccType::U8:
            code << "alias AccType = u32;\n";
            break;
    }
    code << R"(struct Uniforms {
        rows : u32,
        cols : u32,
    }
    struct Matrix {
        values: array<StoreType>
    }
    struct Vector {
        values: array<StoreType>
    }

    @group(0) @binding(0) var<storage, read> matrix : Matrix;
    @group(0) @binding(1) var<storage, read> vector : Vector;
    @group(0) @binding(2) var<storage, read_write> result : Vector;
    @group(0) @binding(3) var<uniform> uniforms : Uniforms;
    )";

    std::function<std::string(std::string)> valueLoad;
    std::function<std::string(std::string)> loopBody;
    std::string writeResult;

    if (GetParam().mStoreType == StoreType::U8 && GetParam().mAccType == AccType::U8) {
        // Data is already 8-bit. Compute 8-bit dot products.
        valueLoad = [](std::string i) { return "vector.values[" + i + "]"; };
        // clang-format off
        loopBody = [](std::string offset) {
            return "sum += vec4<AccType>(\n"
                "dot4U8Packed(matrix.values[(4u * global_id.x + 0u) * uniforms.cols + col" + offset + "], v),\n"
                "dot4U8Packed(matrix.values[(4u * global_id.x + 1u) * uniforms.cols + col" + offset + "], v),\n"
                "dot4U8Packed(matrix.values[(4u * global_id.x + 2u) * uniforms.cols + col" + offset + "], v),\n"
                "dot4U8Packed(matrix.values[(4u * global_id.x + 3u) * uniforms.cols + col" + offset + "], v),\n"
            ");";
        };
        // clang-format on
        writeResult = "result.values[global_id.x] = pack4xU8(sum);\n";
    } else if (GetParam().mStoreType == StoreType::U8) {
        // Data is 8-bit. Expand out to float, compute dot product, and then pack again.
        valueLoad = [](std::string i) {
            return "vec4<AccType>(unpack4xU8(vector.values[" + i + "]))";
        };
        // clang-format off
        loopBody = [](std::string offset) {
            return "sum += vec4<AccType>(\n"
                "dot(vec4<AccType>(unpack4xU8(matrix.values[(4u * global_id.x + 0u) * uniforms.cols + col" + offset + "])), v),\n"
                "dot(vec4<AccType>(unpack4xU8(matrix.values[(4u * global_id.x + 1u) * uniforms.cols + col" + offset + "])), v),\n"
                "dot(vec4<AccType>(unpack4xU8(matrix.values[(4u * global_id.x + 2u) * uniforms.cols + col" + offset + "])), v),\n"
                "dot(vec4<AccType>(unpack4xU8(matrix.values[(4u * global_id.x + 3u) * uniforms.cols + col" + offset + "])), v),\n"
            ");";
        };
        // clang-format on
        writeResult = "result.values[global_id.x] = pack4x8unorm(vec4<f32>(sum));\n";
    } else {
        // Data is in float. Compute dot product in float.
        valueLoad = [](std::string i) { return "vector.values[" + i + "]"; };
        // clang-format off
        loopBody = [](std::string offset) {
            return "sum += vec4<AccType>(\n"
                "dot(matrix.values[(4u * global_id.x + 0u) * uniforms.cols + col" + offset + "], v),\n"
                "dot(matrix.values[(4u * global_id.x + 1u) * uniforms.cols + col" + offset + "], v),\n"
                "dot(matrix.values[(4u * global_id.x + 2u) * uniforms.cols + col" + offset + "], v),\n"
                "dot(matrix.values[(4u * global_id.x + 3u) * uniforms.cols + col" + offset + "], v),\n"
            ");";
        };
        // clang-format on
        writeResult = "result.values[global_id.x] = sum;\n";
    }

    if (GetParam().mSubgroups) {
        // Helper function to generate a subgroup case since:
        // - we don't know the subgroup size until runtime
        // - subgroupBroadcast requires a constant lane.
        auto GenerateSubgroupCase = [&valueLoad, &loopBody](uint32_t size) {
            std::stringstream c;
            c << "  if (sg_size == " << size << "u){\n";
            c << "    for (var col = 0u; col < uniforms.cols / 4u; col = col + " << size << "u) {"
              << "\n";
            c << "      let shared_v = " << valueLoad("col + sg_id") << ";\n";
            if (GetParam().mAccType == AccType::U8) {
                c << "      var v : AccType;\n";
            } else {
                c << "      var v : vec4<AccType>;\n";
            }
            for (uint32_t i = 0; i < size; ++i) {
                c << "      v = subgroupBroadcast(shared_v, " << i << "u);\n";
                c << "        " << loopBody(" + " + std::to_string(i) + "u") << "\n";
            }
            c << "    }\n";
            c << "  }";
            return c.str();
        };

        code << "@compute @workgroup_size(64) fn main("
                "@builtin(global_invocation_id) global_id  : vec3u, "
                "@builtin(subgroup_size) sg_size : u32, "
                "@builtin(subgroup_invocation_id) sg_id : u32"
                ") {\n";
        code << "  var sum : vec4<AccType>"
             << ";\n";
        code << GenerateSubgroupCase(4) << " else " << GenerateSubgroupCase(8) << " else "
             << GenerateSubgroupCase(16) << " else " << GenerateSubgroupCase(32) << " else "
             << GenerateSubgroupCase(64);
        code << "  " << writeResult;
        code << "}";
    } else {
        code << "@compute @workgroup_size(64) fn main(@builtin(global_invocation_id) global_id  : "
                "vec3u) {\n";
        code << "  var sum : vec4<AccType>"
             << ";\n";
        code << "  for (var col = 0u; col < uniforms.cols / 4u; col = col + 1u) {"
             << ";\n";
        code << "    let v = " << valueLoad("col") << ";\n";
        code << "    " << loopBody("") << "\n";
        code << "  }\n";
        code << "  " << writeResult;
        code << "}";
    }
    return code.str();
}

void MatrixVectorMultiplyPerf::Step() {
    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        if (SupportsTimestampQuery()) {
            RecordBeginTimestamp(encoder);
        }
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(mPipeline);
        pass.SetBindGroup(0, mBindGroup);
        for (unsigned int i = 0; i < kNumIterations; ++i) {
            pass.DispatchWorkgroups(GetParam().mRows / (64u * 4u));
        }
        pass.End();
        if (SupportsTimestampQuery()) {
            RecordEndTimestampAndResolveQuerySet(encoder);
        }

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);

    if (SupportsTimestampQuery()) {
        ComputeGPUElapsedTime();
    }
}

TEST_P(MatrixVectorMultiplyPerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_TEST_P(MatrixVectorMultiplyPerf,
                        {D3D12Backend({"disable_robustness"}, {}),
                         MetalBackend({"disable_robustness"}, {}),
                         OpenGLBackend({"disable_robustness"}, {}),
                         VulkanBackend({"disable_robustness"}, {})},
                        {32768u}, /* rows */
                        {2048u},  /* cols */
                        {StoreType::F32, StoreType::F16, StoreType::U8},
                        {AccType::F32, AccType::F16, AccType::U8},
                        {false, true} /* subgroups*/);

}  // anonymous namespace
}  // namespace dawn
