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

// This file contains test for deprecated parts of Dawn's API while following WebGPU's evolution.
// It contains test for the "old" behavior that will be deleted once users are migrated, tests that
// a deprecation warning is emitted when the "old" behavior is used, and tests that an error is
// emitted when both the old and the new behavior are used (when applicable).

#include "tests/DawnTest.h"

#include "utils/WGPUHelpers.h"

namespace {

    struct TsParams {
        uint32_t maxCount;
        float period;
    };

}  // anonymous namespace

class QueryTests : public DawnTest {
  protected:
    wgpu::Buffer CreateResolveBuffer(uint64_t size) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = size;
        descriptor.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc |
                           wgpu::BufferUsage::CopyDst;
        return device.CreateBuffer(&descriptor);
    }
};

class OcclusionQueryTests : public QueryTests {};

// Test creating query set with the type of Occlusion
TEST_P(OcclusionQueryTests, QuerySetCreation) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor.count = 1;
    descriptor.type = wgpu::QueryType::Occlusion;
    device.CreateQuerySet(&descriptor);
}

// Test destroying query set
TEST_P(OcclusionQueryTests, QuerySetDestroy) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor.count = 1;
    descriptor.type = wgpu::QueryType::Occlusion;
    wgpu::QuerySet querySet = device.CreateQuerySet(&descriptor);
    querySet.Destroy();
}

DAWN_INSTANTIATE_TEST(OcclusionQueryTests, D3D12Backend(), MetalBackend(), VulkanBackend());

class PipelineStatisticsQueryTests : public QueryTests {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // Skip all tests if pipeline statistics extension is not supported
        DAWN_SKIP_TEST_IF(!SupportsExtensions({"pipeline_statistics_query"}));
    }

    std::vector<const char*> GetRequiredExtensions() override {
        std::vector<const char*> requiredExtensions = {};
        if (SupportsExtensions({"pipeline_statistics_query"})) {
            requiredExtensions.push_back("pipeline_statistics_query");
        }

        return requiredExtensions;
    }
};

// Test creating query set with the type of PipelineStatistics
TEST_P(PipelineStatisticsQueryTests, QuerySetCreation) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor.count = 1;
    descriptor.type = wgpu::QueryType::PipelineStatistics;
    wgpu::PipelineStatisticName pipelineStatistics[2] = {
        wgpu::PipelineStatisticName::ClipperInvocations,
        wgpu::PipelineStatisticName::VertexShaderInvocations};
    descriptor.pipelineStatistics = pipelineStatistics;
    descriptor.pipelineStatisticsCount = 2;
    device.CreateQuerySet(&descriptor);
}

DAWN_INSTANTIATE_TEST(PipelineStatisticsQueryTests,
                      D3D12Backend(),
                      MetalBackend(),
                      VulkanBackend());

class TimestampExpectation : public detail::Expectation {
  public:
    ~TimestampExpectation() override = default;

    // Expect the timestamp results are greater than 0.
    testing::AssertionResult Check(const void* data, size_t size) override {
        ASSERT(size % sizeof(uint64_t) == 0);
        const uint64_t* timestamps = static_cast<const uint64_t*>(data);
        for (size_t i = 0; i < size / sizeof(uint64_t); i++) {
            if (timestamps[i] == 0) {
                return testing::AssertionFailure()
                       << "Expected data[" << i << "] to be greater than 0." << std::endl;
            }
        }

        return testing::AssertionSuccess();
    }
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

class TimestampQueryTests : public QueryTests {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // Skip all tests if timestamp extension is not supported
        DAWN_SKIP_TEST_IF(!SupportsExtensions({"timestamp_query"}));
    }

    std::vector<const char*> GetRequiredExtensions() override {
        std::vector<const char*> requiredExtensions = {};
        if (SupportsExtensions({"timestamp_query"})) {
            requiredExtensions.push_back("timestamp_query");
        }
        return requiredExtensions;
    }

    wgpu::QuerySet CreateQuerySetForTimestamp(uint32_t queryCount);

    void InternalComputeShaderTest(const char* shader);
};

wgpu::QuerySet TimestampQueryTests::CreateQuerySetForTimestamp(uint32_t queryCount) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor.count = queryCount;
    descriptor.type = wgpu::QueryType::Timestamp;
    return device.CreateQuerySet(&descriptor);
}

