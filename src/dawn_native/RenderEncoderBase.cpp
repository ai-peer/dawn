// Copyright 2019 The Dawn Authors
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

#include "dawn_native/RenderEncoderBase.h"

#include "common/Constants.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/RenderPipeline.h"

#include <math.h>
#include <string.h>

namespace dawn_native {

    RenderEncoderBase::RenderEncoderBase(DeviceBase* device,
                                         CommandRecorder* commandRecorder,
                                         CommandAllocator* allocator)
        : ProgrammablePassEncoder(device, commandRecorder, allocator) {
    }

    RenderEncoderBase::RenderEncoderBase(DeviceBase* device,
                                         CommandRecorder* commandRecorder,
                                         ErrorTag errorTag)
        : ProgrammablePassEncoder(device, commandRecorder, errorTag) {
    }

    void RenderEncoderBase::Draw(uint32_t vertexCount,
                                 uint32_t instanceCount,
                                 uint32_t firstVertex,
                                 uint32_t firstInstance) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        DrawCmd* draw = mAllocator->Allocate<DrawCmd>(Command::Draw);
        draw->vertexCount = vertexCount;
        draw->instanceCount = instanceCount;
        draw->firstVertex = firstVertex;
        draw->firstInstance = firstInstance;
    }

    void RenderEncoderBase::DrawIndexed(uint32_t indexCount,
                                        uint32_t instanceCount,
                                        uint32_t firstIndex,
                                        int32_t baseVertex,
                                        uint32_t firstInstance) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        DrawIndexedCmd* draw = mAllocator->Allocate<DrawIndexedCmd>(Command::DrawIndexed);
        draw->indexCount = indexCount;
        draw->instanceCount = instanceCount;
        draw->firstIndex = firstIndex;
        draw->baseVertex = baseVertex;
        draw->firstInstance = firstInstance;
    }

    void RenderEncoderBase::DrawIndirect(BufferBase* indirectBuffer, uint64_t indirectOffset) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands()) ||
            mCommandRecorder->ConsumedError(GetDevice()->ValidateObject(indirectBuffer))) {
            return;
        }

        if (indirectOffset >= indirectBuffer->GetSize() ||
            indirectOffset + kDrawIndirectSize > indirectBuffer->GetSize()) {
            mCommandRecorder->HandleError("Indirect offset out of bounds");
            return;
        }

        DrawIndirectCmd* cmd = mAllocator->Allocate<DrawIndirectCmd>(Command::DrawIndirect);
        cmd->indirectBuffer = indirectBuffer;
        cmd->indirectOffset = indirectOffset;
    }

    void RenderEncoderBase::DrawIndexedIndirect(BufferBase* indirectBuffer,
                                                uint64_t indirectOffset) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands()) ||
            mCommandRecorder->ConsumedError(GetDevice()->ValidateObject(indirectBuffer))) {
            return;
        }

        if (indirectOffset >= indirectBuffer->GetSize() ||
            indirectOffset + kDrawIndexedIndirectSize > indirectBuffer->GetSize()) {
            mCommandRecorder->HandleError("Indirect offset out of bounds");
            return;
        }

        DrawIndexedIndirectCmd* cmd =
            mAllocator->Allocate<DrawIndexedIndirectCmd>(Command::DrawIndexedIndirect);
        cmd->indirectBuffer = indirectBuffer;
        cmd->indirectOffset = indirectOffset;
    }

    void RenderEncoderBase::SetPipeline(RenderPipelineBase* pipeline) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands()) ||
            mCommandRecorder->ConsumedError(GetDevice()->ValidateObject(pipeline))) {
            return;
        }

        SetRenderPipelineCmd* cmd =
            mAllocator->Allocate<SetRenderPipelineCmd>(Command::SetRenderPipeline);
        cmd->pipeline = pipeline;
    }

    void RenderEncoderBase::SetIndexBuffer(BufferBase* buffer, uint64_t offset) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands()) ||
            mCommandRecorder->ConsumedError(GetDevice()->ValidateObject(buffer))) {
            return;
        }

        SetIndexBufferCmd* cmd = mAllocator->Allocate<SetIndexBufferCmd>(Command::SetIndexBuffer);
        cmd->buffer = buffer;
        cmd->offset = offset;
    }

    void RenderEncoderBase::SetVertexBuffers(uint32_t startSlot,
                                             uint32_t count,
                                             BufferBase* const* buffers,
                                             uint64_t const* offsets) {
        if (mCommandRecorder->ConsumedError(ValidateCanRecordCommands())) {
            return;
        }

        for (size_t i = 0; i < count; ++i) {
            if (mCommandRecorder->ConsumedError(GetDevice()->ValidateObject(buffers[i]))) {
                return;
            }
        }

        SetVertexBuffersCmd* cmd =
            mAllocator->Allocate<SetVertexBuffersCmd>(Command::SetVertexBuffers);
        cmd->startSlot = startSlot;
        cmd->count = count;

        Ref<BufferBase>* cmdBuffers = mAllocator->AllocateData<Ref<BufferBase>>(count);
        for (size_t i = 0; i < count; ++i) {
            cmdBuffers[i] = buffers[i];
        }

        uint64_t* cmdOffsets = mAllocator->AllocateData<uint64_t>(count);
        memcpy(cmdOffsets, offsets, count * sizeof(uint64_t));
    }

}  // namespace dawn_native
