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

#ifndef DAWNNATIVE_RENDERVALIDATIONMETADATA_H_
#define DAWNNATIVE_RENDERVALIDATIONMETADATA_H_

#include "common/RefCounted.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBufferStateTracker.h"
#include "dawn_native/Commands.h"

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace dawn_native {

    class RenderBundleBase;

    // Metadata corresponding to the validation requirements of a single render pass. This metadata
    // is accumulated while its corresponding render pass is encoded, and is later used to encode
    // validation commands to be inserted into the command buffer just before the render pass's own
    // commands.
    class RenderValidationMetadata {
      public:
        struct IndexedIndirectValidationPass {
            uint64_t minOffset;
            uint64_t maxOffset;
            std::vector<uint64_t> offsets;
            Ref<DeferredBufferRef> bufferRef;
        };

        // Tracks information about every draw call which uses the same indirect buffer in this
        // render pass. Calls are grouped by offset range so that validation work can be chunked
        // efficiently.
        class IndexedIndirectBufferValidationInfo {
          public:
            explicit IndexedIndirectBufferValidationInfo(BufferBase* indirectBuffer);

            // Logs a new drawIndexedIndirect call for the render pass. `cmd` is updated with an
            // assigned (and deferred) buffer ref and offset before returning.
            void AddIndexedIndirectDraw(uint64_t offset, DrawIndexedIndirectCmd* cmd);

            std::vector<IndexedIndirectValidationPass>* GetPasses();

          private:
            Ref<BufferBase> mIndirectBuffer;

            // A list of information about validation passes that will need to be executed for the
            // corresponding indirect buffer prior to a single render pass. These are kept sorted by
            // minOffset and may overlap iff the number of offsets in a validation pass exceeds some
            // maximum (roughly ~8M draw calls).
            //
            // Since the most common expected cases will overwhelmingly require only a single
            // validation pass per render pass, this is optimized for efficient updates to a single
            // validation pass definition rather than for efficient manipulation of a large number
            // of validation passes.
            std::vector<IndexedIndirectValidationPass> mPasses;
        };

        // Combination of an indirect buffer reference, and the number of addressable index buffer
        // elements at the time of a draw call.
        using IndexedIndirectConfig = std::pair<BufferBase*, uint64_t>;
        using IndexedIndirectBufferValidationInfoMap =
            std::map<IndexedIndirectConfig, IndexedIndirectBufferValidationInfo>;

        // Added bundles are tracked by this mapping so we don't do redundant accounting or
        // validation work if they're executed more than once in the same render pass.
        using BundleMetadataMap = std::map<RenderBundleBase*, RenderValidationMetadata>;

        RenderValidationMetadata();
        ~RenderValidationMetadata();

        RenderValidationMetadata(const RenderValidationMetadata&) = delete;
        RenderValidationMetadata& operator=(const RenderValidationMetadata&) = delete;

        RenderValidationMetadata(RenderValidationMetadata&&);
        RenderValidationMetadata& operator=(RenderValidationMetadata&&);

        IndexedIndirectBufferValidationInfoMap* GetIndexedIndirectBufferValidationInfo();
        BundleMetadataMap* GetBundleMetadata();

        void AddBundle(RenderBundleBase* bundle);
        void AddIndexedIndirectDraw(wgpu::IndexFormat indexFormat,
                                    uint64_t indexBufferSize,
                                    BufferBase* indirectBuffer,
                                    uint64_t indirectOffset,
                                    DrawIndexedIndirectCmd* cmd);

      private:
        IndexedIndirectBufferValidationInfoMap mIndexedIndirectBufferValidationInfo;
        BundleMetadataMap mBundleMetadata;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_RENDERVALIDATIONMETADATA_H_
