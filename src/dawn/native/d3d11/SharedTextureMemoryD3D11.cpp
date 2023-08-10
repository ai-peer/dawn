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

#include "dawn/native/d3d11/SharedTextureMemoryD3D11.h"

#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d/UtilsD3D.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/SharedFenceD3D11.h"
#include "dawn/native/d3d11/TextureD3D11.h"

namespace dawn::native::d3d11 {

// static
ResultOrError<Ref<SharedTextureMemory>> SharedTextureMemory::Create(
    Device* device,
    const char* label,
    const SharedTextureMemoryDXGISharedHandleDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->handle == nullptr, "shared HANDLE is missing.");

    ComPtr<ID3D11Resource> d3d11Resource;
    DAWN_TRY(CheckHRESULT(device->GetD3D11Device5()->OpenSharedResource1(
                              descriptor->handle, IID_PPV_ARGS(&d3d11Resource)),
                          "D3D11 open shared handle"));

    D3D11_RESOURCE_DIMENSION type;
    d3d11Resource->GetType(&type);
    DAWN_INVALID_IF(type != D3D11_RESOURCE_DIMENSION_TEXTURE2D,
                    "Resource type (%d) was not Texture2D", type);

    ComPtr<ID3D11Texture2D> d3d11Texture;
    DAWN_TRY(
        CheckHRESULT(d3d11Resource->QueryInterface(IID_PPV_ARGS(&d3d11Texture)), "QueryInterface"));

    D3D11_TEXTURE2D_DESC desc;
    d3d11Texture->GetDesc(&desc);
    DAWN_INVALID_IF(desc.ArraySize != 1, "Resource ArraySize (%d) was not 1", desc.ArraySize);
    DAWN_INVALID_IF(desc.MipLevels != 1, "Resource MipLevels (%d) was not 1", desc.MipLevels);
    DAWN_INVALID_IF(desc.SampleDesc.Count != 1, "Resource SampleDesc.Count (%d) was not 1",
                    desc.SampleDesc.Count);

    const CombinedLimits& limits = device->GetLimits();
    DAWN_INVALID_IF(desc.Width > limits.v1.maxTextureDimension2D,
                    "Resource Width (%u) exceeds maxTextureDimension2D (%u).", desc.Width,
                    limits.v1.maxTextureDimension2D);
    DAWN_INVALID_IF(desc.Height > limits.v1.maxTextureDimension2D,
                    "Resource Height (%u) exceeds maxTextureDimension2D (%u).", desc.Height,
                    limits.v1.maxTextureDimension2D);

    SharedTextureMemoryProperties properties;
    properties.size = {static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height), 1};

    DAWN_TRY_ASSIGN(properties.format, d3d::FromUncompressedColorDXGITextureFormat(desc.Format));

    const Format* internalFormat = nullptr;
    DAWN_TRY_ASSIGN(internalFormat, device->GetInternalFormat(properties.format));

    wgpu::TextureUsage textureBindingUsage = (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
                                                 ? wgpu::TextureUsage::TextureBinding
                                                 : wgpu::TextureUsage::None;

    wgpu::TextureUsage storageBindingUsage =
        internalFormat->supportsStorageUsage && (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
            ? wgpu::TextureUsage::StorageBinding
            : wgpu::TextureUsage::None;

    wgpu::TextureUsage renderAttachmentUsage =
        internalFormat->isRenderable && (desc.BindFlags & D3D11_BIND_RENDER_TARGET)
            ? wgpu::TextureUsage::RenderAttachment
            : wgpu::TextureUsage::None;

    if (internalFormat->IsMultiPlanar()) {
        properties.usage = wgpu::TextureUsage::CopySrc | textureBindingUsage;
    } else {
        properties.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                           textureBindingUsage | storageBindingUsage | renderAttachmentUsage;
    }

    Ref<SharedTextureMemory> memory =
        AcquireRef(new SharedTextureMemory(device, label, properties));
    memory->mD3D11Resource = std::move(d3d11Resource);
    return memory;
}

void SharedTextureMemory::DestroyImpl() {
    mD3D11Resource = nullptr;
}

ID3D11Resource* SharedTextureMemory::GetD3D11Resource() const {
    return mD3D11Resource.Get();
}

ResultOrError<Ref<TextureBase>> SharedTextureMemory::CreateTextureImpl(
    const TextureDescriptor* descriptor) {
    return Texture::CreateFromSharedTextureMemory(this, descriptor);
}

MaybeError SharedTextureMemory::BeginAccessImpl(TextureBase* texture,
                                                const BeginAccessDescriptor* descriptor) {
    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        SharedFenceBase* fence = descriptor->fences[i];

        SharedFenceExportInfo exportInfo;
        fence->APIExportInfo(&exportInfo);
        switch (exportInfo.type) {
            case wgpu::SharedFenceType::DXGISharedHandle:
                DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceDXGISharedHandle),
                                "Required feature (%s) for %s is missing.",
                                wgpu::FeatureName::SharedFenceDXGISharedHandle,
                                wgpu::SharedFenceType::DXGISharedHandle);
                break;
            default:
                return DAWN_VALIDATION_ERROR("Unsupported fence type %s.", exportInfo.type);
        }
    }
    return {};
}

ResultOrError<FenceAndSignalValue> SharedTextureMemory::EndAccessImpl(TextureBase* texture) {
    DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceDXGISharedHandle),
                    "Required feature (%s) is missing.",
                    wgpu::FeatureName::SharedFenceDXGISharedHandle);

    SharedFenceDXGISharedHandleDescriptor desc;
    desc.handle = ToBackend(GetDevice())->GetFenceHandle();

    Ref<SharedFence> fence;
    DAWN_TRY_ASSIGN(
        fence, SharedFence::Create(ToBackend(GetDevice()), "Internal shared DXGI fence", &desc));

    return FenceAndSignalValue{std::move(fence), static_cast<uint64_t>(GetLastUsageSerial())};
}

}  // namespace dawn::native::d3d11
