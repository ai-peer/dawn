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

#include "dawn_native/PipelineCache.h"

#include "dawn_native/d3d12/d3d12_platform.h"

namespace dawn_native { namespace d3d12 {

    class Device;

    class PipelineCache : public PipelineCacheBase {
      public:
        PipelineCache(Device* device);
        virtual ~PipelineCache() = default;

        MaybeError loadPipelineCacheIfNecessary() override;
        MaybeError storePipelineCache() override;

        ResultOrError<ComPtr<ID3D12PipelineState>> getOrCreateGraphicsPipeline(
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc,
            uint32_t descKey);

        // TODO
        // ResultOrError<ComPtr<ID3D12PipelineState>> getOrCreateComputePipeline(
        //     const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc,
        //     uint32_t descKey);

      private:
        ComPtr<ID3D12PipelineLibrary> mLibrary;
        std::unique_ptr<uint8_t[]> mLibraryData;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_PIPELINECACHED3D12_H_