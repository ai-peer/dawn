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
//     : PipelineCacheBase(device->GetBlobCache(), key), mDevice(device), mPipeline(pipeline) {}

PipelineCache::~PipelineCache() {
    // if (mHandle == VK_NULL_HANDLE) {
    //     return;
    // }
    // Device* device = ToBackend(GetDevice());
    // device->fn.DestroyPipelineCache(device->GetVkDevice(), mHandle, nullptr);
    // mHandle = VK_NULL_HANDLE;
}

DeviceBase* PipelineCache::GetDevice() const {
    return mDevice;
}

const CachedBlob* PipelineCache::GetBlob() const {
    return &mBlob;
}

// ID3DBlob* PipelineCache::GetBlob() const {
//     return mD3DBlob.Get();
// }

// ComPtr<ID3DBlob>& PipelineCache::GetBlobObject() {
//     return mD3DBlob;
// }
// ComPtr<ID3DBlob>& PipelineCache::GetBlobObject() {
//     return mD3DBlob;
// }

ResultOrError<CachedBlob> PipelineCache::SerializeToBlobImpl() {
    // CachedBlob blob;
    // if (mCachedPSO.pCachedBlob == nullptr) {

    // if (mD3DBlob->GetBufferPointer() == nullptr || mD3DBlob->GetBufferSize() == 0) {

    // if (mBlob.Empty()) {
    //     // return mBlob;
    //     CachedBlob emptyBlob;
    //     return emptyBlob;
    //     // return blob;
    // }

    // size_t bufferSize;
    // Device* device = ToBackend(GetDevice());
    // DAWN_TRY(CheckVkSuccess(
    //     device->fn.GetPipelineCacheData(device->GetVkDevice(), mHandle, &bufferSize, nullptr),
    //     "GetPipelineCacheData"));

    // Device* device = ToBackend(GetDevice());

    // device-> pipelineState->GetCachedBlob();

    // ComPtr<ID3DBlob> cacheBlob;

    // Pipeline* device = ToBackend(mPipeline);
    // ID3D12PipelineState* pipelineState = mPipeline->GetPipelineState();

    // temp
    // Can probably move this to outside (compute Handle)

    // //GetCachedBlob
    // ID3D12PipelineState* pipelineState =
    //     reinterpret_cast<const ComputePipeline*>(mPipeline)->GetPipelineState();

    // DAWN_TRY(CheckHRESULT(
    //     pipelineState->GetCachedBlob(&cacheBlob),
    //     "D3D12 pipeline state get cached blob"));

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

    // mCachedPSO.pCachedBlob = mD3DBlob->GetBufferPointer();
    // mCachedPSO.CachedBlobSizeInBytes = mD3DBlob->GetBufferSize();

    // TODO: add compiled D3D12_SHADER_BYTECODE ???

    // CachedBlob blob(mCachedPSO.CachedBlobSizeInBytes);

    // CachedBlob blob(d3dBlob.Get());
    // return blob;

    if (d3dBlob->GetBufferSize() != 0) {
        mBlob.Reset(d3dBlob->GetBufferSize());
        memcpy(mBlob.Data(), d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize());
    }
    // return mBlob;

    // TODO: change this to return unique_ptr etc.
    CachedBlob blob(mBlob);
    return blob;
}

void PipelineCache::Initialize() {
    // CachedBlob blob = PipelineCacheBase::Initialize();
    mBlob = PipelineCacheBase::Initialize();

    // ??? copy into d3dblob

    // D3DCreateBlob(blob.Size(), &mD3DBlob);

    // // copy to d3d blob
    // // dawn try checkhrresult?
    // memcpy(mD3DBlob->GetBufferPointer(), blob.Data(), blob.Size());

    // // // ????? pointer lifetime
    // // // turn blob to ID3DBlob
    // mCachedPSO.pCachedBlob = blob.Data();
    // mCachedPSO.CachedBlobSizeInBytes = blob.Size();

    // mCachedPSO.pCachedBlob = mD3DBlob->GetBufferPointer();
    // mCachedPSO.CachedBlobSizeInBytes = mD3DBlob->GetBufferSize();

    // VkPipelineCacheCreateInfo createInfo;
    // createInfo.flags = 0;
    // createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    // createInfo.pNext = nullptr;
    // createInfo.initialDataSize = blob.Size();
    // createInfo.pInitialData = blob.Data();

    // Device* device = ToBackend(GetDevice());
    // mHandle = VK_NULL_HANDLE;
    // GetDevice()->ConsumedError(CheckVkSuccess(
    //     device->fn.CreatePipelineCache(device->GetVkDevice(), &createInfo, nullptr, &*mHandle),
    //     "CreatePipelineCache"));
}

}  // namespace dawn::native::d3d12
