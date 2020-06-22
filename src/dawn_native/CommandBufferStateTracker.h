// Copyright 2017 The Dawn Authors
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

#ifndef DAWNNATIVE_COMMANDBUFFERSTATETRACKER_H
#define DAWNNATIVE_COMMANDBUFFERSTATETRACKER_H

#include "common/Constants.h"
#include "common/ityp_array.h"
#include "dawn_native/BindingInfo.h"
#include "dawn_native/Error.h"
#include "dawn_native/Forward.h"

#include <bitset>
#include <map>
#include <set>

namespace dawn_native {

    class CommandBufferStateTracker {
      public:
        // Non-state-modifying validation functions
        MaybeError ValidateCanDispatch();
        MaybeError ValidateCanDraw(uint32_t vertexCount,
                                   uint32_t instanceCount,
                                   uint32_t firstVertex,
                                   uint32_t firstInstance);
        MaybeError ValidateCanDrawIndexed(uint32_t indexCount,
                                          uint32_t instanceCount,
                                          uint32_t firstIndex,
                                          uint32_t firstInstance);
        MaybeError ValidateCanDrawIndirect();
        MaybeError ValidateCanDrawIndexedIndirect();

        // State-modifying methods
        void SetComputePipeline(ComputePipelineBase* pipeline);
        void SetRenderPipeline(RenderPipelineBase* pipeline);
        void SetBindGroup(BindGroupIndex index, BindGroupBase* bindgroup);
        void SetIndexBuffer(uint64_t size);
        void SetVertexBuffer(uint32_t slot, uint64_t size);

        static constexpr size_t kNumAspects = 4;
        using ValidationAspects = std::bitset<kNumAspects>;

      private:
        MaybeError ValidateOperation(ValidationAspects requiredAspects);
        void RecomputeLazyAspects(ValidationAspects aspects);
        MaybeError CheckMissingAspects(ValidationAspects aspects);

        void SetPipelineCommon(PipelineBase* pipeline);

        MaybeError ValidateVertexBufferSizes(uint64_t minElementsVertex,
                                             uint64_t minElementsInstance);

        ValidationAspects mAspects;

        ityp::array<BindGroupIndex, BindGroupBase*, kMaxBindGroups> mBindgroups = {};
        std::bitset<kMaxVertexBuffers> mVertexBufferSlotsUsed;

        std::array<uint64_t, kMaxVertexBuffers> mVertexBufferSizes;
        uint64_t mIndexBufferSize;

        PipelineLayoutBase* mLastPipelineLayout = nullptr;
        RenderPipelineBase* mLastRenderPipeline = nullptr;

        const RequiredBufferSizes* mMinimumBufferSizes = nullptr;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_COMMANDBUFFERSTATETRACKER_H
