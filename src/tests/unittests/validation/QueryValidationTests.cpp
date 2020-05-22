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
    wgpu::QuerySetDescriptor CreateDefaultQuerySetDescriptor() {
        wgpu::QuerySetDescriptor descriptor;
        descriptor.type = wgpu::QueryType::Occlusion;
        descriptor.count = 1;

        return descriptor;
    }
};

// Test the validation of query count
TEST_F(QueryValidationTest, QueryCount) {
    wgpu::QuerySetDescriptor defaultDescriptor = CreateDefaultQuerySetDescriptor();
    // It is an error to create a queryset with a count not greater than 0.
    wgpu::QuerySetDescriptor descriptor = defaultDescriptor;
    descriptor.count = 0;

    ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
}

// Test the validation of queryset creation without enabling extensions
TEST_F(QueryValidationTest, CreateQuerySetWithoutEnablingExtensions) {
    wgpu::QuerySetDescriptor defaultDescriptor = CreateDefaultQuerySetDescriptor();

    // Create fail without pipeline statistics query extension enabled
    {
        wgpu::QuerySetDescriptor descriptor = defaultDescriptor;
        descriptor.type = wgpu::QueryType::PipelineStatistics;
        descriptor.count = 1;

        ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
    }

    // Create fail without timestamp query extension enabled
    {
        wgpu::QuerySetDescriptor descriptor = defaultDescriptor;
        descriptor.type = wgpu::QueryType::Timestamp;
        descriptor.count = 1;

        ASSERT_DEVICE_ERROR(device.CreateQuerySet(&descriptor));
    }
}
