// Copyright 2021 The Dawn Authors
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
#include <vector>

#include "dawn/dawn_proc.h"
#include "dawn/native/Adapter.h"
#include "dawn/native/DawnNative.h"
#include "dawn/native/Device.h"
#include "dawn/native/Toggles.h"
#include "dawn/native/dawn_platform.h"
#include "dawn/tests/MockCallback.h"
#include "dawn/utils/SystemUtils.h"
#include "dawn/utils/WGPUHelpers.h"
#include "gtest/gtest.h"

namespace {

using testing::Contains;
using testing::MockCallback;
using testing::NotNull;
using testing::SaveArg;
using testing::StrEq;

class DeviceCreationTest : public testing::Test {
  protected:
    void SetUp() override {
        dawnProcSetProcs(&dawn::native::GetProcs());

        const char* disallowUnsafeApisToggle = "disallow_unsafe_apis";
        WGPUDawnTogglesDescriptor unsafeInstanceTogglesDesc = {};
        unsafeInstanceTogglesDesc.chain.sType = WGPUSType::WGPUSType_DawnTogglesDescriptor;
        unsafeInstanceTogglesDesc.disabledTogglesCount = 1;
        unsafeInstanceTogglesDesc.disabledToggles = &disallowUnsafeApisToggle;

        WGPUInstanceDescriptor unsafeInstanceDesc = {};
        unsafeInstanceDesc.nextInChain = &unsafeInstanceTogglesDesc.chain;

        // Create an instance with default toggles, where DisallowUnsafeApis is enabled.
        instance = std::make_unique<dawn::native::Instance>();
        // Create an instance with toggle DisallowUnsafeApis disabled.
        unsafeInstance = std::make_unique<dawn::native::Instance>(&unsafeInstanceDesc);
        // Discover adapters with default instance toggles
        instance->DiscoverDefaultAdapters();
        // Discover adapters with instance toggle DisallowUnsafeApis disabled
        unsafeInstance->DiscoverDefaultAdapters();

        // Get the adapter created by instance with default toggles
        for (dawn::native::Adapter& nativeAdapter : instance->GetAdapters()) {
            wgpu::AdapterProperties properties;
            nativeAdapter.GetProperties(&properties);

            if (properties.backendType == wgpu::BackendType::Null) {
                adapter = wgpu::Adapter(nativeAdapter.Get());
                break;
            }
        }

        // Get the adapter created by instance with instance toggle DisallowUnsafeApis disabled
        for (dawn::native::Adapter& nativeAdapter : unsafeInstance->GetAdapters()) {
            wgpu::AdapterProperties properties;
            nativeAdapter.GetProperties(&properties);

            if (properties.backendType == wgpu::BackendType::Null) {
                unsafeAdapter = wgpu::Adapter(nativeAdapter.Get());
                break;
            }
        }

        ASSERT_NE(adapter, nullptr);
        ASSERT_NE(unsafeAdapter, nullptr);
    }

    void TearDown() override {
        adapter = nullptr;
        unsafeAdapter = nullptr;
        instance = nullptr;
        dawnProcSetProcs(nullptr);
    }

    static constexpr size_t kTotalFeaturesCount =
        static_cast<size_t>(dawn::native::Feature::EnumCount);

