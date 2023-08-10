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

#include "dawn/native/d3d12/SharedFenceD3D12.h"

#include "dawn/native/d3d12/DeviceD3D12.h"

namespace dawn::native::d3d {

template class SharedFence<d3d12::SharedFence, ID3D12Fence>;

}  // namespace dawn::native::d3d

namespace dawn::native::d3d12 {

ResultOrError<ComPtr<ID3D12Fence>> SharedFence::OpenSharedHandle(HANDLE handle) {
    ComPtr<ID3D12Fence> fence;
    DAWN_TRY(CheckHRESULT(
        ToBackend(GetDevice())->GetD3D12Device()->OpenSharedHandle(handle, IID_PPV_ARGS(&fence)),
        "D3D12 fence open shared handle"));
    return fence;
}

}  // namespace dawn::native::d3d12
