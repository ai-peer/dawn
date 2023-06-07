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

#include <memory>

#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"
#include "dawn/platform/DawnPlatform.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::StrEq;
using testing::StrictMock;

class TestPlatform : public dawn::platform::Platform {
  public:
    MOCK_METHOD(void, ReportError, (wgpu::ErrorType type, std::string_view message), (override));
};

// Test that errors are surfaced to the platform's ReportError method.
TEST(DawnPlatformTests, ReportError) {
    dawnProcSetProcs(&dawn::native::GetProcs());

    StrictMock<TestPlatform> platform;

    wgpu::DawnInstanceDescriptor dawnInstanceDesc = {};
    dawnInstanceDesc.platform = &platform;

    WGPUInstanceDescriptor instanceDesc = {};
    instanceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&dawnInstanceDesc);

    auto nativeInstance = std::make_unique<dawn::native::Instance>(&instanceDesc);
    nativeInstance->DiscoverDefaultPhysicalDevices();

    wgpu::Instance instance(nativeInstance->Get());
    wgpu::Adapter adapter;
    instance.RequestAdapter(
        nullptr,
        [](WGPURequestAdapterStatus, WGPUAdapter cAdapter, const char*, void* userdata) {
            *static_cast<wgpu::Adapter*>(userdata) = wgpu::Adapter::Acquire(cAdapter);
        },
        &adapter);

    instance.ProcessEvents();
    ASSERT_NE(adapter, nullptr);

    wgpu::Device device;
    wgpu::DeviceDescriptor deviceDesc = {};
    adapter.RequestDevice(
        &deviceDesc,
        [](WGPURequestDeviceStatus, WGPUDevice cDevice, const char*, void* userdata) {
            *static_cast<wgpu::Device*>(userdata) = wgpu::Device::Acquire(cDevice);
        },
        &device);
    instance.ProcessEvents();
    ASSERT_NE(device, nullptr);

    // Validation error is not reported, but OOM error is.
    EXPECT_CALL(platform, ReportError(wgpu::ErrorType::OutOfMemory, StrEq("fake oom error")))
        .Times(1);
    EXPECT_CALL(platform, ReportError(wgpu::ErrorType::Validation, _)).Times(0);

    // Inject a validation and an OOM error.
    device.InjectError(wgpu::ErrorType::Validation, "fake validation error");
    device.InjectError(wgpu::ErrorType::OutOfMemory, "fake oom error");
}

// Test that there are no crashes if the platform is not provided.
TEST(DawnPlatformTests, ReportErrorNoPlatform) {
    dawnProcSetProcs(&dawn::native::GetProcs());

    wgpu::DawnInstanceDescriptor dawnInstanceDesc = {};
    dawnInstanceDesc.platform = nullptr;

    WGPUInstanceDescriptor instanceDesc = {};
    instanceDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&dawnInstanceDesc);

    auto nativeInstance = std::make_unique<dawn::native::Instance>(&instanceDesc);
    nativeInstance->DiscoverDefaultPhysicalDevices();

    wgpu::Instance instance(nativeInstance->Get());
    wgpu::Adapter adapter;
    instance.RequestAdapter(
        nullptr,
        [](WGPURequestAdapterStatus, WGPUAdapter cAdapter, const char*, void* userdata) {
            *static_cast<wgpu::Adapter*>(userdata) = wgpu::Adapter::Acquire(cAdapter);
        },
        &adapter);

    instance.ProcessEvents();
    ASSERT_NE(adapter, nullptr);

    wgpu::Device device;
    wgpu::DeviceDescriptor deviceDesc = {};
    adapter.RequestDevice(
        &deviceDesc,
        [](WGPURequestDeviceStatus, WGPUDevice cDevice, const char*, void* userdata) {
            *static_cast<wgpu::Device*>(userdata) = wgpu::Device::Acquire(cDevice);
        },
        &device);
    instance.ProcessEvents();
    ASSERT_NE(device, nullptr);

    // Inject a validation and an OOM error.
    device.InjectError(wgpu::ErrorType::Validation, "fake validation error");
    device.InjectError(wgpu::ErrorType::OutOfMemory, "fake oom error");
}

}  // anonymous namespace
