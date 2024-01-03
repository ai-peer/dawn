// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/d3d12/SharedBufferMemoryD3D12.h"

#include <utility>
#include "dawn/native/Buffer.h"
#include "dawn/native/ChainUtils.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d/UtilsD3D.h"
#include "dawn/native/d3d12/BufferD3D12.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/native/d3d12/SharedFenceD3D12.h"

namespace dawn::native::d3d12 {

// static
SharedBufferMemory::SharedBufferMemory(Device* device,
                                       const char* label,
                                       SharedBufferMemoryProperties properties,
                                       ComPtr<ID3D12Resource> resource)
    : SharedBufferMemoryBase(device, label, properties) {
    resource->QueryInterface(IID_PPV_ARGS(&mDXGIKeyedMutex));
}

ResultOrError<Ref<BufferBase>> SharedBufferMemory::CreateBufferImpl(
    const UnpackedPtr<BufferDescriptor>& descriptor) {
    // return Buffer::CreateFromSharedBufferMemory(this, descriptor);
    return DAWN_UNIMPLEMENTED_ERROR("Unimplemented");
}

ResultOrError<Ref<SharedFenceBase>> SharedBufferMemory::CreateFenceImpl(
    const SharedFenceDXGISharedHandleDescriptor* desc) {
    return SharedFence::Create(ToBackend(GetDevice()), "Internal shared DXGI fence", desc);
}

ResultOrError<Ref<SharedBufferMemory>> SharedBufferMemory::Create(
    Device* device,
    const char* label,
    const SharedBufferMemoryD3D12ResourceDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->handle == nullptr, "shared HANDLE is missing.");

    ComPtr<ID3D12Resource> d3d12Resource;
    // DAWN_TRY(CheckHRESULT(device->GetD3D12Device()->OpenSharedHandle(descriptor->handle,
    //                                                                IID_PPV_ARGS(&d3d12Resource)),
    //                                                                "D3D12 open shared handle"));

    D3D12_RESOURCE_DESC desc = d3d12Resource->GetDesc();
    DAWN_INVALID_IF(desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER,
                    "Resource dimension (%d) was not Buffer", desc.Dimension);

    const CombinedLimits& limits = device->GetLimits();
    DAWN_INVALID_IF(desc.Width > limits.v1.maxBufferSize,
                    "Resource Width (%u) exceeds maxBufferSize (%u).", desc.Width,
                    limits.v1.maxBufferSize);
    DAWN_INVALID_IF(desc.Height > 1, "Resource Height (%u) exceeds 1.", desc.Height);
    DAWN_INVALID_IF(!(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS),
                    "Resource did not have D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS flag");

    SharedBufferMemoryProperties properties;
    properties.size = desc.Width;

    // The usages that the underlying D3D12 texture supports are partially
    // dependent on its creation flags. Note that the SharedTextureMemory
    // frontend takes care of stripping out any usages that are not supported
    // for `format`.
    wgpu::TextureUsage storageBindingUsage =
        (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
            ? wgpu::TextureUsage::StorageBinding
            : wgpu::TextureUsage::None;

    properties.usage =
        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst | storageBindingUsage;

    auto result =
        AcquireRef(new SharedBufferMemory(device, label, properties, std::move(d3d12Resource)));
    result->Initialize();
    return result;
}

ID3D12Resource* SharedBufferMemory::GetD3DResource() const {
    return mResource.Get();
}

MaybeError SharedBufferMemory::BeginAccessImpl(
    BufferBase* buffer,
    const UnpackedPtr<BeginAccessDescriptor>& descriptor) {
    DAWN_TRY(descriptor.ValidateSubset<>());
    for (size_t i = 0; i < descriptor->fenceCount; ++i) {
        SharedFenceBase* fence = descriptor->fences[i];

        SharedFenceExportInfo exportInfo;
        DAWN_TRY(fence->ExportInfo(&exportInfo));
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

    if (mDXGIKeyedMutex) {
        DAWN_TRY(CheckHRESULT(mDXGIKeyedMutex->AcquireSync(kDXGIKeyedMutexAcquireKey, INFINITE),
                              "Acquire keyed mutex"));
    }

    // Reset state to COMMON. BeginAccess contains a list of fences to wait on after
    // which the texture's usage will complete on the GPU.
    // All textures created from SharedTextureMemory must have
    // flag D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS. All textures with that flag are eligible
    // to decay to COMMON.
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#state-decay-to-common

    // Need to verify state is common

    return {};
}

ResultOrError<FenceAndSignalValue> SharedBufferMemory::EndAccessImpl(
    BufferBase* buffer,
    UnpackedPtr<EndAccessState>& state) {
    DAWN_TRY(state.ValidateSubset<>());
    DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceDXGISharedHandle),
                    "Required feature (%s) is missing.",
                    wgpu::FeatureName::SharedFenceDXGISharedHandle);

    if (mDXGIKeyedMutex) {
        mDXGIKeyedMutex->ReleaseSync(kDXGIKeyedMutexAcquireKey);
    }

    SharedFenceDXGISharedHandleDescriptor desc;
    desc.handle = ToBackend(GetDevice())->GetFenceHandle();

    Ref<SharedFenceBase> fence;
    DAWN_TRY_ASSIGN(fence, CreateFenceImpl(&desc));

    return FenceAndSignalValue{
        std::move(fence),
        static_cast<uint64_t>(buffer->GetSharedBufferMemoryContents()->GetLastUsageSerial())};
}

void SharedBufferMemory::DestroyImpl() {
    ToBackend(GetDevice())->ReferenceUntilUnused(std::move(mResource));
    mResource = nullptr;
}

}  // namespace dawn::native::d3d12
