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

#include "dawn/common/Log.h"
#include "dawn/native/d3d12/D3D12Error.h"
#include "dawn/native/d3d12/DeviceD3D12.h"

namespace dawn::native::d3d12 {

// static
Ref<Fence> Fence::Create(Device* device) {
    ComPtr<ID3D12Fence> d3d12Fence;
    HRESULT hr = device->GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_SHARED,
                                                       IID_PPV_ARGS(&d3d12Fence));
    if (FAILED(hr)) {
        dawn::ErrorLog() << "CreateFence failed with error 0x" << std::hex << hr;
        return nullptr;
    }
    return AcquireRef(new Fence(device, std::move(d3d12Fence), ExecutionSerial(0)));
}

// static
Ref<Fence> Fence::CreateFromHandle(Device* device,
                                   HANDLE unownedHandle,
                                   ExecutionSerial fenceValue) {
    HANDLE ownedHandle = nullptr;
    HRESULT hr = ::DuplicateHandle(::GetCurrentProcess(), unownedHandle, ::GetCurrentProcess(),
                                   &ownedHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);
    if (FAILED(hr)) {
        dawn::ErrorLog() << "DuplicateHandle failed with error 0x" << std::hex << hr;
        return nullptr;
    }
    ComPtr<ID3D12Fence> d3d12Fence;
    hr = device->GetD3D12Device()->OpenSharedHandle(ownedHandle, IID_PPV_ARGS(&d3d12Fence));
    if (FAILED(hr)) {
        dawn::ErrorLog() << "CreateFence failed with error 0x" << std::hex << hr;
        ::CloseHandle(ownedHandle);
        return nullptr;
    }
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

ResultOrError<ExecutionSerial> Fence::IncrementAndSignal() {
    DAWN_TRY(CheckHRESULT(
        mDevice->GetCommandQueue()->Signal(mD3D12Fence.Get(), static_cast<UINT64>(mFenceValue) + 1),
        "D3D12 fence signal"));
    mFenceValue++;
    return ExecutionSerial(mFenceValue);
}

HANDLE Fence::GetSharedHandle() {
    if (mSharedHandle == nullptr) {
        HRESULT hr = mDevice->GetD3D12Device()->CreateSharedHandle(
            mD3D12Fence.Get(), nullptr, GENERIC_ALL, nullptr, &mSharedHandle);
        if (FAILED(hr)) {
            dawn::ErrorLog() << "CreateSharedHandle failed with error 0x" << std::hex << hr;
            return nullptr;
        }
    }
    return mSharedHandle;
}

ID3D12Fence* Fence::GetD3D12Fence() const {
    return mD3D12Fence.Get();
}

}  // namespace dawn::native::d3d12
