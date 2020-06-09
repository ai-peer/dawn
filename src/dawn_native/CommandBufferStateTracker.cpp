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

#include "dawn_native/CommandBufferStateTracker.h"

#include "common/Assert.h"
#include "common/BitSetIterator.h"
#include "dawn_native/BindGroup.h"
#include "dawn_native/ComputePipeline.h"
#include "dawn_native/Forward.h"
#include "dawn_native/PipelineLayout.h"
#include "dawn_native/RenderPipeline.h"

namespace dawn_native {

    namespace {
        bool BufferSizesAtLeastAsBig(dawn_native::BindGroupBase* bindGroup,
                                     const std::vector<uint64_t>& pipelineMinimumBufferSizes) {
            const uint64_t* boundBufferSizes = bindGroup->GetUnverifiedBufferSizes();
            uint32_t unverifiedBufferCount = bindGroup->GetLayout()->GetUnverifiedBufferCount();
            ASSERT(unverifiedBufferCount == pipelineMinimumBufferSizes.size());

            for (size_t i = 0; i < unverifiedBufferCount; ++i) {
                if (boundBufferSizes[i] < pipelineMinimumBufferSizes[i]) {
                    return false;
                }
            }

            return true;
        }
    }  // namespace

    enum ValidationAspect {
        VALIDATION_ASPECT_PIPELINE,
        VALIDATION_ASPECT_BIND_GROUPS,
        VALIDATION_ASPECT_VERTEX_BUFFERS,
        VALIDATION_ASPECT_INDEX_BUFFER,
        VALIDATION_ASPECT_MIN_BUFFER_SIZES,

        VALIDATION_ASPECT_COUNT
    };
    static_assert(VALIDATION_ASPECT_COUNT == CommandBufferStateTracker::kNumAspects, "");

    static constexpr CommandBufferStateTracker::ValidationAspects kDispatchAspects =
        1 << VALIDATION_ASPECT_PIPELINE | 1 << VALIDATION_ASPECT_BIND_GROUPS |
        1 << VALIDATION_ASPECT_MIN_BUFFER_SIZES;

    static constexpr CommandBufferStateTracker::ValidationAspects kDrawAspects =
        1 << VALIDATION_ASPECT_PIPELINE | 1 << VALIDATION_ASPECT_BIND_GROUPS |
        1 << VALIDATION_ASPECT_MIN_BUFFER_SIZES | 1 << VALIDATION_ASPECT_VERTEX_BUFFERS;

    static constexpr CommandBufferStateTracker::ValidationAspects kDrawIndexedAspects =
        1 << VALIDATION_ASPECT_PIPELINE | 1 << VALIDATION_ASPECT_BIND_GROUPS |
        1 << VALIDATION_ASPECT_MIN_BUFFER_SIZES | 1 << VALIDATION_ASPECT_VERTEX_BUFFERS |
        1 << VALIDATION_ASPECT_INDEX_BUFFER;

    static constexpr CommandBufferStateTracker::ValidationAspects kLazyAspects =
        1 << VALIDATION_ASPECT_BIND_GROUPS | 1 << VALIDATION_ASPECT_MIN_BUFFER_SIZES |
        1 << VALIDATION_ASPECT_VERTEX_BUFFERS;

    MaybeError CommandBufferStateTracker::ValidateCanDispatch() {
        return ValidateOperation(kDispatchAspects);
    }

    MaybeError CommandBufferStateTracker::ValidateCanDraw() {
        return ValidateOperation(kDrawAspects);
    }

    MaybeError CommandBufferStateTracker::ValidateCanDrawIndexed() {
        return ValidateOperation(kDrawIndexedAspects);
    }

    MaybeError CommandBufferStateTracker::ValidateOperation(ValidationAspects requiredAspects) {
        // Fast return-true path if everything is good
        ValidationAspects missingAspects = requiredAspects & ~mAspects;
        if (missingAspects.none()) {
            return {};
        }

        // Generate an error immediately if a non-lazy aspect is missing as computing lazy aspects
        // requires the pipeline to be set.
        DAWN_TRY(CheckMissingAspects(missingAspects & ~kLazyAspects));

        RecomputeLazyAspects(missingAspects);

        DAWN_TRY(CheckMissingAspects(requiredAspects & ~mAspects));

        return {};
    }

