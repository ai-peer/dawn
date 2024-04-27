// Copyright 2023 The Dawn & Tint Authors
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

#include "dawn/native/d3d11/QueueD3D11.h"

#include <limits>
#include <utility>

#include "dawn/native/WaitAnySystemEvent.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/CommandBufferD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/SharedFenceD3D11.h"
#include "dawn/native/d3d11/TextureD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::d3d11 {

ResultOrError<Ref<Queue>> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    Ref<Queue> queue = AcquireRef(new Queue(device, descriptor));
    DAWN_TRY(queue->Initialize());
    return queue;
}

MaybeError Queue::Initialize() {
    // Create the fence.
    DAWN_TRY(CheckHRESULT(ToBackend(GetDevice())
                              ->GetD3D11Device5()
                              ->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&mFence)),
                          "D3D11: creating fence"));

    DAWN_TRY_ASSIGN(mSharedFence, SharedFence::Create(ToBackend(GetDevice()),
                                                      "Internal shared DXGI fence", mFence));

    return {};
}

MaybeError Queue::InitializePendingContext() {
    // Initialize mPendingCommands. After this, calls to the use the command context
    // are thread safe.
    CommandRecordingContext commandContext;
    DAWN_TRY(commandContext.Initialize(ToBackend(GetDevice())));

    mPendingCommands.Use(
        [&](auto pendingCommandContext) { *pendingCommandContext = std::move(commandContext); });

    // Configure the command context's uniform buffer. This is used to emulate builtins.
    // Creating the buffer is done outside of Initialize because it requires mPendingCommands
    // to already be initialized.
    Ref<BufferBase> uniformBuffer;
    DAWN_TRY_ASSIGN(uniformBuffer,
                    CommandRecordingContext::CreateInternalUniformBuffer(GetDevice()));
    mPendingCommands->SetInternalUniformBuffer(std::move(uniformBuffer));

    return {};
}

void Queue::DestroyImpl() {
    // Release the shared fence here to prevent a ref-cycle with the device, but do not destroy the
    // underlying native fence so that we can return a SharedFence on EndAccess after destruction.
    mSharedFence = nullptr;

    mPendingCommands.Use([&](auto pendingCommands) {
        pendingCommands->Destroy();
        mPendingCommandsNeedSubmit.store(false, std::memory_order_release);
    });
}

ResultOrError<Ref<d3d::SharedFence>> Queue::GetOrCreateSharedFence() {
    if (mSharedFence == nullptr) {
        DAWN_ASSERT(!IsAlive());
        return SharedFence::Create(ToBackend(GetDevice()), "Internal shared DXGI fence", mFence);
    }
    return mSharedFence;
}

ScopedCommandRecordingContext Queue::GetScopedPendingCommandContext(SubmitMode submitMode) {
    return mPendingCommands.Use([&](auto commands) {
        if (submitMode == SubmitMode::Normal) {
            mPendingCommandsNeedSubmit.store(true, std::memory_order_release);
        }
        return ScopedCommandRecordingContext(std::move(commands));
    });
}

ScopedSwapStateCommandRecordingContext Queue::GetScopedSwapStatePendingCommandContext(
    SubmitMode submitMode) {
    return mPendingCommands.Use([&](auto commands) {
        if (submitMode == SubmitMode::Normal) {
            mPendingCommandsNeedSubmit.store(true, std::memory_order_release);
        }
        return ScopedSwapStateCommandRecordingContext(std::move(commands));
    });
}

MaybeError Queue::SubmitPendingCommands() {
    bool needsSubmit = mPendingCommands.Use([&](auto pendingCommands) {
        pendingCommands->ReleaseKeyedMutexes();
        return mPendingCommandsNeedSubmit.exchange(false, std::memory_order_acq_rel);
    });
    if (needsSubmit) {
        return NextSerial();
    }
    return {};
}

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    // CommandBuffer::Execute() will modify the state of the global immediate device context, it may
    // affect following usage of it.
    // TODO(dawn:1770): figure how if we need to track and restore the state of the immediate device
    // context.
    TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording, "CommandBufferD3D11::Execute");
    {
        auto commandContext =
            GetScopedSwapStatePendingCommandContext(QueueBase::SubmitMode::Normal);
        for (uint32_t i = 0; i < commandCount; ++i) {
            DAWN_TRY(ToBackend(commands[i])->Execute(&commandContext));
        }
    }
    DAWN_TRY(SubmitPendingCommands());
    TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording, "CommandBufferD3D11::Execute");

    return {};
}

