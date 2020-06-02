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

#include "tests/unittests/validation/ValidationTest.h"

#include "utils/WGPUHelpers.h"

class QueryValidationTest : public ValidationTest {
  protected:
    bool SupportsExtensions(const std::vector<const char*>& extensions) {
        std::set<std::string> supportedExtensionsSet;
        for (const char* supportedExtensionName : adapter.GetSupportedExtensions()) {
            supportedExtensionsSet.insert(supportedExtensionName);
        }

        for (const char* extensionName : extensions) {
            if (supportedExtensionsSet.find(extensionName) == supportedExtensionsSet.end()) {
                return false;
            }
        }

        return true;
    }
};

// Test the validation of query count
TEST_F(QueryValidationTest, QueryCount) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor.type = wgpu::QueryType::Occlusion;
    // It is an error to create a queryset with a count not greater than 0.
    // TODO(hao.x.li): Zero-sized buffer is going to be allowed, if that, we need update the test
    descriptor.count = 0;
    ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
}

// Test the validation of query type
TEST_F(QueryValidationTest, QueryType) {
    wgpu::QuerySetDescriptor descriptor;
    // Undefined query type
    descriptor.count = 1;
    ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
}

// Test the validation of extensions enabled
TEST_F(QueryValidationTest, WithoutExtensionsEnabled) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor.count = 1;

    // Create fail without pipeline statistics query extension enabled
    {
        descriptor.type = wgpu::QueryType::PipelineStatistics;
        ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
    }

    // Create fail without timestamp query extension enabled
    {
        descriptor.type = wgpu::QueryType::Timestamp;
        ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
    }
}

// Test the validation of pipeline statistics query with extension enabled
TEST_F(QueryValidationTest, PipelineStatisticsQuery) {
    bool isPipelineStatisticsQuerySupported = SupportsExtensions({"pipeline_statistics_query"});
    // Skip if the pipeline statistics query is not supported.
    if (isPipelineStatisticsQuerySupported == false) {
        GTEST_SKIP();
    }

    wgpu::Device deviceWithExtension =
        CreateDeviceFromAdapter(adapter, {"pipeline_statistics_query"});

    wgpu::QuerySetDescriptor descriptor;
    descriptor.type = wgpu::QueryType::PipelineStatistics;
    descriptor.count = 1;
    // At least one pipeline statistics is set if query type is PipelineStatistics.
    {
        descriptor.pipelineStatisticsCount = 0;
        ASSERT_DEVICE_ERROR(deviceWithExtension.CreateQuerySet(&descriptor));
    }
    // Invalid pipeline statistics name
    {
        std::array<wgpu::PipelineStatisticsName, static_cast<size_t>(1)> pipelineStatisticsSet;
        descriptor.pipelineStatistics = pipelineStatisticsSet.data();
        descriptor.pipelineStatisticsCount = pipelineStatisticsSet.size();
        ASSERT_DEVICE_ERROR(deviceWithExtension.CreateQuerySet(&descriptor));
    }
}
