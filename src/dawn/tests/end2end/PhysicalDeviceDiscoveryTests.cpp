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
#include <utility>

#include "dawn/common/GPUInfo.h"
#include "dawn/native/DawnNative.h"
#include "dawn/webgpu_cpp.h"

#if defined(DAWN_ENABLE_BACKEND_VULKAN)
#include "dawn/native/VulkanBackend.h"
#endif  // defined(DAWN_ENABLE_BACKEND_VULKAN)

#if defined(DAWN_ENABLE_BACKEND_D3D11)
#include "dawn/native/D3D11Backend.h"
#endif  // defined(DAWN_ENABLE_BACKEND_D3D11)

#if defined(DAWN_ENABLE_BACKEND_D3D12)
#include "dawn/native/D3D12Backend.h"
#endif  // defined(DAWN_ENABLE_BACKEND_D3D12)

#if defined(DAWN_ENABLE_BACKEND_METAL)
#include "dawn/native/MetalBackend.h"
#endif  // defined(DAWN_ENABLE_BACKEND_METAL)

#if defined(DAWN_ENABLE_BACKEND_DESKTOP_GL) || defined(DAWN_ENABLE_BACKEND_OPENGLES)
#include "GLFW/glfw3.h"
#include "dawn/native/OpenGLBackend.h"
#endif  // defined(DAWN_ENABLE_BACKEND_DESKTOP_GL) || defined(DAWN_ENABLE_BACKEND_OPENGLES)

#include <gtest/gtest.h>

namespace dawn {
namespace {

class AdapterEnumerationTests : public ::testing::Test {};

// Test only enumerating the fallback adapters
TEST(AdapterEnumerationTests, OnlyFallback) {
    native::Instance instance;

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.forceFallbackAdapter = true;

    const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
    for (const auto& adapter : adapters) {
        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);

        EXPECT_EQ(properties.backendType, wgpu::BackendType::Vulkan);
        EXPECT_EQ(properties.adapterType, wgpu::AdapterType::CPU);
        EXPECT_TRUE(gpu_info::IsGoogleSwiftshader(properties.vendorID, properties.deviceID));
    }
}

// Test enumerating only Vulkan physical devices
TEST(AdapterEnumerationTests, OnlyVulkan) {
    native::Instance instance;

    wgpu::RequestAdapterOptionsBackendType backendTypeOptions = {};
    backendTypeOptions.backendType = wgpu::BackendType::Vulkan;

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.nextInChain = &backendTypeOptions;

    const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
    for (const auto& adapter : adapters) {
        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);

        EXPECT_EQ(properties.backendType, wgpu::BackendType::Vulkan);
    }
}

// Test enumerating only D3D11 physical devices
TEST(AdapterEnumerationTests, OnlyD3D11) {
    native::Instance instance;

    wgpu::RequestAdapterOptionsBackendType backendTypeOptions = {};
    backendTypeOptions.backendType = wgpu::BackendType::D3D11;

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.nextInChain = &backendTypeOptions;

    const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
    for (const auto& adapter : adapters) {
        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);

        EXPECT_EQ(properties.backendType, wgpu::BackendType::D3D11);
    }
}

#if defined(DAWN_ENABLE_BACKEND_D3D11)
// Test enumerating a D3D11 physical device from a prexisting DXGI adapter
TEST(AdapterEnumerationTests, MatchingDXGIAdapterD3D11) {
    using Microsoft::WRL::ComPtr;

    ComPtr<IDXGIFactory4> dxgiFactory;
    HRESULT hr = ::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
    ASSERT_EQ(hr, S_OK);

    for (uint32_t adapterIndex = 0;; ++adapterIndex) {
        ComPtr<IDXGIAdapter1> dxgiAdapter = nullptr;
        if (dxgiFactory->EnumAdapters1(adapterIndex, &dxgiAdapter) == DXGI_ERROR_NOT_FOUND) {
            break;  // No more adapters to enumerate.
        }

        native::Instance instance;

        native::d3d::RequestAdapterOptionsIDXGIAdapter dxgiAdapterOptions = {};
        dxgiAdapterOptions.dxgiAdapter = std::move(dxgiAdapter);

        wgpu::RequestAdapterOptionsBackendType backendTypeOptions = {};
        backendTypeOptions.backendType = wgpu::BackendType::D3D11;

        wgpu::RequestAdapterOptions adapterOptions = {};
        adapterOptions.nextInChain = &backendTypeOptions;
        backendTypeOptions.chain.next = &dxgiAdapterOptions;

        const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
        for (const auto& adapter : adapters) {
            wgpu::AdapterProperties properties;
            adapter.GetProperties(&properties);

            EXPECT_EQ(properties.backendType, wgpu::BackendType::D3D11);
        }
    }
}
#endif  // defined(DAWN_ENABLE_BACKEND_D3D11)

