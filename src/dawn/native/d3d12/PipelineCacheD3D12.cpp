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

#include "dawn/native/d3d12/PipelineCacheD3D12.h"

#include "dawn/common/Assert.h"
#include "dawn/native/Device.h"
#include "dawn/native/Error.h"
#include "dawn/native/Pipeline.h"
#include "dawn/native/d3d12/D3D12Error.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/native/d3d12/Forward.h"

namespace dawn::native {

// static
template <>
CachedBlob CachedBlob::Create(ComPtr<ID3DBlob> blob) {
    // Detach so the deleter callback can "own" the reference
    ID3DBlob* ptr = blob.Detach();
    return CachedBlob(reinterpret_cast<uint8_t*>(ptr->GetBufferPointer()), ptr->GetBufferSize(),
                      [=]() {
                          // Reattach and drop to delete it.
                          ComPtr<ID3DBlob> b;
                          b.Attach(ptr);
                          b = nullptr;
                      });
}

}  // namespace dawn::native

namespace dawn::native::d3d12 {

CachedBlob LoadCachedBlob(DeviceBase* device, const CacheKey& key) {
    BlobCache* blobCache = device->GetBlobCache();
    if (!blobCache) {
        return CachedBlob();
    }
    return blobCache->Load(key);
}

void StoreCachedBlob(DeviceBase* device, const CacheKey& key, ComPtr<ID3DBlob> d3dBlob) {
    if (d3dBlob->GetBufferSize() > 0) {
        BlobCache* blobCache = device->GetBlobCache();
        if (blobCache) {
            blobCache->Store(key, CachedBlob::Create(d3dBlob));
        }
    }
}

}  // namespace dawn::native::d3d12
