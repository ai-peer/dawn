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

#include "utils/DawnHelpers.h"

namespace {

    constexpr unsigned int kNumIterations = 50;
    constexpr uint32_t kBufferSize = 1024 * 1024;

    enum class UploadMethod {
        SetSubData,
        CreateBufferMapped,
    };

    using BufferUploadParams = std::tuple<DawnTestParam, UploadMethod>;

    const auto TestParams = ::testing::Combine(
        FILTER_BACKENDS(D3D12Backend, MetalBackend, OpenGLBackend, VulkanBackend),
        ::testing::Values(UploadMethod::SetSubData, UploadMethod::CreateBufferMapped));

    std::string PrintParams(const ::testing::TestParamInfo<BufferUploadParams> info) {
        std::ostringstream ostream;
        ostream << detail::GetDawnTestParamName(info);

        switch (std::get<UploadMethod>(info.param)) {
            case UploadMethod::SetSubData:
                ostream << "_SetSubData";
                break;
            case UploadMethod::CreateBufferMapped:
                ostream << "_CreateBufferMapped";
                break;
        }

        return ostream.str();
    }
}  // namespace

// Test uploading |kBufferSize| bytes of data |kNumIterations| times.
class BufferUploadPerf : public DawnPerfTestWithParams<BufferUploadParams> {
  public:
    BufferUploadPerf() : DawnPerfTestWithParams(kNumIterations), data(kBufferSize) {
        dawn::BufferDescriptor desc = {};
        desc.size = kBufferSize;
        desc.usage = dawn::BufferUsageBit::CopyDst;

        dst = device.CreateBuffer(&desc);
    }
    ~BufferUploadPerf() override = default;

  private:
    void Step() override;

    dawn::Buffer dst;
    std::vector<uint8_t> data;
};

void BufferUploadPerf::Step() {
    switch (std::get<UploadMethod>(GetParam())) {
        case UploadMethod::SetSubData: {
            for (unsigned int i = 0; i < kNumIterations; ++i) {
                dst.SetSubData(0, kBufferSize, data.data());
            }
            queue.Submit(0, nullptr);
        } break;

        case UploadMethod::CreateBufferMapped: {
            dawn::BufferDescriptor desc = {};
            desc.size = kBufferSize;
            desc.usage = dawn::BufferUsageBit::CopySrc | dawn::BufferUsageBit::MapWrite;

            dawn::CommandEncoder encoder = device.CreateCommandEncoder();

            for (unsigned int i = 0; i < kNumIterations; ++i) {
                auto result = device.CreateBufferMapped(&desc);
                memcpy(result.data, data.data(), kBufferSize);
                result.buffer.Unmap();
                encoder.CopyBufferToBuffer(result.buffer, 0, dst, 0, kBufferSize);
            }

            dawn::CommandBuffer commands = encoder.Finish();
            queue.Submit(1, &commands);
        } break;
    }

    WaitForGPU();
}

TEST_P(BufferUploadPerf, Run) {
    RunTest();
}

INSTANTIATE_TEST_SUITE_P(, BufferUploadPerf, TestParams, PrintParams);
