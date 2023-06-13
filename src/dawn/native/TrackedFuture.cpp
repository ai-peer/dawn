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
#include "dawn/native/Queue.h"

namespace dawn::native {

// TrackedFuture

TrackedFuture::TrackedFuture(FutureID futureID,
                             InstanceBase* instance,
                             WGPUCallbackModeFlags callbackMode)
    : mFutureID(futureID), mInstance(instance), mCallbackMode(callbackMode) {
    bool shouldHaveID = mCallbackMode & WGPUCallbackMode_Future;
    bool hasID = mFutureID != 0;
    ASSERT(hasID == shouldHaveID);
}

TrackedFuture::~TrackedFuture() {
    ASSERT(mCompleted);
}

FutureID TrackedFuture::GetID() const {
    ASSERT(mFutureID != 0);
    return mFutureID;
}

OSEventPrimitive::T TrackedFuture::GetPrimitive() const {
    return mReceiver.Get();
}

void TrackedFuture::EnsureComplete() {
    bool wasNotComplete = mCompleted.exchange(true);
    if (wasNotComplete) {
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
    FutureID futureID = instance->CreateFutureID(callbackInfo.mode);
    Ref<WorkDoneFuture> future = AcquireRef(new WorkDoneFuture(futureID, queue, callbackInfo));

    MaybeError initResult = future->Init();
    if (initResult.IsError()) {
        // mInitStatus has been set with an error code.
        future->EnsureComplete();
        DAWN_UNUSED(instance->ConsumedError(std::move(initResult)));
    } else {
        // The callback has already been registered with the queue.
        // If it's a waitable future, track it with the instance as well.
        if (futureID != 0) {
            instance->TrackFuture(futureID, future.Get());
        }
    }

    return futureID;
}

WorkDoneFuture::WorkDoneFuture(FutureID futureID,
                               QueueBase* queue,
                               const WGPUQueueWorkDoneCallbackInfo& callbackInfo)
    : TrackedFuture(futureID, queue->GetInstance(), callbackInfo.mode),
      mQueue(queue),
      mCallback(callbackInfo.callback),
      mUserdata(callbackInfo.userdata) {
    mSerial = queue->GetScheduledWorkDoneSerial();
}

DeviceBase* WorkDoneFuture::GetWaitDevice() const {
    // TODO(crbug.com/dawn/1987): When adding support for mixed sources, return nullptr here when
    // the device has the mixed sources feature enabled (it can expose this fence as an OS event).
    return mQueue->GetDevice();
}

MaybeError WorkDoneFuture::Init() {
    // Device lost errors
    mInitStatus = WGPUQueueWorkDoneStatus_DeviceLost;
    DAWN_TRY(mQueue->GetDevice()->ValidateIsAlive());

    // Validation errors
    mInitStatus = WGPUQueueWorkDoneStatus_Error;
    DAWN_TRY(TrackedFuture::Validate());
    DAWN_TRY(mQueue->GetDevice()->ValidateObject(mQueue.Get()));

    // Unexpected errors
    mInitStatus = WGPUQueueWorkDoneStatus_Unknown;
    DAWN_TRY_ASSIGN(mReceiver, mQueue->CreateWorkDoneEvent(mSerial));

    mInitStatus = WGPUQueueWorkDoneStatus_Success;
    return {};
}

void WorkDoneFuture::Complete() {
    WGPUQueueWorkDoneStatus status = mInitStatus;
    if (status == WGPUQueueWorkDoneStatus_Success) {
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
