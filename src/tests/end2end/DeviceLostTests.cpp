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

class DeviceLostTest : public DawnTest {
  protected:
    void TestSetUp() override {
        if (UsesWire()) {
            return;
        }
        DawnTest::TestSetUp();
    }
};

TEST_P(DeviceLostTest, SubmitFailsAfterDeviceLost) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(queue.Submit(0, nullptr));
}

TEST_P(DeviceLostTest, CreateBindGroupFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(nullptr));
}

TEST_P(DeviceLostTest, CreateBufferFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateBuffer(nullptr));
}

TEST_P(DeviceLostTest, CreateComputePipelineFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateComputePipeline(nullptr));
}

TEST_P(DeviceLostTest, CreatePipelineLayoutFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreatePipelineLayout(nullptr));
}

TEST_P(DeviceLostTest, CreateQueueFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateQueue());
}

TEST_P(DeviceLostTest, CreateRenderBundleEncoderFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateRenderBundleEncoder(nullptr));
}

TEST_P(DeviceLostTest, CreateRenderPipelineFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(nullptr));
}

TEST_P(DeviceLostTest, CreateSamplerFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateSampler(nullptr));
}

TEST_P(DeviceLostTest, CreateShaderModuleFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateShaderModule(nullptr));
}

TEST_P(DeviceLostTest, CreateSwapChainFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateSwapChain(nullptr));
}

TEST_P(DeviceLostTest, CreateTextureFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    device.LoseForTesting();
    ASSERT_DEVICE_ERROR(device.CreateTexture(nullptr));
}

TEST_P(DeviceLostTest, BufferSetSubDataFails) {
    DAWN_SKIP_TEST_IF(UsesWire());
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    device.LoseForTesting();
    uint32_t value = 0;
    ASSERT_DEVICE_ERROR(buffer.SetSubData(0, sizeof(value), &value));
}

DAWN_INSTANTIATE_TEST(DeviceLostTest, D3D12Backend);