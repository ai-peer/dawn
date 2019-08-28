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

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include "dawn_native/D3D12Backend.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

using Microsoft::WRL::ComPtr;

namespace {

    class D3D12ResourceTestBase : public DawnTest {
      public:
        void SetUp() override {
            DawnTest::SetUp();
            if (UsesWire()) {
                return;
            }

            // Create the D3D11 device/contexts that will be used in subsequent tests
            ComPtr<ID3D12Device> d3d12Device;
            dawn_native::d3d12::GetD3D12Device(device.Get(), &d3d12Device);

            const LUID adapterLuid = d3d12Device->GetAdapterLuid();

            ComPtr<IDXGIFactory4> dxgiFactory;
            HRESULT hr = ::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory));
            ASSERT_EQ(hr, S_OK);

            ComPtr<IDXGIAdapter> dxgiAdapter;
            hr = dxgiFactory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&dxgiAdapter));
            ASSERT_EQ(hr, S_OK);

            ComPtr<ID3D11Device> d3d11Device;
            D3D_FEATURE_LEVEL d3dFeatureLevel;
            ComPtr<ID3D11DeviceContext> d3d11DeviceContext;
            hr = ::D3D11CreateDevice(dxgiAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                                     nullptr, 0, D3D11_SDK_VERSION, &d3d11Device, &d3dFeatureLevel,
                                     &d3d11DeviceContext);
            ASSERT_EQ(hr, S_OK);

            mD3d11Device = std::move(d3d11Device);
            mD3d11DeviceContext = std::move(d3d11DeviceContext);
        }

      protected:
        void WrapSharedHandle(const dawn::TextureDescriptor* descriptor,
                              dawn::Texture* dawnTexture) const {
            D3D11_TEXTURE2D_DESC desc;
            desc.Width = 10;
            desc.Height = 10;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags =
                D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

            ComPtr<ID3D11Texture2D> d3d11Texture;
            HRESULT hr = mD3d11Device->CreateTexture2D(&desc, nullptr, &d3d11Texture);
            ASSERT_EQ(hr, S_OK);

            ComPtr<IDXGIResource1> dxgiResource;
            hr = d3d11Texture.As(&dxgiResource);
            ASSERT_EQ(hr, S_OK);

            HANDLE sharedHandle;
            hr = dxgiResource->CreateSharedHandle(
                nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
                &sharedHandle);
            ASSERT_EQ(hr, S_OK);

            DawnTexture texture = dawn_native::d3d12::WrapSharedHandle(
                device.Get(), reinterpret_cast<const DawnTextureDescriptor*>(descriptor),
                sharedHandle);
            // Now that we've created all of our resources, we can close the handle
            // since we no longer need it.
            ::CloseHandle(sharedHandle);

            *dawnTexture = dawn::Texture::Acquire(texture);
        }

        ComPtr<ID3D11Device> mD3d11Device;
        ComPtr<ID3D11DeviceContext> mD3d11DeviceContext;
    };
}  // anonymous namespace

// A small fixture used to initialize default data for the D3D12Resource validation tests.
// These tests are skipped if the harness is using the wire.
class D3D12ResourceValidationTests : public D3D12ResourceTestBase {
  public:
    void SetUp() override {
        D3D12ResourceTestBase::SetUp();

        descriptor.dimension = dawn::TextureDimension::e2D;
        descriptor.format = dawn::TextureFormat::BGRA8Unorm;
        descriptor.size = {10, 10, 1};
        descriptor.sampleCount = 1;
        descriptor.arrayLayerCount = 1;
        descriptor.mipLevelCount = 1;
        descriptor.usage = dawn::TextureUsageBit::OutputAttachment;
    }

  protected:
    dawn::TextureDescriptor descriptor;
};

// Test a successful wrapping of an D3D12Resource in a texture
TEST_P(D3D12ResourceValidationTests, Success) {
    DAWN_SKIP_TEST_IF(UsesWire());

    dawn::Texture texture;
    WrapSharedHandle(&descriptor, &texture);

    ASSERT_NE(texture.Get(), nullptr);
}

// Test an error occurs if the texture descriptor is invalid
TEST_P(D3D12ResourceValidationTests, InvalidTextureDescriptor) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.nextInChain = this;

    dawn::Texture texture;
    ASSERT_DEVICE_ERROR(WrapSharedHandle(&descriptor, &texture));

    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor mip level count isn't 1
TEST_P(D3D12ResourceValidationTests, InvalidMipLevelCount) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.mipLevelCount = 2;

    dawn::Texture texture;
    ASSERT_DEVICE_ERROR(WrapSharedHandle(&descriptor, &texture));

    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor array layer count isn't 1
TEST_P(D3D12ResourceValidationTests, InvalidArrayLayerCount) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.arrayLayerCount = 2;

    dawn::Texture texture;
    ASSERT_DEVICE_ERROR(WrapSharedHandle(&descriptor, &texture));

    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor sample count isn't 1
TEST_P(D3D12ResourceValidationTests, InvalidSampleCount) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.sampleCount = 4;

    dawn::Texture texture;
    ASSERT_DEVICE_ERROR(WrapSharedHandle(&descriptor, &texture));

    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor width doesn't match the texture's
TEST_P(D3D12ResourceValidationTests, InvalidWidth) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.size.width = 11;

    dawn::Texture texture;
    ASSERT_DEVICE_ERROR(WrapSharedHandle(&descriptor, &texture));

    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor height doesn't match the texture's
TEST_P(D3D12ResourceValidationTests, InvalidHeight) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.size.height = 11;

    dawn::Texture texture;
    ASSERT_DEVICE_ERROR(WrapSharedHandle(&descriptor, &texture));

    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor format isn't compatible with the D3D12 Resource
TEST_P(D3D12ResourceValidationTests, InvalidFormat) {
    DAWN_SKIP_TEST_IF(UsesWire());
    descriptor.format = dawn::TextureFormat::R8Unorm;

    dawn::Texture texture;
    ASSERT_DEVICE_ERROR(WrapSharedHandle(&descriptor, &texture));

    ASSERT_EQ(texture.Get(), nullptr);
}

DAWN_INSTANTIATE_TEST(D3D12ResourceValidationTests, D3D12Backend);
