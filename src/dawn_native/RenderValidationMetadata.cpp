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

namespace dawn_native {

    RenderValidationMetadata::RenderValidationMetadata() = default;

    RenderValidationMetadata::~RenderValidationMetadata() = default;

    RenderValidationMetadata::RenderValidationMetadata(RenderValidationMetadata&&) = default;

    RenderValidationMetadata& RenderValidationMetadata::operator=(RenderValidationMetadata&&) =
        default;

    void RenderValidationMetadata::Extend(const RenderValidationMetadata& metadata) {
        const auto& draws = metadata.mIndexedIndirectDraws;
        mIndexedIndirectDraws.reserve(mIndexedIndirectDraws.size() + draws.size());
        for (const auto& data : draws) {
            mIndexedIndirectDraws.push_back(data);
        }
    }

    void RenderValidationMetadata::AddIndexedIndirectDraw(
        CommandBufferStateTracker* commandBufferState,
        BufferBase* indirectBuffer,
        uint64_t indirectOffset,
        DrawIndexedIndirectCmd* cmd) {
        mIndexedIndirectDraws.emplace_back();
        auto& draw = mIndexedIndirectDraws.back();
        const size_t indexBufferSize = commandBufferState->GetIndexBufferSize();
        const size_t indexNumBytes =
            commandBufferState->GetIndexFormat() == wgpu::IndexFormat::Uint32 ? 4 : 2;
        draw.firstInvalidIndex = indexBufferSize / indexNumBytes;
        draw.clientIndirectBuffer = indirectBuffer;
        draw.clientIndirectOffset = indirectOffset;
        draw.cmd = cmd;
    }

}  // namespace dawn_native
