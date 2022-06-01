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

#ifndef SRC_DAWN_NATIVE_D3D12_PIPELINECACHED3D12_H_
#define SRC_DAWN_NATIVE_D3D12_PIPELINECACHED3D12_H_

#include "dawn/native/BlobCache.h"

#include "dawn/native/d3d12/d3d12_platform.h"

namespace dawn::native {

class DeviceBase;

namespace d3d12 {

CachedBlob LoadCachedBlob(DeviceBase* device, const CacheKey& key);
void StoreCachedBlob(DeviceBase* device, const CacheKey& key, ComPtr<ID3DBlob> d3dBlob);

}  // namespace d3d12

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_D3D12_PIPELINECACHED3D12_H_
