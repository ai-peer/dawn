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

class OcclusionQueryTests : public DawnTest {};

TEST_P(OcclusionQueryTests, QuerySetCreation) {
    wgpu::QuerySetDescriptor discriptor;
    discriptor.count = 1;
    discriptor.type = wgpu::QueryType::Occlusion;
    device.CreateQuerySet(&discriptor);
}

DAWN_INSTANTIATE_TEST(OcclusionQueryTests, D3D12Backend());

class PipelineStatisticsQueryTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // Skip all tests if pipeline statistics extension is not supported
        if (!isPipelineStatisticsSupported) {
            GTEST_SKIP();
        }
    }

    std::vector<const char*> GetRequiredExtensions() override {
        std::vector<const char*> requiredExtensions = {};
        isPipelineStatisticsSupported = SupportsExtensions({"pipeline_statistics_query"});
        if (isPipelineStatisticsSupported) {
            requiredExtensions.push_back("pipeline_statistics_query");
        }

        return requiredExtensions;
    }

    bool isPipelineStatisticsSupported = false;
};

TEST_P(PipelineStatisticsQueryTests, QuerySetCreation) {
    wgpu::QuerySetDescriptor discriptor;
    discriptor.count = 1;
    discriptor.type = wgpu::QueryType::PipelineStatistics;
    std::vector<wgpu::PipelineStatisticName> pipelineStatistics = {
        wgpu::PipelineStatisticName::ClipperInvocations,
        wgpu::PipelineStatisticName::VertexShaderInvocations};
    discriptor.pipelineStatistics = pipelineStatistics.data();
    discriptor.pipelineStatisticsCount = 2;

    device.CreateQuerySet(&discriptor);
}

DAWN_INSTANTIATE_TEST(PipelineStatisticsQueryTests, D3D12Backend());

class TimestampQueryTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // Skip all tests if timestamp extension is not supported
        if (!isTimestampSupported) {
            GTEST_SKIP();
        }
    }

    std::vector<const char*> GetRequiredExtensions() override {
        std::vector<const char*> requiredExtensions = {};
        isTimestampSupported = SupportsExtensions({"timestamp_query"});
        if (isTimestampSupported) {
            requiredExtensions.push_back("timestamp_query");
        }
        return requiredExtensions;
    }

    bool isTimestampSupported = false;
};

TEST_P(TimestampQueryTests, QuerySetCreation) {
    wgpu::QuerySetDescriptor discriptor;
    discriptor.count = 1;
    discriptor.type = wgpu::QueryType::Timestamp;

    device.CreateQuerySet(&discriptor);
}

DAWN_INSTANTIATE_TEST(TimestampQueryTests, D3D12Backend());
