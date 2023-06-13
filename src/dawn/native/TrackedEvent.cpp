// Copyright 2023 The Dawn Authors
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

#include "dawn/native/TrackedEvent.h"

#include <utility>

#include "dawn/native/Device.h"
#include "dawn/native/Instance.h"
#include "dawn/native/OSEvent.h"
#include "dawn/native/Queue.h"

namespace dawn::native {

namespace {

WGPUCallbackModeFlags ValidateCallbackMode(WGPUCallbackModeFlags mode) {
    static constexpr WGPUCallbackModeFlags kAllFlags =
        WGPUCallbackMode_Future | WGPUCallbackMode_ProcessEvents | WGPUCallbackMode_Spontaneous;

    // These cases are undefined behaviors according to the API contract.
    ASSERT(mode != 0);
    ASSERT((mode & kAllFlags) == mode);
    ASSERT((mode & WGPUCallbackMode_Future) == 0 || (mode & WGPUCallbackMode_ProcessEvents) == 0);
    return mode;
}

}  // namespace

// TrackedEvent

TrackedEvent::TrackedEvent(InstanceBase* instance, WGPUCallbackModeFlags callbackMode)
    : mInstance(instance), mCallbackMode(ValidateCallbackMode(callbackMode)) {}

TrackedEvent::~TrackedEvent() {
    ASSERT(mCompleted);
}

OSEventPrimitive::T TrackedEvent::GetPrimitive() const {
    return mReceiver.Get();
}

void TrackedEvent::SetReceiver(OSEventReceiver&& receiver) {
    mReceiver = std::move(receiver);
}

bool TrackedEvent::IsEarlyReady() const {
    return mEarlyReady;
}

void TrackedEvent::EnsureCompleteFromProcessEvents() {
    ASSERT(mCallbackMode & WGPUCallbackMode_ProcessEvents);
    EnsureComplete();
}

void TrackedEvent::EnsureCompleteFromWaitAny() {
    ASSERT(mCallbackMode & WGPUCallbackMode_Future);
    EnsureComplete();
}

void TrackedEvent::EnsureComplete() {
    bool alreadyComplete = mCompleted.exchange(true);
    if (!alreadyComplete) {
        Complete();
    }
}

TrackedEvent::WaitRef TrackedEvent::TakeWaitRef() {
    return WaitRef{this};
}

void TrackedEvent::TriggerEarlyFailure() {
    mEarlyReady = true;
    if (mCallbackMode & WGPUCallbackMode_Spontaneous) {
        bool alreadyComplete = mCompleted.exchange(true);
        // If it was already complete, but there was an error, we have no place
        // to report it, so ASSERT. This shouldn't happen.
        ASSERT(!alreadyComplete);
        Complete();
    }
}

// TrackedEvent::WaitRef

TrackedEvent::WaitRef::WaitRef(TrackedEvent* event) : mRef(event) {
#if DAWN_ENABLE_ASSERTS
    bool wasAlreadyWaited = mRef->mCurrentlyBeingWaited.exchange(true);
    ASSERT(!wasAlreadyWaited);
#endif
}

TrackedEvent::WaitRef::~WaitRef() {
#if DAWN_ENABLE_ASSERTS
    if (mRef.Get() != nullptr) {
        bool wasAlreadyWaited = mRef->mCurrentlyBeingWaited.exchange(false);
        ASSERT(wasAlreadyWaited);
    }
#endif
}

TrackedEvent* TrackedEvent::WaitRef::operator->() {
    return mRef.Get();
}

const TrackedEvent* TrackedEvent::WaitRef::operator->() const {
    return mRef.Get();
}

// WorkDoneEvent

FutureID WorkDoneEvent::Create(QueueBase* queue,
                               const WGPUQueueWorkDoneCallbackInfo& callbackInfo) {
    InstanceBase* instance = queue->GetInstance();
    Ref<WorkDoneEvent> event = AcquireRef(new WorkDoneEvent(queue, callbackInfo));

    WGPUQueueWorkDoneStatus validationEarlyStatus;
    if (instance->ConsumedError(event->Validate(&validationEarlyStatus))) {
        event->FailEarly(validationEarlyStatus);
        event->mReceiver = OSEventReceiver::CreateAlreadySignaled();
    } else {
        event->mReceiver = queue->InsertWorkDoneEvent();
    }

    // TODO(crbug.com/dawn/1987): Should check for early completions here, so
    // when there's an error, spontaneous callbacks can get called immediately.

    // FIXME: Handle WGPUCallbackMode_ProcessEvents

    return instance->GetEventManager().Track(callbackInfo.mode, event.Get());
}

WorkDoneEvent::WorkDoneEvent(QueueBase* queue, const WGPUQueueWorkDoneCallbackInfo& callbackInfo)
    : TrackedEvent(queue->GetInstance(), callbackInfo.mode),
      mQueue(queue),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata) {}

void WorkDoneEvent::FailEarly(WGPUQueueWorkDoneStatus earlyStatus) {
    ASSERT(earlyStatus != WGPUQueueWorkDoneStatus_Success);
    mEarlyStatus = earlyStatus;
    TrackedEvent::TriggerEarlyFailure();
}

DeviceBase* WorkDoneEvent::GetWaitDevice() const {
    // TODO(crbug.com/dawn/1987): When adding support for mixed sources, return nullptr here when
    // the device has the mixed sources feature enabled (it can expose this fence as an OS event).
    return mQueue->GetDevice();
}

MaybeError WorkDoneEvent::Validate(WGPUQueueWorkDoneStatus* earlyStatus) {
    // Device lost errors
    *earlyStatus = WGPUQueueWorkDoneStatus_DeviceLost;
    DAWN_TRY(mQueue->GetDevice()->ValidateIsAlive());

    // Validation errors
    *earlyStatus = WGPUQueueWorkDoneStatus_Error;
    DAWN_TRY(mQueue->GetDevice()->ValidateObject(mQueue.Get()));

    return {};
}

void WorkDoneEvent::Complete() {
    WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Unknown;
    if (mEarlyReady) {
        status = mEarlyStatus;
    } else {
        if (mQueue->GetDevice()->IsLost()) {
            status = WGPUQueueWorkDoneStatus_DeviceLost;
        } else {
            // Assume that if Complete() is being called, the (backend) queue serial has passed.
            status = WGPUQueueWorkDoneStatus_Success;
        }
    }

    mCallback(status, mUserdata);
}

}  // namespace dawn::native
