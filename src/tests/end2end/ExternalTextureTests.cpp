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

#include "tests/DawnTest.h"

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "dawn_native/D3D12Backend.h"

using Microsoft::WRL::ComPtr;

namespace {
    class ExternalTextureTestD3D12 : public DawnTest {
      public:
        void SetUp() override {
            DawnTest::SetUp();
            if (UsesWire()) {
                return;
            }

            // Create the D3D11 device/contexts that will be used in subsequent tests
            ComPtr<ID3D12Device> d3d12Device = dawn_native::d3d12::GetD3D12Device(device.Get());

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

            baseDawnDescriptor.dimension = wgpu::TextureDimension::e2D;
            baseDawnDescriptor.format = wgpu::TextureFormat::RGBA8Unorm;
            baseDawnDescriptor.size = {kTestWidth, kTestHeight, 1};
            baseDawnDescriptor.sampleCount = 1;
            baseDawnDescriptor.mipLevelCount = 1;
            baseDawnDescriptor.usage = wgpu::TextureUsage::Sampled | wgpu::TextureUsage::CopySrc;

            baseD3dDescriptor.Width = kTestWidth;
            baseD3dDescriptor.Height = kTestHeight;
            baseD3dDescriptor.MipLevels = 1;
            baseD3dDescriptor.ArraySize = 1;
            baseD3dDescriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            baseD3dDescriptor.SampleDesc.Count = 1;
            baseD3dDescriptor.SampleDesc.Quality = 0;
            baseD3dDescriptor.Usage = D3D11_USAGE_DEFAULT;
            baseD3dDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            baseD3dDescriptor.CPUAccessFlags = 0;
            baseD3dDescriptor.MiscFlags =
                D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        }

      protected:
        void WrapSharedHandle(const wgpu::TextureDescriptor* dawnDesc,
                              const D3D11_TEXTURE2D_DESC* baseD3dDescriptor,
                              wgpu::Texture* dawnTexture,
                              ID3D11Texture2D** d3d11TextureOut,
                              std::unique_ptr<dawn_native::d3d12::ExternalImageDXGI>*
                                  externalImageOut = nullptr) const {
            ComPtr<ID3D11Texture2D> d3d11Texture;
            HRESULT hr = mD3d11Device->CreateTexture2D(baseD3dDescriptor, nullptr, &d3d11Texture);
            ASSERT_EQ(hr, S_OK);

            ComPtr<IDXGIResource1> dxgiResource;
            hr = d3d11Texture.As(&dxgiResource);
            ASSERT_EQ(hr, S_OK);

            HANDLE sharedHandle;
            hr = dxgiResource->CreateSharedHandle(
                nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
                &sharedHandle);
            ASSERT_EQ(hr, S_OK);

            dawn_native::d3d12::ExternalImageDescriptorDXGISharedHandle externalImageDesc;
            externalImageDesc.cTextureDescriptor =
                reinterpret_cast<const WGPUTextureDescriptor*>(dawnDesc);
            externalImageDesc.sharedHandle = sharedHandle;

            std::unique_ptr<dawn_native::d3d12::ExternalImageDXGI> externalImage =
                dawn_native::d3d12::ExternalImageDXGI::Create(device.Get(), &externalImageDesc);

            // Now that we've created all of our resources, we can close the handle
            // since we no longer need it.
            ::CloseHandle(sharedHandle);

            // Cannot access a non-existent external image (ex. validation error).
            if (externalImage == nullptr) {
                return;
            }

            dawn_native::d3d12::ExternalImageAccessDescriptorDXGIKeyedMutex externalAccessDesc;
            externalAccessDesc.acquireMutexKey = 0;
            externalAccessDesc.usage = static_cast<WGPUTextureUsageFlags>(dawnDesc->usage);

            *dawnTexture = wgpu::Texture::Acquire(
                externalImage->ProduceTexture(device.Get(), &externalAccessDesc));
            *d3d11TextureOut = d3d11Texture.Detach();

            if (externalImageOut != nullptr) {
                *externalImageOut = std::move(externalImage);
            }
        }

        static constexpr size_t kTestWidth = 10;
        static constexpr size_t kTestHeight = 10;

        ComPtr<ID3D11Device> mD3d11Device;
        ComPtr<ID3D11DeviceContext> mD3d11DeviceContext;

        D3D11_TEXTURE2D_DESC baseD3dDescriptor;
        wgpu::TextureDescriptor baseDawnDescriptor;
    };

    // TODO
    class ExternalTextureTestVulkan : public DawnTest {};

    // TODO
    class ExternalTextureTestMetal : public DawnTest {};
}  // anonymous namespace

TEST_P(ExternalTextureTestD3D12, ImportExternalTextureSuccess) {
    DAWN_SKIP_TEST_IF(UsesWire());

    wgpu::Texture texture;
    ComPtr<ID3D11Texture2D> d3d11Texture;
    WrapSharedHandle(&baseDawnDescriptor, &baseD3dDescriptor, &texture, &d3d11Texture);

    ASSERT_NE(texture.Get(), nullptr);

    // Create a texture view for the external texture
    wgpu::TextureView externalTextureView = texture.CreateView();

    // Create an ExternalTextureDescriptor from the texture view
    wgpu::ExternalTextureDescriptor externalDesc;
    externalDesc.externalTextureView = externalTextureView;

    // Import the external texture
    wgpu::ExternalTexture externalTexture = device.ImportExternalTexture(&externalDesc);

    ASSERT_NE(externalTexture.Get(), nullptr);
}

DAWN_INSTANTIATE_TEST(ExternalTextureTestD3D12, D3D12Backend());
DAWN_INSTANTIATE_TEST(ExternalTextureTestVulkan, VulkanBackend());
DAWN_INSTANTIATE_TEST(ExternalTextureTestMetal, MetalBackend());