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

#include "dawn/native/metal/QueueWorkDoneFutureMTL.h"

#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/ToBackend.h"
#include "dawn/native/metal/DeviceMTL.h"

#include <poll.h>

namespace dawn::native::metal {

namespace {

int ToPollTimeout(Milliseconds ms) {
    if (uint64_t(ms) > std::numeric_limits<int>::max()) {
        return -1;  // Round long timeout up to infinity
    }
    return int(uint64_t(ms));
}

}  // namespace

// EventPipe

// static
ResultOrError<EventPipe> EventPipe::Create() {
    int pipeFds[2];
    int status = pipe(pipeFds);
    if (status == -1) {
        return DAWN_INTERNAL_ERROR("Failed to create POSIX pipe");
    }
    return EventPipe{pipeFds};
}

EventPipe::EventPipe(EventPipe&& other) : mReceiver(other.mReceiver), mSender(other.mSender) {
    *this = std::move(other);
}

EventPipe& EventPipe::operator=(EventPipe&& other) {
    mReceiver = other.mReceiver;
    mSender = other.mSender;
    other.mReceiver = other.mSender = PosixFd(-1);
    return *this;
}

EventPipe::~EventPipe() {
    if (int(mReceiver) >= 0) {
        close(int(mReceiver));
    }
    if (int(mSender) >= 0) {
        close(int(mSender));
    }
}

EventPipe::EventPipe(const int (&fds)[2]) : mReceiver(fds[0]), mSender(fds[1]) {}

MaybeError EventPipe::Signal() {
    ASSERT(int(mSender) >= 0);
    // Send one byte to signal the receiver
    char zero[1] = {};
    int status = write(int(mSender), zero, 1);
    if (DAWN_UNLIKELY(status < 0)) {
        return DAWN_INTERNAL_ERROR("write() failed");
    }

    close(int(mSender));
    mSender = PosixFd(-1);
    return {};
}

ResultOrError<bool> EventPipe::Ready(Milliseconds timeout) const {
    ASSERT(int(mReceiver) >= 0);
    int pollTimeout = ToPollTimeout(timeout);
    struct pollfd pfd = {int(mReceiver), POLLIN, 0};
    int status = poll(&pfd, 1, pollTimeout);
    if (DAWN_UNLIKELY(status < 0)) {
        return DAWN_INTERNAL_ERROR("poll() failed");
    }
    return status > 0;
}

PosixFd EventPipe::GetReceiverFd() const {
    return mReceiver;
}

// QueueWorkDoneFuture

// static
ResultOrError<QueueWorkDoneFuture*> QueueWorkDoneFuture::Create(
    QueueBase* queue,
    ExecutionSerial serial,
    QueueWorkDoneDescriptor const* descriptor) {
    Device* device = ToBackend(queue->GetDevice());

    EventPipe eventPipe;
    DAWN_TRY_ASSIGN(eventPipe, EventPipe::Create());

    const QueueWorkDoneDescriptorFd* descFd = nullptr;
    FindInChain(descriptor->nextInChain, &descFd);
    QueueWorkDoneFuture* future =
        new QueueWorkDoneFuture(queue, serial, descFd != nullptr, std::move(eventPipe));

    DAWN_TRY(device->InsertWaitingFuture(serial, future));
    return future;
}

QueueWorkDoneFuture::QueueWorkDoneFuture(QueueBase* queue,
                                         ExecutionSerial serial,
                                         bool requestedFd,
                                         EventPipe&& eventPipe)
    : QueueWorkDoneFutureBase(queue, serial, requestedFd), mEventPipe(std::move(eventPipe)) {}

MaybeError QueueWorkDoneFuture::Signal() {
    return mEventPipe.Signal();
}

ResultOrError<wgpu::WaitStatus> QueueWorkDoneFuture::Wait(Milliseconds timeout) {
    switch (mState) {
        case State::Observed:
            return wgpu::WaitStatus::NonePending;
        case State::Ready:
            CallCallbackIfAny();
            return wgpu::WaitStatus::SomeCompleted;
        case State::Pending: {
            bool ready;
            DAWN_TRY_ASSIGN(ready, mEventPipe.Ready(timeout));
            if (ready) {
                mState = State::Ready;
                CallCallbackIfAny();
                return wgpu::WaitStatus::SomeCompleted;
            } else {
                return wgpu::WaitStatus::TimedOut;
            }
            break;
        }
    }
}

PosixFd QueueWorkDoneFuture::GetFdInternal() const {
    return mEventPipe.GetReceiverFd();
}

void QueueWorkDoneFuture::APIGetResult(QueueWorkDoneResult*) const {
    UNREACHABLE();  // FIXME
}

}  // namespace dawn::native::metal