    std::unique_ptr<dawn::native::Instance> instance;
    std::unique_ptr<dawn::native::Instance> unsafeInstance;
    wgpu::Adapter adapter;
    wgpu::Adapter unsafeAdapter;
    dawn::native::FeaturesInfo featuresInfo;
};

// Test successful call to CreateDevice with no descriptor
TEST_F(DeviceCreationTest, CreateDeviceNoDescriptorSuccess) {
    wgpu::Device device = adapter.CreateDevice();
    EXPECT_NE(device, nullptr);
}

// Test successful call to CreateDevice with descriptor.
TEST_F(DeviceCreationTest, CreateDeviceSuccess) {
    wgpu::DeviceDescriptor desc = {};
    wgpu::Device device = adapter.CreateDevice(&desc);
    EXPECT_NE(device, nullptr);
}

// Test successful call to CreateDevice with toggle descriptor.
TEST_F(DeviceCreationTest, CreateDeviceWithTogglesSuccess) {
    wgpu::DeviceDescriptor desc = {};
    wgpu::DawnTogglesDescriptor deviceTogglesDesc = {};
    desc.nextInChain = &deviceTogglesDesc;

    const char* toggle = "skip_validation";
    deviceTogglesDesc.enabledToggles = &toggle;
    deviceTogglesDesc.enabledTogglesCount = 1;

    wgpu::Device device = adapter.CreateDevice(&desc);
    EXPECT_NE(device, nullptr);

    auto toggles = dawn::native::GetTogglesUsed(device.Get());
    EXPECT_THAT(toggles, Contains(StrEq(toggle)));
}

// Test experimental features are guarded by DisallowUnsafeApis adapter toggle.
TEST_F(DeviceCreationTest,
       CreateDeviceRequiringExperimentalFeaturesWithoutDisablingDisallowUnsafeApis) {
    // Ensure that DisallowUnsafeApis adapter toggle is not disabled.
    auto adapterBase = reinterpret_cast<dawn::native::AdapterBase*>(adapter.Get());
    dawn::native::TogglesState adapterToggles = adapterBase->GetAdapterTogglesState();
    ASSERT_FALSE(adapterToggles.IsDisabled(dawn::native::Toggle::DisallowUnsafeAPIs));

    for (size_t i = 0; i < kTotalFeaturesCount; i++) {
        dawn::native::Feature feature = static_cast<dawn::native::Feature>(i);
        wgpu::FeatureName featureName = dawn::native::FeatureEnumToAPIFeature(feature);

        // Only test experimental features
        if (featuresInfo.GetFeatureInfo(featureName)->featureState ==
            dawn::native::FeatureInfo::FeatureState::Stable) {
            continue;
        }

        wgpu::DeviceDescriptor deviceDescriptor;
        deviceDescriptor.requiredFeatures = &featureName;
        deviceDescriptor.requiredFeaturesCount = 1;

        // Test creating device without device toggle would fail.
        {
            wgpu::Device device = adapter.CreateDevice(&deviceDescriptor);
            EXPECT_EQ(device, nullptr);
        }

        // Test creating device with DisallowUnsafeApis device toggle disabled would also failed, as
        // supported features are determinated by adapter toggles.
        {
            const char* const disableToggles[] = {"disallow_unsafe_apis"};
            wgpu::DawnTogglesDescriptor deviceToggleDesc;
            deviceToggleDesc.disabledToggles = disableToggles;
            deviceToggleDesc.disabledTogglesCount = 1;
            deviceDescriptor.nextInChain = &deviceToggleDesc;

            wgpu::Device device = adapter.CreateDevice(&deviceDescriptor);
            EXPECT_EQ(device, nullptr);
        }
    }
}

// Test experimental features are allowed when DisallowUnsafeApis adapter toggle is disabled.
TEST_F(DeviceCreationTest,
       CreateDeviceRequiringExperimentalFeaturesWithDisallowUnsafeApisDisabled) {
    for (size_t i = 0; i < kTotalFeaturesCount; i++) {
        dawn::native::Feature feature = static_cast<dawn::native::Feature>(i);
        wgpu::FeatureName featureName = dawn::native::FeatureEnumToAPIFeature(feature);

        // Only test experimental features
        if (featuresInfo.GetFeatureInfo(featureName)->featureState ==
            dawn::native::FeatureInfo::FeatureState::Stable) {
            continue;
        }

        wgpu::DeviceDescriptor deviceDescriptor;
        deviceDescriptor.requiredFeatures = &featureName;
        deviceDescriptor.requiredFeaturesCount = 1;

        // Test creating device without deviec toggle would succeed.
        {
            // unsafeAdapter has DisallowUnsafeApis adapter toggle disabled.
            wgpu::Device device = unsafeAdapter.CreateDevice(&deviceDescriptor);
            EXPECT_NE(device, nullptr);

            ASSERT_EQ(1u, device.EnumerateFeatures(nullptr));
            wgpu::FeatureName enabledFeature;
            device.EnumerateFeatures(&enabledFeature);
            EXPECT_EQ(enabledFeature, featureName);
            device.Release();
        }

        // Test creating device with DisallowUnsafeApis device toggle disabled would also succeed,
        // as supported features are determinated by adapter toggles.
        {
            const char* const disableToggles[] = {"disallow_unsafe_apis"};
            wgpu::DawnTogglesDescriptor deviceToggleDesc;
            deviceToggleDesc.disabledToggles = disableToggles;
            deviceToggleDesc.disabledTogglesCount = 1;
            deviceDescriptor.nextInChain = &deviceToggleDesc;

            wgpu::Device device = unsafeAdapter.CreateDevice(&deviceDescriptor);
            EXPECT_NE(device, nullptr);

            ASSERT_EQ(1u, device.EnumerateFeatures(nullptr));
            wgpu::FeatureName enabledFeature;
            device.EnumerateFeatures(&enabledFeature);
            EXPECT_EQ(enabledFeature, featureName);
            device.Release();
        }

        // Test creating device with DisallowUnsafeApis device toggle enabled would also succeed,
        // as supported features are determinated by adapter toggles.
        {
            const char* const enableToggles[] = {"disallow_unsafe_apis"};
            wgpu::DawnTogglesDescriptor deviceToggleDesc;
            deviceToggleDesc.enabledToggles = enableToggles;
            deviceToggleDesc.enabledTogglesCount = 1;
            deviceDescriptor.nextInChain = &deviceToggleDesc;

            wgpu::Device device = unsafeAdapter.CreateDevice(&deviceDescriptor);
            EXPECT_NE(device, nullptr);

            ASSERT_EQ(1u, device.EnumerateFeatures(nullptr));
            wgpu::FeatureName enabledFeature;
            device.EnumerateFeatures(&enabledFeature);
            EXPECT_EQ(enabledFeature, featureName);
            device.Release();
        }
    }
}

TEST_F(DeviceCreationTest, CreateDeviceWithCacheSuccess) {
    // Default device descriptor should have the same cache key as a device descriptor with a
    // default cache descriptor.
    {
        wgpu::DeviceDescriptor desc = {};
        wgpu::Device device1 = adapter.CreateDevice(&desc);
        EXPECT_NE(device1, nullptr);

        wgpu::DawnCacheDeviceDescriptor cacheDesc = {};
        desc.nextInChain = &cacheDesc;
        wgpu::Device device2 = adapter.CreateDevice(&desc);

        EXPECT_EQ(dawn::native::FromAPI(device1.Get())->GetCacheKey(),
                  dawn::native::FromAPI(device2.Get())->GetCacheKey());
    }
    // Default device descriptor should not have the same cache key as a device descriptor with
    // a non-default cache descriptor.
    {
        wgpu::DeviceDescriptor desc = {};
        wgpu::Device device1 = adapter.CreateDevice(&desc);
        EXPECT_NE(device1, nullptr);

        wgpu::DawnCacheDeviceDescriptor cacheDesc = {};
        desc.nextInChain = &cacheDesc;
        const char* isolationKey = "isolation key";
        cacheDesc.isolationKey = isolationKey;
        wgpu::Device device2 = adapter.CreateDevice(&desc);
        EXPECT_NE(device2, nullptr);

        EXPECT_NE(dawn::native::FromAPI(device1.Get())->GetCacheKey(),
                  dawn::native::FromAPI(device2.Get())->GetCacheKey());
    }
    // Two non-default cache descriptors should not have the same cache key.
    {
        wgpu::DawnCacheDeviceDescriptor cacheDesc = {};
        const char* isolationKey1 = "isolation key 1";
        const char* isolationKey2 = "isolation key 2";
        wgpu::DeviceDescriptor desc = {};
        desc.nextInChain = &cacheDesc;

        cacheDesc.isolationKey = isolationKey1;
        wgpu::Device device1 = adapter.CreateDevice(&desc);
        EXPECT_NE(device1, nullptr);

        cacheDesc.isolationKey = isolationKey2;
        wgpu::Device device2 = adapter.CreateDevice(&desc);
        EXPECT_NE(device2, nullptr);

        EXPECT_NE(dawn::native::FromAPI(device1.Get())->GetCacheKey(),
                  dawn::native::FromAPI(device2.Get())->GetCacheKey());
    }
}

// Test successful call to RequestDevice with descriptor
TEST_F(DeviceCreationTest, RequestDeviceSuccess) {
    WGPUDevice cDevice;
    {
        MockCallback<WGPURequestDeviceCallback> cb;
        EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Success, NotNull(), nullptr, this))
            .WillOnce(SaveArg<1>(&cDevice));