void TimestampQueryTests::InternalComputeShaderTest(const char* shader) {
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
    wgpu::ShaderModule module = utils::CreateShaderModuleFromWGSL(device, shader);

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

// Test creating query set with the type of Timestamp
TEST_P(TimestampQueryTests, QuerySetCreation) {
    CreateQuerySetForTimestamp(1);
}

// Test calling timestamp query from command encoder
TEST_P(TimestampQueryTests, TimestampOnCommandEncoder) {
    // TODO(hao.x.li@intel.com): Crash occurs if we only call WriteTimestamp in a command encoder
    // without any copy commands on Metal on AMD GPU. See https://crbug.com/dawn/545.
    DAWN_SKIP_TEST_IF(IsMetal() && IsAMD());

    constexpr uint32_t kQueryCount = 2;

    wgpu::QuerySet querySet = CreateQuerySetForTimestamp(kQueryCount);
    wgpu::Buffer destination = CreateResolveBuffer(kQueryCount * sizeof(uint64_t));

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.WriteTimestamp(querySet, 0);
    encoder.WriteTimestamp(querySet, 1);
    encoder.ResolveQuerySet(querySet, 0, kQueryCount, destination, 0);
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER(destination, 0, kQueryCount * sizeof(uint64_t), new TimestampExpectation);
}

// Test calling timestamp query from render pass encoder
TEST_P(TimestampQueryTests, TimestampOnRenderPass) {
    constexpr uint32_t kQueryCount = 2;

    wgpu::QuerySet querySet = CreateQuerySetForTimestamp(kQueryCount);
    wgpu::Buffer destination = CreateResolveBuffer(kQueryCount * sizeof(uint64_t));

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 1, 1);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    pass.WriteTimestamp(querySet, 0);
    pass.WriteTimestamp(querySet, 1);
    pass.EndPass();
    encoder.ResolveQuerySet(querySet, 0, kQueryCount, destination, 0);
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER(destination, 0, kQueryCount * sizeof(uint64_t), new TimestampExpectation);
}

// Test calling timestamp query from compute pass encoder
TEST_P(TimestampQueryTests, TimestampOnComputePass) {
    constexpr uint32_t kQueryCount = 2;

    wgpu::QuerySet querySet = CreateQuerySetForTimestamp(kQueryCount);
    wgpu::Buffer destination = CreateResolveBuffer(kQueryCount * sizeof(uint64_t));

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.WriteTimestamp(querySet, 0);
    pass.WriteTimestamp(querySet, 1);
    pass.EndPass();
    encoder.ResolveQuerySet(querySet, 0, kQueryCount, destination, 0);
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER(destination, 0, kQueryCount * sizeof(uint64_t), new TimestampExpectation);
}

// Test resolving timestamp query to one slot in the buffer
TEST_P(TimestampQueryTests, ResolveToBufferWithOffset) {
    // TODO(hao.x.li@intel.com): Fail to resolve query to buffer with offset on Windows Vulkan and
    // Metal on Intel platforms, need investigation.
    DAWN_SKIP_TEST_IF(IsWindows() && IsIntel() && IsVulkan());
    DAWN_SKIP_TEST_IF(IsIntel() && IsMetal());

    // TODO(hao.x.li@intel.com): Crash occurs if we only call WriteTimestamp in a command encoder
    // without any copy commands on Metal on AMD GPU. See https://crbug.com/dawn/545.
    DAWN_SKIP_TEST_IF(IsMetal() && IsAMD());

    constexpr uint32_t kQueryCount = 2;
    constexpr uint64_t kZero = 0;

    wgpu::QuerySet querySet = CreateQuerySetForTimestamp(kQueryCount);

    // Resolve the query result to first slot in the buffer, other slots should not be written
    {
        wgpu::Buffer destination = CreateResolveBuffer(kQueryCount * sizeof(uint64_t));
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.WriteTimestamp(querySet, 0);
        encoder.WriteTimestamp(querySet, 1);
        encoder.ResolveQuerySet(querySet, 0, 1, destination, 0);
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        EXPECT_BUFFER(destination, 0, sizeof(uint64_t), new TimestampExpectation);
        EXPECT_BUFFER_U64_RANGE_EQ(&kZero, destination, sizeof(uint64_t), 1);
    }

    // Resolve the query result to the buffer with offset, the slots before the offset
    // should not be written
    {
        wgpu::Buffer destination = CreateResolveBuffer(kQueryCount * sizeof(uint64_t));
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.WriteTimestamp(querySet, 0);
        encoder.WriteTimestamp(querySet, 1);
        encoder.ResolveQuerySet(querySet, 0, 1, destination, sizeof(uint64_t));
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        EXPECT_BUFFER_U64_RANGE_EQ(&kZero, destination, 0, 1);
        EXPECT_BUFFER(destination, sizeof(uint64_t), sizeof(uint64_t), new TimestampExpectation);
    }
}

