// Copyright 2019 The Dawn Authors
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

#include "common/Assert.h"
#include "tests/ParamGenerator.h"
#include "utils/DawnHelpers.h"

namespace {

    constexpr unsigned int kNumDraws = 10000;

    enum class PipelineType {
        Static,  // Keep the same pipeline for all draws.
        Dynamic, // Change the pipeline between draws.
    };

    enum class UniformDataType {
        Static,  // Don't update per-draw uniform data.
        Dynamic, // Update the per-draw uniform data once per frame.
    };

    enum class UniformBindingType {
        Static,  // Use multiple static bind groups.
        Dynamic, // Use bind groups with dynamic offsets.
    };

    enum class WithTexture {
        No,   // Render without a texture
        Yes,  // Render with a texture
    };

    struct DrawCallParams : DawnTestParam {
        DrawCallParams(const DawnTestParam& param,
                       PipelineType pipelineType,
                       UniformDataType uniformDataType,
                       UniformBindingType uniformBindingType,
                       WithTexture withTexture)
            : DawnTestParam(param),
              pipelineType(pipelineType),
              uniformDataType(uniformDataType),
              uniformBindingType(uniformBindingType),
              withTexture(withTexture) {}

        PipelineType pipelineType;
        UniformDataType uniformDataType;
        UniformBindingType uniformBindingType;
        WithTexture withTexture;
    };

    std::ostream& operator<<(std::ostream& ostream, const DrawCallParams& param) {
        ostream << static_cast<const DawnTestParam&>(param);

        switch (param.pipelineType) {
            case PipelineType::Static:
                ostream << "_StaticPipeline";
                break;
            case PipelineType::Dynamic:
                ostream << "_DynamicPipeline";
                break;
        }

        switch (param.uniformDataType) {
            case UniformDataType::Static:
                ostream << "_StaticData";
                break;
            case UniformDataType::Dynamic:
                ostream << "_DynamicData";
                break;
        }

        switch (param.uniformBindingType) {
            case UniformBindingType::Static:
                ostream << "_StaticBindGroups";
                break;
            case UniformBindingType::Dynamic:
                ostream << "_DynamicBindGroup";
                break;
        }

        switch (param.withTexture) {
            case WithTexture::No:
                ostream << "_WithoutTexture";
                break;
            case WithTexture::Yes:
                ostream << "_WithTexture";
                break;
        }

        return ostream;
    }

}  // anonymous namespace

class DrawCallPerf : public DawnPerfTestWithParams<DrawCallParams> {
  public:
    DrawCallPerf() : DawnPerfTestWithParams(kNumDraws, 3) {
    }
    ~DrawCallPerf() override = default;

    void TestSetUp() override;

  private:
    void Step() override;
};

void DrawCallPerf::TestSetUp() {
    DawnPerfTestWithParams::TestSetUp();

    switch (GetParam().withTexture) {
        case WithTexture::No:
            break;
        case WithTexture::Yes:
            break;
        default:
            UNREACHABLE();
            break;
    }

    switch (GetParam().uniformBindingType) {
        case UniformBindingType::Static:
            break;
        case UniformBindingType::Dynamic:
            break;
        default:
            UNREACHABLE();
            break;
    }

    switch (GetParam().pipelineType) {
        case PipelineType::Static:
            break;
        case PipelineType::Dynamic:
            break;
        default:
            UNREACHABLE();
            break;
    }

    switch (GetParam().uniformDataType) {
        case UniformDataType::Static:
            break;
        case UniformDataType::Dynamic:
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void DrawCallPerf::Step() {
    if (GetParam().pipelineType == PipelineType::Static) {

    }

    switch (GetParam().uniformDataType) {
        case UniformDataType::Static:
            break;
        case UniformDataType::Dynamic:
            break;
        default:
            UNREACHABLE();
            break;
    }

    for (unsigned int i = 0; i < kNumDraws; ++i) {
        if (GetParam().pipelineType == PipelineType::Dynamic) {

        }

        switch (GetParam().uniformBindingType) {
            case UniformBindingType::Static:
                break;
            case UniformBindingType::Dynamic:
                break;
            default:
                UNREACHABLE();
                break;
        }
    }
}

TEST_P(DrawCallPerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_PERF_TEST_SUITE_P(DrawCallPerf,
    {D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend},
    {PipelineType::Static, PipelineType::Dynamic},
    {UniformDataType::Static, UniformDataType::Dynamic},
    {UniformBindingType::Static, UniformBindingType::Dynamic},
    {WithTexture::No, WithTexture::Yes});