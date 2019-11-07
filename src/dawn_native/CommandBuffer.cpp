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

#include "dawn_native/CommandBuffer.h"

#include "dawn_native/CommandEncoder.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Texture.h"

namespace dawn_native {

    CommandBufferBase::CommandBufferBase(CommandEncoderBase* encoder,
                                         const CommandBufferDescriptor*)
        : ObjectBase(encoder->GetDevice()),
          mCommands(encoder->AcquireCommands()),
          mResourceUsages(encoder->AcquireResourceUsages()) {
    }

    CommandBufferBase::CommandBufferBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag) {
    }

    CommandBufferBase::~CommandBufferBase() {
        Clear();
    }

    // static
    CommandBufferBase* CommandBufferBase::MakeError(DeviceBase* device) {
        return new CommandBufferBase(device, ObjectBase::kError);
    }

    MaybeError CommandBufferBase::ValidateCanUseInSubmitNow() const {
        ASSERT(!IsError());

        if (mCommands.IsDestroyed()) {
            return DAWN_VALIDATION_ERROR("Command buffer reused in submit");
        }
        return {};
    }

    void CommandBufferBase::Clear() {
        FreeCommands(&mCommands);

        // TODO(enga): Applications are usually continuously recording commands.
        // Recycle command blocks instead of completely discarding them.
        mCommands = CommandIterator{};
    }

    const CommandBufferResourceUsage& CommandBufferBase::GetResourceUsages() const {
        return mResourceUsages;
    }

    bool IsCompleteSubresourceCopiedTo(const TextureBase* texture,
                                       const Extent3D copySize,
                                       const uint32_t mipLevel) {
        Extent3D extent = texture->GetMipLevelPhysicalSize(mipLevel);

        if (extent.depth == copySize.depth && extent.width == copySize.width &&
            extent.height == copySize.height) {
            return true;
        }
        return false;
    }
}  // namespace dawn_native