// Test enumerating only D3D12 physical devices
TEST(AdapterEnumerationTests, OnlyD3D12) {
    native::Instance instance;

    wgpu::RequestAdapterOptionsBackendType backendTypeOptions = {};
    backendTypeOptions.backendType = wgpu::BackendType::D3D12;

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.nextInChain = &backendTypeOptions;

    const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
    for (const auto& adapter : adapters) {
        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);

        EXPECT_EQ(properties.backendType, wgpu::BackendType::D3D12);
    }
}

#if defined(DAWN_ENABLE_BACKEND_D3D12)
// Test enumerating a D3D12 physical device from a prexisting DXGI adapter
TEST(AdapterEnumerationTests, MatchingDXGIAdapterD3D12) {
    using Microsoft::WRL::ComPtr;

    ComPtr<IDXGIFactory4> dxgiFactory;
    HRESULT hr = ::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
    ASSERT_EQ(hr, S_OK);

    for (uint32_t adapterIndex = 0;; ++adapterIndex) {
        ComPtr<IDXGIAdapter1> dxgiAdapter = nullptr;
        if (dxgiFactory->EnumAdapters1(adapterIndex, &dxgiAdapter) == DXGI_ERROR_NOT_FOUND) {
            break;  // No more adapters to enumerate.
        }

        native::Instance instance;

        native::d3d::RequestAdapterOptionsIDXGIAdapter dxgiAdapterOptions = {};
        dxgiAdapterOptions.dxgiAdapter = std::move(dxgiAdapter);

        wgpu::RequestAdapterOptionsBackendType backendTypeOptions = {};
        backendTypeOptions.backendType = wgpu::BackendType::D3D12;

        wgpu::RequestAdapterOptions adapterOptions = {};
        adapterOptions.nextInChain = &backendTypeOptions;
        backendTypeOptions.chain.next = &dxgiAdapterOptions;

        const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
        for (const auto& adapter : adapters) {
            wgpu::AdapterProperties properties;
            adapter.GetProperties(&properties);

            EXPECT_EQ(properties.backendType, wgpu::BackendType::D3D12);
        }
    }
}
#endif  // defined(DAWN_ENABLE_BACKEND_D3D12)

// Test enumerating only Metal physical devices
TEST(AdapterEnumerationTests, OnlyMetal) {
    native::Instance instance;

    wgpu::RequestAdapterOptionsBackendType backendTypeOptions = {};
    backendTypeOptions.backendType = wgpu::BackendType::Metal;

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.nextInChain = &backendTypeOptions;

    const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
    for (const auto& adapter : adapters) {
        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);

        EXPECT_EQ(properties.backendType, wgpu::BackendType::Metal);
    }
}

// Test enumerating the Metal backend, then the Vulkan backend
// does not duplicate physical devices.
TEST(AdapterEnumerationTests, OneBackendThenTheOther) {
    wgpu::RequestAdapterOptionsBackendType backendTypeOptions = {};
    backendTypeOptions.backendType = wgpu::BackendType::Metal;

    wgpu::RequestAdapterOptions adapterOptions = {};
    adapterOptions.nextInChain = &backendTypeOptions;

    native::Instance instance;

    // Enumerate metal adapters. We should only see metal adapters.
    uint32_t metalAdapterCount = 0;
    {
        const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
        metalAdapterCount = adapters.size();
        for (const auto& adapter : adapters) {
            wgpu::AdapterProperties properties;
            adapter.GetProperties(&properties);

            ASSERT_EQ(properties.backendType, wgpu::BackendType::Metal);
        }
    }
    // Enumerate vulkan adapters. We should only see vulkan adapters.
    {
        backendTypeOptions.backendType = wgpu::BackendType::Vulkan;

        const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
        for (const auto& adapter : adapters) {
            wgpu::AdapterProperties properties;
            adapter.GetProperties(&properties);

            ASSERT_EQ(properties.backendType, wgpu::BackendType::Vulkan);
        }
    }

    // Enumerate metal adapters. We should see the same number of metal adapters.
    {
        backendTypeOptions.backendType = wgpu::BackendType::Metal;

        const auto& adapters = instance.EnumerateAdapters(&adapterOptions);
        uint32_t metalAdapterCount2 = adapters.size();
        for (const auto& adapter : adapters) {
            wgpu::AdapterProperties properties;
            adapter.GetProperties(&properties);

            ASSERT_EQ(properties.backendType, wgpu::BackendType::Metal);
        }
        EXPECT_EQ(metalAdapterCount, metalAdapterCount2);
    }
}

}  // anonymous namespace
}  // namespace dawn
