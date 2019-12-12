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

// Test that DeviceLostCallback is invoked when LostForTestimg is called
TEST_P(DeviceLostTest, DeviceLostCallbackIsCalled) {
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(1);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();
}

// Test that CreateBufferMappedAsync fails with DeviceLostError
// TEST_P(DeviceLostTest, CreateBufferMappedAsyncFails) {
//     wgpu::BufferDescriptor descriptor;
//     descriptor.size = 4;
//     descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::MapWrite;
//     wgpu::Buffer buffer = device.CreateBuffer(&descriptor);
//     EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
//     device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
//     device.LoseForTesting();
//     ASSERT_DEVICE_ERROR([&]() {
//         bool done = true;
//         buffer.MapWriteAsync(
//             [](WGPUBufferMapAsyncStatus status, void* data, uint64_t, void* userdata) {
//                 ASSERT_EQ(WGPUBufferMapAsyncStatus_DeviceLost, status);
//                 ASSERT_EQ(nullptr, data);

//                 *static_cast<bool*>(userdata) = true;
//             },
//             &done);

//         while (!done) {
//             WaitABit();
//         }
//     }());
// }

// Test that submit fails when device is lost
TEST_P(DeviceLostTest, SubmitFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for Submit fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::CommandBuffer commands;
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    commands = encoder.Finish();
    ASSERT_DEVICE_ERROR(queue.Submit(0, &commands));
}

// Test that create bind group fails when device is lost
TEST_P(DeviceLostTest, CreateBindGroupFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for CreateBindGroup
    // fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::BindGroupLayoutBinding binding = {0, wgpu::ShaderStage::None,
                                            wgpu::BindingType::UniformBuffer};
    wgpu::BindGroupLayoutDescriptor descriptor;
    descriptor.bindingCount = 1;
    descriptor.bindings = &binding;
    ASSERT_DEVICE_ERROR(device.CreateBindGroupLayout(&descriptor));
}

// Test that create buffer fails when device is lost
TEST_P(DeviceLostTest, CreateBufferFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for CreateBuffer fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::BufferDescriptor bufferDescriptor;
    bufferDescriptor.size = sizeof(float);
    bufferDescriptor.usage = wgpu::BufferUsage::CopySrc;
    ASSERT_DEVICE_ERROR(device.CreateBuffer(&bufferDescriptor));
}

// test that create compute pipeline fails when device is lost
TEST_P(DeviceLostTest, CreateComputePipelineFails) {
    // excpect 3 calls device lost callback, once for when LoseForTesting, once for
    // CreateShaderModule, and once for CreateComputePipeline fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(3);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::ComputePipelineDescriptor descriptor;
    descriptor.computeStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
        #version 450
        layout(set = 0, binding = 0) uniform UniformBuffer {
            vec4 pos;
        };
        void main() {
        })");

    descriptor.computeStage.entryPoint = "main";
    ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&descriptor));
}

TEST_P(DeviceLostTest, CreatePipelineLayoutFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for
    // CreatePipelineLayout fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::PipelineLayoutDescriptor descriptor;
    descriptor.bindGroupLayoutCount = 0;
    descriptor.bindGroupLayouts = nullptr;
    ASSERT_DEVICE_ERROR(device.CreatePipelineLayout(&descriptor));
}

// Tests that CreateRenderBundleEncoder fails when device is lost
TEST_P(DeviceLostTest, CreateRenderBundleEncoderFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for
    // CreateRenderBundleEncoder fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::RenderBundleEncoderDescriptor descriptor;
    descriptor.colorFormatsCount = 0;
    descriptor.colorFormats = nullptr;
    ASSERT_DEVICE_ERROR(device.CreateRenderBundleEncoder(&descriptor));
}

// Tests that CreateRenderPipeline fails when device is lost
TEST_P(DeviceLostTest, CreateRenderPipelineFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for
    // CreateRenderPipeline fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::RenderPipelineDescriptor descriptor;
    ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
}

// Tests that CreateSampler fails when device is lost
TEST_P(DeviceLostTest, CreateSamplerFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for CreateSampler
    // fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::SamplerDescriptor descriptor;
    ASSERT_DEVICE_ERROR(device.CreateSampler(&descriptor));
}

// Tests that CreateShaderModule fails when device is lost
TEST_P(DeviceLostTest, CreateShaderModuleFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for CreateShaderModule
    // fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::ShaderModuleDescriptor descriptor;
    ASSERT_DEVICE_ERROR(device.CreateShaderModule(&descriptor));
}

// Tests that CreateSwapChain fails when device is lost
TEST_P(DeviceLostTest, CreateSwapChainFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for CreateSwapChain
    // fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::SwapChainDescriptor descriptor;
    ASSERT_DEVICE_ERROR(device.CreateSwapChain(&descriptor));
}

// Tests that CreateTexture fails when device is lost
TEST_P(DeviceLostTest, CreateTextureFails) {
    // excpect device lost callback twice, once for when LoseForTesting, once for CreateTexture
    // fails
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    wgpu::TextureDescriptor descriptor;
    ASSERT_DEVICE_ERROR(device.CreateTexture(&descriptor));
}

// Tests that Buffer::SetSubData fails when device is lost
TEST_P(DeviceLostTest, BufferSetSubDataFails) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    // excpect device lost callback twice, once for when LoseForTesting, once for Buffer::SetSubData
    EXPECT_CALL(*mockDeviceLostCallback, Call(_, this)).Times(2);
    device.SetDeviceLostCallback(ToMockDeviceLostCallback, this);
    device.LoseForTesting();

    uint32_t value = 0;
    ASSERT_DEVICE_ERROR(buffer.SetSubData(0, sizeof(value), &value));
}

DAWN_INSTANTIATE_TEST(DeviceLostTest, D3D12Backend);