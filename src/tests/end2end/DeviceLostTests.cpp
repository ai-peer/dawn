// Copyright 2019 The Dawn Authors
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

#include <gmock/gmock.h>

using namespace testing;
#include <iostream>

class MockDeviceLostCallback {
  public:
    MOCK_METHOD2(Call, void(const char* message, void* userdata));
};

static std::unique_ptr<MockDeviceLostCallback> mockDeviceLostCallback;
// static void ToMockDeviceLostCallback(const char* message, void* userdata) {
//     mockDeviceLostCallback->Call(message, userdata);
// }

class DeviceLostTest : public DawnTest {
  protected:
    void TestSetUp() override {
        DawnTest::TestSetUp();
        mockDeviceLostCallback = std::make_unique<MockDeviceLostCallback>();
    }

    void TearDown() override {
        mockDeviceLostCallback = nullptr;
        DawnTest::TearDown();
    }

    static void OnDeviceLost(const char* message, void* userdata) {
        DawnTestBase* self = static_cast<DawnTestBase*>(userdata);
        self->StartExpectDeviceError();
    }
};

TEST_P(DeviceLostTest, DeviceLostCallback) {
    device.SetDeviceLostCallback(OnDeviceLost, this);
    device.LoseForTesting();
}

// TEST_P(DeviceLostTest, CreateBindGroupFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateBindGroup(nullptr));
// }

// TEST_P(DeviceLostTest, CreateBufferFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateBuffer(nullptr));
// }

// TEST_P(DeviceLostTest, CreateComputePipelineFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateComputePipeline(nullptr));
// }

// TEST_P(DeviceLostTest, CreatePipelineLayoutFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreatePipelineLayout(nullptr));
// }

// TEST_P(DeviceLostTest, CreateRenderBundleEncoderFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateRenderBundleEncoder(nullptr));
// }

// TEST_P(DeviceLostTest, CreateRenderPipelineFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(nullptr));
// }

// TEST_P(DeviceLostTest, CreateSamplerFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateSampler(nullptr));
// }

// TEST_P(DeviceLostTest, CreateShaderModuleFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateShaderModule(nullptr));
// }

// TEST_P(DeviceLostTest, CreateSwapChainFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateSwapChain(nullptr));
// }

// TEST_P(DeviceLostTest, CreateTextureFails) {
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR(device.CreateTexture(nullptr));
// }

// TEST_P(DeviceLostTest, BufferSetSubDataFails) {
//     wgpu::BufferDescriptor descriptor;
//     descriptor.size = 4;
//     descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
//     wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

//     device.LoseForTesting();
//     uint32_t value = 0;
//     ASSERT_DEVICE_ERROR(buffer.SetSubData(0, sizeof(value), &value));
// }

DAWN_INSTANTIATE_TEST(DeviceLostTest, D3D12Backend);