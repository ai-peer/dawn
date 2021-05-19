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

// D3D12Backend.cpp: contains the definition of symbols exported by D3D12Backend.h so that they
// can be compiled twice: once export (shared library), once not exported (static library)

#include "dawn_native/D3D12Backend.h"

#include "common/Log.h"
#include "common/Math.h"
#include "common/SwapChainUtils.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/NativeSwapChainImplD3D12.h"
#include "dawn_native/d3d12/PlatformFunctions.h"
#include "dawn_native/d3d12/ResidencyManagerD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

namespace dawn_native { namespace d3d12 {

    ComPtr<ID3D12Device> GetD3D12Device(WGPUDevice device) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        return backendDevice->GetD3D12Device();
    }

    DawnSwapChainImplementation CreateNativeSwapChainImpl(WGPUDevice device, HWND window) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        DawnSwapChainImplementation impl;
        impl = CreateSwapChainImplementation(new NativeSwapChainImpl(backendDevice, window));
        impl.textureUsage = WGPUTextureUsage_Present;

        return impl;
    }

    WGPUTextureFormat GetNativeSwapChainPreferredFormat(
        const DawnSwapChainImplementation* swapChain) {
        NativeSwapChainImpl* impl = reinterpret_cast<NativeSwapChainImpl*>(swapChain->userData);
        return static_cast<WGPUTextureFormat>(impl->GetPreferredFormat());
    }

    ExternalImageDescriptorDXGISharedHandle::ExternalImageDescriptorDXGISharedHandle()
        : ExternalImageDescriptor(ExternalImageType::DXGISharedHandle) {
    }

    ExternalImageDXGI::ExternalImageDXGI(ComPtr<ID3D12Resource> d3d12Resource,
                                         ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex,
                                         ComPtr<ID3D11On12Device> d3d11on12Device,
                                         ComPtr<ID3D11DeviceContext2> d3d11On12DeviceContext,
                                         const WGPUTextureDescriptor* descriptor)
        : mD3D12Resource(std::move(d3d12Resource)),
          mDxgiKeyedMutex(std::move(dxgiKeyedMutex)),
          mD3d11On12Device(std::move(d3d11on12Device)),
          mD3d11On12DeviceContext(std::move(d3d11On12DeviceContext)),
          mUsage(descriptor->usage),
          mDimension(descriptor->dimension),
          mSize(descriptor->size),
          mFormat(descriptor->format),
          mMipLevelCount(descriptor->mipLevelCount),
          mSampleCount(descriptor->sampleCount) {
        ASSERT(descriptor->nextInChain == nullptr);
    }

    ExternalImageDXGI::~ExternalImageDXGI() {
        ComPtr<ID3D11Resource> d3d11Resource;
        HRESULT hr = mDxgiKeyedMutex.As(&d3d11Resource);
        if (FAILED(hr)) {
            return;
        }

        ID3D11Resource* d3d11ResourceRaw = d3d11Resource.Get();
        mD3d11On12Device->ReleaseWrappedResources(&d3d11ResourceRaw, 1);

        d3d11Resource.Reset();
        mDxgiKeyedMutex.Reset();

        // 11on12 has a bug where D3D12 resources used only for keyed shared mutexes
        // are not released until work is submitted to the device context and flushed.
        // The most minimal work we can get away with is issuing a TiledResourceBarrier.

        // ID3D11DeviceContext2 is available in Win8.1 and above. This suffices for a
        // D3D12 backend since both D3D12 and 11on12 first appeared in Windows 10.
        mD3d11On12DeviceContext->TiledResourceBarrier(nullptr, nullptr);
        mD3d11On12DeviceContext->Flush();
    }

    WGPUTexture ExternalImageDXGI::ProduceTexture(
        WGPUDevice device,
        const ExternalImageAccessDescriptorDXGIKeyedMutex* descriptor) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        // Ensure the texture usage is allowed
        if (!IsSubset(descriptor->usage, mUsage)) {
            dawn::ErrorLog() << "Texture usage is not valid for external image";
            return nullptr;
        }

        TextureDescriptor textureDescriptor = {};
        textureDescriptor.usage = static_cast<wgpu::TextureUsage>(descriptor->usage);
        textureDescriptor.dimension = static_cast<wgpu::TextureDimension>(mDimension);
        textureDescriptor.size = {mSize.width, mSize.height, mSize.depthOrArrayLayers};
        textureDescriptor.format = static_cast<wgpu::TextureFormat>(mFormat);
        textureDescriptor.mipLevelCount = mMipLevelCount;
        textureDescriptor.sampleCount = mSampleCount;

        Ref<TextureBase> texture = backendDevice->CreateExternalTexture(
            &textureDescriptor, mD3D12Resource, mDxgiKeyedMutex,
            ExternalMutexSerial(descriptor->acquireMutexKey), descriptor->isSwapChainTexture,
            descriptor->isInitialized);
        return reinterpret_cast<WGPUTexture>(texture.Detach());
    }

    // static
    std::unique_ptr<ExternalImageDXGI> ExternalImageDXGI::Create(
        WGPUDevice device,
        const ExternalImageDescriptorDXGISharedHandle* descriptor) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        Microsoft::WRL::ComPtr<ID3D12Resource> d3d12Resource;
        if (FAILED(backendDevice->GetD3D12Device()->OpenSharedHandle(
                descriptor->sharedHandle, IID_PPV_ARGS(&d3d12Resource)))) {
            return nullptr;
        }

        const TextureDescriptor* textureDescriptor =
            reinterpret_cast<const TextureDescriptor*>(descriptor->cTextureDescriptor);

        if (backendDevice->ConsumedError(
                ValidateTextureDescriptor(backendDevice, textureDescriptor))) {
            return nullptr;
        }

        if (backendDevice->ConsumedError(
                ValidateTextureDescriptorCanBeWrapped(textureDescriptor))) {
            return nullptr;
        }

        if (backendDevice->ConsumedError(
                ValidateD3D12TextureCanBeWrapped(d3d12Resource.Get(), textureDescriptor))) {
            return nullptr;
        }

        // Shared handle is assumed to support resource sharing capability. The resource
        // shared capability tier must agree to share resources between D3D devices.
        const Format* format =
            backendDevice->GetInternalFormat(textureDescriptor->format).AcquireSuccess();
        if (format->IsMultiPlanar()) {
            if (backendDevice->ConsumedError(ValidateD3D12VideoTextureCanBeShared(
                    backendDevice, D3D12TextureFormat(textureDescriptor->format)))) {
                return nullptr;
            }
        }

        // We use IDXGIKeyedMutexes to synchronize access between D3D11 and D3D12. D3D11/12 fences
        // are a viable alternative but are, unfortunately, not available on all versions of Windows
        // 10. Since D3D12 does not directly support keyed mutexes, we need to wrap the D3D12
        // resource using 11on12 and QueryInterface the D3D11 representation for the keyed mutex.
        ComPtr<ID3D11Device> d3d11Device;
        ComPtr<ID3D11DeviceContext> d3d11DeviceContext;

        ID3D12Device* d3d12Device = backendDevice->GetD3D12Device();
        IUnknown* const iUnknownQueue = backendDevice->GetCommandQueue().Get();
        if (FAILED(backendDevice->GetFunctions()->d3d11on12CreateDevice(
                d3d12Device, 0, nullptr, 0, &iUnknownQueue, 1, 1, &d3d11Device, &d3d11DeviceContext,
                /*d3dFeatureLevel*/ nullptr))) {
            return nullptr;
        }

        ComPtr<ID3D11On12Device> d3d11on12Device;
        if (FAILED(d3d11Device.As(&d3d11on12Device))) {
            return nullptr;
        }

        ComPtr<ID3D11DeviceContext2> d3d11DeviceContext2;
        if (FAILED(d3d11DeviceContext.As(&d3d11DeviceContext2))) {
            return nullptr;
        }

        ComPtr<ID3D11Texture2D> d3d11Texture;
        D3D11_RESOURCE_FLAGS resourceFlags;
        resourceFlags.BindFlags = 0;
        resourceFlags.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        resourceFlags.CPUAccessFlags = 0;
        resourceFlags.StructureByteStride = 0;
        if (FAILED(d3d11on12Device->CreateWrappedResource(
                d3d12Resource.Get(), &resourceFlags, D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COMMON, IID_PPV_ARGS(&d3d11Texture)))) {
            return nullptr;
        }

        ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex;
        if (FAILED(d3d11Texture.As(&dxgiKeyedMutex))) {
            return nullptr;
        }

        std::unique_ptr<ExternalImageDXGI> result(new ExternalImageDXGI(
            std::move(d3d12Resource), std::move(dxgiKeyedMutex), std::move(d3d11on12Device),
            std::move(d3d11DeviceContext2), descriptor->cTextureDescriptor));
        return result;
    }

    uint64_t SetExternalMemoryReservation(WGPUDevice device,
                                          uint64_t requestedReservationSize,
                                          MemorySegment memorySegment) {
        Device* backendDevice = reinterpret_cast<Device*>(device);

        return backendDevice->GetResidencyManager()->SetExternalMemoryReservation(
            memorySegment, requestedReservationSize);
    }

    AdapterDiscoveryOptions::AdapterDiscoveryOptions(ComPtr<IDXGIAdapter> adapter)
        : AdapterDiscoveryOptionsBase(WGPUBackendType_D3D12), dxgiAdapter(std::move(adapter)) {
    }
}}  // namespace dawn_native::d3d12
