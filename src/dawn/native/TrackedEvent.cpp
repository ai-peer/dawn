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
#include "dawn/native/OSEventReceiver.h"
#include "dawn/native/Queue.h"

namespace dawn::native {

// TrackedEvent

TrackedEvent::TrackedEvent(InstanceBase* instance, WGPUCallbackModeFlags callbackMode)
    : mInstance(instance), mCallbackMode(callbackMode) {}

TrackedEvent::~TrackedEvent() {
    ASSERT(mCompleted);
}

OSEventPrimitive::T TrackedEvent::GetPrimitive() const {
    return mReceiver.Get();
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

void TrackedEvent::TriggerEarlyReady() {
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
    Ref<WorkDoneEvent> event = AcquireRef(new WorkDoneEvent(queue, callbackInfo));

    WGPUQueueWorkDoneStatus validationEarlyStatus;
    if (queue->GetDevice()->ConsumedError(event->Validate(&validationEarlyStatus))) {
        event->SetEarlyReady(validationEarlyStatus);
        event->mReceiver = OSEventReceiver::CreateAlreadySignaled();
    } else {
        event->mReceiver = queue->InsertWorkDoneEvent();
    }

    // TODO(crbug.com/dawn/1987): Spontaneous callbacks should be called here (or in Track?) if
    // early-ready.

    InstanceBase* instance = queue->GetInstance();
    return instance->GetEventManager()->TrackEvent(callbackInfo.mode, event.Get());
}

DeviceBase* WorkDoneEvent::GetWaitDevice() const {
    // TODO(crbug.com/dawn/1987): When adding support for mixed sources, return nullptr here when
    // the device has the mixed sources feature enabled, and so can expose the fence as an OS event.
    return mQueue->GetDevice();
}

WorkDoneEvent::WorkDoneEvent(QueueBase* queue, const WGPUQueueWorkDoneCallbackInfo& callbackInfo)
    : TrackedEvent(queue->GetInstance(), callbackInfo.mode),
      mQueue(queue),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata) {}

void WorkDoneEvent::SetEarlyReady(WGPUQueueWorkDoneStatus earlyStatus) {
    mEarlyStatus = earlyStatus;
    TrackedEvent::TriggerEarlyReady();
}

MaybeError WorkDoneEvent::Validate(WGPUQueueWorkDoneStatus* earlyStatus) {
    // Device loss (we pretend the operation succeeded without validating)
    *earlyStatus = WGPUQueueWorkDoneStatus_Success;
    DAWN_TRY(mQueue->GetDevice()->ValidateIsAlive());

    // Validation errors
    *earlyStatus = WGPUQueueWorkDoneStatus_Error;
    DAWN_TRY(mQueue->GetDevice()->ValidateObject(mQueue.Get()));

    return {};
}

void WorkDoneEvent::Complete() {
    // There are no error cases other than the mEarlyStatus ones.
    WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Success;
    if (mEarlyReady) {
        status = mEarlyStatus;
    }

    mCallback(status, mUserdata);
}

}  // namespace dawn::native
