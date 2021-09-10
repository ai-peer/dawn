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

#include "dawn_native/RenderValidationMetadata.h"

#include "common/Constants.h"
#include "common/RefCounted.h"
#include "dawn_native/RenderBundle.h"

#include <algorithm>
#include <utility>

namespace dawn_native {

    namespace {

        // In the unlikely scenario that indirect offsets used over a single buffer span more than
        // this length of the buffer, we split the validation work into multiple passes.
        constexpr uint64_t kMaxPassOffsetRange =
            kMaxStorageBufferBindingSize - kDrawIndexedIndirectSize;

        // Maximum number of draw calls allowed per validation pass. If the number of draws calls
        // exceeds this, even for a single indirect buffer with offsets that all span less than
        // `kMaxPassOffsetRange` bytes, we split the validation work into multiple passes. This
        // limitation is imposed to ensure ample room for the validation metadata to fit within a
        // storage-bound buffer.
        constexpr uint64_t kMaxDrawCallsPerValidationPass = (kMaxStorageBufferBindingSize - 12) / 4;

    }  // namespace

    RenderValidationMetadata::IndexedIndirectBufferValidationInfo::
        IndexedIndirectBufferValidationInfo(BufferBase* indirectBuffer)
        : mIndirectBuffer(indirectBuffer) {
    }

    void RenderValidationMetadata::IndexedIndirectBufferValidationInfo::AddIndexedIndirectDraw(
        IndexedIndirectDraw draw) {
        const uint64_t newOffset = draw.clientBufferOffset;
        auto it = mPasses.begin();
        while (it != mPasses.end()) {
            IndexedIndirectValidationPass& pass = *it;
            if (pass.draws.size() >= kMaxDrawCallsPerValidationPass) {
                // This pass is full. If its minOffset is to the right of the new offset, we can
                // just insert a new pass here.
                if (newOffset < pass.minOffset) {
                    break;
                }

                // Otherwise keep looking.
                ++it;
                continue;
            }

            if (newOffset < pass.minOffset && pass.maxOffset - newOffset <= kMaxPassOffsetRange) {
                // We can extend this pass to the left in order to fit the new offset.
                pass.minOffset = newOffset;
                pass.draws.push_back(std::move(draw));
                return;
            }

            if (newOffset > pass.maxOffset && newOffset - pass.minOffset <= kMaxPassOffsetRange) {
                // We can extend this pass to the right in order to fit the new offset.
                pass.maxOffset = newOffset;
                pass.draws.push_back(std::move(draw));
                return;
            }

            if (newOffset < pass.minOffset) {
                // We want to insert a new pass just before this one.
                break;
            }

            ++it;
        }

        IndexedIndirectValidationPass newPass;
        newPass.minOffset = newOffset;
        newPass.maxOffset = newOffset;
        newPass.draws.push_back(std::move(draw));

        mPasses.insert(it, std::move(newPass));
    }

    void RenderValidationMetadata::IndexedIndirectBufferValidationInfo::AddPass(
        const IndexedIndirectValidationPass& newPass) {
        auto it = mPasses.begin();
        while (it != mPasses.end()) {
            IndexedIndirectValidationPass& pass = *it;
            uint64_t min = std::min(newPass.minOffset, pass.minOffset);
            uint64_t max = std::max(newPass.maxOffset, pass.maxOffset);
            if (max - min <= kMaxPassOffsetRange &&
                pass.draws.size() + newPass.draws.size() <= kMaxDrawCallsPerValidationPass) {
                pass.minOffset = min;
                pass.maxOffset = max;
                pass.draws.insert(pass.draws.end(), newPass.draws.begin(), newPass.draws.end());
                return;
            }

            if (newPass.minOffset < pass.minOffset) {
                break;
            }

            ++it;
        }
        mPasses.push_back(newPass);
    }

    const std::vector<RenderValidationMetadata::IndexedIndirectValidationPass>&
    RenderValidationMetadata::IndexedIndirectBufferValidationInfo::GetPasses() const {
        return mPasses;
    }

    RenderValidationMetadata::RenderValidationMetadata() = default;

    RenderValidationMetadata::~RenderValidationMetadata() = default;

    RenderValidationMetadata::RenderValidationMetadata(RenderValidationMetadata&&) = default;

    RenderValidationMetadata& RenderValidationMetadata::operator=(RenderValidationMetadata&&) =
        default;

    RenderValidationMetadata::IndexedIndirectBufferValidationInfoMap*
    RenderValidationMetadata::GetIndexedIndirectBufferValidationInfo() {
        return &mIndexedIndirectBufferValidationInfo;
    }

    void RenderValidationMetadata::AddBundle(RenderBundleBase* bundle) {
        auto result = mAddedBundles.insert(bundle);
        if (!result.second) {
            return;
        }

        for (const auto& entry :
             bundle->GetValidationMetadata().mIndexedIndirectBufferValidationInfo) {
            const IndexedIndirectConfig& config = entry.first;
            auto it = mIndexedIndirectBufferValidationInfo.lower_bound(config);
            if (it != mIndexedIndirectBufferValidationInfo.end() && it->first == config) {
                // We already have passes for the same config. Merge the new ones in.
                for (const IndexedIndirectValidationPass& pass : entry.second.GetPasses()) {
                    it->second.AddPass(pass);
                }
            } else {
                mIndexedIndirectBufferValidationInfo.emplace_hint(it, config, entry.second);
            }
        }
    }

    void RenderValidationMetadata::AddIndexedIndirectDraw(
        wgpu::IndexFormat indexFormat,
        uint64_t indexBufferSize,
        BufferBase* indirectBuffer,
        uint64_t indirectOffset,
        BufferLocation* drawCmdIndirectBufferLocation) {
        uint64_t numIndexBufferElements;
        switch (indexFormat) {
            case wgpu::IndexFormat::Uint16:
                numIndexBufferElements = indexBufferSize / 2;
                break;
            case wgpu::IndexFormat::Uint32:
                numIndexBufferElements = indexBufferSize / 4;
                break;
            default:
                numIndexBufferElements = 0;
                break;
        }

        const IndexedIndirectConfig config(indirectBuffer, numIndexBufferElements);
        auto it = mIndexedIndirectBufferValidationInfo.find(config);
        if (it == mIndexedIndirectBufferValidationInfo.end()) {
            auto result = mIndexedIndirectBufferValidationInfo.emplace(
                config, IndexedIndirectBufferValidationInfo(indirectBuffer));
            it = result.first;
        }

        IndexedIndirectDraw draw;
        draw.clientBufferOffset = indirectOffset;
        draw.bufferLocation = drawCmdIndirectBufferLocation;
        it->second.AddIndexedIndirectDraw(std::move(draw));
    }

}  // namespace dawn_native
