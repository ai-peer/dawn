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

#ifndef DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_
#define DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_

#include "dawn_native/PersistentCache.h"
#include "dawn_native/d3d12/D3D12Error.h"

#include "dawn_native/d3d12/d3d12_platform.h"

#include <unordered_set>

namespace dawn_native { namespace d3d12 {

    class Adapter;
    class SharedPipelineCaches;

    // Uses a d3d pipeline library to cache pipelines in-memory so they can be cached to disk.
    class PipelineCache : public RefCounted {
      public:
        PipelineCache(ComPtr<ID3D12Device> device,
                      SharedPipelineCaches* sharedPipelineCaches,
                      Ref<dawn_platform::CachedBlob> pipelineCacheBlob);

        ~PipelineCache() override;

        static ResultOrError<Ref<PipelineCache>> Create(
            ComPtr<ID3D12Device> device,
            SharedPipelineCaches* sharedPipelineCaches,
            Ref<dawn_platform::CachedBlob> pipelineCacheBlob);

        // Combines load/store operations into a single call. If the load was successful,
        // a cached PSO corresponding to |descHash| is returned to the caller.
        // Else, a new PSO is created from |desc| and stored back in the pipeline cache.
        template <typename PipelineStateDescT>
        ResultOrError<ComPtr<ID3D12PipelineState>> GetOrCreate(const PipelineStateDescT& desc,
                                                               size_t descHash) {
            ComPtr<ID3D12PipelineState> pso;
            const std::wstring& descHashW = std::to_wstring(descHash);
            if (mLibrary != nullptr) {
                DAWN_TRY(LoadPipeline(descHashW.c_str(), desc, pso));
                if (pso != nullptr) {
                    mHitCountForTesting++;
                    return std::move(pso);
                }
            }

            DAWN_TRY(CreatePipeline(desc, pso));
            if (pso != nullptr && mLibrary != nullptr) {
                DAWN_TRY(StorePipeline(descHashW.c_str(), pso.Get()));
                mIsLibraryBlobDirty = true;
            }

            return std::move(pso);
        }

        // Called by a device to save added pipelines back into the persistent cache.
        // Returns true if the pipeline cache data was successfully stored.
        bool SyncPipelineCacheData(PersistentCache* persistentCache);

        // Functors necessary for the unordered_set<PipelineCache*>-based cache.
        struct HashFunc {
            size_t operator()(const PipelineCache* entry) const;
        };

        struct EqualityFunc {
            bool operator()(const PipelineCache* a, const PipelineCache* b) const;
        };

        void ResetForTesting();
        size_t GetHitCountForTesting() const;

      private:
        MaybeError Initialize();

        // Overloads per pipeline state descriptor type
        MaybeError LoadPipeline(LPCWSTR name,
                                const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                ComPtr<ID3D12PipelineState>& pso);

        MaybeError LoadPipeline(LPCWSTR name,
                                const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
                                ComPtr<ID3D12PipelineState>& pso);

        MaybeError CreatePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
                                  ComPtr<ID3D12PipelineState>& pso);

        MaybeError CreatePipeline(D3D12_COMPUTE_PIPELINE_STATE_DESC desc,
                                  ComPtr<ID3D12PipelineState>& pso);

        MaybeError StorePipeline(LPCWSTR name, ID3D12PipelineState* pso);

        ComPtr<ID3D12Device> mDevice;

        // Cached blob is nullptr if the library was initialized to be empty.
        // Otherwise, the cached blob must remain valid for the lifetime of the |mLibrary|.
        Ref<dawn_platform::CachedBlob> mLibraryBlob;
        ComPtr<ID3D12PipelineLibrary> mLibrary;

        SharedPipelineCaches* mSharedPipelineCaches = nullptr;

        size_t mHitCountForTesting = 0;
        bool mIsLibraryBlobDirty = true;
    };

    // Shares pipelines between devices then persistently stores them back to disk.
    class SharedPipelineCaches {
      public:
        SharedPipelineCaches(Adapter* adapter);
        ~SharedPipelineCaches();

        ResultOrError<Ref<PipelineCache>> GetOrCreate(PersistentCache* persistentCache);

        void StorePipelineCacheData(const uint8_t* data,
                                    size_t size,
                                    PersistentCache* persistentCache);
        Ref<dawn_platform::CachedBlob> LoadPipelineCacheData(PersistentCache* persistentCache);

        void RemoveCacheEntry(PipelineCache* pipelineCache);

        void ResetForTesting();

      private:
        using Cache = std::
            unordered_set<PipelineCache*, PipelineCache::HashFunc, PipelineCache::EqualityFunc>;

        Cache mCache;

        Adapter* mAdapter = nullptr;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_