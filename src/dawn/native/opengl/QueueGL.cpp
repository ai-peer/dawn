// Copyright 2018 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/opengl/QueueGL.h"

#include "dawn/native/opengl/BufferGL.h"
#include "dawn/native/opengl/CommandBufferGL.h"
#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/EGLFunctions.h"
#include "dawn/native/opengl/TextureGL.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::opengl {

ResultOrError<Ref<Queue>> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    return AcquireRef(new Queue(device, descriptor));
}

Queue::Queue(Device* device, const QueueDescriptor* descriptor)
    : QueueBase(device, descriptor),
      mEGLSyncType(device->GetEGLExtensions()[EGLExtension::FenceSyncKHR] ? EGL_SYNC_FENCE_KHR
                                                                          : EGL_SYNC_REUSABLE_KHR) {
    DAWN_ASSERT(device->GetEGLExtensions()[EGLExtension::FenceSyncKHR] ||
                device->GetEGLExtensions()[EGLExtension::ReusableSyncKHR]);
}

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
    TextureCopy textureCopy;
    textureCopy.texture = destination.texture;
    textureCopy.mipLevel = destination.mipLevel;
    textureCopy.origin = destination.origin;
    textureCopy.aspect = SelectFormatAspects(destination.texture->GetFormat(), destination.aspect);

    SubresourceRange range = GetSubresourcesAffectedByCopy(textureCopy, writeSizePixel);
    if (IsCompleteSubresourceCopiedTo(destination.texture, writeSizePixel, destination.mipLevel,
                                      destination.aspect)) {
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

GLenum Queue::ClientWaitSync(EGLSyncKHR sync, Nanoseconds timeout) {
    const Device* device = ToBackend(GetDevice());
    const EGLFunctions& egl = device->GetEGL(/*makeCurrent=*/false);

    return egl.ClientWaitSyncKHR(device->GetEGLDisplay(), sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                                 uint64_t(timeout));
}

ResultOrError<bool> Queue::WaitForQueueSerial(ExecutionSerial serial, Nanoseconds timeout) {
    // Search for the first fence >= serial.
    return mFencesInFlight.Use([&](auto fencesInFlight) -> ResultOrError<bool> {
        EGLSyncKHR waitSync = nullptr;
        for (auto it = fencesInFlight->begin(); it != fencesInFlight->end(); ++it) {
            if (it->second >= serial) {
                waitSync = it->first;
                break;
            }
        }
        if (waitSync == nullptr) {
            // Fence sync not found. This serial must have already completed.
            // Return a success status.
            return true;
        }

        // Wait for the fence sync.
        GLenum result = ClientWaitSync(waitSync, timeout);
        switch (result) {
            case EGL_TIMEOUT_EXPIRED_KHR:
                return false;
            case EGL_CONDITION_SATISFIED_KHR:
                return true;
            case EGL_FALSE:
                return DAWN_INTERNAL_ERROR("glClientWaitSync failed");
            default:
                DAWN_UNREACHABLE();
        }
    });
}

void Queue::SubmitFenceSync() {
    mFencesInFlight.Use([&](auto fencesInFlight) {
        if (!mHasPendingCommands) {
            return;
        }
        const Device* device = ToBackend(GetDevice());
        const EGLFunctions& egl = device->GetEGL(/*makeCurrent=*/true);

        EGLSyncKHR sync = egl.CreateSyncKHR(device->GetEGLDisplay(), mEGLSyncType, nullptr);
        DAWN_ASSERT(sync != EGL_NO_SYNC_KHR);

        if (mEGLSyncType == EGL_SYNC_REUSABLE_KHR) {
            EGLBoolean status = egl.SignalSyncKHR(device->GetEGLDisplay(), sync, EGL_SIGNALED_KHR);
            DAWN_ASSERT(status == EGL_TRUE);
        }

        IncrementLastSubmittedCommandSerial();
        fencesInFlight->emplace_back(sync, GetLastSubmittedCommandSerial());
        mHasPendingCommands = false;
    });
}

bool Queue::HasPendingCommands() const {
    return mHasPendingCommands;
}

MaybeError Queue::SubmitPendingCommands() {
    SubmitFenceSync();
    return {};
}

ResultOrError<ExecutionSerial> Queue::CheckAndUpdateCompletedSerials() {
    const Device* device = ToBackend(GetDevice());
    const EGLFunctions& egl = device->GetEGL(/*makeCurrent=*/false);
    EGLDisplay display = device->GetEGLDisplay();

    return mFencesInFlight.Use([&](auto fencesInFlight) {
        ExecutionSerial fenceSerial{0};
        while (!fencesInFlight->empty()) {
            auto [sync, tentativeSerial] = fencesInFlight->front();

            // Fence are added in order, so we can stop searching as soon
            // as we see one that's not ready.
            GLenum result = ClientWaitSync(sync, Nanoseconds(0));
            if (result == EGL_TIMEOUT_EXPIRED_KHR) {
                return fenceSerial;
            }
            // Update fenceSerial since fence is ready.
            fenceSerial = tentativeSerial;

            egl.DestroySyncKHR(display, sync);

            fencesInFlight->pop_front();

            DAWN_ASSERT(fenceSerial > GetCompletedCommandSerial());
        }
        return fenceSerial;
    });
}

void Queue::ForceEventualFlushOfCommands() {
    mHasPendingCommands = true;
}

MaybeError Queue::WaitForIdleForDestruction() {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->GetGL();
    gl.Finish();
    DAWN_TRY(CheckPassedSerials());
    DAWN_ASSERT(mFencesInFlight->empty());
    return {};
}

}  // namespace dawn::native::opengl
