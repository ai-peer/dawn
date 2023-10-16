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

#include "dawn/native/metal/QueueMTL.h"

#include "dawn/common/Math.h"
#include "dawn/native/Buffer.h"
#include "dawn/native/CommandValidation.h"
#include "dawn/native/Commands.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/MetalBackend.h"
#include "dawn/native/metal/CommandBufferMTL.h"
#include "dawn/native/metal/DeviceMTL.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::metal {

ResultOrError<Ref<Queue>> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    Ref<Queue> queue = AcquireRef(new Queue(device, descriptor));
    DAWN_TRY(queue->Initialize());
    return queue;
}

Queue::Queue(Device* device, const QueueDescriptor* descriptor)
    : QueueBase(device, descriptor), mCompletedSerial(0) {}

Queue::~Queue() = default;

void Queue::DestroyImpl() {
    QueueBase::DestroyImpl();
    // Forget all pending commands.
    mCommandContext.AcquireCommands();
    UpdateWaitingEvents(kMaxExecutionSerial);
    mCommandQueue = nullptr;
    mLastSubmittedCommands = nullptr;
    mMtlSharedEvent = nullptr;
}

MaybeError Queue::Initialize() {
    id<MTLDevice> mtlDevice = ToBackend(GetDevice())->GetMTLDevice();

    mCommandQueue.Acquire([mtlDevice newCommandQueue]);
    if (mCommandQueue == nil) {
        return DAWN_INTERNAL_ERROR("Failed to allocate MTLCommandQueue.");
    }

    if (@available(macOS 10.14, iOS 12.0, *)) {
        mMtlSharedEvent.Acquire([mtlDevice newSharedEvent]);
    }

    return mCommandContext.PrepareNextCommandBuffer(*mCommandQueue);
}

void Queue::UpdateWaitingEvents(ExecutionSerial completedSerial) {
    mWaitingEvents.Use([&](auto signals) {
        for (auto& s : signals->senders.IterateUpTo(completedSerial)) {
            std::move(s).Signal();
        }
        signals->senders.ClearUpTo(completedSerial);
    });
}

MaybeError Queue::WaitForIdleForDestruction() {
    // Forget all pending commands.
    mCommandContext.AcquireCommands();
    DAWN_TRY(CheckPassedSerials());

    // Wait for all commands to be finished so we can free resources
    while (GetCompletedCommandSerial() != GetLastSubmittedCommandSerial()) {
        usleep(100);
        DAWN_TRY(CheckPassedSerials());
    }

    return {};
}

void Queue::WaitForCommandsToBeScheduled() {
    if (GetDevice()->ConsumedError(SubmitPendingCommandBuffer())) {
        return;
    }

    // Only lock the object while we take a reference to it, otherwise we could block further
    // progress if the driver calls the scheduled handler (which also acquires the lock) before
    // finishing the waitUntilScheduled.
    NSPRef<id<MTLCommandBuffer>> lastSubmittedCommands;
    {
        std::lock_guard<std::mutex> lock(mLastSubmittedCommandsMutex);
        lastSubmittedCommands = mLastSubmittedCommands;
    }
    [*lastSubmittedCommands waitUntilScheduled];
}

CommandRecordingContext* Queue::GetPendingCommandContext(SubmitMode submitMode) {
    if (submitMode == SubmitMode::Normal) {
        mCommandContext.SetNeedsSubmit();
    }
    mCommandContext.MarkUsed();
    return &mCommandContext;
}

MaybeError Queue::SubmitPendingCommandBuffer() {
    if (!mCommandContext.NeedsSubmit()) {
        return {};
    }

    auto platform = GetDevice()->GetPlatform();

    IncrementLastSubmittedCommandSerial();

    // Acquire the pending command buffer, which is retained. It must be released later.
    NSPRef<id<MTLCommandBuffer>> pendingCommands = mCommandContext.AcquireCommands();

    // Replace mLastSubmittedCommands with the mutex held so we avoid races between the
    // schedule handler and this code.
    {
        std::lock_guard<std::mutex> lock(mLastSubmittedCommandsMutex);
        mLastSubmittedCommands = pendingCommands;
    }

    // Make a local copy of the pointer to the commands because it's not clear how ObjC blocks
    // handle types with copy / move constructors being referenced in the block.
    id<MTLCommandBuffer> pendingCommandsPointer = pendingCommands.Get();
    [*pendingCommands addScheduledHandler:^(id<MTLCommandBuffer>) {
        // This is DRF because we hold the mutex for mLastSubmittedCommands and pendingCommands
        // is a local value (and not the member itself).
        std::lock_guard<std::mutex> lock(mLastSubmittedCommandsMutex);
        if (this->mLastSubmittedCommands.Get() == pendingCommandsPointer) {
            this->mLastSubmittedCommands = nullptr;
        }
    }];

    // Update the completed serial once the completed handler is fired. Make a local copy of
    // mLastSubmittedSerial so it is captured by value.
    ExecutionSerial pendingSerial = GetLastSubmittedCommandSerial();
    // this ObjC block runs on a different thread
    [*pendingCommands addCompletedHandler:^(id<MTLCommandBuffer>) {
        TRACE_EVENT_ASYNC_END0(platform, GPUWork, "DeviceMTL::SubmitPendingCommandBuffer",
                               uint64_t(pendingSerial));
        DAWN_ASSERT(uint64_t(pendingSerial) > mCompletedSerial.load());
        this->mCompletedSerial = uint64_t(pendingSerial);

        this->UpdateWaitingEvents(pendingSerial);
    }];

    TRACE_EVENT_ASYNC_BEGIN0(platform, GPUWork, "DeviceMTL::SubmitPendingCommandBuffer",
                             uint64_t(pendingSerial));
    if (@available(macOS 10.14, *)) {
        id rawEvent = *mMtlSharedEvent;
        id<MTLSharedEvent> sharedEvent = static_cast<id<MTLSharedEvent>>(rawEvent);
        [*pendingCommands encodeSignalEvent:sharedEvent value:static_cast<uint64_t>(pendingSerial)];
    }
    [*pendingCommands commit];

    return mCommandContext.PrepareNextCommandBuffer(*mCommandQueue);
}

