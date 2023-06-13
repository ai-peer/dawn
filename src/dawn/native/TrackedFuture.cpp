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

TrackedFuture::TrackedFuture(InstanceBase* instance, wgpu::CallbackFlag callbackFlags)
    : mInstance(instance), mCallbackFlags(callbackFlags) {
    mFutureID = mInstance->TrackFuture(this);
}

TrackedFuture::~TrackedFuture() {
    ASSERT(mCompleted);
}

FutureID TrackedFuture::GetID() const {
    return mFutureID;
}

EventPrimitive::T TrackedFuture::GetPrimitive() const {
    return mReceiver.Get();
}

void TrackedFuture::Complete() {
    ASSERT(!mCompleted);
    CompleteImpl();
    mCompleted = true;
}

// WorkDoneFuture

ResultOrError<Ref<WorkDoneFuture>> WorkDoneFuture::Create(DeviceBase* device,
                                                          wgpu::CallbackFlag callbackFlags,
                                                          WGPUQueueWorkDoneCallback callback,
                                                          void* userdata) {
    Ref<WorkDoneFuture> future =
        AcquireRef(new WorkDoneFuture(device, callbackFlags, callback, userdata));
    DAWN_TRY_ASSIGN(future->mReceiver, device->CreateWorkDoneEvent(future->mSerial));
    return future;
}

WorkDoneFuture::WorkDoneFuture(DeviceBase* device,
                               wgpu::CallbackFlag callbackFlags,
                               WGPUQueueWorkDoneCallback callback,
                               void* userdata)
    : TrackedFuture(device->GetInstance(), callbackFlags),
      mDevice(device),
      mCallback(callback),
      mUserdata(userdata) {
    mSerial = device->GetScheduledWorkDoneSerial();
}

void WorkDoneFuture::CompleteImpl() {
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
