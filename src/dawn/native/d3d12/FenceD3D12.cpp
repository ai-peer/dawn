// Copyright 2022 The Dawn Authors
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

#include "dawn/native/d3d12/FenceD3D12.h"

#include <utility>

#include "dawn/common/Log.h"
#include "dawn/native/Error.h"
#include "dawn/native/d3d12/D3D12Error.h"
#include "dawn/native/d3d12/DeviceD3D12.h"

namespace dawn::native::d3d12 {

// static
ResultOrError<Ref<Fence>> Fence::CreateFromHandle(Device* device,
                                                  HANDLE unownedHandle,
                                                  ExecutionSerial fenceValue) {
    ASSERT(unownedHandle != nullptr);
    HANDLE ownedHandle = nullptr;
    if (!::DuplicateHandle(::GetCurrentProcess(), unownedHandle, ::GetCurrentProcess(),
                           &ownedHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        return DAWN_DEVICE_LOST_ERROR("D3D12 fence dup handle");
    }
    ComPtr<ID3D12Fence> d3d12Fence;
    DAWN_TRY_WITH_CLEANUP(CheckHRESULT(device->GetD3D12Device()->OpenSharedHandle(
                                           ownedHandle, IID_PPV_ARGS(&d3d12Fence)),
                                       "D3D12 fence open handle"),
                          { ::CloseHandle(ownedHandle); });
    return AcquireRef(new Fence(device, std::move(d3d12Fence), fenceValue, ownedHandle));
}

Fence::Fence(Device* device,
             ComPtr<ID3D12Fence> d3d12Fence,
             ExecutionSerial fenceValue,
             HANDLE sharedHandle)
    : mDevice(device),
      mD3D12Fence(std::move(d3d12Fence)),
      mFenceValue(fenceValue),
      mSharedHandle(sharedHandle) {}

Fence::~Fence() {
    if (mSharedHandle != nullptr) {
        ::CloseHandle(mSharedHandle);
    }
}

MaybeError Fence::Wait() {
    return CheckHRESULT(
        mDevice->GetCommandQueue()->Wait(mD3D12Fence.Get(), static_cast<UINT64>(mFenceValue)),
        "D3D12 fence wait");
}

}  // namespace dawn::native::d3d12
