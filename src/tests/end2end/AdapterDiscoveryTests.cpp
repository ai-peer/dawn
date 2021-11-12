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

#include "common/GPUInfo.h"
#include "common/Platform.h"
#include "dawn/webgpu_cpp.h"
#include "dawn_native/DawnNative.h"
#include "dawn_native/VulkanBackend.h"

#include <gtest/gtest.h>

namespace {

    class AdapterDiscoveryTests : public ::testing::Test {};

    class BackendAdapterDiscoveryTests : public ::testing::TestWithParam<wgpu::BackendType> {};

}  // anonymous namespace

// Test only discovering the SwiftShader adapter
TEST_F(AdapterDiscoveryTests, OnlySwiftShader) {
    dawn_native::Instance instance;

    dawn_native::vulkan::AdapterDiscoveryOptions options;
    options.forceSwiftShader = true;
    instance.DiscoverAdapters(&options);

    for (const auto& adapter : instance.GetAdapters()) {
        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);

        EXPECT_EQ(properties.backendType, wgpu::BackendType::Vulkan);
        EXPECT_EQ(properties.adapterType, wgpu::AdapterType::CPU);
        EXPECT_TRUE(gpu_info::IsSwiftshader(properties.vendorID, properties.deviceID));
    }
}

// Test discovering adapters only on one backend.
TEST_P(BackendAdapterDiscoveryTests, OnlyBackend) {
    dawn_native::Instance instance;

    dawn_native::AdapterDiscoveryOptionsBase options(static_cast<WGPUBackendType>(GetParam()));
    instance.DiscoverAdapters(&options);

    for (const auto& adapter : instance.GetAdapters()) {
        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);

        EXPECT_EQ(properties.backendType, GetParam());
    }
}

// Don't instantiate on OpenGL because that backend can't be trivially
// instantiated without providing getProc to load GL procs.
INSTANTIATE_TEST_SUITE_P(,
                         BackendAdapterDiscoveryTests,
                         ::testing::ValuesIn({wgpu::BackendType::Null, wgpu::BackendType::Vulkan,
                                              wgpu::BackendType::Metal, wgpu::BackendType::D3D12}));
