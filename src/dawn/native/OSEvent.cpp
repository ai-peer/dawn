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

#include "dawn/native/OSEvent.h"

#if DAWN_PLATFORM_IS(WINDOWS)
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

#include <limits>
#include <utility>
#include <vector>

#include "dawn/common/Compiler.h"
#include "dawn/common/FutureUtils.h"
#include "dawn/native/TrackedEvent.h"

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

// OSEventPrimitive

bool OSEventPrimitive::IsValid() const {
#if DAWN_PLATFORM_IS(WINDOWS)
    return v != nullptr;
#else
    return v >= 0;
#endif
}

void OSEventPrimitive::Close() {
    if (IsValid()) {
#if DAWN_PLATFORM_IS(WINDOWS)
        CloseHandle(v);
#else
        close(v);
#endif
        static constexpr T kInvalid = OSEventPrimitive{}.v;
        v = kInvalid;
    }
}

// OSEventReceiver

OSEventReceiver OSEventReceiver::CreateAlreadySignaled() {
#if DAWN_PLATFORM_IS(WINDOWS)
    UNREACHABLE();  // FIXME: unimplemented
#else
    int status;

    int pipeFds[2];
    status = pipe(pipeFds);
    if (DAWN_UNLIKELY(status < 0)) {
        abort();
    }

    OSEventReceiver receiver{pipeFds[0]};

    int sender = pipeFds[1];

    char zero[1] = {0};
    status = write(sender, zero, 1);
    if (DAWN_UNLIKELY(status < 0)) {
        abort();
    }

    status = close(sender);
    if (DAWN_UNLIKELY(status < 0)) {
        abort();
    }

    return receiver;
#endif
}

bool OSEventReceiver::Wait(size_t count, TrackedFutureWaitInfo* futures, Nanoseconds timeout) {
#if DAWN_PLATFORM_IS(WINDOWS)
    static_assert(kTimedWaitAnyMaxCountDefault == MAXIMUM_WAIT_OBJECTS);
    ASSERT(count <= MAXIMUM_WAIT_OBJECTS);
    std::vector<HANDLE> handles(count);
    for (size_t i = 0; i < count; ++i) {
        handles[i] = futures[i].future->GetPrimitive();
    }

    DWORD code = WaitForMultipleObjects(handles.size(), handles.data(), /* bWaitAll */ false,
                                        ToMilliseconds(timeout));
    if (DAWN_UNLIKELY(code == WAIT_FAILED)) {
        return DAWN_INTERNAL_ERROR("WaitForMultipleObjects failed");
    }

    for (size_t i = 0; i < count; ++i) {
        futures[i].ready = false;
    }
    if (code == WAIT_TIMEOUT) {
        return false;
    }

    ASSERT(code < WAIT_ABANDONED_0);
    DWORD signaledIndex = code - WAIT_OBJECT_0;
    if (signaledIndex < count) {
        futures[signaledIndex].ready = true;
    } else {
        ASSERT(false);
    }

    return true;
#else
    std::vector<pollfd> pollfds(count);
    for (size_t i = 0; i < count; ++i) {
        pollfds[i] = pollfd{futures[i].event->GetPrimitive(), POLLIN, 0};
    }

    int status = poll(pollfds.data(), pollfds.size(), ToMilliseconds(timeout));

    if (DAWN_UNLIKELY(status < 0)) {
        abort();
    }
    if (status == 0) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        int revents = pollfds[i].revents;
        static constexpr int kAllowedEvents = POLLIN | POLLHUP;
        if (DAWN_UNLIKELY((revents & kAllowedEvents) != revents)) {
            abort();
        }
    }

    for (size_t i = 0; i < count; ++i) {
        bool ready = (pollfds[i].revents & POLLIN) != 0;
        futures[i].ready = ready;
    }

    return true;
#endif
}

OSEventReceiver::OSEventReceiver(OSEventPrimitive::T primitive) : mPrimitive{primitive} {}

OSEventReceiver::OSEventReceiver(OSEventReceiver&& other) {
    *this = std::move(other);
}

OSEventReceiver& OSEventReceiver::operator=(OSEventReceiver&& other) {
    mPrimitive = other.mPrimitive;
    other.mPrimitive = OSEventPrimitive{};
    return *this;
}

OSEventReceiver::~OSEventReceiver() {
    mPrimitive.Close();
}

OSEventPrimitive::T OSEventReceiver::Get() const {
    ASSERT(mPrimitive.IsValid());
    return mPrimitive.v;
}

}  // namespace dawn::native
