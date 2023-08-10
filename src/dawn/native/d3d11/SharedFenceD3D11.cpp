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

#include "dawn/native/d3d11/SharedFenceD3D11.h"

#include "dawn/native/d3d11/DeviceD3D11.h"

namespace dawn::native::d3d {

template class SharedFence<d3d11::SharedFence, ID3D11Fence>;

}  // namespace dawn::native::d3d

namespace dawn::native::d3d11 {

ResultOrError<ComPtr<ID3D11Fence>> SharedFence::OpenSharedHandle(HANDLE handle) {
    ComPtr<ID3D11Fence> fence;
    DAWN_TRY(CheckHRESULT(
        ToBackend(GetDevice())->GetD3D11Device5()->OpenSharedFence(handle, IID_PPV_ARGS(&fence)),
        "D3D11 fence open shared handle"));
    return fence;
}

}  // namespace dawn::native::d3d11
