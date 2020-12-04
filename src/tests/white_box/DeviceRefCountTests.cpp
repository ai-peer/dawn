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

#include <gtest/gtest.h>

#include "common/Assert.h"
#include "dawn/dawn_proc.h"
#include "dawn/webgpu.h"
#include "dawn/webgpu_cpp.h"
#include "dawn_native/Device.h"
#include "dawn_native/NullBackend.h"

class DeviceRefCountTest : public testing::Test {
  protected:
    void SetUp() override {
        instance = std::make_unique<dawn_native::Instance>();
        instance->DiscoverDefaultAdapters();

        std::vector<dawn_native::Adapter> adapters = instance->GetAdapters();

        // Validation tests run against the null backend, find the corresponding adapter
        bool foundNullAdapter = false;
        for (auto& currentAdapter : adapters) {
            wgpu::AdapterProperties adapterProperties;
            currentAdapter.GetProperties(&adapterProperties);

            if (adapterProperties.backendType == wgpu::BackendType::Null) {
                adapter = currentAdapter;
                foundNullAdapter = true;
                break;
            }
        }

        ASSERT(foundNullAdapter);

        dawnProcSetProcs(&dawn_native::GetProcs());
    }

    dawn_native::Adapter adapter;
    std::unique_ptr<dawn_native::Instance> instance;
};

// Test that the Device's ref count is 1 on creation.
TEST_F(DeviceRefCountTest, Creation) {
    wgpu::Device device = wgpu::Device::Acquire(adapter.CreateDevice());
    dawn_native::DeviceBase* deviceImpl = reinterpret_cast<dawn_native::DeviceBase*>(device.Get());
    EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 1u);
}

// Test that creating a child object increase's the device's ref count, and decreases it
// when the child is destroyed.
TEST_F(DeviceRefCountTest, CreateChildObject) {
    wgpu::Device device = wgpu::Device::Acquire(adapter.CreateDevice());
    dawn_native::DeviceBase* deviceImpl = reinterpret_cast<dawn_native::DeviceBase*>(device.Get());
    EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 1u);
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 2u);
    }
    EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 1u);
}

// Test that both external and internal references change the Device's ref count
TEST_F(DeviceRefCountTest, ExternalInternalRefs) {
    wgpu::Device device = wgpu::Device::Acquire(adapter.CreateDevice());
    dawn_native::DeviceBase* deviceImpl = reinterpret_cast<dawn_native::DeviceBase*>(device.Get());
    EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 1u);
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 2u);

        {
            wgpu::Device deviceRef = device;
            EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 3u);
        }
        EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 2u);
    }
    EXPECT_EQ(deviceImpl->GetRefCountForTesting(), 1u);
}