MaybeError Queue::CheckAndMapReadyBuffers(ExecutionSerial completedSerial) {
    auto commandContext = GetScopedPendingCommandContext(QueueBase::SubmitMode::Passive);
    for (auto buffer : mPendingMapBuffers.IterateUpTo(completedSerial)) {
        DAWN_TRY(buffer->FinalizeMap(&commandContext, completedSerial));
    }
    mPendingMapBuffers.ClearUpTo(completedSerial);
    return {};
}

void Queue::TrackPendingMapBuffer(Ref<Buffer>&& buffer, ExecutionSerial readySerial) {
    mPendingMapBuffers.Enqueue(buffer, readySerial);
}

MaybeError Queue::WriteBufferImpl(BufferBase* buffer,
                                  uint64_t bufferOffset,
                                  const void* data,
                                  size_t size) {
    if (size == 0) {
        // skip the empty write
        return {};
    }

    auto commandContext = GetScopedPendingCommandContext(QueueBase::SubmitMode::Normal);
    return ToBackend(buffer)->Write(&commandContext, bufferOffset, data, size);
}

MaybeError Queue::WriteTextureImpl(const ImageCopyTexture& destination,
                                   const void* data,
                                   size_t dataSize,
                                   const TextureDataLayout& dataLayout,
                                   const Extent3D& writeSizePixel) {
    if (writeSizePixel.width == 0 || writeSizePixel.height == 0 ||
        writeSizePixel.depthOrArrayLayers == 0) {
        return {};
    }

    auto commandContext = GetScopedPendingCommandContext(QueueBase::SubmitMode::Normal);
    TextureCopy textureCopy;
    textureCopy.texture = destination.texture;
    textureCopy.mipLevel = destination.mipLevel;
    textureCopy.origin = destination.origin;
    textureCopy.aspect = SelectFormatAspects(destination.texture->GetFormat(), destination.aspect);

    SubresourceRange subresources = GetSubresourcesAffectedByCopy(textureCopy, writeSizePixel);

    Texture* texture = ToBackend(destination.texture);
    DAWN_TRY(texture->SynchronizeTextureBeforeUse(&commandContext));
    return texture->Write(&commandContext, subresources, destination.origin, writeSizePixel,
                          static_cast<const uint8_t*>(data) + dataLayout.offset,
                          dataLayout.bytesPerRow, dataLayout.rowsPerImage);
}

bool Queue::HasPendingCommands() const {
    return mPendingCommandsNeedSubmit.load(std::memory_order_acquire);
}

ResultOrError<ExecutionSerial> Queue::CheckAndUpdateCompletedSerials() {
    if (!mPendingEvents.empty()) {
        StackVector<HANDLE, 8> handles;
        handles->reserve(mPendingEvents.size());
        // Gether events in reversed order.
        for (auto it = mPendingEvents.rbegin(); it != mPendingEvents.rend(); ++it) {
            handles->push_back(it->receiver.GetPrimitive().Get());
        }
        DWORD result = WaitForMultipleObjects(handles->size(), handles->data(), /*bWaitAll=*/false,
                                              /*dwMilliseconds=*/0);
        DAWN_INTERNAL_ERROR_IF(result == WAIT_FAILED, "WaitForMultipleObjects() failed");
        if (result != WAIT_TIMEOUT) {
            DAWN_CHECK(result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + mPendingEvents.size());
            const size_t completedEvents = mPendingEvents.size() - (result - WAIT_OBJECT_0);
            std::vector<SystemEventReceiver> receivers;
            receivers.reserve(completedEvents);
            for (auto it = mPendingEvents.begin(); it != mPendingEvents.begin() + completedEvents;
                 ++it) {
                receivers.emplace_back(std::move(it->receiver));
            }
            mPendingEvents.erase(mPendingEvents.begin(), mPendingEvents.begin() + completedEvents);

            DAWN_TRY(ReturnSystemEventReceivers(std::move(receivers)));
        }
    }

    ExecutionSerial completedSerial(uint64_t(GetLastSubmittedCommandSerial()) -
                                    mPendingEvents.size());

    DAWN_TRY(CheckAndMapReadyBuffers(completedSerial));

    return completedSerial;
}