void Queue::ExportLastSignaledEvent(ExternalImageMTLSharedEventDescriptor* desc) {
    // Ensure commands are submitted before getting the last submited serial.
    // Ignore the error since we still want to export the serial of the last successful
    // submission - that was the last serial that was actually signaled.
    ForceEventualFlushOfCommands();

    if (GetDevice()->ConsumedError(SubmitPendingCommandBuffer())) {
        desc->sharedEvent = nullptr;
        return;
    }

    desc->sharedEvent = *mMtlSharedEvent;
    desc->signaledValue = static_cast<uint64_t>(GetLastSubmittedCommandSerial());
}

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    @autoreleasepool {
        CommandRecordingContext* commandContext = GetPendingCommandContext();

        TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording, "CommandBufferMTL::FillCommands");
        for (uint32_t i = 0; i < commandCount; ++i) {
            DAWN_TRY(ToBackend(commands[i])->FillCommands(commandContext));
        }
        TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording, "CommandBufferMTL::FillCommands");

        DAWN_TRY(SubmitPendingCommandBuffer());

        return {};
    }
}

bool Queue::HasPendingCommands() const {
    return mCommandContext.NeedsSubmit();
}

ResultOrError<ExecutionSerial> Queue::CheckAndUpdateCompletedSerials() {
    uint64_t frontendCompletedSerial{GetCompletedCommandSerial()};
    // sometimes we increase the serials, in which case the completed serial in
    // the device base will surpass the completed serial we have in the metal backend, so we
    // must update ours when we see that the completed serial from device base has
    // increased.
    //
    // This update has to be atomic otherwise there is a race with the `addCompletedHandler`
    // call below and this call could set the mCompletedSerial backwards.
    uint64_t current = mCompletedSerial.load();
    while (frontendCompletedSerial > current &&
           !mCompletedSerial.compare_exchange_weak(current, frontendCompletedSerial)) {
    }

    return ExecutionSerial(mCompletedSerial.load());
}

void Queue::ForceEventualFlushOfCommands() {
    if (mCommandContext.WasUsed()) {
        mCommandContext.SetNeedsSubmit();
    }
}

class Queue::CompletionEvent : public EventManager::TrackedEvent {
  public:
    CompletionEvent(Queue* queue, ExecutionSerial serial, SystemEventReceiver&& receiver);
    ~CompletionEvent() override;

    bool MustWaitUsingDevice() const override;
    void Complete(EventCompletionType) override;

  private:
    WeakRef<Queue> mQueue;
    ExecutionSerial mSerial;
};

Queue::CompletionEvent::CompletionEvent(Queue* queue,
                                        ExecutionSerial serial,
                                        SystemEventReceiver&& receiver)
    : TrackedEvent(queue->GetDevice(), wgpu::CallbackMode::WaitAnyOnly, std::move(receiver)),
      mQueue(GetWeakRef(queue)),
      mSerial(serial) {}

Queue::CompletionEvent::~CompletionEvent() {
    EnsureComplete(EventCompletionType::Shutdown);
}

bool Queue::CompletionEvent::MustWaitUsingDevice() const {
    return false;
}

void Queue::CompletionEvent::Complete(EventCompletionType) {
    auto queue = mQueue.Promote();
    if (queue != nullptr) {
        queue->UntrackCompletionEvent(mSerial);
    }
}

Ref<EventManager::TrackedEvent> Queue::GetOrCreateCompletionEvent(ExecutionSerial serial) {
    return mWaitingEvents.Use([&](auto signals) {
        // Check to see if we have a receiver already.
        auto it = signals->receivers.find(serial);
        if (it != signals->receivers.end()) {
            return it->second;
        }

        SystemEventReceiver receiver;
        if (serial <= ExecutionSerial(mCompletedSerial.load(std::memory_order_acquire))) {
            // Now that we hold the lock, check against mCompletedSerial before inserting.
            // This serial may have just completed. If it did, make an already-signaled
            // receiver.
            receiver = SystemEventReceiver::CreateAlreadySignaled();
        } else {
            // Create a new pipe and insert the sender into the list which will be
            // signaled inside Metal's queue completion handler.
            SystemEventPipeSender sender;
            std::tie(sender, receiver) = CreateSystemEventPipe();
            signals->senders.Enqueue(std::move(sender), serial);
        }

        bool inserted;
        std::tie(it, inserted) = signals->receivers.emplace(
            serial, AcquireRef(new CompletionEvent(this, serial, std::move(receiver))));
        DAWN_ASSERT(inserted);
        return it->second;
    });
}

void Queue::UntrackCompletionEvent(ExecutionSerial serial) {
    mWaitingEvents.Use([&](auto signals) { signals->receivers.erase(serial); });
}

}  // namespace dawn::native::metal
