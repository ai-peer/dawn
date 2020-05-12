// Copyright 2017 The Dawn Authors
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

#include "utils/WGPUHelpers.h"

#include "common/Log.h"

class BasicTests : public DawnTest {
};

// Test adapter filter by vendor id.
TEST_P(BasicTests, VendorIdFilter) {
    DAWN_SKIP_TEST_IF(!HasVendorIdFilter());

    ASSERT_EQ(GetAdapterProperties().vendorID, GetVendorIdFilter());
}

// Test Buffer::SetSubData changes the content of the buffer, but really this is the most
// basic test possible, and tests the test harness
TEST_P(BasicTests, BufferSetSubData) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    DAWN_DEBUG() << "create buffer";
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
    DAWN_DEBUG() << "post create buffer";

    uint32_t value = 0x01020304;
    DAWN_DEBUG() << "set sub data";
    buffer.SetSubData(0, sizeof(value), &value);
    DAWN_DEBUG() << "post set sub data";

    DAWN_DEBUG() << "expect buffer value";
    EXPECT_BUFFER_U32_EQ(value, buffer, 0);
    DAWN_DEBUG() << "post expect";
}

// Test a validation error for buffer setSubData, but really this is the most basic test possible
// for ASSERT_DEVICE_ERROR
TEST_P(BasicTests, BufferSetSubDataError) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    uint8_t value = 187;
    ASSERT_DEVICE_ERROR(buffer.SetSubData(1000, sizeof(value), &value));
}

DAWN_INSTANTIATE_TEST(BasicTests, D3D12Backend(), MetalBackend(), OpenGLBackend(), VulkanBackend());
