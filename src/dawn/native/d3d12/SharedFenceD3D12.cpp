// Copyright 2023 The Dawn & Tint Authors
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

#include "dawn/native/d3d12/SharedFenceD3D12.h"

#include <utility>

#include "dawn/native/ChainUtils.h"
#include "dawn/native/D3D12Backend.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d12/DeviceD3D12.h"

namespace dawn::native::d3d12 {
SharedFence::SharedFence(Device* device, const char* label, ComPtr<ID3D12Fence> fence)
    : d3d::SharedFence(device, label), mFence(std::move(fence)) {}

// static
ResultOrError<Ref<SharedFence>> SharedFence::Create(
    Device* device,
    const char* label,
    const SharedFenceDXGISharedHandleDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->handle == nullptr, "shared HANDLE is missing.");

    SystemHandle ownedHandle;
    DAWN_TRY_ASSIGN(ownedHandle, SystemHandle::Duplicate(descriptor->handle));

    Ref<SharedFence> fence = AcquireRef(new SharedFence(device, label, std::move(ownedHandle)));
    DAWN_TRY(CheckHRESULT(device->GetD3D12Device()->OpenSharedHandle(descriptor->handle,
                                                                     IID_PPV_ARGS(&fence->mFence)),
                          "D3D12 fence open shared handle"));
    fence->mType = wgpu::SharedFenceType::DXGISharedHandle;
    return fence;
}

// static
ResultOrError<Ref<SharedFence>> SharedFence::Create(Device* device,
                                                    const char* label,
                                                    ComPtr<ID3D12Fence> d3d12Fence,
                                                    wgpu::SharedFenceType type) {
    Ref<SharedFence> fence;
    switch (type) {
        case wgpu::SharedFenceType::DXGISharedHandle: {
            SystemHandle ownedHandle;
            DAWN_TRY(CheckHRESULT(
                device->GetD3D12Device()->CreateSharedHandle(d3d12Fence.Get(), nullptr, GENERIC_ALL,
                                                             nullptr, ownedHandle.GetMut()),
                "D3D12 create fence handle"));
            DAWN_ASSERT(ownedHandle.IsValid());
            fence = AcquireRef(new SharedFence(device, label, std::move(ownedHandle)));
            fence->mFence = std::move(d3d12Fence);
            fence->mType = wgpu::SharedFenceType::DXGISharedHandle;
        } break;
        case wgpu::SharedFenceType::D3D12Fence: {
            fence = AcquireRef(new SharedFence(device, label, std::move(d3d12Fence)));
            fence->mType = wgpu::SharedFenceType::D3D12Fence;
        } break;
        default:
            DAWN_UNREACHABLE();
    }

    return fence;
}

// static
ResultOrError<Ref<SharedFence>> SharedFence::Create(
    Device* device,
    const char* label,
    const SharedFenceD3D12FenceDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->fence == nullptr, "shared D3D12Fence is missing.");

    ComPtr<ID3D12Fence> d3d12Fence = descriptor->fence;

    ID3D12Device* resourceDevice = nullptr;
    d3d12Fence->GetDevice(__uuidof(*resourceDevice), reinterpret_cast<void**>(&resourceDevice));
    DAWN_INVALID_IF(resourceDevice != device->GetD3D12Device(),
                    "The D3D12 device of the fence and the D3D12 device of %s must be same.",
                    device);
    resourceDevice->Release();

    Ref<SharedFence> fence = AcquireRef(new SharedFence(device, label, std::move(d3d12Fence)));
    fence->mType = wgpu::SharedFenceType::D3D12Fence;
    return fence;
}

void SharedFence::DestroyImpl() {
    ToBackend(GetDevice())->ReferenceUntilUnused(std::move(mFence));
    mFence = nullptr;
}

ID3D12Fence* SharedFence::GetD3DFence() const {
    return mFence.Get();
}

MaybeError SharedFence::ExportInfoImpl(UnpackedPtr<SharedFenceExportInfo>& info) const {
    info->type = mType;

    switch (mType) {
        case wgpu::SharedFenceType::DXGISharedHandle:
            DAWN_TRY(info.ValidateSubset<SharedFenceDXGISharedHandleExportInfo>());
            {
                auto* exportInfo = info.Get<SharedFenceDXGISharedHandleExportInfo>();
                if (exportInfo != nullptr) {
                    exportInfo->handle = mHandle.Get();
                }
            }

            break;
        case wgpu::SharedFenceType::D3D12Fence:
            DAWN_TRY(info.ValidateSubset<SharedFenceD3D12FenceExportInfo>());
            {
                auto* exportInfo = info.Get<SharedFenceD3D12FenceExportInfo>();
                if (exportInfo != nullptr) {
                    exportInfo->fence = mFence.Get();
                }
            }
            break;
        default:
            DAWN_UNREACHABLE();
    }

    return {};
}

}  // namespace dawn::native::d3d12