    void CommandBufferStateTracker::RecomputeLazyAspects(ValidationAspects aspects) {
        ASSERT(mAspects[VALIDATION_ASPECT_PIPELINE]);
        ASSERT((aspects & ~kLazyAspects).none());

        if (aspects[VALIDATION_ASPECT_BIND_GROUPS]) {
            bool matchesLayout = true;
            bool matchesMinBufferSizes = true;

            for (uint32_t i :
                 IterateBitSet(~mVerifiedGroups & mLastPipelineLayout->GetBindGroupLayoutsMask())) {
                if (mBindgroups[i] == nullptr ||
                    mLastPipelineLayout->GetBindGroupLayout(i) != mBindgroups[i]->GetLayout()) {
                    matchesLayout = false;
                    break;
                } else if (!BufferSizesAtLeastAsBig(mBindgroups[i], (*mMinimumBufferSizes)[i])) {
                    matchesMinBufferSizes = false;
                    break;
                } else {
                    mVerifiedGroups.set(i);
                }
            }

            if (matchesLayout) {
                mAspects.set(VALIDATION_ASPECT_BIND_GROUPS);
            }

            if (matchesMinBufferSizes) {
                mAspects.set(VALIDATION_ASPECT_MIN_BUFFER_SIZES);
            }
        }

        if (aspects[VALIDATION_ASPECT_VERTEX_BUFFERS]) {
            ASSERT(mLastRenderPipeline != nullptr);

            const std::bitset<kMaxVertexBuffers>& requiredVertexBuffers =
                mLastRenderPipeline->GetVertexBufferSlotsUsed();
            if ((mVertexBufferSlotsUsed & requiredVertexBuffers) == requiredVertexBuffers) {
                mAspects.set(VALIDATION_ASPECT_VERTEX_BUFFERS);
            }
        }
    }

    MaybeError CommandBufferStateTracker::CheckMissingAspects(ValidationAspects aspects) {
        if (!aspects.any()) {
            return {};
        }

        if (aspects[VALIDATION_ASPECT_INDEX_BUFFER]) {
            return DAWN_VALIDATION_ERROR("Missing index buffer");
        }

        if (aspects[VALIDATION_ASPECT_VERTEX_BUFFERS]) {
            return DAWN_VALIDATION_ERROR("Missing vertex buffer");
        }

        if (aspects[VALIDATION_ASPECT_BIND_GROUPS]) {
            return DAWN_VALIDATION_ERROR("Missing bind group");
        }

        if (aspects[VALIDATION_ASPECT_MIN_BUFFER_SIZES]) {
            return DAWN_VALIDATION_ERROR("Bound buffer too small");
        }

        if (aspects[VALIDATION_ASPECT_PIPELINE]) {
            return DAWN_VALIDATION_ERROR("Missing pipeline");
        }

        UNREACHABLE();
    }

    void CommandBufferStateTracker::SetComputePipeline(ComputePipelineBase* pipeline) {
        SetPipelineCommon(pipeline);
    }

    void CommandBufferStateTracker::SetRenderPipeline(RenderPipelineBase* pipeline) {
        mLastRenderPipeline = pipeline;
        SetPipelineCommon(pipeline);
    }

    void CommandBufferStateTracker::SetBindGroup(uint32_t index, BindGroupBase* bindgroup) {
        mBindgroups[index] = bindgroup;
        mAspects.reset(VALIDATION_ASPECT_BIND_GROUPS);
        mAspects.reset(VALIDATION_ASPECT_MIN_BUFFER_SIZES);
        mVerifiedGroups.reset(index);
    }

    void CommandBufferStateTracker::SetIndexBuffer() {
        mAspects.set(VALIDATION_ASPECT_INDEX_BUFFER);
    }

    void CommandBufferStateTracker::SetVertexBuffer(uint32_t slot) {
        mVertexBufferSlotsUsed.set(slot);
    }

    void CommandBufferStateTracker::SetPipelineCommon(PipelineBase* pipeline) {
        mLastPipelineLayout = pipeline->GetLayout();
        mMinimumBufferSizes = pipeline->GetMinimumBufferSizes();

        mAspects.set(VALIDATION_ASPECT_PIPELINE);

        // Reset lazy aspects so they get recomputed on the next operation.
        mAspects &= ~kLazyAspects;
    }

}  // namespace dawn_native
