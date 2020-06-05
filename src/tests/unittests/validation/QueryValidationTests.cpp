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

        deviceWithExtensions = CreateDeviceWithExtension();
    }

    void AssertCreateQuerySetSuccess(wgpu::Device cDevice,
                                     const wgpu::QuerySetDescriptor* descriptor) {
        wgpu::QuerySet querySet = cDevice.CreateQuerySet(descriptor);
        querySet.Destroy();
    }

    void AssertCreateQuerySetError(wgpu::Device cDevice,
                                   const wgpu::QuerySetDescriptor* descriptor) {
        ASSERT_DEVICE_ERROR(cDevice.CreateQuerySet(descriptor));
    }

    wgpu::QuerySetDescriptor CreateQuerySetDescriptor(
        wgpu::QueryType queryType,
        uint32_t queryCount,
        const wgpu::PipelineStatisticsName* pipelineStatistics = nullptr,
        uint32_t pipelineStatisticsCount = 0) {
        wgpu::QuerySetDescriptor descriptor;
        descriptor.type = queryType;
        descriptor.count = queryCount;
        descriptor.pipelineStatistics = pipelineStatistics;
        descriptor.pipelineStatisticsCount = pipelineStatisticsCount;

        return descriptor;
    }

    wgpu::Device CreateDeviceWithExtension() {
        std::vector<const char*> extensions = {};
        isPipelineStatisticsQuerySupported = SupportsExtensions({"pipeline_statistics_query"});
        isTimestampQuerySupported = SupportsExtensions({"timestamp_query"});

        if (isPipelineStatisticsQuerySupported) {
            extensions.push_back("pipeline_statistics_query");
        }

        if (isTimestampQuerySupported) {
            extensions.push_back("timestamp_query");
        }

        return CreateDeviceFromAdapter(adapter, extensions);
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

// Test create query set with/without extensions
TEST_F(QueryValidationTest, Creation) {
    wgpu::QuerySetDescriptor descriptor;

    // Create QuerySet for Occlusion Query
    {
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::Occlusion, 1);

        // Create success on default device without any extension enabled
        AssertCreateQuerySetSuccess(device, &descriptor);

        // Create success on the device with extension enabled.
        // Occlusion query does not require any extension.
        AssertCreateQuerySetSuccess(deviceWithExtensions, &descriptor);
    }

    // Create QuerySet for PipelineStatistics Query
    {
        wgpu::PipelineStatisticsName pipelineStatistics =
            wgpu::PipelineStatisticsName::VertexShaderInvocations;
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1,
                                              &pipelineStatistics, 1);

        // Create fail on default device without any extension enabled
        AssertCreateQuerySetError(device, &descriptor);

        // Create success on the device if the extension is enabled.
        if (isPipelineStatisticsQuerySupported) {
            AssertCreateQuerySetSuccess(deviceWithExtensions, &descriptor);
        }
    }

    // Create QuerySet for Timestamp Query
    {
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::Timestamp, 1);

        // Create fail on default device without any extension enabled
        AssertCreateQuerySetError(device, &descriptor);

        // Create success on the device if the extension is enabled.
        if (isTimestampQuerySupported) {
            AssertCreateQuerySetSuccess(deviceWithExtensions, &descriptor);
        }
    }
}

// Test create query set with invalid type
TEST_F(QueryValidationTest, InvalidQueryType) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor = CreateQuerySetDescriptor(static_cast<wgpu::QueryType>(0xFFFFFFFF), 1);
    AssertCreateQuerySetError(device, &descriptor);
}

// Test create query set with invalid pipeline statistics count
TEST_F(QueryValidationTest, PipelineStatisticsCount) {
    // Skip if the pipeline statistics query is not supported.
    DAWN_SKIP_TEST_IF(!isPipelineStatisticsQuerySupported);

    wgpu::QuerySetDescriptor descriptor;
    wgpu::PipelineStatisticsName pipelineStatistics =
        wgpu::PipelineStatisticsName::VertexShaderInvocations;

    // Set pipeline statistics count with 0
    {
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1,
                                              &pipelineStatistics, 0);
        AssertCreateQuerySetError(deviceWithExtensions, &descriptor);
    }

    // Set pipeline statistics count exceeds maximum
    {
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1,
                                              &pipelineStatistics, 6);
        AssertCreateQuerySetError(deviceWithExtensions, &descriptor);
    }
}

// Test create query set with invalid pipeline statistics name
TEST_F(QueryValidationTest, PipelineStatisticsName) {
    // Skip if the pipeline statistics query is not supported.
    DAWN_SKIP_TEST_IF(!isPipelineStatisticsQuerySupported);

    wgpu::QuerySetDescriptor descriptor;

    // Set pipeline statistics with nullptr
    {
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1, nullptr, 1);
        AssertCreateQuerySetError(deviceWithExtensions, &descriptor);
    }

    // Set pipeline statistics with invalid name
    {
        wgpu::PipelineStatisticsName invalidName =
            static_cast<wgpu::PipelineStatisticsName>(0xFFFFFFFF);
        descriptor =
            CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1, &invalidName, 1);
        AssertCreateQuerySetError(deviceWithExtensions, &descriptor);
    }

    // Duplicate pipeline statistics name
    {
        std::array<wgpu::PipelineStatisticsName, 2> pipelineStatisticsSet;
        pipelineStatisticsSet[0] = wgpu::PipelineStatisticsName::VertexShaderInvocations;
        pipelineStatisticsSet[1] = wgpu::PipelineStatisticsName::VertexShaderInvocations;
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1,
                                              pipelineStatisticsSet.data(), 2);
        AssertCreateQuerySetError(deviceWithExtensions, &descriptor);
    }

    // The number of pipeline statistics name is less than pipeline statistics count
    {
        std::array<wgpu::PipelineStatisticsName, 2> pipelineStatisticsSet;
        pipelineStatisticsSet[0] = wgpu::PipelineStatisticsName::VertexShaderInvocations;
        pipelineStatisticsSet[1] = wgpu::PipelineStatisticsName::FragmentShaderInvocations;
        descriptor = CreateQuerySetDescriptor(wgpu::QueryType::PipelineStatistics, 1,
                                              pipelineStatisticsSet.data(), 3);
        AssertCreateQuerySetError(deviceWithExtensions, &descriptor);
    }
}

// Test destroy a destroyed query set
TEST_F(QueryValidationTest, DestroyDestroyedQuerySet) {
    wgpu::QuerySetDescriptor descriptor;
    descriptor = CreateQuerySetDescriptor(wgpu::QueryType::Occlusion, 1);
    wgpu::QuerySet querySet = device.CreateQuerySet(&descriptor);
    querySet.Destroy();
    querySet.Destroy();
}
