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

#include "dawn/native/QueueWorkDoneFuture.h"

#include <memory>
#include <utility>

#include "dawn/native/Device.h"
#include "dawn/native/ObjectType_autogen.h"
#include "dawn/native/Queue.h"

namespace dawn::native {

/*
// QueueWorkDoneFutureBase::SubmittedWorkDone

struct QueueWorkDoneFutureBase::SubmittedWorkDone : TrackTaskCallback {
    SubmittedWorkDone(dawn::platform::Platform* platform, QueueWorkDoneFutureBase* future)
        : TrackTaskCallback(platform), mFuture(future) {}
    ~SubmittedWorkDone() override = default;

  private:
    void FinishImpl() override {
        mFuture->mState = FutureBase::State::Ready;
        mFuture->CallCallbackIfAny();
        mFuture = nullptr;
    }
    void HandleDeviceLossImpl() override { FinishImpl(); }
    void HandleShutDownImpl() override { FinishImpl(); }

    Ref<QueueWorkDoneFutureBase> mFuture;
};
*/

// QueueWorkDoneFutureBase

QueueWorkDoneFutureBase::QueueWorkDoneFutureBase(QueueBase* queue, ExecutionSerial serial)
    : FutureBase(queue->GetDevice()), mQueue(queue), mSerial(serial) {}

ObjectType QueueWorkDoneFutureBase::GetType() const {
    return ObjectType::QueueWorkDoneFuture;
}

void QueueWorkDoneFutureBase::DestroyImpl() {}

/*
wgpu::WaitStatus QueueWorkDoneFutureBase::Wait(uint64_t timeout) {
    ASSERT(timeout == UINT64_MAX);
    if (mState != State::Pending) {
        return wgpu::WaitStatus::NonePending;
    }

    auto task = std::make_unique<SubmittedWorkDone>(GetDevice()->GetPlatform(), this);
    mQueue->TrackTaskAfterEventualFlush(std::move(task));
    while (mState == State::Pending) { // FIXME: bad spin-wait
        GetDevice()->APITick();
    }
    return wgpu::WaitStatus::SomeCompleted;
}
*/

WGPUQueueWorkDoneResult const* QueueWorkDoneFutureBase::APIGetResult() {
    return &mResult;
}

void QueueWorkDoneFutureBase::APIThen(wgpu::CallbackMode,
                                      WGPUQueueWorkDoneFutureCallback callback,
                                      void* userdata) {
    ASSERT(mCallback == nullptr);
    mCallback = callback;
    mUserdata = userdata;
    if (mState == State::Ready) {
        CallCallbackIfAny();
    }
}

void QueueWorkDoneFutureBase::CallCallbackIfAny() {
    ASSERT(mState == State::Ready);
    if (mCallback) {
        mCallback(reinterpret_cast<WGPUQueueWorkDoneFuture>(this), mUserdata);
        mState = State::Observed;
    }
}

}  // namespace dawn::native
