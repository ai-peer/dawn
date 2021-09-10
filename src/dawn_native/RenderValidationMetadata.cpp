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
#include <limits>

namespace dawn_native {

    namespace {

        // In the unlikely scenario that indirect offsets used over a single buffer span more than
        // this length of the buffer, we split the validation work into multiple passes.
        constexpr uint64_t kMaxPassOffsetRange = kMaxStorageBufferBindingSize / 2;

        // Maximum number of draw calls allowed per validation pass. If the number of draws calls
        // exceeds this, even for a single indirect buffer with offsets that all span less than
        // `kMaxPassOffsetRange` bytes, we split the validation work into multiple passes. This
        // limitation is imposed to ensure ample room for the validation metadata to fit in a
        // storage-bound buffer.
        constexpr uint64_t kMaxDrawCallsPerValidationPass = (kMaxStorageBufferBindingSize - 12) / 4;

    }  // namespace

    RenderValidationMetadata::IndexedIndirectBufferValidationInfo::
        IndexedIndirectBufferValidationInfo(BufferBase* indirectBuffer)
        : mIndirectBuffer(indirectBuffer) {
    }

    void RenderValidationMetadata::IndexedIndirectBufferValidationInfo::AddIndexedIndirectDraw(
        uint64_t offset,
        DrawIndexedIndirectCmd* cmd) {
        auto it = mPasses.begin();
        while (it != mPasses.end()) {
            IndexedIndirectValidationPass& pass = *it;
            if (pass.offsets.size() >= kMaxDrawCallsPerValidationPass) {
                // This pass is full. If its minOffset is to the right of the new offset, we can
                // just insert a new pass here.
                if (offset < pass.minOffset) {
                    break;
                }

                // Otherwise keep looking.
                ++it;
                continue;
            }

            if (offset < pass.minOffset && pass.maxOffset - offset <= kMaxPassOffsetRange) {
                // We can extend this pass to the left in order to fit the new offset.
                cmd->indirectBufferRef = pass.bufferRef;
                cmd->indirectOffset = pass.offsets.size() * kDrawIndexedIndirectSize;
                pass.minOffset = offset;
                pass.offsets.push_back(offset);
                return;
            }

            if (offset > pass.maxOffset && offset - pass.minOffset <= kMaxPassOffsetRange) {
                // We can extend this pass to the right in order to fit the new offset.
                cmd->indirectBufferRef = pass.bufferRef;
                cmd->indirectOffset = pass.offsets.size() * kDrawIndexedIndirectSize;
                pass.maxOffset = offset;
                pass.offsets.push_back(offset);
                return;
            }

            if (offset < pass.minOffset) {
                // We want to insert a new pass just before this one.
                break;
            }

            ++it;
        }

        IndexedIndirectValidationPass newPass;
        newPass.minOffset = offset;
        newPass.maxOffset = offset;
        newPass.offsets.push_back(offset);
        newPass.bufferRef = AcquireRef(new DeferredBufferRef());

        cmd->indirectBufferRef = newPass.bufferRef;
        cmd->indirectOffset = 0;

        mPasses.insert(it, std::move(newPass));
    }

    std::vector<RenderValidationMetadata::IndexedIndirectValidationPass>*
    RenderValidationMetadata::IndexedIndirectBufferValidationInfo::GetPasses() {
        return &mPasses;
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

    RenderValidationMetadata::BundleMetadataMap* RenderValidationMetadata::GetBundleMetadata() {
        return &mBundleMetadata;
    }

    void RenderValidationMetadata::AddBundle(RenderBundleBase* bundle) {
        auto it = mBundleMetadata.find(bundle);
        if (it != mBundleMetadata.end()) {
            return;
        }

        mBundleMetadata[bundle].mIndexedIndirectBufferValidationInfo =
            bundle->GetValidationMetadata().mIndexedIndirectBufferValidationInfo;
    }

    void RenderValidationMetadata::AddIndexedIndirectDraw(wgpu::IndexFormat indexFormat,
                                                          uint64_t indexBufferSize,
                                                          BufferBase* indirectBuffer,
                                                          uint64_t indirectOffset,
                                                          DrawIndexedIndirectCmd* cmd) {
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
        it->second.AddIndexedIndirectDraw(indirectOffset, cmd);
    }

}  // namespace dawn_native
