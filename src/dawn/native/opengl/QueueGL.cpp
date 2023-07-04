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

#include "dawn/native/opengl/QueueGL.h"

#include "dawn/native/opengl/BufferGL.h"
#include "dawn/native/opengl/CommandBufferGL.h"
#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/TextureGL.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::opengl {

ResultOrError<Ref<Queue>> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    return AcquireRef(new Queue(device, descriptor));
}

Queue::Queue(Device* device, const QueueDescriptor* descriptor) : QueueBase(device, descriptor) {}

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording, "CommandBufferGL::Execute");
    for (uint32_t i = 0; i < commandCount; ++i) {
        DAWN_TRY(ToBackend(commands[i])->Execute());
    }
    TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording, "CommandBufferGL::Execute");

    return {};
}

MaybeError Queue::WriteBufferImpl(BufferBase* buffer,
                                  uint64_t bufferOffset,
                                  const void* data,
                                  size_t size) {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->GetGL();

    ToBackend(buffer)->EnsureDataInitializedAsDestination(bufferOffset, size);

    gl.BindBuffer(GL_ARRAY_BUFFER, ToBackend(buffer)->GetHandle());
    gl.BufferSubData(GL_ARRAY_BUFFER, bufferOffset, size, data);
    buffer->MarkUsedInPendingCommands();
    return {};
}

MaybeError Queue::WriteTextureImpl(const ImageCopyTexture& destination,
                                   const void* data,
                                   const TextureDataLayout& dataLayout,
                                   const Extent3D& writeSizePixel) {
    DAWN_INVALID_IF(destination.aspect == wgpu::TextureAspect::StencilOnly,
                    "Writes to stencil textures unsupported on the OpenGL backend.");

    TextureCopy textureCopy;
    textureCopy.texture = destination.texture;
    textureCopy.mipLevel = destination.mipLevel;
    textureCopy.origin = destination.origin;
    textureCopy.aspect = SelectFormatAspects(destination.texture->GetFormat(), destination.aspect);

    SubresourceRange range = GetSubresourcesAffectedByCopy(textureCopy, writeSizePixel);
    if (IsCompleteSubresourceCopiedTo(destination.texture, writeSizePixel, destination.mipLevel)) {
        destination.texture->SetIsSubresourceContentInitialized(true, range);
    } else {
        DAWN_TRY(ToBackend(destination.texture)->EnsureSubresourceContentInitialized(range));
    }
    DoTexSubImage(ToBackend(GetDevice())->GetGL(), textureCopy, data, dataLayout, writeSizePixel);
    ToBackend(destination.texture)->Touch();
    return {};
}

void Queue::OnGLUsed() {
    mHasPendingCommands = true;
}

void Queue::SubmitFenceSync() {
    if (!mHasPendingCommands) {
        return;
    }

    const OpenGLFunctions& gl = ToBackend(GetDevice())->GetGL();
    GLsync sync = gl.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    IncrementLastSubmittedCommandSerial();
    mFencesInFlight.emplace(sync, GetLastSubmittedCommandSerial());
    mHasPendingCommands = false;
}

bool Queue::HasPendingCommands() const {
    return mHasPendingCommands;
}

ResultOrError<ExecutionSerial> Queue::CheckAndUpdateCompletedSerials() {
    const Device* device = ToBackend(GetDevice());
    const OpenGLFunctions& gl = device->GetGL();

    ExecutionSerial fenceSerial{0};
    while (!mFencesInFlight.empty()) {
        auto [sync, tentativeSerial] = mFencesInFlight.front();

        // Fence are added in order, so we can stop searching as soon
        // as we see one that's not ready.

        // TODO(crbug.com/dawn/633): Remove this workaround after the deadlock issue is fixed.
        if (device->IsToggleEnabled(Toggle::FlushBeforeClientWaitSync)) {
            gl.Flush();
        }
        GLenum result = gl.ClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (result == GL_TIMEOUT_EXPIRED) {
            return fenceSerial;
        }
        // Update fenceSerial since fence is ready.
        fenceSerial = tentativeSerial;

        gl.DeleteSync(sync);

        mFencesInFlight.pop();

        ASSERT(fenceSerial > GetCompletedCommandSerial());
    }
    return fenceSerial;
}

void Queue::ForceEventualFlushOfCommands() {
    mHasPendingCommands = true;
}

MaybeError Queue::WaitForIdleForDestruction() {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->GetGL();
    gl.Finish();
    DAWN_TRY(CheckPassedSerials());
    ASSERT(mFencesInFlight.empty());
    return {};
}

}  // namespace dawn::native::opengl