// Test the accuracy of timestamp compute shader which uses unsinged 32-bit integers and floats to
// simulate the subtraction and multiplication of unsinged 64-bit integers
TEST_P(TimestampQueryTests, TimestampComputeShader) {
    InternalComputeShaderTest(R"(
        [[block]] struct Timestamps {
            # Low 32-bits : t[0].x
            # High 32-bits: t[0].y
            [[offset(0)]] t : [[stride(8)]] array<vec2<u32>>;
        };

        [[block]] struct Params {
            [[offset(0)]] maxCount : u32;
            [[offset(4)]] period : f32;
        };

        [[set(0), binding(0)]] var<storage_buffer> input : Timestamps;
        [[set(0), binding(1)]] var<storage_buffer> output : Timestamps;
        [[set(0), binding(2)]] var<uniform> params : Params;

        [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

        [[stage(compute), workgroup_size(1, 1, 1)]]
        fn main() -> void {
            var index : u32 = GlobalInvocationID.x;

            if (index >= params.maxCount) { return; }
            if (input.t[index].x == 0u && input.t[index].y == 0u) { return; }

            # Find the first non-zero value as a timestamp baseline
            var baseIndex : u32 = 0u;
            for (var i : u32 = 0u; i <= index; i = i + 1) {
                if (input.t[i].x != 0 || input.t[i].y != 0) {
                    baseIndex = i;
                    break;
                }
            }

            # If the timestamp is less than the last timestamp, it means this is a
            # timestamp which has been reset, set all the following to zero.
            var prev : vec2<u32> = vec2<u32>(0u, 0u);
            for (var i : u32 = 1u; i <= index; i = i + 1) {
                # Record the previos timestamp. For example,
                # if the src data is [0, t0, 0, t1], the previos timestamp should be t0.
                if (prev.x == 0 && prev.y == 0 && input.t[i].x != 0 && input.t[i].y != 0 ) {
                    prev.x = input.t[i].x;
                    prev.y = input.t[i].y;
                }

                # Skip the zero value checking which is not a timestamp.
                if (input.t[i].x == 0 && input.t[i].y == 0) { continue; }

                if (input.t[i].y < prev.y) {
                    return;
                }
                if (input.t[i].y == prev.y && input.t[i].x < prev.x) {
                    return;
                }

                # Record the current timestamp as the previos.
                prev.x = input.t[i].x;
                prev.y = input.t[i].y;
            }

            # Subtract each value from the baseline
            output.t[index].y = input.t[index].y - input.t[baseIndex].y;
            if (input.t[index].x < input.t[baseIndex].x) {
               output.t[index].y = output.t[index].y - 1u;
            }
            output.t[index].x = input.t[index].x - input.t[baseIndex].x;

            # Multiply output values with params.period
            var w: u32 = 0u;
            # Check if the product of low 32-bits and the period exceeds the maximum of u32.
            # Otherwise, just do the multiplication
            if (output.t[index].x > u32(f32(0xFFFFFFFFu) / params.period)) {
                # Use two u32 to represent the high 16-bits and low 16-bits of this u32
                var lo : u32 = output.t[index].x & 0xFFFF;
                var hi : u32 = output.t[index].x >> 16;

                var t0 : u32 = u32(round(f32(lo) * params.period));
                var t1 : u32 = u32(round(f32(hi) * params.period)) + (t0 >> 16);
                w = t1 >> 16;

                var result_lo : u32 = t1 << 16;
                result_lo = result_lo | (t0 & 0xFFFF);
                output.t[index].x = result_lo;
            } else {
                output.t[index].x = u32(round(f32(output.t[index].x) * params.period));
            }

            # Get the nearest integer to the float result. For high 32bits, the round function will
            # greatly help reduce the accuracy loss of the final result.
            output.t[index].y = u32(round(f32(output.t[index].y) * params.period)) + w;

            return;
        })");
}

DAWN_INSTANTIATE_TEST(TimestampQueryTests, D3D12Backend(), MetalBackend(), VulkanBackend());
