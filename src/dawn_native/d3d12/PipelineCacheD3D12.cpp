// Copyright 2020 The Dawn Authors
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
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

#include <sstream>

namespace dawn_native { namespace d3d12 {

    PipelineCache::PipelineCache(Device* device, bool isPipelineLibrarySupported)
        : PipelineCacheBase(device),
          mDevice(device),
          mIsPipelineLibrarySupported(isPipelineLibrarySupported) {
        // Generate the key for this pipeline cache.
        std::stringstream stream;
        stream << GetMetadataForKey();

        // Append the subsystem to ensure the retrieved pipeline cache is compatible.
        const PCIExtendedInfo& info = ToBackend(device->GetAdapter())->GetPCIExtendedInfo();
        stream << std::hex << info.subSysId;

        // TODO: Replace this with a secure hash?
        size_t hash = Hash(stream.str());
        mPipelineCacheKey = PersistentCache::CreateKey(hash);
    }

    MaybeError PipelineCache::loadPipelineCacheIfNecessary() {
        if (mIsPipelineCacheLoaded) {
            return {};
        }

        const size_t librarySize = mDevice->GetPersistentCache()->getDataSize(mPipelineCacheKey);
        if (librarySize > 0) {
            mLibraryData.reset(new uint8_t[librarySize]);
            mDevice->GetPersistentCache()->loadData(mPipelineCacheKey, mLibraryData.get(),
                                                    librarySize);
        }

        ASSERT(mLibraryData == nullptr || librarySize > 0);

        DAWN_TRY(CheckHRESULT(mDevice->GetD3D12Device1()->CreatePipelineLibrary(
                                  mLibraryData.get(), librarySize, IID_PPV_ARGS(&mLibrary)),
                              "ID3D12Device1::CreatePipelineLibrary"));

        mIsPipelineCacheLoaded = true;

        return {};
    }

    MaybeError PipelineCache::storePipelineCache() {
        if (!mIsPipelineCacheLoaded) {
            return {};
        }

        ASSERT(mLibrary != nullptr);

        const size_t librarySize = mLibrary->GetSerializedSize();
        std::unique_ptr<uint8_t[]> pSerializedData(new uint8_t[librarySize]);
        DAWN_TRY(CheckHRESULT(mLibrary->Serialize(pSerializedData.get(), librarySize),
                              "ID3D12PipelineLibrary::Serialize"));

        mDevice->GetPersistentCache()->storeData(mPipelineCacheKey, pSerializedData.get(),
                                                 librarySize);
        return {};
    }

    ResultOrError<ComPtr<ID3D12PipelineState>> PipelineCache::loadGraphicsPipeline(
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
        uint32_t descKey) {
        ComPtr<ID3D12PipelineState> pso;
        if (!mIsPipelineLibrarySupported) {
            return std::move(pso);
        }

        DAWN_TRY(loadPipelineCacheIfNecessary());

        const std::wstring descKeyW = std::to_wstring(descKey);
        HRESULT hr = mLibrary->LoadGraphicsPipeline(descKeyW.c_str(), &desc, IID_PPV_ARGS(&pso));
        if (SUCCEEDED(hr)) {
            mCacheHitCount++;
            return std::move(pso);
        }

        // Only E_INVALIDARG is considered cache-miss.
        if (hr != E_INVALIDARG) {
            DAWN_TRY(CheckHRESULT(hr, "ID3D12PipelineLibrary::LoadGraphicsPipeline"));
        }

        return std::move(pso);
    }

    ResultOrError<ComPtr<ID3D12PipelineState>> PipelineCache::loadComputePipeline(
        const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
        uint32_t descKey) {
        ComPtr<ID3D12PipelineState> pso;
        if (!mIsPipelineLibrarySupported) {
            return std::move(pso);
        }

        DAWN_TRY(loadPipelineCacheIfNecessary());

        const std::wstring descKeyW = std::to_wstring(descKey);
        HRESULT hr = mLibrary->LoadComputePipeline(descKeyW.c_str(), &desc, IID_PPV_ARGS(&pso));
        if (SUCCEEDED(hr)) {
            mCacheHitCount++;
            return std::move(pso);
        }

        // Only E_INVALIDARG is considered cache-miss.
        if (hr != E_INVALIDARG) {
            DAWN_TRY(CheckHRESULT(hr, "ID3D12PipelineLibrary::LoadComputePipeline"));
        }

        return std::move(pso);
    }

    MaybeError PipelineCache::storePipeline(ID3D12PipelineState* pso, uint32_t descKey) {
        ASSERT(pso != nullptr);
        if (!mIsPipelineLibrarySupported) {
            return {};
        }

        DAWN_TRY(loadPipelineCacheIfNecessary());

        const std::wstring descKeyW = std::to_wstring(descKey);
        // Always error since we expect a duplicate PSO to never be stored.
        DAWN_TRY(CheckHRESULT(mLibrary->StorePipeline(descKeyW.c_str(), pso),
                              "ID3D12PipelineLibrary::StorePipeline"));
        return {};
    }

    size_t PipelineCache::GetPipelineCacheHitCountForTesting() const {
        return mCacheHitCount;
    }
}}  // namespace dawn_native::d3d12