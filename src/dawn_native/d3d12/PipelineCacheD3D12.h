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

#ifndef DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_
#define DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_

#include "dawn_native/Error.h"
#include "dawn_native/PersistentCache.h"
#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    // Load/store PSOs from a pipeline library which can be saved to disk using the persistent cache
    // API.
    class PipelineCache {
      public:
        PipelineCache(Device* device, bool isPipelineLibrarySupported);
        ~PipelineCache() = default;

        MaybeError storePipelineCache();

        ResultOrError<ComPtr<ID3D12PipelineState>> loadGraphicsPipeline(
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
            size_t descKey);

        ResultOrError<ComPtr<ID3D12PipelineState>> loadComputePipeline(
            const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
            size_t descKey);

        MaybeError storePipeline(ID3D12PipelineState* pso, size_t descKey);

        size_t GetPipelineCacheHitCountForTesting() const;

      private:
        MaybeError loadPipelineCacheIfNecessary();

        Device* mDevice = nullptr;

        bool mIsPipelineLibrarySupported;

        ComPtr<ID3D12PipelineLibrary> mLibrary;
        std::unique_ptr<uint8_t[]> mLibraryData;  // Cannot outlive |mLibrary|

        size_t mCacheHitCount = 0;
        PersistentCacheKey mPipelineCacheKey;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_