        wgpu::DeviceDescriptor desc = {};
        adapter.RequestDevice(&desc, cb.Callback(), cb.MakeUserdata(this));
    }

    wgpu::Device device = wgpu::Device::Acquire(cDevice);
    EXPECT_NE(device, nullptr);
}

// Test successful call to RequestDevice with a null descriptor
TEST_F(DeviceCreationTest, RequestDeviceNullDescriptorSuccess) {
    WGPUDevice cDevice;
    {
        MockCallback<WGPURequestDeviceCallback> cb;
        EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Success, NotNull(), nullptr, this))
            .WillOnce(SaveArg<1>(&cDevice));

        adapter.RequestDevice(nullptr, cb.Callback(), cb.MakeUserdata(this));
    }

    wgpu::Device device = wgpu::Device::Acquire(cDevice);
    EXPECT_NE(device, nullptr);
}

// Test failing call to RequestDevice with invalid feature
TEST_F(DeviceCreationTest, RequestDeviceFailure) {
    MockCallback<WGPURequestDeviceCallback> cb;
    EXPECT_CALL(cb, Call(WGPURequestDeviceStatus_Error, nullptr, NotNull(), this)).Times(1);

    wgpu::DeviceDescriptor desc = {};
    wgpu::FeatureName invalidFeature = static_cast<wgpu::FeatureName>(WGPUFeatureName_Force32);
    desc.requiredFeatures = &invalidFeature;
    desc.requiredFeaturesCount = 1;

    adapter.RequestDevice(&desc, cb.Callback(), cb.MakeUserdata(this));
}

}  // anonymous namespace