void Queue::ForceEventualFlushOfCommands() {}

ResultOrError<bool> Queue::WaitForQueueSerial(ExecutionSerial serial, Nanoseconds timeout) {
    ExecutionSerial completedSerial = GetCompletedCommandSerial();
    if (serial <= completedSerial) {
        return true;
    }

    if (serial > GetLastSubmittedCommandSerial()) {
        return DAWN_FORMAT_INTERNAL_ERROR(
            "Wait a serial(%llu) which is greater than last submitted command serial(%llu).",
            uint64_t(serial), uint64_t(GetLastSubmittedCommandSerial()));
    }

    DAWN_ASSERT(!mPendingEvents.empty());
    DAWN_ASSERT(serial >= mPendingEvents.front().serial);
    DAWN_ASSERT(serial <= mPendingEvents.back().serial);
    auto it = std::lower_bound(
        mPendingEvents.begin(), mPendingEvents.end(), serial,
        [](const SerialEventReceiverPair& a, const ExecutionSerial& b) { return a.serial < b; });
    DAWN_ASSERT(it != mPendingEvents.end());
    DAWN_ASSERT(it->serial == serial);

    DWORD result = WaitForSingleObject(it->receiver.GetPrimitive().Get(), ToMilliseconds(timeout));
    DAWN_INTERNAL_ERROR_IF(result == WAIT_FAILED, "WaitForSingleObject() failed");

    if (result == WAIT_OBJECT_0) {
        // Events before |it| should be signalled as well.
        std::vector<SystemEventReceiver> receivers;
        ++it;
        for (auto iit = mPendingEvents.begin(); iit != it; ++iit) {
            receivers.emplace_back(std::move(iit->receiver));
        }
        mPendingEvents.erase(mPendingEvents.begin(), it);
        DAWN_TRY(ReturnSystemEventReceivers(std::move(receivers)));
        return true;
    }

    return false;
}

MaybeError Queue::WaitForIdleForDestruction() {
    DAWN_TRY(NextSerial());
    // Wait for all in-flight commands to finish executing
    DAWN_TRY_ASSIGN(std::ignore, WaitForQueueSerial(GetLastSubmittedCommandSerial(),
                                                    std::numeric_limits<Nanoseconds>::max()));
    return CheckPassedSerials();
}

MaybeError Queue::NextSerial() {
    IncrementLastSubmittedCommandSerial();
    ExecutionSerial lastSubmittedSerial = GetLastSubmittedCommandSerial();
    // TODO(penghuang): only signal fence when it is needed.
    TRACE_EVENT1(GetDevice()->GetPlatform(), General, "D3D11Device::SignalFence", "serial",
                 uint64_t(lastSubmittedSerial));

    auto commandContext = GetScopedPendingCommandContext(SubmitMode::Passive);
    DAWN_TRY(CheckHRESULT(commandContext.Signal(mFence.Get(), uint64_t(lastSubmittedSerial)),
                          "D3D11 command queue signal fence"));

    SystemEventReceiver receiver;
    DAWN_TRY_ASSIGN(receiver, GetSystemEventReceiver());
    commandContext.Flush1(D3D11_CONTEXT_TYPE_ALL, receiver.GetPrimitive().Get());

    DAWN_ASSERT(mPendingEvents.empty() ||
                uint64_t(mPendingEvents.back().serial) + 1 == uint64_t(lastSubmittedSerial));
    mPendingEvents.push_back({lastSubmittedSerial, std::move(receiver)});

    return {};
}

void Queue::SetEventOnCompletion(ExecutionSerial serial, HANDLE event) {
    mFence->SetEventOnCompletion(static_cast<uint64_t>(serial), event);
}

}  // namespace dawn::native::d3d11
