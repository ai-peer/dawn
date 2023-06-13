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

// EventReceiver

EventReceiver::EventReceiver(PosixFd fd) : mFd(fd) {}

EventReceiver::EventReceiver(EventReceiver&& other) {
    *this = std::move(other);
}

EventReceiver& EventReceiver::operator=(EventReceiver&& other) {
    mFd = other.mFd;
    other.mFd = PosixFd{-1};
    return *this;
}

EventReceiver::~EventReceiver() {
    if (int{mFd} >= 0) {
        close(int{mFd});
    }
}

PosixFd EventReceiver::GetFd() const {
    ASSERT(int{mFd} >= 0);
    return mFd;
}

// EventPipeSender

// static
ResultOrError<std::pair<EventPipeSender, EventReceiver>> EventPipeSender::CreateEventPipe() {
    int pipeFds[2];
    int status = pipe(pipeFds);
    if (status == -1) {
        return DAWN_INTERNAL_ERROR("Failed to create POSIX pipe");
    }

    EventReceiver receiver{PosixFd{pipeFds[0]}};

    EventPipeSender sender;
    sender.mFd = pipeFds[1];

    return std::make_pair(std::move(sender), std::move(receiver));
}

EventPipeSender::EventPipeSender(EventPipeSender&& other) {
    *this = std::move(other);
}

EventPipeSender& EventPipeSender::operator=(EventPipeSender&& other) {
    mFd = other.mFd;
    other.mFd = -1;
    return *this;
}

EventPipeSender::~EventPipeSender() {
    ASSERT(mFd == -1);
}

MaybeError EventPipeSender::Signal() {
    ASSERT(mFd >= 0);
    // Send one byte to signal the receiver
    char zero[1] = {0};
    int status = write(mFd, zero, 1);
    if (DAWN_UNLIKELY(status < 0)) {
        ASSERT(false);
        return DAWN_INTERNAL_ERROR("write() failed");
    }

    close(mFd);
    mFd = -1;
    return {};
}

}  // namespace dawn::native
