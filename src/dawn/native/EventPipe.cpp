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

#if DAWN_PLATFORM_IS(WINDOWS)
#include <handleapi.h>
#include <synchapi.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

#include <limits>
#include <utility>

#include "dawn/common/Compiler.h"

namespace dawn::native {

namespace {

template <typename T, T Infinity>
T ToMillisecondsGeneric(Nanoseconds timeout) {
    uint64_t ns = uint64_t{timeout};
    uint64_t ms = 0;
    if (ns > 0) {
        ms = (ns - 1) / 1'000'000 + 1;
        if (ms > std::numeric_limits<T>::max()) {
            return Infinity;  // Round long timeout up to infinity
        }
    }
    return static_cast<T>(ms);
}

#if DAWN_PLATFORM_IS(WINDOWS)
#define ToMilliseconds ToMillisecondsGeneric<DWORD, INFINITE>
#else
#define ToMilliseconds ToMillisecondsGeneric<int, -1>
#endif

}  // namespace

// EventPrimitive

bool EventPrimitive::IsValid() const {
#if DAWN_PLATFORM_IS(WINDOWS)
    return v != nullptr;
#else
    return v >= 0;
#endif
}

void EventPrimitive::Close() {
    if (IsValid()) {
#if DAWN_PLATFORM_IS(WINDOWS)
        CloseHandle(v);
#else
        close(v);
#endif
        static constexpr T kInvalid = EventPrimitive{}.v;
        v = kInvalid;
    }
}

// EventReceiver

ResultOrError<wgpu::WaitStatus> EventReceiver::Poll(std::vector<EventPollInfo>* infos,
                                                    Nanoseconds timeout) {
    if (infos->size() == 0) {
        return wgpu::WaitStatus::Success;
    }

#if DAWN_PLATFORM_IS(WINDOWS)
    if (infos->size() > MAXIMUM_WAIT_OBJECTS) {
        // FIXME
        return DAWN_INTERNAL_ERROR("too many wait objects");
    }

    return DAWN_INTERNAL_ERROR("unimplemented");
#else
    std::vector<pollfd> pollfds(infos->size());
    for (size_t i = 0; i < infos->size(); ++i) {
        pollfds[i] = pollfd{(*infos)[i].primitive, POLLIN, 0};
    }

    int status = poll(pollfds.data(), pollfds.size(), ToMilliseconds(timeout));

    if (DAWN_UNLIKELY(status < 0)) {
        return DAWN_INTERNAL_ERROR("poll() failed");
    }
    for (size_t i = 0; i < infos->size(); ++i) {
        int revents = pollfds[i].revents;
        static constexpr int kAllowedEvents = POLLIN | POLLHUP;
        if (DAWN_UNLIKELY((revents & kAllowedEvents) != revents)) {
            return DAWN_INTERNAL_ERROR("poll() produced unexpected revents");
        }
    }

    for (size_t i = 0; i < infos->size(); ++i) {
        bool ready = (pollfds[i].revents & POLLIN) != 0;
        (*infos)[i].ready = ready;
    }

    return status == 0 ? wgpu::WaitStatus::TimedOut : wgpu::WaitStatus::Success;
#endif
}

EventReceiver::EventReceiver(EventPrimitive::T primitive) : mPrimitive(primitive) {}

EventReceiver::EventReceiver(EventReceiver&& other) {
    *this = std::move(other);
}

EventReceiver& EventReceiver::operator=(EventReceiver&& other) {
    mPrimitive = other.mPrimitive;
    other.mPrimitive = EventPrimitive{};
    return *this;
}

EventReceiver::~EventReceiver() {
    mPrimitive.Close();
}

EventPrimitive::T EventReceiver::Get() const {
    ASSERT(mPrimitive.IsValid());
    return mPrimitive.v;
}

// EventPipeSender

// static
ResultOrError<std::pair<EventPipeSender, EventReceiver>> EventPipeSender::CreateEventPipe() {
#if DAWN_PLATFORM_IS(WINDOWS)
    return DAWN_INTERNAL_ERROR("unimplemented");
#else
    int pipeFds[2];
    int status = pipe(pipeFds);
    if (status == -1) {
        return DAWN_INTERNAL_ERROR("Failed to create POSIX pipe");
    }

    EventReceiver receiver{pipeFds[0]};

    EventPipeSender sender;
    sender.mPrimitive = {pipeFds[1]};

    return std::make_pair(std::move(sender), std::move(receiver));
#endif
}

EventPipeSender::EventPipeSender(EventPipeSender&& other) {
    *this = std::move(other);
}

EventPipeSender& EventPipeSender::operator=(EventPipeSender&& other) {
    mPrimitive = other.mPrimitive;
    other.mPrimitive = EventPrimitive{};
    return *this;
}

EventPipeSender::~EventPipeSender() {
    ASSERT(!mPrimitive.IsValid());
}

MaybeError EventPipeSender::Signal() {
    ASSERT(mPrimitive.IsValid());
#if DAWN_PLATFORM_IS(WINDOWS)
    return DAWN_INTERNAL_ERROR("unimplemented");
#else
    // Send one byte to signal the receiver
    char zero[1] = {0};
    int status = write(mPrimitive.v, zero, 1);
    if (DAWN_UNLIKELY(status < 0)) {
        ASSERT(false);
        return DAWN_INTERNAL_ERROR("write() failed");
    }
#endif

    mPrimitive.Close();
    return {};
}

}  // namespace dawn::native
