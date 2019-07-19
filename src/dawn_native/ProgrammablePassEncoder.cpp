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

#include "dawn_native/ProgrammablePassEncoder.h"

#include "dawn_native/BindGroup.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <string.h>

namespace dawn_native {

    ProgrammablePassEncoder::ProgrammablePassEncoder(DeviceBase* device,
                                                     CommandRecorder* commandRecorder,
                                                     CommandAllocator* allocator)
        : mDevice(device), mCommandRecorder(commandRecorder), mAllocator(allocator) {
        DAWN_ASSERT(allocator != nullptr);
    }

    ProgrammablePassEncoder::ProgrammablePassEncoder(DeviceBase* device,
                                                     CommandRecorder* commandRecorder,
                                                     ObjectBase::ErrorTag errorTag)
        : mDevice(device), mCommandRecorder(commandRecorder), mAllocator(nullptr) {
    }

    void ProgrammablePassEncoder::InsertDebugMarker(const char* groupLabel) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        InsertDebugMarkerCmd* cmd =
            mAllocator->Allocate<InsertDebugMarkerCmd>(Command::InsertDebugMarker);
        cmd->length = strlen(groupLabel);

        char* label = mAllocator->AllocateData<char>(cmd->length + 1);
        memcpy(label, groupLabel, cmd->length + 1);
    }

    void ProgrammablePassEncoder::PopDebugGroup() {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        mAllocator->Allocate<PopDebugGroupCmd>(Command::PopDebugGroup);
    }

    void ProgrammablePassEncoder::PushDebugGroup(const char* groupLabel) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        PushDebugGroupCmd* cmd = mAllocator->Allocate<PushDebugGroupCmd>(Command::PushDebugGroup);
        cmd->length = strlen(groupLabel);

        char* label = mAllocator->AllocateData<char>(cmd->length + 1);
        memcpy(label, groupLabel, cmd->length + 1);
    }

    void ProgrammablePassEncoder::SetBindGroup(uint32_t groupIndex,
                                               BindGroupBase* group,
                                               uint32_t dynamicOffsetCount,
                                               const uint64_t* dynamicOffsets) {
        const BindGroupLayoutBase* layout = group->GetLayout();

        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands()) ||
            mCommandRecorder->ConsumedError(mDevice->ValidateObject(group))) {
            return;
        }

        if (groupIndex >= kMaxBindGroups) {
            mCommandRecorder->HandleError("Setting bind group over the max");
            return;
        }

        // Dynamic offsets count must match the number required by the layout perfectly.
        if (layout->GetDynamicBufferCount() != dynamicOffsetCount) {
            mCommandRecorder->HandleError("dynamicOffset count mismatch");
        }

        for (uint32_t i = 0; i < dynamicOffsetCount; ++i) {
            if (dynamicOffsets[i] % kMinDynamicBufferOffsetAlignment != 0) {
                mCommandRecorder->HandleError("Dynamic Buffer Offset need to be aligned");
                return;
            }

            BufferBinding bufferBinding = group->GetBindingAsBufferBinding(i);

            // During BindGroup creation, validation ensures binding offset + binding size <= buffer
            // size.
            DAWN_ASSERT(bufferBinding.buffer->GetSize() >= bufferBinding.size);
            DAWN_ASSERT(bufferBinding.buffer->GetSize() - bufferBinding.size >=
                        bufferBinding.offset);

            if (dynamicOffsets[i] >
                bufferBinding.buffer->GetSize() - bufferBinding.offset - bufferBinding.size) {
                mCommandRecorder->HandleError("dynamic offset out of bounds");
                return;
            }
        }

        SetBindGroupCmd* cmd = mAllocator->Allocate<SetBindGroupCmd>(Command::SetBindGroup);
        cmd->index = groupIndex;
        cmd->group = group;
        cmd->dynamicOffsetCount = dynamicOffsetCount;
        if (dynamicOffsetCount > 0) {
            uint64_t* offsets = mAllocator->AllocateData<uint64_t>(cmd->dynamicOffsetCount);
            memcpy(offsets, dynamicOffsets, dynamicOffsetCount * sizeof(uint64_t));
        }
    }

    MaybeError ProgrammablePassEncoder::ValidateCanRecordCommands() const {
        if (mAllocator == nullptr) {
            return DAWN_VALIDATION_ERROR("Recording in an error or already ended pass encoder");
        }

        return nullptr;
    }

}  // namespace dawn_native
