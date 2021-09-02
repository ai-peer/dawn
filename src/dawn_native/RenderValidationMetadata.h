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
#include <vector>

namespace dawn_native {

    class RenderValidationMetadata {
      public:
        struct IndexedIndirectDraw {
            // The number of indicies available in the index buffer at the configured offset when
            // the draw call was made.
            uint32_t firstInvalidIndex;

            // The draw call's provided indirect buffer and offset.
            Ref<BufferBase> clientIndirectBuffer;
            uint64_t clientIndirectOffset;

            // The DeferredBufferRef used by the corresponding draw command. This is stored so it
            // can be updated with an appropriate concrete Buffer ref at encoding time.
            Ref<DeferredBufferRef> bufferRef;
        };

        RenderValidationMetadata();
        ~RenderValidationMetadata();

        RenderValidationMetadata(const RenderValidationMetadata&) = delete;
        RenderValidationMetadata& operator=(const RenderValidationMetadata&) = delete;

        RenderValidationMetadata(RenderValidationMetadata&&);
        RenderValidationMetadata& operator=(RenderValidationMetadata&&);

        const std::vector<IndexedIndirectDraw>& GetIndexedIndirectDraws() const {
            return mIndexedIndirectDraws;
        }

        void Extend(const RenderValidationMetadata& metadata);

        void AddIndexedIndirectDraw(CommandBufferStateTracker* commandBufferState,
                                    BufferBase* indirectBuffer,
                                    uint64_t indirectOffset,
                                    DeferredBufferRef* bufferRef);

      private:
        std::vector<IndexedIndirectDraw> mIndexedIndirectDraws;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_RENDERVALIDATIONMETADATA_H_
