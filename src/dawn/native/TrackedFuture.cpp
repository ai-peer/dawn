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

#include "dawn/native/Device.h"
#include "dawn/native/Instance.h"

namespace dawn::native {

// TrackedFuture

TrackedFuture::TrackedFuture(InstanceBase* instance, wgpu::CallbackMode callbackMode)
    : mInstance(instance), mCallbackMode(callbackMode) {
    mFutureID = mInstance->TrackFuture(this);
}

TrackedFuture::~TrackedFuture() {
    ASSERT(mCompleted);
}

FutureID TrackedFuture::GetID() const {
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

ResultOrError<FutureID> WorkDoneFuture::Create(DeviceBase* device,
                                               wgpu::CallbackMode callbackMode,
                                               WGPUQueueWorkDoneCallback callback,
                                               void* userdata) {
    // FIXME: validate callbackMode

    Ref<WorkDoneFuture> future =
        AcquireRef(new WorkDoneFuture(device, callbackMode, callback, userdata));
    DAWN_TRY_ASSIGN(future->mReceiver, device->CreateWorkDoneEvent(future->mSerial));

    // TrackedFuture tracks itself on creation, so we can drop the Ref as soon as we're done with it
    return future->GetID();
}

WorkDoneFuture::WorkDoneFuture(DeviceBase* device,
                               wgpu::CallbackMode callbackMode,
                               WGPUQueueWorkDoneCallback callback,
                               void* userdata)
    : TrackedFuture(device->GetInstance(), callbackMode),
      mDevice(device),
      mCallback(callback),
      mUserdata(userdata) {
    mSerial = device->GetScheduledWorkDoneSerial();
}

DeviceBase* WorkDoneFuture::GetWaitDevice() const {
    // TODO(crbug.com/dawn/1987): When adding support for mixed sources, return nullptr here when
    // the device has the mixed sources feature enabled (so can expose the fence as an OS event).
    return mDevice.Get();
}

void WorkDoneFuture::Complete() {
    WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Unknown;
    if (mDevice->IsLost()) {
        status = WGPUQueueWorkDoneStatus_DeviceLost;
    } else if (mDevice->GetCompletedCommandSerial() >= mSerial) {
        status = WGPUQueueWorkDoneStatus_Success;
    } else {
        // FIXME: should ValidateOnSubmittedWorkDone
        UNREACHABLE();
    }

    mCallback(status, mUserdata);
}

}  // namespace dawn::native
