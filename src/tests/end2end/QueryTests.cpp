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

class QueryTests : public DawnTest {
  protected:
    std::vector<const char*> GetRequiredExtensions() override {
        std::vector<const char*> requiredExtensions = {};
        isPipelineStatisticsQuerySupported = SupportsExtensions({"pipeline_statistics_query"});
        isTimestampQuerySupported = SupportsExtensions({"timestamp_query"});
        if (isPipelineStatisticsQuerySupported) {
            requiredExtensions.push_back("pipeline_statistics_query");
        }
        if (isTimestampQuerySupported) {
            requiredExtensions.push_back("timestamp_query");
        }

        return requiredExtensions;
    }

    bool isPipelineStatisticsQuerySupported = false;
    bool isTimestampQuerySupported = false;
};

// Test QuerySet with Occlusion Query
TEST_P(QueryTests, CreateQuerySetForOcclusionQuery) {
    wgpu::QuerySetDescriptor discriptor;
    discriptor.count = 1;
    discriptor.type = wgpu::QueryType::Occlusion;

    device.CreateQuerySet(&discriptor);
}

// Test QuerySet with Pipeline Statistics Query
TEST_P(QueryTests, CreateQuerySetForPipelineStatisticsQuery) {
    DAWN_SKIP_TEST_IF(isPipelineStatisticsQuerySupported == false);

    wgpu::QuerySetDescriptor discriptor;
    discriptor.count = 1;
    discriptor.type = wgpu::QueryType::PipelineStatistics;

    device.CreateQuerySet(&discriptor);
}

// Test QuerySet with Timestamp Query
TEST_P(QueryTests, CreateQuerySetForTimestampQuery) {
    DAWN_SKIP_TEST_IF(isTimestampQuerySupported == false);

    wgpu::QuerySetDescriptor discriptor;
    discriptor.count = 1;
    discriptor.type = wgpu::QueryType::Timestamp;

    device.CreateQuerySet(&discriptor);
}

DAWN_INSTANTIATE_TEST(QueryTests, D3D12Backend(), MetalBackend(), VulkanBackend());
