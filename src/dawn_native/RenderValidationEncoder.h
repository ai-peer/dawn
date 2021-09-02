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

#ifndef DAWNNATIVE_RENDERVALIDATIONENCODER_H_
#define DAWNNATIVE_RENDERVALIDATIONENCODER_H_

#include "common/RefCounted.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBufferStateTracker.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/PassResourceUsageTracker.h"

#include <cstdint>
#include <vector>

namespace dawn_native {

    class CommandEncoder;

    // Tracks information about what validation work needs to be done immediately prior to a render
    // pass or render bundle execution.
    class RenderValidationEncoder {
      public:
        RenderValidationEncoder();
        ~RenderValidationEncoder();

        RenderValidationEncoder(const RenderValidationEncoder&) = delete;
        RenderValidationEncoder& operator=(const RenderValidationEncoder&) = delete;

        RenderValidationEncoder(RenderValidationEncoder&&);
        RenderValidationEncoder& operator=(RenderValidationEncoder&&);

        void EnqueueBundle(RenderValidationEncoder* validationEncoder);
        void EnqueueIndexedIndirectDraw(CommandBufferStateTracker* commandBufferState,
                                        BufferBase* indirectBuffer,
                                        uint64_t indirectOffset,
                                        DrawIndexedIndirectCmd* cmd);

        MaybeError EncodeValidationCommands(DeviceBase* device,
                                            CommandEncoder* commandEncoder,
                                            RenderPassResourceUsageTracker* usageTracker);

      private:
        MaybeError EncodeValidationCommandsImpl(DeviceBase* device,
                                                CommandEncoder* commandEncoder,
                                                RenderPassResourceUsageTracker* usageTracker);
        size_t GetNumIndexedIndirectDraws() const;

        struct IndexedIndirectDraw {
            // The number of indicies available in the index buffer at the currently configured
            // offset.
            uint32_t firstInvalidIndex;

            // The caller's provided indirect buffer and offset.
            Ref<BufferBase> clientIndirectBuffer;
            uint64_t clientIndirectOffset;

            // The encoded command corresponding to this draw call in some command buffer.
            DrawIndexedIndirectCmd* cmd;
        };

        std::vector<RenderValidationEncoder*> mBundleValidationEncoders;
        std::vector<IndexedIndirectDraw> mIndexedIndirectDraws;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_RENDERVALIDATIONENCODER_H_