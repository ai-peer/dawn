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

#ifndef SRC_DAWN_NATIVE_D3D12_EXTERNALIMAGERESOURCESD3D12_H_
#define SRC_DAWN_NATIVE_D3D12_EXTERNALIMAGERESOURCESD3D12_H_

#include <wrl/client.h>

#include <memory>

#include "dawn/common/RefCounted.h"

struct ID3D12Resource;
struct ID3D12Fence;

namespace dawn::native::d3d12 {

class D3D11on12ResourceCache;
class Device;

class ExternalImageResourcesD3D12 : public RefCounted {
  public:
    ExternalImageResourcesD3D12(Device* backendDevice,
                                Microsoft::WRL::ComPtr<ID3D12Resource> d3d12Resource,
                                Microsoft::WRL::ComPtr<ID3D12Fence> d3d12Fence);
    ~ExternalImageResourcesD3D12() override;

    ExternalImageResourcesD3D12(const ExternalImageResourcesD3D12&) = delete;
    ExternalImageResourcesD3D12& operator=(const ExternalImageResourcesD3D12&) = delete;

    void Destroy();

    // Backend device and other resources are set to null after device destruction.
    Device* GetBackendDevice() const { return mBackendDevice; }

    ID3D12Resource* GetD3D12Resource() const { return mD3D12Resource.Get(); }

    ID3D12Fence* GetD3D12Fence() const { return mD3D12Fence.Get(); }

    D3D11on12ResourceCache* GetD3D11on12ResourceCache() const {
        return mD3D11on12ResourceCache.get();
    }

  private:
    Device* mBackendDevice;
    Microsoft::WRL::ComPtr<ID3D12Resource> mD3D12Resource;
    Microsoft::WRL::ComPtr<ID3D12Fence> mD3D12Fence;
    std::unique_ptr<D3D11on12ResourceCache> mD3D11on12ResourceCache;
};

}  // namespace dawn::native::d3d12

#endif  // SRC_DAWN_NATIVE_D3D12_EXTERNALIMAGERESOURCESD3D12_H_
