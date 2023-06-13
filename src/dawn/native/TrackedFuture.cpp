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

#include "dawn/native/TrackedFuture.h"

#include <utility>

#include "dawn/native/Device.h"
#include "dawn/native/Instance.h"
#include "dawn/native/OSEvent.h"
#include "dawn/native/Queue.h"

namespace dawn::native {

// TrackedFuture

TrackedFuture::TrackedFuture(InstanceBase* instance, WGPUCallbackModeFlags callbackMode)
    : mInstance(instance), mCallbackMode(callbackMode) {}

TrackedFuture::~TrackedFuture() {
    ASSERT(mCompleted);
}

FutureID TrackedFuture::GetID() const {
    ASSERT(mFutureID != 0);
    return mFutureID;
}

void TrackedFuture::SetID(FutureID futureID) {
    ASSERT(mFutureID == 0);
    mFutureID = futureID;
}

OSEventPrimitive::T TrackedFuture::GetPrimitive() const {
    return mReceiver.Get();
}

void TrackedFuture::SetReceiver(OSEventReceiver&& receiver) {
    mReceiver = std::move(receiver);
}

bool TrackedFuture::IsEarlyReady() const {
    return mEarlyReady;
}

void TrackedFuture::EnsureComplete() {
    bool alreadyComplete = mCompleted.exchange(true);
    if (!alreadyComplete) {
        Complete();
    }
}

TrackedFuture::WaitRef TrackedFuture::TakeWaitRef() {
    return WaitRef{this};
}

MaybeError TrackedFuture::Validate() {
    DAWN_TRY(mInstance->ValidateCallbackModeAllowFuture(mCallbackMode));
    return {};
}

void TrackedFuture::TriggerEarlyFailure() {
    mEarlyReady = true;
    if (mCallbackMode & WGPUCallbackMode_Spontaneous) {
        bool alreadyComplete = mCompleted.exchange(true);
        // If it was already complete, but there was an error, we have no place
        // to report it, so ASSERT. This shouldn't happen.
        ASSERT(!alreadyComplete);
        Complete();
    }
}

// TrackedFuture::WaitRef

TrackedFuture::WaitRef::WaitRef(TrackedFuture* future) : mRef(future) {
#if DAWN_ENABLE_ASSERTS
    bool wasAlreadyWaited = mRef->mCurrentlyBeingWaited.exchange(true);
    ASSERT(!wasAlreadyWaited);
#endif
}

TrackedFuture::WaitRef::~WaitRef() {
#if DAWN_ENABLE_ASSERTS
    if (mRef.Get() != nullptr) {
        bool wasAlreadyWaited = mRef->mCurrentlyBeingWaited.exchange(false);
        ASSERT(wasAlreadyWaited);
    }
#endif
}

TrackedFuture* TrackedFuture::WaitRef::operator->() {
    return mRef.Get();
}

const TrackedFuture* TrackedFuture::WaitRef::operator->() const {
    return mRef.Get();
}

// WorkDoneFuture

FutureID WorkDoneFuture::Create(QueueBase* queue,
                                const WGPUQueueWorkDoneCallbackInfo& callbackInfo) {
    InstanceBase* instance = queue->GetInstance();
    Ref<WorkDoneFuture> future = AcquireRef(new WorkDoneFuture(queue, callbackInfo));

    // Register the future with the instance. It's OK that it doesn't have a receiver
    // yet; it has to handle that case anyway in case creating an EventPipe fails.
    FutureID futureID = instance->TrackFuture(callbackInfo.mode, future.Get());

    WGPUQueueWorkDoneStatus validationStatus;
    if (instance->ConsumedError(future->Validate(&validationStatus))) {
        future->FailEarly(validationStatus);
        return futureID;
    }

    // Register the future with the queue.
    if (instance->ConsumedError(queue->InsertWorkDoneEvent(), &future->mReceiver)) {
        // This means there was an unexpected error inside a syscall.
        // FIXME: Code would be considerably simpler if we simply release-asserted.
        future->FailEarly(WGPUQueueWorkDoneStatus_Unknown);
        return futureID;
    }

    // TODO(crbug.com/dawn/1987): Should check for early completions here, so
    // when there's an error, spontaneous callbacks can get called immediately.

    // FIXME: Handle WGPUCallbackMode_ProcessEvents

    return futureID;
}

WorkDoneFuture::WorkDoneFuture(QueueBase* queue, const WGPUQueueWorkDoneCallbackInfo& callbackInfo)
    : TrackedFuture(queue->GetInstance(), callbackInfo.mode),
      mQueue(queue),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata) {}

void WorkDoneFuture::FailEarly(WGPUQueueWorkDoneStatus status) {
    ASSERT(status != WGPUQueueWorkDoneStatus_Success);
    mEarlyStatus = status;
    TrackedFuture::TriggerEarlyFailure();
}

DeviceBase* WorkDoneFuture::GetWaitDevice() const {
    // TODO(crbug.com/dawn/1987): When adding support for mixed sources, return nullptr here when
    // the device has the mixed sources feature enabled (it can expose this fence as an OS event).
    return mQueue->GetDevice();
}

MaybeError WorkDoneFuture::Validate(WGPUQueueWorkDoneStatus* earlyStatus) {
    // Device lost errors
    *earlyStatus = WGPUQueueWorkDoneStatus_DeviceLost;
    DAWN_TRY(mQueue->GetDevice()->ValidateIsAlive());

    // Validation errors
    *earlyStatus = WGPUQueueWorkDoneStatus_Error;
    DAWN_TRY(TrackedFuture::Validate());
    DAWN_TRY(mQueue->GetDevice()->ValidateObject(mQueue.Get()));

    return {};
}

void WorkDoneFuture::Complete() {
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
