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

#include "dawn/native/D3D12Backend.h"

#include <memory>
#include <utility>

#include "dawn/common/Log.h"
#include "dawn/common/Math.h"
#include "dawn/common/SwapChainUtils.h"
#include "dawn/native/d3d12/D3D11on12Util.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/native/d3d12/ExternalImageResourcesD3D12.h"
#include "dawn/native/d3d12/NativeSwapChainImplD3D12.h"
#include "dawn/native/d3d12/ResidencyManagerD3D12.h"
#include "dawn/native/d3d12/TextureD3D12.h"

namespace dawn::native::d3d12 {

ComPtr<ID3D12Device> GetD3D12Device(WGPUDevice device) {
    return ToBackend(FromAPI(device))->GetD3D12Device();
}

DawnSwapChainImplementation CreateNativeSwapChainImpl(WGPUDevice device, HWND window) {
    Device* backendDevice = ToBackend(FromAPI(device));

    DawnSwapChainImplementation impl;
    impl = CreateSwapChainImplementation(new NativeSwapChainImpl(backendDevice, window));
    impl.textureUsage = WGPUTextureUsage_Present;

    return impl;
}

WGPUTextureFormat GetNativeSwapChainPreferredFormat(const DawnSwapChainImplementation* swapChain) {
    NativeSwapChainImpl* impl = reinterpret_cast<NativeSwapChainImpl*>(swapChain->userData);
    return static_cast<WGPUTextureFormat>(impl->GetPreferredFormat());
}

ExternalImageDescriptorDXGISharedHandle::ExternalImageDescriptorDXGISharedHandle()
    : ExternalImageDescriptor(ExternalImageType::DXGISharedHandle) {}

ExternalImageDXGI::ExternalImageDXGI(ExternalImageResourcesD3D12* resources,
                                     const WGPUTextureDescriptor* descriptor)
    : mResources(resources),
      mUsage(descriptor->usage),
      mDimension(descriptor->dimension),
      mSize(descriptor->size),
      mFormat(descriptor->format),
      mMipLevelCount(descriptor->mipLevelCount),
      mSampleCount(descriptor->sampleCount) {
    ASSERT(mResources != nullptr);
    ASSERT(!descriptor->nextInChain ||
           descriptor->nextInChain->sType == WGPUSType_DawnTextureInternalUsageDescriptor);
    if (descriptor->nextInChain) {
        mUsageInternal =
            reinterpret_cast<const WGPUDawnTextureInternalUsageDescriptor*>(descriptor->nextInChain)
                ->internalUsage;
    }
}

ExternalImageDXGI::~ExternalImageDXGI() {
    AcquireRef(mResources)->Destroy();
}

bool ExternalImageDXGI::IsValid() const {
    return mResources->GetBackendDevice() != nullptr;
}

WGPUTexture ExternalImageDXGI::ProduceTexture(
    WGPUDevice device,
    const ExternalImageAccessDescriptorDXGISharedHandle* descriptor) {
    return ProduceTexture(descriptor);
}

WGPUTexture ExternalImageDXGI::ProduceTexture(
    const ExternalImageAccessDescriptorDXGISharedHandle* descriptor) {
    Device* backendDevice = mResources->GetBackendDevice();
    if (backendDevice == nullptr) {
        dawn::ErrorLog() << "Cannot produce texture from external image after device destruction";
        return nullptr;
    }

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

    DawnTextureInternalUsageDescriptor internalDesc = {};
    if (mUsageInternal) {
        textureDescriptor.nextInChain = &internalDesc;
        internalDesc.internalUsage = static_cast<wgpu::TextureUsage>(mUsageInternal);
        internalDesc.sType = wgpu::SType::DawnTextureInternalUsageDescriptor;
    }

    ComPtr<ID3D12Resource> d3d12Resource = mResources->GetD3D12Resource();
    ComPtr<ID3D12Fence> d3d12Fence = mResources->GetD3D12Fence();

    Ref<D3D11on12ResourceCacheEntry> d3d11on12Resource;
    if (!d3d12Fence) {
        d3d11on12Resource = mResources->GetD3D11on12ResourceCache()->GetOrCreateD3D11on12Resource(
            backendDevice, d3d12Resource.Get());
        if (d3d11on12Resource == nullptr) {
            dawn::ErrorLog() << "Unable to create 11on12 resource for external image";
            return nullptr;
        }
    }

    Ref<TextureBase> texture = backendDevice->CreateD3D12ExternalTexture(
        &textureDescriptor, std::move(d3d12Resource), std::move(d3d12Fence),
        std::move(d3d11on12Resource), descriptor->fenceWaitValue, descriptor->fenceSignalValue,
        descriptor->isSwapChainTexture, descriptor->isInitialized);

    return ToAPI(texture.Detach());
}

// static
std::unique_ptr<ExternalImageDXGI> ExternalImageDXGI::Create(
    WGPUDevice device,
    const ExternalImageDescriptorDXGISharedHandle* descriptor) {
    Device* backendDevice = ToBackend(FromAPI(device));

    Ref<ExternalImageResourcesD3D12> resources =
        backendDevice->CreateExternalImageResources(descriptor);
    if (resources == nullptr) {
        dawn::ErrorLog() << "Unable to acquire D3D12 external image resources";
        return nullptr;
    }

    const TextureDescriptor* textureDescriptor = FromAPI(descriptor->cTextureDescriptor);

    if (backendDevice->ConsumedError(ValidateTextureDescriptor(backendDevice, textureDescriptor))) {
        return nullptr;
    }

    if (backendDevice->ConsumedError(
            ValidateTextureDescriptorCanBeWrapped(textureDescriptor),
            "validating that a D3D12 external image can be wrapped with %s", textureDescriptor)) {
        return nullptr;
    }

    if (backendDevice->ConsumedError(
            ValidateD3D12TextureCanBeWrapped(resources->GetD3D12Resource(), textureDescriptor))) {
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

    std::unique_ptr<ExternalImageDXGI> result(
        new ExternalImageDXGI(resources.Detach(), descriptor->cTextureDescriptor));
    return result;
}

uint64_t SetExternalMemoryReservation(WGPUDevice device,
                                      uint64_t requestedReservationSize,
                                      MemorySegment memorySegment) {
    Device* backendDevice = ToBackend(FromAPI(device));

    return backendDevice->GetResidencyManager()->SetExternalMemoryReservation(
        memorySegment, requestedReservationSize);
}

AdapterDiscoveryOptions::AdapterDiscoveryOptions()
    : AdapterDiscoveryOptionsBase(WGPUBackendType_D3D12), dxgiAdapter(nullptr) {}

AdapterDiscoveryOptions::AdapterDiscoveryOptions(ComPtr<IDXGIAdapter> adapter)
    : AdapterDiscoveryOptionsBase(WGPUBackendType_D3D12), dxgiAdapter(std::move(adapter)) {}
}  // namespace dawn::native::d3d12
