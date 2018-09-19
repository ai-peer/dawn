// Copyright 2018 The Dawn Authors
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

#ifndef DAWNNATIVE_PROGRAMMABLEPASSENCODER_H_
#define DAWNNATIVE_PROGRAMMABLEPASSENCODER_H_

#include "dawn_native/Error.h"
#include "dawn_native/RefCounted.h"

#include "dawn_native/dawn_platform.h"

namespace dawn_native {

    class CommandAllocator;
    class DeviceBase;

    // This is called ComputePassEncoderBase to match the code generator expectations. Note that it
    // is a pure frontend type to record in its parent CommandBufferBuilder and never has a backend
    // implementation.
    // TODO(cwallez@chromium.org): Remove that generator limitation and rename to ComputePassEncoder
    // TODO(cwallez@chromium.org): Factor common code with RenderPassEncoderBase into a
    // ProgrammablePassEncoder
    class ProgrammablePassEncoder : public RefCounted {
      public:
        ProgrammablePassEncoder(DeviceBase* device, CommandBufferBuilder* topLevelBuilder, CommandAllocator* allocator);

        void EndPass();

        void SetBindGroup(uint32_t groupIndex, BindGroupBase* group);
        void SetPushConstants(dawn::ShaderStageBit stages,
                              uint32_t offset,
                              uint32_t count,
                              const void* data);

        DeviceBase* GetDevice() const;

      protected:
        MaybeError ValidateCanRecordCommands() const;

        DeviceBase* mDevice;
        
        // The allocator is borrowed from the top level builder. Keep a reference to the builder
        // to make sure the allocator isn't freed.
        Ref<CommandBufferBuilder> mTopLevelBuilder = nullptr;
        // mAllocator is cleared at the end of the pass so it acts as a tag that EndPass was called
        CommandAllocator* mAllocator = nullptr;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_PROGRAMMABLEPASSENCODER_H_
