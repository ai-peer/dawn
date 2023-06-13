// Copyright 2023 The Dawn Authors
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

#include <vector>

#include "dawn/tests/DawnTest.h"

namespace dawn {
namespace {

class WaitAnyTests : public DawnTest {};

TEST_P(WaitAnyTests, Validation_UnsupportedTimeout) {
    // IMPORTANT: DawnTest overrides RequestAdapter and RequestDevice and mixes
    // up the two instances. Use these to bypass the override.
    auto* requestAdapter = reinterpret_cast<WGPUProcInstanceRequestAdapter>(
        wgpuGetProcAddress(nullptr, "wgpuInstanceRequestAdapter"));
    auto* requestDevice = reinterpret_cast<WGPUProcAdapterRequestDevice>(
        wgpuGetProcAddress(nullptr, "wgpuAdapterRequestDevice"));

    wgpu::InstanceDescriptor desc{};
    wgpu::Instance instance2 = wgpu::CreateInstance(&desc);
    // UnsupportedTimeout is not validated if no futures are passed.
    for (uint64_t timeout : {1ull, 0ull, UINT64_MAX}) {
        ASSERT_EQ(instance2.WaitAny(0, nullptr, timeout), wgpu::WaitStatus::Success);
    }

    wgpu::Adapter adapter2;
    requestAdapter(
        instance2.Get(), nullptr,
        [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char*, void* userdata) {
            ASSERT_EQ(status, WGPURequestAdapterStatus_Success);
            *reinterpret_cast<wgpu::Adapter*>(userdata) = wgpu::Adapter(adapter);
        },
        &adapter2);
    ASSERT_TRUE(adapter2);

    wgpu::Device device2;
    requestDevice(
        adapter2.Get(), nullptr,
        [](WGPURequestDeviceStatus status, WGPUDevice device, const char*, void* userdata) {
            ASSERT_EQ(status, WGPURequestDeviceStatus_Success);
            *reinterpret_cast<wgpu::Device*>(userdata) = wgpu::Device(device);
        },
        &device2);
    ASSERT_TRUE(device2);

    for (uint64_t timeout : {1ull, 0ull, UINT64_MAX}) {
        wgpu::FutureWaitInfo info{device2.GetQueue().OnSubmittedWorkDoneF(
            {wgpu::CallbackMode::Future, [](WGPUQueueWorkDoneStatus, void*) {}, nullptr})};
        ASSERT_EQ(instance2.WaitAny(1, &info, timeout),
                  timeout == 0 ? wgpu::WaitStatus::Success : wgpu::WaitStatus::UnsupportedTimeout);
    }
}

TEST_P(WaitAnyTests, Validation_UnsupportedCount) {
    for (int count : {64, 65}) {
        std::vector<wgpu::FutureWaitInfo> infos;
        for (int i = 0; i < count; ++i) {
            infos.push_back({queue.OnSubmittedWorkDoneF(
                {wgpu::CallbackMode::Future, [](WGPUQueueWorkDoneStatus, void*) {}, nullptr})});
        }
        wgpu::WaitStatus status = GetInstance().WaitAny(infos.size(), infos.data(), 1);
        if (count <= 64) {
            ASSERT_EQ(status, wgpu::WaitStatus::Success);
        } else {
            ASSERT_EQ(status, wgpu::WaitStatus::UnsupportedCount);
        }
    }
}

TEST_P(WaitAnyTests, Validation_UnsupportedMixedSources) {
    wgpu::Device device2 = CreateDevice();
    wgpu::Queue queue2 = device2.GetQueue();
    std::vector<wgpu::FutureWaitInfo> infos{{
        {queue.OnSubmittedWorkDoneF(
            {wgpu::CallbackMode::Future, [](WGPUQueueWorkDoneStatus, void*) {}, nullptr})},
        {queue2.OnSubmittedWorkDoneF(
            {wgpu::CallbackMode::Future, [](WGPUQueueWorkDoneStatus, void*) {}, nullptr})},
    }};
    wgpu::WaitStatus status = GetInstance().WaitAny(infos.size(), infos.data(), 1);
    ASSERT_EQ(status, wgpu::WaitStatus::UnsupportedMixedSources);
    // FIXME
}

DAWN_INSTANTIATE_TEST(
    WaitAnyTests,
    // TODO(crbug.com/dawn/1987): Enable tests for the rest of the backends
    // TODO(crbug.com/dawn/1987): Enable tests on the wire (though they'll behave differently)
    D3D12Backend(),
    MetalBackend());

}  // anonymous namespace
}  // namespace dawn
