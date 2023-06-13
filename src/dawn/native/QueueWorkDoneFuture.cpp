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

namespace {

struct SubmittedWorkDone : TrackTaskCallback {
    SubmittedWorkDone(dawn::platform::Platform* platform,
                      WGPUQueueWorkDoneCallback callback,
                      void* userdata)
        : TrackTaskCallback(platform), mCallback(callback), mUserdata(userdata) {}
    ~SubmittedWorkDone() override = default;

  private:
    void FinishImpl() override {
        ASSERT(mCallback != nullptr);
        ASSERT(mSerial != kMaxExecutionSerial);
        mCallback(WGPUQueueWorkDoneStatus_Success, mUserdata);
        mCallback = nullptr;
    }
    void HandleDeviceLossImpl() override {
        ASSERT(mCallback != nullptr);
        mCallback(WGPUQueueWorkDoneStatus_DeviceLost, mUserdata);
        mCallback = nullptr;
    }
    void HandleShutDownImpl() override { HandleDeviceLossImpl(); }

    WGPUQueueWorkDoneCallback mCallback = nullptr;
    void* mUserdata;
};

}  // namespace

// QueueWorkDoneFutureBase

QueueWorkDoneFutureBase::QueueWorkDoneFutureBase(QueueBase* queue)
    : FutureBase(queue->GetDevice()), mQueue(queue) {}

ObjectType QueueWorkDoneFutureBase::GetType() const {
    return ObjectType::QueueWorkDoneFuture;
}

void QueueWorkDoneFutureBase::DestroyImpl() {}

wgpu::WaitStatus QueueWorkDoneFutureBase::Wait(uint64_t timeout) {
    ASSERT(timeout == UINT64_MAX);
    if (mState != State::Pending) {
        return wgpu::WaitStatus::NonePending;
    }

    ASSERT(mState == State::Pending);
    auto callback = [](WGPUQueueWorkDoneStatus status, void* userdata) {
        auto* self = static_cast<QueueWorkDoneFutureBase*>(userdata);
        self->mState = State::Ready;
        self->CallCallbackIfAny();
    };
    auto task = std::make_unique<SubmittedWorkDone>(GetDevice()->GetPlatform(), callback, this);
    mQueue->TrackTaskAfterEventualFlush(std::move(task));
    while (mState == State::Pending) {
        GetDevice()->APITick();
    }
    return wgpu::WaitStatus::SomeCompleted;
}

WGPUQueueWorkDoneResult const* QueueWorkDoneFutureBase::APIGetResult() {
    UNREACHABLE();  // FIXME
    return nullptr;
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
