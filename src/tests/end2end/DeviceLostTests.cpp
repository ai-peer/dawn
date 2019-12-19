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
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

using namespace testing;

class MockDeviceLostCallback {
  public:
    MOCK_METHOD2(Call, void(const char* message, void* userdata));
};

static std::unique_ptr<MockDeviceLostCallback> mockDeviceLostCallback;
static void ToMockDeviceLostCallback(const char* message, void* userdata) {
    mockDeviceLostCallback->Call(message, userdata);
    DawnTestBase* self = static_cast<DawnTestBase*>(userdata);
    self->StartExpectDeviceError();
}

class DeviceLostTest : public DawnTest {
  protected:
    void TestSetUp() override {
        if (UsesWire()) {
            return;
        }
        DawnTest::TestSetUp();
        mockDeviceLostCallback = std::make_unique<MockDeviceLostCallback>();
    }

    void TearDown() override {
        DawnTest::TearDown();
        mockDeviceLostCallback = nullptr;
    }

    void SetCallbackAndLoseForTesting() {
        device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
        EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(1);
        device.LoseForTesting();
    }
};

// Test that DeviceLostCallback is invoked when LostForTestimg is called
TEST_P(DeviceLostTest, DeviceLostCallbackIsCalled) {
    DAWN_SKIP_TEST_IF(UsesWire());
    SetCallbackAndLoseForTesting();
}

// Test that submit fails when device is lost
TEST_P(DeviceLostTest, SubmitFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    SetCallbackAndLoseForTesting();

    wgpu::CommandBuffer commands;
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    commands = encoder.Finish();
    ASSERT_DEVICE_ERROR(queue.Submit(0, &commands));
}

// Test that CreateBindGroup fails when device is lost
TEST_P(DeviceLostTest, CreateBindGroupFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    SetCallbackAndLoseForTesting();

    wgpu::BindGroupLayoutBinding binding = {0, wgpu::ShaderStage::None,
                                            wgpu::BindingType::UniformBuffer};
    wgpu::BindGroupLayoutDescriptor descriptor;
    descriptor.bindingCount = 1;
    descriptor.bindings = &binding;
    ASSERT_DEVICE_ERROR(device.CreateBindGroupLayout(&descriptor));
}

// Test that CreateBuffer fails when device is lost
TEST_P(DeviceLostTest, CreateBufferFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    SetCallbackAndLoseForTesting();

    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = sizeof(float);
    bufferDescriptor.usage = wgpu::BufferUsage::CopySrc;
    ASSERT_DEVICE_ERROR(device.CreateBuffer(&bufferDescriptor));
}

// Test that CreatePipelineLayout fails when device is lost
TEST_P(DeviceLostTest, CreatePipelineLayoutFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    SetCallbackAndLoseForTesting();

    wgpu::PipelineLayoutDescriptor descriptor;
    descriptor.bindGroupLayoutCount = 0;
    descriptor.bindGroupLayouts = nullptr;
    ASSERT_DEVICE_ERROR(device.CreatePipelineLayout(&descriptor));
}

// Tests that CreateRenderBundleEncoder fails when device is lost
TEST_P(DeviceLostTest, CreateRenderBundleEncoderFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    SetCallbackAndLoseForTesting();

    wgpu::RenderBundleEncoderDescriptor descriptor;
    descriptor.colorFormatsCount = 0;
    descriptor.colorFormats = nullptr;
    ASSERT_DEVICE_ERROR(device.CreateRenderBundleEncoder(&descriptor));
}

DAWN_INSTANTIATE_TEST(DeviceLostTest, D3D12Backend, VulkanBackend);