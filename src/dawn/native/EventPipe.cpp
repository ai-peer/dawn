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

#include "dawn/native/EventPipe.h"

#include <poll.h>
#include <unistd.h>
#include <limits>
#include <utility>

#include "dawn/common/Compiler.h"

namespace dawn::native {

namespace {

int ToPollTimeout(Nanoseconds timeout) {
    uint64_t ns = uint64_t{timeout};
    uint64_t ms = 0;
    if (ns > 0) {
        ms = (ns - 1) / 1'000'000 + 1;
        if (ms > std::numeric_limits<int>::max()) {
            return -1;  // Round long timeout up to infinity
        }
    }
    return static_cast<int>(ms);
}

}  // namespace

ResultOrError<int> Poll(std::vector<pollfd>* pollfds, Nanoseconds timeout) {
    int status = poll(pollfds->data(), pollfds->size(), ToPollTimeout(timeout));
    if (DAWN_UNLIKELY(status < 0)) {
        return DAWN_INTERNAL_ERROR("poll() failed");
    }
    return status;
}

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
    if (int{mReceiver} >= 0) {
        close(int{mReceiver});
    }
    if (int{mSender} >= 0) {
        // FIXME: probably should just change Create to return the two halves separately
        ASSERT(false);

        close(int{mSender});
    }
}

EventPipe::EventPipe(const int (&fds)[2]) : mReceiver(fds[0]), mSender(fds[1]) {}

MaybeError EventPipe::Signal() {
    ASSERT(int{mSender} >= 0);
    // Send one byte to signal the receiver
    char zero[1] = {0};
    int status = write(int{mSender}, zero, 1);
    if (DAWN_UNLIKELY(status < 0)) {
        ASSERT(false);
        return DAWN_INTERNAL_ERROR("write() failed");
    }

    close(int{mSender});
    mSender = PosixFd(-1);
    return {};
}

PosixFd EventPipe::AcquireReceiverFd() {
    PosixFd fd = mReceiver;
    ASSERT(fd != PosixFd(-1));
    mReceiver = PosixFd(-1);
    return fd;
}

}  // namespace dawn::native
