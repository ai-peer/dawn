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

#include "dawn/native/d3d12/ExternalImageResourcesD3D12.h"

#include <d3d12.h>

#include <utility>

#include "dawn/native/d3d12/D3D11on12Util.h"
#include "dawn/native/d3d12/DeviceD3D12.h"

namespace dawn::native::d3d12 {

ExternalImageResourcesD3D12::ExternalImageResourcesD3D12(
    Device* backendDevice,
    Microsoft::WRL::ComPtr<ID3D12Resource> d3d12Resource,
    Microsoft::WRL::ComPtr<ID3D12Fence> d3d12Fence)
    : mBackendDevice(backendDevice),
      mD3D12Resource(std::move(d3d12Resource)),
      mD3D12Fence(std::move(d3d12Fence)),
      mD3D11on12ResourceCache(std::make_unique<D3D11on12ResourceCache>()) {
    ASSERT(mBackendDevice != nullptr);
    ASSERT(mD3D12Resource != nullptr);
}

ExternalImageResourcesD3D12::~ExternalImageResourcesD3D12() {
    Destroy();
}

void ExternalImageResourcesD3D12::Destroy() {
    if (mBackendDevice != nullptr) {
        mD3D11on12ResourceCache.reset();
        mD3D12Fence.Reset();
        mD3D12Resource.Reset();
        mBackendDevice->ReleaseExternalImageResources(this);
        mBackendDevice = nullptr;
    }
}

}  // namespace dawn::native::d3d12
