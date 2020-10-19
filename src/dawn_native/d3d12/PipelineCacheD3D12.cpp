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

#include "dawn_native/d3d12/AdapterD3D12.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"

#include <sstream>

namespace dawn_native { namespace d3d12 {

    PipelineCache::PipelineCache(Device* device, bool isPipelineLibrarySupported)
        : mDevice(device), mIsPipelineLibrarySupported(isPipelineLibrarySupported) {
        // Generate the key for this pipeline cache.
        std::stringstream stream;
        {
            PCIInfo info = device->GetAdapter()->GetPCIInfo();
            stream << std::hex << info.deviceId;
            stream << std::hex << info.vendorId;
        }
        {
            const PCIExtendedInfo& info = ToBackend(device->GetAdapter())->GetPCIExtendedInfo();
            stream << std::hex << info.subSysId;
        }

        mPipelineCacheKey = PersistentCache::CreateKey(stream.str());
    }

    MaybeError PipelineCache::loadPipelineCacheIfNecessary() {
        if (mLibrary != nullptr || !mIsPipelineLibrarySupported) {
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

        return {};
    }

    MaybeError PipelineCache::storePipelineCache() {
        if (mLibrary == nullptr) {
            return {};
        }

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
        size_t descKey) {
        DAWN_TRY(loadPipelineCacheIfNecessary());

        ComPtr<ID3D12PipelineState> pso;
        if (mLibrary == nullptr) {
            return std::move(pso);
        }

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
        size_t descKey) {
        DAWN_TRY(loadPipelineCacheIfNecessary());

        ComPtr<ID3D12PipelineState> pso;
        if (mLibrary == nullptr) {
            return std::move(pso);
        }

        // LoadComputePipeline returns E_INVALIDARG if the key does not exit or the |desc| is
        // incompatible with the stored PSO. While the former error can be ignored, these errors
        // cannot be distinguished by HRESULT so both are ignored and checked by backend validation
        // instead.
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12pipelinelibrary-loadcomputepipeline
        HRESULT hr = mLibrary->LoadComputePipeline(std::to_wstring(descKey).c_str(), &desc,
                                                   IID_PPV_ARGS(&pso));
        if (SUCCEEDED(hr)) {
            mCacheHitCount++;
            return std::move(pso);
        }

        // Any other HRESULT error is not considered a cache-miss and must error.
        if (hr != E_INVALIDARG) {
            DAWN_TRY(CheckHRESULT(hr, "ID3D12PipelineLibrary::LoadComputePipeline"));
        }

        return std::move(pso);
    }

    MaybeError PipelineCache::storePipeline(ID3D12PipelineState* pso, size_t descKey) {
        ASSERT(pso != nullptr);
        if (mLibrary == nullptr) {
            return {};
        }

        // StorePipeline returns an error HRESULT if the key was previously stored or failed to
        // allocate storage, none of these errors can be ignored.
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12pipelinelibrary-storepipeline
        DAWN_TRY(CheckHRESULT(mLibrary->StorePipeline(std::to_wstring(descKey).c_str(), pso),
                              "ID3D12PipelineLibrary::StorePipeline"));
        return {};
    }

    size_t PipelineCache::GetPipelineCacheHitCountForTesting() const {
        return mCacheHitCount;
    }
}}  // namespace dawn_native::d3d12