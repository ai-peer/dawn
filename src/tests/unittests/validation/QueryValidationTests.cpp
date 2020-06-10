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
    void SetUp() override {
        ValidationTest::SetUp();

        // Initialize the device with required extensions
        std::vector<const char*> extensions = {};
        isPipelineStatisticsQuerySupported = SupportsExtensions({"pipeline_statistics_query"});
        isTimestampQuerySupported = SupportsExtensions({"timestamp_query"});

        if (isPipelineStatisticsQuerySupported) {
            extensions.push_back("pipeline_statistics_query");
        }

        if (isTimestampQuerySupported) {
            extensions.push_back("timestamp_query");
        }

        deviceWithExtensions = CreateDeviceFromAdapter(adapter, extensions);
    }

    wgpu::QuerySetDescriptor CreateQuerySetDescriptor(
        wgpu::QueryType queryType,
        uint32_t queryCount,
        const std::vector<wgpu::PipelineStatisticsName>* pipelineStatistics = nullptr) {
        wgpu::QuerySetDescriptor descriptor;
        descriptor.type = queryType;
        descriptor.count = queryCount;

        if (pipelineStatistics != nullptr) {
            descriptor.pipelineStatistics = pipelineStatistics->data();
            descriptor.pipelineStatisticsCount = pipelineStatistics->size();
        }

        return descriptor;
    }

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

    bool isPipelineStatisticsQuerySupported;
    bool isTimestampQuerySupported;

    wgpu::Device deviceWithExtensions;
};

// Test creating query set with/without extensions
TEST_F(QueryValidationTest, Creation) {
    wgpu::QuerySetDescriptor descriptor;
    wgpu::QuerySet querySet;

    // Create QuerySet for Occlusion Query
    {
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::Occlusion, 1);

        // Success on default device without any extension enabled
        querySet = device.CreateQuerySet(&descriptor);
        querySet.Destroy();

        // Success on the device with extension enabled.
        // Occlusion query does not require any extension.
        querySet = deviceWithExtensions.CreateQuerySet(&descriptor);
        querySet.Destroy();
    }

    // Create QuerySet for PipelineStatistics Query
    {
        std::vector<wgpu::PipelineStatisticsName> pipelineStatistics = {
            wgpu::PipelineStatisticsName::VertexShaderInvocations};

        descriptor =
            CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1, &pipelineStatistics);

        // Fail on default device without any extension enabled
        ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));

        // Success on the device if the extension is enabled.
        if (isPipelineStatisticsQuerySupported) {
            querySet = deviceWithExtensions.CreateQuerySet(&descriptor);
            querySet.Destroy();
        }
    }

    // Create QuerySet for Timestamp Query
    {
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::Timestamp, 1);

        // Fail on default device without any extension enabled
        ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));

        // Success on the device if the extension is enabled.
        if (isTimestampQuerySupported) {
            querySet = deviceWithExtensions.CreateQuerySet(&descriptor);
            querySet.Destroy();
        }
    }
}

// Test creating query set with invalid type
TEST_F(QueryValidationTest, InvalidQueryType) {
    wgpu::QuerySetDescriptor descriptor =
        CreateQuerySetDescriptor(static_cast<wgpu::QueryType>(0xFFFFFFFF), 1);
    ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
}

// Test creating query set with unnecessary pipeline statistics
TEST_F(QueryValidationTest, UnnecessaryPipelineStatistics) {
    wgpu::QuerySetDescriptor descriptor;
    std::vector<wgpu::PipelineStatisticsName> pipelineStatistics = {
        wgpu::PipelineStatisticsName::VertexShaderInvocations};

    // Fail to create with pipeline statistics for Occlusion query
    {
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::Occlusion, 1, &pipelineStatistics);
        ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
    }

    // Fail to create with pipeline statistics for Timestamp query
    {
        if (isTimestampQuerySupported) {
            descriptor =
                CreateQuerySetDescriptor(wgpu::QueryType::Timestamp, 1, &pipelineStatistics);
            deviceWithExtensions.CreateQuerySet(&descriptor);
        }
    }
}

// Test creating query set with invalid pipeline statistics
TEST_F(QueryValidationTest, InvalidPipelineStatistics) {
    // Skip if the pipeline statistics query is not supported.
    DAWN_SKIP_TEST_IF(!isPipelineStatisticsQuerySupported);

    wgpu::QuerySetDescriptor descriptor;
    std::vector<wgpu::PipelineStatisticsName> pipelineStatistics;

    // Success to create with all pipeline statistics name
    {
        pipelineStatistics = {wgpu::PipelineStatisticsName::ClipperInvocations,
                              wgpu::PipelineStatisticsName::ClipperPrimitivesOut,
                              wgpu::PipelineStatisticsName::ComputeShaderInvocations,
                              wgpu::PipelineStatisticsName::FragmentShaderInvocations,
                              wgpu::PipelineStatisticsName::VertexShaderInvocations};
        descriptor =
            CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1, &pipelineStatistics);
        wgpu::QuerySet querySet = deviceWithExtensions.CreateQuerySet(&descriptor);
        querySet.Destroy();
    }

    // Fail to create with empty pipeline statistics
    {
        pipelineStatistics = {};
        descriptor =
            CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1, &pipelineStatistics);
        ASSERT_DEVICE_ERROR(deviceWithExtensions.CreateQuerySet(&descriptor));
    }

    // Fail to create with invalid pipeline statistics
    {
        pipelineStatistics = {static_cast<wgpu::PipelineStatisticsName>(0xFFFFFFFF)};
        descriptor =
            CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1, &pipelineStatistics);
        ASSERT_DEVICE_ERROR(deviceWithExtensions.CreateQuerySet(&descriptor));
    }

    // Fail to create with duplicate pipeline statistics
    {
        pipelineStatistics = {wgpu::PipelineStatisticsName::VertexShaderInvocations,
                              wgpu::PipelineStatisticsName::VertexShaderInvocations};
        descriptor =
            CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1, &pipelineStatistics);
        ASSERT_DEVICE_ERROR(deviceWithExtensions.CreateQuerySet(&descriptor));
    }
}

// Test destroying a destroyed query set
TEST_F(QueryValidationTest, DestroyDestroyedQuerySet) {
    wgpu::QuerySetDescriptor descriptor = CreateQuerySetDescriptor(wgpu::QueryType::Occlusion, 1);
    wgpu::QuerySet querySet = device.CreateQuerySet(&descriptor);
    querySet.Destroy();
    querySet.Destroy();
}
