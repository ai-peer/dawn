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
#include "dawn/native/d3d12/ComputePipelineD3D12.h"
#include "dawn/native/d3d12/D3D12Error.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/native/d3d12/Forward.h"
#include "dawn/native/d3d12/RenderPipelineD3D12.h"

namespace dawn::native::d3d12 {

// static
Ref<PipelineCache> PipelineCache::Create(DeviceBase* device, const CacheKey& key) {
    Ref<PipelineCache> cache = AcquireRef(new PipelineCache(device, key));
    cache->Initialize();
    return cache;
}

PipelineCache::PipelineCache(DeviceBase* device, const CacheKey& key)
    : PipelineCacheBase(device->GetBlobCache(), key), mDevice(device) {}

PipelineCache::~PipelineCache() = default;

DeviceBase* PipelineCache::GetDevice() const {
    return mDevice;
}

const void* PipelineCache::GetCachedBlobPointer() const {
    return mBlob.Data();
}

size_t PipelineCache::GetCachedBlobSizeInBytes() const {
    return mBlob.Size();
}

ResultOrError<CachedBlob> PipelineCache::SerializeToBlobImpl() {
    // XOR, one and only one of mComputePipeline and mRenderPipeline should be set
    DAWN_ASSERT((mComputePipeline == nullptr) != (mRenderPipeline == nullptr));
    ComPtr<ID3DBlob> d3dBlob;
    if (mComputePipeline) {
        DAWN_TRY(CheckHRESULT(mComputePipeline->GetPipelineState()->GetCachedBlob(&d3dBlob),
                              "D3D12 compute pipeline state get cached blob"));
    } else {
        DAWN_TRY(CheckHRESULT(mRenderPipeline->GetPipelineState()->GetCachedBlob(&d3dBlob),
                              "D3D12 render pipeline state get cached blob"));
    }

    if (d3dBlob->GetBufferSize() != 0) {
        mBlob.Reset(d3dBlob->GetBufferSize());
        memcpy(mBlob.Data(), d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize());
    }

    CachedBlob blob(mBlob.Data(), mBlob.Size());
    return blob;
}

void PipelineCache::Initialize() {
    mBlob = PipelineCacheBase::Initialize();
}

}  // namespace dawn::native::d3d12
