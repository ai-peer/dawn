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

#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d12/DeviceD3D12.h"

namespace dawn::native::d3d12 {

// static
ResultOrError<Ref<SharedFence>> SharedFence::Create(
    Device* device,
    const char* label,
    const SharedFenceDXGISharedHandleDescriptor* descriptor) {
    DAWN_INVALID_IF(descriptor->handle == nullptr, "shared HANDLE is missing.");

    SystemHandle ownedHandle;
    DAWN_TRY_ASSIGN(ownedHandle, SystemHandle::Duplicate(descriptor->handle));

    return AcquireRef(new SharedFence(device, label, std::move(ownedHandle)));
}

// static
ResultOrError<Ref<SharedFence>> SharedFence::Wrap(Device* device,
                                                  const char* label,
                                                  HANDLE unownedHandle) {
    DAWN_INVALID_IF(unownedHandle == nullptr, "shared HANDLE is missing.");
    return AcquireRef(new SharedFence(device, label, unownedHandle));
}

void SharedFence::DestroyImpl() {
    if (mFence != nullptr) {
        ToBackend(GetDevice())->ReferenceUntilUnused(std::move(mFence));
        mFence = nullptr;
    }
}

ResultOrError<ID3D12Fence*> SharedFence::GetD3DFence() {
    if (mFence == nullptr) {
        DAWN_TRY(CheckHRESULT(ToBackend(GetDevice())
                                  ->GetD3D12Device()
                                  ->OpenSharedHandle(mHandle, IID_PPV_ARGS(&mFence)),
                              "D3D12 fence open shared handle"));
    }
    return mFence.Get();
}

}  // namespace dawn::native::d3d12
