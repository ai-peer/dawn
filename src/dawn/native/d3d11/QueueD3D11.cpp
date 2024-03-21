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

#include <utility>

#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/CommandBufferD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/SharedFenceD3D11.h"
#include "dawn/native/d3d11/TextureD3D11.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

#define USE_ENQUEUE_SET_EVENT 1
#define USE_QUERY 0

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

    // Create the fence event.
    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    DAWN_ASSERT(mFenceEvent != nullptr);

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
    while (!mAvailableEvents.empty()) {
        ::CloseHandle(mAvailableEvents.back());
        mAvailableEvents.pop_back();
    }

    while (!mPendingEvents.empty()) {
        ::CloseHandle(mPendingEvents.back());
        mPendingEvents.pop_back();
    }

    if (mFenceEvent != nullptr) {
        ::CloseHandle(mFenceEvent);
        mFenceEvent = nullptr;
    }

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
#if (!USE_ENQUEUE_SET_EVENT) && (!USE_QUERY)
    ExecutionSerial completedSerial = ExecutionSerial(mFence->GetCompletedValue());
#endif
#if (USE_ENQUEUE_SET_EVENT)
    ExecutionSerial completedSerial = GetCompletedCommandSerial();
    while (!mPendingEvents.empty()) {
        HANDLE hEvent = mPendingEvents.front();
        DWORD result = WaitForSingleObject(hEvent, 0);
        if (result == WAIT_TIMEOUT) {
            break;
        }
        if (result == WAIT_FAILED) {
            completedSerial = ExecutionSerial(UINT64_MAX);
            break;
        }
        ++completedSerial;
        mAvailableEvents.push_back(hEvent);
        mPendingEvents.pop_front();
    }
#endif
#if (USE_QUERY)
    ExecutionSerial completedSerial = GetCompletedCommandSerial();
    {
        auto commandContext = GetScopedPendingCommandContext(SubmitMode::Passive);
        while (!mPendingQueries.empty()) {
            ComPtr<ID3D11Query> query = mPendingQueries.front();
            HRESULT hr = commandContext.GetData(query.Get(), /*pData=*/nullptr, /*DataSize=*/0,
                                                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (hr == S_FALSE) {
                break;
            }
            DAWN_ASSERT(hr == S_OK);
            ++completedSerial;
            mAvailableQueries.push_back(std::move(query));
            mPendingQueries.pop_front();
        }
    }
#endif
    if (DAWN_UNLIKELY(completedSerial == ExecutionSerial(UINT64_MAX))) {
        // GetCompletedValue returns UINT64_MAX if the device was removed.
        // Try to query the failure reason.
        ID3D11Device* d3d11Device = ToBackend(GetDevice())->GetD3D11Device();
        DAWN_TRY(CheckHRESULT(d3d11Device->GetDeviceRemovedReason(),
                              "ID3D11Device::GetDeviceRemovedReason"));
        // Otherwise, return a generic device lost error.
        return DAWN_DEVICE_LOST_ERROR("Device lost");
    }

    if (completedSerial <= GetCompletedCommandSerial()) {
        return ExecutionSerial(0);
    }

    DAWN_TRY(CheckAndMapReadyBuffers(completedSerial));

    return completedSerial;
}

void Queue::ForceEventualFlushOfCommands() {}

MaybeError Queue::WaitForIdleForDestruction() {
    DAWN_TRY(NextSerial());
    // Wait for all in-flight commands to finish executing
    DAWN_TRY(WaitForSerial(GetLastSubmittedCommandSerial()));

    return {};
}

MaybeError Queue::NextSerial() {
    IncrementLastSubmittedCommandSerial();

    TRACE_EVENT1(GetDevice()->GetPlatform(), General, "D3D11Device::SignalFence", "serial",
                 uint64_t(GetLastSubmittedCommandSerial()));

    auto commandContext = GetScopedPendingCommandContext(SubmitMode::Passive);

#if (!USE_ENQUEUE_SET_EVENT) && (!USE_QUERY)
    DAWN_TRY(
        CheckHRESULT(commandContext.Signal(mFence.Get(), uint64_t(GetLastSubmittedCommandSerial())),
                     "D3D11 command queue signal fence"));
#endif

#if (USE_ENQUEUE_SET_EVENT)
    HANDLE hEvent = NULL;
    if (!mAvailableEvents.empty()) {
        hEvent = mAvailableEvents.back();
        ResetEvent(hEvent);
        mAvailableEvents.pop_back();
    } else {
        hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        DAWN_ASSERT(hEvent != nullptr);
    }
    ToBackend(GetDevice())->GetDXGIDevice3()->EnqueueSetEvent(hEvent);
    mPendingEvents.push_back(hEvent);
#endif

#if (USE_QUERY)
    ComPtr<ID3D11Query> query;
    if (!mAvailableQueries.empty()) {
        query = std::move(mAvailableQueries.back());
        mAvailableQueries.pop_back();
    } else {
        const D3D11_QUERY_DESC desc{D3D11_QUERY_EVENT, 0};

        DAWN_TRY(CheckHRESULT(ToBackend(GetDevice())->GetD3D11Device5()->CreateQuery(&desc, &query),
                              "D3D11 create query failed!"));
    }
    commandContext.End(query.Get());
    mPendingQueries.push_back(std::move(query));
#endif
    return {};
}

MaybeError Queue::WaitForSerial(ExecutionSerial serial) {
    DAWN_TRY(CheckPassedSerials());
    if (GetCompletedCommandSerial() >= serial) {
        return {};
    }

#if (!USE_ENQUEUE_SET_EVENT) && (!USE_QUERY)
    DAWN_TRY(CheckHRESULT(mFence->SetEventOnCompletion(uint64_t(serial), mFenceEvent),
                          "D3D11 set event on completion"));
    WaitForSingleObject(mFenceEvent, INFINITE);
#endif
#if (USE_ENQUEUE_SET_EVENT)
    uint64_t pendingEventIndex =
        static_cast<uint64_t>(serial) - static_cast<uint64_t>(GetCompletedCommandSerial()) - 1;
    HANDLE hEvent = mPendingEvents.at(pendingEventIndex);
    DWORD result = WaitForSingleObject(hEvent, INFINITE);
    DAWN_ASSERT(result == WAIT_OBJECT_0);
#endif
#if (USE_QUERY)
    // Flush and wait.
    {
        auto commandContext = GetScopedPendingCommandContext(SubmitMode::Passive);
        HANDLE hEvent = CreateEvent(nullptr, false, false, nullptr);
        commandContext.Flush1(D3D11_CONTEXT_TYPE_ALL, hEvent);
        DWORD result = WaitForSingleObject(hEvent, INFINITE);
        DAWN_ASSERT(result == WAIT_OBJECT_0);
        CloseHandle(hEvent);
    }
#endif
    return CheckPassedSerials();
}

void Queue::SetEventOnCompletion(ExecutionSerial serial, HANDLE event) {
#if (!USE_ENQUEUE_SET_EVENT) && (!USE_QUERY)
    mFence->SetEventOnCompletion(static_cast<uint64_t>(serial), event);
#endif
#if (USE_ENQUEUE_SET_EVENT)
    ToBackend(GetDevice())->GetDXGIDevice3()->EnqueueSetEvent(event);
#endif
#if (USE_QUERY)
    auto commandContext = GetScopedPendingCommandContext(SubmitMode::Passive);
    commandContext.Flush1(D3D11_CONTEXT_TYPE_ALL, event);
#endif
}

}  // namespace dawn::native::d3d11
