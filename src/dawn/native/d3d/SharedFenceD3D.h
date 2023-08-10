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

#ifndef SRC_DAWN_NATIVE_D3D_SHARED_FENCE_D3D_H_
#define SRC_DAWN_NATIVE_D3D_SHARED_FENCE_D3D_H_

#include "dawn/native/ChainUtils.h"
#include "dawn/native/SharedFence.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d/DeviceD3D.h"
#include "dawn/native/d3d12/d3d12_platform.h"

namespace dawn::native::d3d {

class Device;

template <typename Derived, typename ID3DFenceType>
class SharedFence : public SharedFenceBase {
  public:
    static ResultOrError<Ref<Derived>> Create(
        Device* device,
        const char* label,
        const SharedFenceDXGISharedHandleDescriptor* descriptor) {
        DAWN_INVALID_IF(descriptor->handle == nullptr, "shared HANDLE is missing.");

        HANDLE ownedHandle;
        if (!::DuplicateHandle(::GetCurrentProcess(), descriptor->handle, ::GetCurrentProcess(),
                               &ownedHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            return DAWN_INTERNAL_ERROR("Failed to DuplicateHandle");
        }

        Ref<Derived> fence = AcquireRef(new Derived(device, label, ownedHandle));
        DAWN_TRY_ASSIGN(fence->mD3DFence,
                        static_cast<SharedFence*>(fence.Get())->OpenSharedHandle(ownedHandle));
        return fence;
    }

    ID3DFenceType* GetD3DFence() const { return mD3DFence.Get(); }

    ~SharedFence() override {
        if (mSharedHandle) {
            ::CloseHandle(mSharedHandle);
        }
    }

  private:
    SharedFence(Device* device, const char* label, HANDLE ownedHandle)
        : SharedFenceBase(device, label), mSharedHandle(ownedHandle) {}

    void DestroyImpl() override { mD3DFence = nullptr; }

    MaybeError ExportInfoImpl(SharedFenceExportInfo* info) const override {
        info->type = wgpu::SharedFenceType::DXGISharedHandle;

        DAWN_TRY(ValidateSingleSType(info->nextInChain,
                                     wgpu::SType::SharedFenceDXGISharedHandleExportInfo));

        SharedFenceDXGISharedHandleExportInfo* exportInfo = nullptr;
        FindInChain(info->nextInChain, &exportInfo);

        if (exportInfo != nullptr) {
            exportInfo->handle = mSharedHandle;
        }
        return {};
    }

    virtual ResultOrError<ComPtr<ID3DFenceType>> OpenSharedHandle(HANDLE handle) = 0;

    HANDLE mSharedHandle = nullptr;
    ComPtr<ID3DFenceType> mD3DFence = nullptr;
};

}  // namespace dawn::native::d3d

namespace dawn::native {
namespace d3d11 {
class SharedFence;
}
namespace d3d12 {
class SharedFence;
}

extern template class d3d::SharedFence<d3d11::SharedFence, ID3D11Fence>;
extern template class d3d::SharedFence<d3d12::SharedFence, ID3D12Fence>;

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_D3D_SHARED_FENCE_D3D_H_
