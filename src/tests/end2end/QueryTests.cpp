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

class QueryTests : public DawnTest {
  protected:
    wgpu::Buffer CreateBuffer(uint64_t size) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = size;
        descriptor.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
        return device.CreateBuffer(&descriptor);
    }

    void MapAsyncAndWait(const wgpu::Buffer& buffer,
                         wgpu::MapMode mode,
                         size_t offset,
                         size_t size) {
        bool done = false;
        buffer.MapAsync(
            mode, offset, size,
            [](WGPUBufferMapAsyncStatus status, void* userdata) {
                ASSERT_EQ(WGPUBufferMapAsyncStatus_Success, status);
                *static_cast<bool*>(userdata) = true;
            },
            &done);

        while (!done) {
            WaitABit();
        }
    }

    wgpu::Buffer GetMappedBuffer(const wgpu::Buffer& buffer, size_t offset, size_t size) {
        wgpu::BufferDescriptor descriptor;
        descriptor.size = size;
        descriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer mappedBuffer = device.CreateBuffer(&descriptor);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(buffer, offset, mappedBuffer, 0, size);
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        MapAsyncAndWait(mappedBuffer, wgpu::MapMode::Read, 0, size);

        return mappedBuffer;
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

DAWN_INSTANTIATE_TEST(OcclusionQueryTests, D3D12Backend());

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

DAWN_INSTANTIATE_TEST(PipelineStatisticsQueryTests, D3D12Backend());

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

    wgpu::QuerySet CreateQuerySetForTimestamp(uint32_t queryCount) {
        wgpu::QuerySetDescriptor descriptor;
        descriptor.count = queryCount;
        descriptor.type = wgpu::QueryType::Timestamp;
        return device.CreateQuerySet(&descriptor);
    }

    // Check the results of timestamp query are greater than 0.
    void CheckTimestampResult(const wgpu::Buffer& buffer, uint32_t count) {
        wgpu::Buffer mappedBuffer = GetMappedBuffer(buffer, 0, count * sizeof(uint64_t));
        for (uint32_t i = 0; i < count; i++) {
            uint64_t result = *static_cast<const uint64_t*>(
                mappedBuffer.GetConstMappedRange(i * sizeof(uint64_t), sizeof(uint64_t)));
            ASSERT_GT(result, (uint64_t)0);
        }
    }
};

// Test creating query set with the type of Timestamp
TEST_P(TimestampQueryTests, QuerySetCreation) {
    CreateQuerySetForTimestamp(1);
}

// Test calling timestamp query from command encoder
TEST_P(TimestampQueryTests, TimestampOnCommandEncoder) {
    uint32_t kQueryCount = 2;
    wgpu::QuerySet querySet = CreateQuerySetForTimestamp(kQueryCount);
    wgpu::Buffer destination = CreateBuffer(kQueryCount * sizeof(uint64_t));

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.WriteTimestamp(querySet, 0);
    encoder.WriteTimestamp(querySet, 1);
    encoder.ResolveQuerySet(querySet, 0, kQueryCount, destination, 0);
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    CheckTimestampResult(destination, kQueryCount);
}

// Test calling timestamp query from render pass encoder
TEST_P(TimestampQueryTests, TimestampOnRenderPass) {
    uint32_t kQueryCount = 2;
    wgpu::QuerySet querySet = CreateQuerySetForTimestamp(kQueryCount);
    wgpu::Buffer destination = CreateBuffer(kQueryCount * sizeof(uint64_t));

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, 1, 1);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    pass.WriteTimestamp(querySet, 0);
    pass.WriteTimestamp(querySet, 1);
    pass.EndPass();
    encoder.ResolveQuerySet(querySet, 0, kQueryCount, destination, 0);
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    CheckTimestampResult(destination, kQueryCount);
}

// Test calling timestamp query from compute pass encoder
TEST_P(TimestampQueryTests, TimestampOnComputePass) {
    uint32_t kQueryCount = 2;
    wgpu::QuerySet querySet = CreateQuerySetForTimestamp(kQueryCount);
    wgpu::Buffer destination = CreateBuffer(kQueryCount * sizeof(uint64_t));

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.WriteTimestamp(querySet, 0);
    pass.WriteTimestamp(querySet, 1);
    pass.EndPass();
    encoder.ResolveQuerySet(querySet, 0, kQueryCount, destination, 0);
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    CheckTimestampResult(destination, kQueryCount);
}

DAWN_INSTANTIATE_TEST(TimestampQueryTests, D3D12Backend());
