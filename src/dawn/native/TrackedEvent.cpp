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

TrackedEvent::TrackedEvent(InstanceBase* instance,
                           WGPUCallbackModeFlags callbackMode,
                           OSEventReceiver&& receiver)
    : mInstance(instance), mCallbackMode(callbackMode), mReceiver(std::move(receiver)) {}

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

void TrackedEvent::CompleteIfSpontaneous() {
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
    Ref<WorkDoneEvent> event;

    WGPUQueueWorkDoneStatus validationEarlyStatus;
    if (queue->GetDevice()->ConsumedError(Validate(queue, &validationEarlyStatus))) {
        // If the callback is spontaneous, it'll get called in here.
        event = AcquireRef(new WorkDoneEvent(queue, callbackInfo, validationEarlyStatus));
    } else {
        event = AcquireRef(new WorkDoneEvent(queue, callbackInfo, queue->InsertWorkDoneEvent()));
    }

    InstanceBase* instance = queue->GetInstance();
    return instance->GetEventManager()->TrackEvent(callbackInfo.mode, event.Get());
}

MaybeError WorkDoneEvent::Validate(QueueBase* queue, WGPUQueueWorkDoneStatus* earlyStatus) {
    DeviceBase* device = queue->GetDevice();

    // Device loss (we pretend the operation succeeded without validating)
    *earlyStatus = WGPUQueueWorkDoneStatus_Success;
    DAWN_TRY(device->ValidateIsAlive());

    // Validation errors
    *earlyStatus = WGPUQueueWorkDoneStatus_Error;
    DAWN_TRY(device->ValidateObject(queue));

    return {};
}

WorkDoneEvent::WorkDoneEvent(QueueBase* queue,
                             const WGPUQueueWorkDoneCallbackInfo& callbackInfo,
                             WGPUQueueWorkDoneStatus earlyStatus)
    : TrackedEvent(queue->GetInstance(),
                   callbackInfo.mode,
                   OSEventReceiver::CreateAlreadySignaled()),
      mQueue(queue),
      mEarlyStatus(earlyStatus),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata) {
    CompleteIfSpontaneous();
}

WorkDoneEvent::WorkDoneEvent(QueueBase* queue,
                             const WGPUQueueWorkDoneCallbackInfo& callbackInfo,
                             OSEventReceiver&& receiver)
    : TrackedEvent(queue->GetInstance(), callbackInfo.mode, std::move(receiver)),
      mQueue(queue),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata) {}

DeviceBase* WorkDoneEvent::GetWaitDevice() const {
    // TODO(crbug.com/dawn/1987): When adding support for mixed sources, return nullptr here when
    // the device has the mixed sources feature enabled, and so can expose the fence as an OS event.
    return mQueue->GetDevice();
}

void WorkDoneEvent::Complete() {
    // WorkDoneEvent has no error cases other than the mEarlyStatus ones.
    WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Success;
    if (mEarlyStatus) {
        status = mEarlyStatus.value();
    }

    mCallback(status, mUserdata);
}

}  // namespace dawn::native
