// Copyright 2021 The Dawn Authors
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

#include "dawn_native/d3d12/PipelineCacheD3D12.h"

#include "common/HashUtils.h"
#include "dawn_native/d3d12/AdapterD3D12.h"

namespace dawn_native { namespace d3d12 {

    namespace {
        MaybeError CheckLoadPipelineHRESULT(HRESULT result, const char* context) {
            // If PSO does not exist, LoadPipeline returns E_INVALIDARG. Since a PSO cache miss
            // will cause a new PSO to be created, this error is handled and can be ignored.
            if (result == E_INVALIDARG) {
                return {};
            }
            return CheckHRESULT(result, context);
        }

        MaybeError CheckStorePipelineHRESULT(HRESULT result, const char* context) {
            // If PSO was already stored, StorePipeline returns E_INVALIDARG.
#if defined(_DEBUG)
            // Stored pipelines that contain debug shaders will never match the one stored should
            // these shaders ever be re-compiled. Since a newly created PSO containing debug shaders
            // does not need to be stored, this error is handled and can be ignored.
            if (result == E_INVALIDARG) {
                return {};
            }
#endif

            return CheckHRESULT(result, context);
        }

    }  // anonymous namespace

    // SharedPipelineCache

    SharedPipelineCaches::SharedPipelineCaches(Adapter* adapter) : mAdapter(adapter) {
    }

    SharedPipelineCaches::~SharedPipelineCaches() {
        ASSERT(mCache.empty());
    }

    void SharedPipelineCaches::ResetForTesting() {
        for (auto& entry : mCache) {
            entry->ResetForTesting();
        }
        mCache.clear();
    }

    void SharedPipelineCaches::StorePipelineCacheData(const uint8_t* data,
                                                      size_t size,
                                                      PersistentCache* persistentCache) {
        persistentCache->StoreData(mAdapter->GetPipelineCacheKey(), data, size);
    }

    Ref<dawn_platform::CachedBlob> SharedPipelineCaches::LoadPipelineCacheData(
        PersistentCache* persistentCache) {
        return persistentCache->LoadData(mAdapter->GetPipelineCacheKey());
    }

    ResultOrError<Ref<PipelineCache>> SharedPipelineCaches::GetOrCreate(
        PersistentCache* persistentCache) {
        ComPtr<ID3D12Device> device = mAdapter->GetDevice();

        // Try to use an existing pipeline cache which was created from a cached blob.
        // If no cached blob exists, a empty pipeline cache will be created and it's blob stored
        // back in the cache.
        Ref<dawn_platform::CachedBlob> pipelineCacheBlob =
            persistentCache->LoadData(mAdapter->GetPipelineCacheKey());

        PipelineCache blueprint(/*device*/ nullptr, this, pipelineCacheBlob);
        auto iter = mCache.find(&blueprint);
        if (iter != mCache.end()) {
            return Ref<PipelineCache>(*iter);
        }

        Ref<PipelineCache> pipelineCache;
        DAWN_TRY_ASSIGN(pipelineCache,
                        PipelineCache::Create(std::move(device), this, pipelineCacheBlob));

        // Ensure the pipeline cache data is synced with the cached blob in the persistent cache.
        // If the pipeline cache data cannot be stored in the persistent cache, do not cache the
        // pipeline cache on this adapter. This is because pipelines added from one device's
        // pipeline cache could overwrite another device's pipeline cache if they are not shared.
        const bool didSync = pipelineCache->SyncPipelineCacheData(persistentCache);
        if (didSync) {
            mCache.insert(pipelineCache.Get());
        }

        return std::move(pipelineCache);
    }

    void SharedPipelineCaches::RemoveCacheEntry(PipelineCache* pipelineCache) {
        const size_t removedCount = mCache.erase(pipelineCache);
        ASSERT(removedCount == 1);
    }

    // PipelineCache

    // static
    ResultOrError<Ref<PipelineCache>> PipelineCache::Create(
        ComPtr<ID3D12Device> device,
        SharedPipelineCaches* sharedPipelineCaches,
        Ref<dawn_platform::CachedBlob> pipelineCacheBlob) {
        Ref<PipelineCache> pipelineCache = AcquireRef(new PipelineCache(
            std::move(device), sharedPipelineCaches, std::move(pipelineCacheBlob)));
        DAWN_TRY(pipelineCache->Initialize());
        return std::move(pipelineCache);
    }

    MaybeError PipelineCache::Initialize() {
        // Support must be queried through the d3d device before calling CreatePipelineLibrary.
        // ID3D12PipelineLibrary was introduced since Windows 10 Anniversary Update (WDDM 2.1+).
        ComPtr<ID3D12Device1> device1;
        if (FAILED(mDevice.As(&device1))) {
            return {};
        }

        // If a cached blob exists, create a new library from it by passing it to
        // CreatePipelineLibrary. If no cached blob exists, passing (data=nullptr, size=0) creates
        // an empty library. Creating a library from an existing blob does not require the pipeline
        // cache data to be dirtied since it will be initialized with pipelines from the cached
        // blob.
        size_t librarySize = 0;
        const uint8_t* libraryData = nullptr;
        if (mLibraryBlob.Get()) {
            librarySize = mLibraryBlob->size();
            libraryData = mLibraryBlob->data();
            mIsLibraryBlobDirty = false;
        }

        DAWN_TRY(CheckHRESULT(
            device1->CreatePipelineLibrary(libraryData, librarySize, IID_PPV_ARGS(&mLibrary)),
            "ID3D12Device1::CreatePipelineLibrary"));

        return {};
    }

