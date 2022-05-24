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

#include <vector>

#include "dawn/common/Constants.h"
#include "dawn/common/ityp_array.h"
#include "dawn/native/BindingInfo.h"
#include "dawn/native/BlobCache.h"
#include "dawn/native/PipelineLayout.h"

#include "dawn/native/ObjectBase.h"
#include "dawn/native/PipelineCache.h"

#include "dawn/native/d3d12/d3d12_platform.h"

namespace dawn::native::d3d12 {

class Device;
class ComputePipeline;
class RenderPipeline;

class PipelineCache final : public PipelineCacheBase {
  public:
    static Ref<PipelineCache> Create(DeviceBase* device, const CacheKey& key);

    DeviceBase* GetDevice() const;

    // Directly set cached blob from the d3d blob.
    void SetBlob(ComPtr<ID3DBlob> d3dBlob);

  private:
    explicit PipelineCache(DeviceBase* device, const CacheKey& key);
    ~PipelineCache() override;

    void Initialize();
    MaybeError SerializeToBlobImpl() override;

    DeviceBase* mDevice;
};

}  // namespace dawn::native::d3d12

#endif  // SRC_DAWN_NATIVE_D3D12_PIPELINECACHED3D12_H_
