// Copyright 2020 The Dawn Authors
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

#include "dawn_native/QueryHelper.h"
#include "utils/WGPUHelpers.h"

namespace {

    struct TsParams {
        uint32_t maxCount;
        float period;
    };

    class InternalShaderExpectation : public detail::Expectation {
      public:
        ~InternalShaderExpectation() override = default;

        InternalShaderExpectation(const uint64_t* values, const unsigned int count) {
            mExpected.assign(values, values + count);
        }

        // Expect the actual results are approximately equal to the expected values.
        testing::AssertionResult Check(const void* data, size_t size) override {
            DAWN_ASSERT(size == sizeof(uint64_t) * mExpected.size());
            const uint64_t* actual = static_cast<const uint64_t*>(data);
            for (size_t i = 0; i < mExpected.size(); ++i) {
                if (abs(static_cast<int64_t>(mExpected[i] - actual[i])) >
                    mExpected[i] * mExpectedErrorRate) {
                    return testing::AssertionFailure()
                           << "Expected data[" << i << "] to be " << mExpected[i] << ", actual "
                           << actual[i] << ". Error rate is larger than " << mExpectedErrorRate
                           << std::endl;
                }
            }

            return testing::AssertionSuccess();
        }

      private:
        std::vector<uint64_t> mExpected;
        float mExpectedErrorRate = 1.0e-5;
    };

}  // anonymous namespace

class QueryInternalShaderTests : public DawnTest {};

// Test the accuracy of timestamp compute shader which uses unsinged 32-bit integers and floats to
// simulate the subtraction and multiplication of unsinged 64-bit integers
TEST_P(QueryInternalShaderTests, TimestampComputeShader) {
    constexpr uint32_t kTimestampCount = 8;
    // A gpu frequency on Intel D3D12 (ticks/second)
    constexpr uint64_t kGPUFrequency = 12000048;
    constexpr uint64_t kNsPerSecond = 1000000000;

    // Original timestamp values for testing
    std::array<uint64_t, kTimestampCount> timestamps;
    timestamps[0] = 0;            // not written at beginning
    timestamps[1] = 10079569507;  // t0
    timestamps[2] = 10394415012;  // t1
    timestamps[3] = 0;            // not written between timestamps
    timestamps[4] = 11713454943;  // t2
    timestamps[5] = 38912556941;  // t3 (t3 - t0 is a big delta)
    timestamps[6] = 10080295766;  // t4 (reset)
    timestamps[7] = 39872473956;  // t5 (after reset)

    // Find the first non-zero value as a first timestamp
    size_t baseIndex = 0;
    uint64_t preTimestamp = 0;
    bool timestampReset = false;
    for (size_t i = 0; i < timestamps.size(); i++) {
        if (timestamps[i] != 0) {
            baseIndex = i;
            preTimestamp = timestamps[i];
            break;
        }
    }

    // Expected results: timestamp value (non zero) - first timestamp value
    std::array<uint64_t, timestamps.size()> expected;
    for (size_t i = 0; i < timestamps.size(); i++) {
        if (timestamps[i] == 0) {
            // Not a timestamp value, keep original value
            expected[i] = timestamps[i];
        } else {
            // If the timestamp is less than the previous timestamp, set all the following to zero.
            if (timestampReset || timestamps[i] < preTimestamp) {
                timestampReset = true;
                expected[i] = 0;
                continue;
            }

            // Maybe the timestamp delta * 10^9 is larger than the maximum of uint64, so cast the
            // delta value to double (higher precision than float)
            expected[i] =
                static_cast<uint64_t>(static_cast<double>(timestamps[i] - timestamps[baseIndex]) *
                                      kNsPerSecond / kGPUFrequency);

            preTimestamp = timestamps[i];
        }
    }

    // Set up compute shader and pipeline
    wgpu::ShaderModule module =
        utils::CreateShaderModuleFromWGSL(device, dawn_native::sTimestampCompute);

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.computeStage.module = module;
    csDesc.computeStage.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    // Set up input storage buffer
    wgpu::Buffer inputBuf =
        utils::CreateBufferFromData(device, timestamps.data(), kTimestampCount * sizeof(uint64_t),
                                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
    EXPECT_BUFFER_U64_RANGE_EQ(timestamps.data(), inputBuf, 0, kTimestampCount);

    // Set up output storage buffer
    wgpu::BufferDescriptor outputDesc;
    outputDesc.size = kTimestampCount * sizeof(uint64_t);
    outputDesc.usage =
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer outputBuf = device.CreateBuffer(&outputDesc);

    // Set up params uniform buffer
    TsParams params = {kTimestampCount, static_cast<float>(kNsPerSecond) /
                                            kGPUFrequency /* timestamp period in ns */};
    wgpu::Buffer paramsBuf =
        utils::CreateBufferFromData(device, &params, sizeof(params), wgpu::BufferUsage::Uniform);

    // Set up bind group and issue dispatch
    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                             {
                                 {0, inputBuf, 0, kTimestampCount * sizeof(uint64_t)},
                                 {1, outputBuf, 0, kTimestampCount * sizeof(uint64_t)},
                                 {2, paramsBuf, 0, sizeof(params)},
                             });

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.Dispatch(kTimestampCount);
        pass.EndPass();

        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);

    EXPECT_BUFFER(outputBuf, 0, kTimestampCount * sizeof(uint64_t),
                  new InternalShaderExpectation(expected.data(), kTimestampCount));
}

DAWN_INSTANTIATE_TEST(QueryInternalShaderTests, D3D12Backend(), MetalBackend(), VulkanBackend());