    PipelineCache::PipelineCache(ComPtr<ID3D12Device> device,
                                 SharedPipelineCaches* sharedPipelineCaches,
                                 Ref<dawn_platform::CachedBlob> pipelineCacheBlob)
        : mDevice(std::move(device)),
          mLibraryBlob(std::move(pipelineCacheBlob)),
          mSharedPipelineCaches(sharedPipelineCaches) {
        ASSERT(mSharedPipelineCaches != nullptr);
    }

    PipelineCache::~PipelineCache() {
        if (mLibraryBlob.Get()) {
            mSharedPipelineCaches->RemoveCacheEntry(this);
        }
    }

    bool PipelineCache::SyncPipelineCacheData(PersistentCache* persistentCache) {
        // Library support is required to save pipelines as a cached blob.
        if (mLibrary == nullptr || !mIsLibraryBlobDirty) {
            return true;
        }

        // Pipeline cache that contains debug shaders does not need to be persistently stored since
        // the shaders contain debug data unique to compilation which will never match the
        // re-compiled shaders used to lookup the pipeline. Since serializing duplicate pipelines is
        // not allowed, we return early.
#if defined(_DEBUG)
        if (mIsLibraryBlobDirty) {
            return false;
        }
#endif

        const size_t librarySize = mLibrary->GetSerializedSize();
        ASSERT(librarySize > 0);

        // TODO(bryan.bernhart@intel.com): Consider to resize and reuse this buffer.
        std::vector<uint8_t> libraryData(librarySize);
        if (SUCCEEDED(mLibrary->Serialize(libraryData.data(), libraryData.size()))) {
            mSharedPipelineCaches->StorePipelineCacheData(libraryData.data(), libraryData.size(),
                                                          persistentCache);
        }

        // Store a ref to the cached blob we stored if there wasn't one already.
        // The ref is needed to lookup this pipeline cache in the shared cache.
        // If there is no ref, the pipeline cache will not be shared by returning false.
        if (mLibraryBlob.Get() == nullptr) {
            mLibraryBlob = mSharedPipelineCaches->LoadPipelineCacheData(persistentCache);
            if (mLibraryBlob.Get() == nullptr) {
                return false;
            }
        }

        mIsLibraryBlobDirty = false;
        return true;
    }

    size_t PipelineCache::HashFunc::operator()(const PipelineCache* entry) const {
        size_t hash = 0;
        HashCombine(&hash, entry->mLibraryBlob.Get());
        return hash;
    }

    bool PipelineCache::EqualityFunc::operator()(const PipelineCache* a,
                                                 const PipelineCache* b) const {
        return a->mLibraryBlob.Get() == b->mLibraryBlob.Get();
    }

    void PipelineCache::ResetForTesting() {
        mSharedPipelineCaches = nullptr;
    }

    size_t PipelineCache::GetHitCountForTesting() const {
        return mHitCountForTesting;
    }

    MaybeError PipelineCache::StorePipeline(LPCWSTR name, ID3D12PipelineState* pso) {
        return CheckStorePipelineHRESULT(mLibrary->StorePipeline(name, pso),
                                         "ID3D12PipelineLibrary::StorePipeline");
    }

    MaybeError PipelineCache::LoadPipeline(LPCWSTR name,
                                           const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                           ComPtr<ID3D12PipelineState>& pso) {
        return CheckLoadPipelineHRESULT(
            mLibrary->LoadGraphicsPipeline(name, &desc, IID_PPV_ARGS(&pso)),
            "ID3D12PipelineLibrary::LoadGraphicsPipeline");
    }

    MaybeError PipelineCache::LoadPipeline(LPCWSTR name,
                                           const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
                                           ComPtr<ID3D12PipelineState>& pso) {
        return CheckLoadPipelineHRESULT(
            mLibrary->LoadComputePipeline(name, &desc, IID_PPV_ARGS(&pso)),
            "ID3D12PipelineLibrary::LoadComputePipeline");
    }

    MaybeError PipelineCache::CreatePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                             ComPtr<ID3D12PipelineState>& pso) {
        return CheckHRESULT(mDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)),
                            "ID3D12Device::CreateGraphicsPipelineState");
    }

    MaybeError PipelineCache::CreatePipeline(D3D12_COMPUTE_PIPELINE_STATE_DESC desc,
                                             ComPtr<ID3D12PipelineState>& pso) {
        return CheckHRESULT(mDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso)),
                            "ID3D12Device::CreateComputePipelineState");
    }
}}  // namespace dawn_native::d3d12