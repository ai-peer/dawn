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

#include "dawn/native/OSEventReceiver.h"

#if DAWN_PLATFORM_IS(WINDOWS)
#include <windows.h>
#elif DAWN_PLATFORM_IS(POSIX)
#include <poll.h>
#include <unistd.h>
#endif

#include <algorithm>
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
// #define ToMilliseconds ToMillisecondsGeneric<DWORD, INFINITE>
#elif DAWN_PLATFORM_IS(POSIX)
#define ToMilliseconds ToMillisecondsGeneric<int, -1>
#endif

}  // namespace

// OSEventPrimitive

bool OSEventPrimitive::IsValid() const {
#if DAWN_PLATFORM_IS(WINDOWS)
    return v != nullptr;
#elif DAWN_PLATFORM_IS(POSIX)
    return v >= 0;
#else
    CHECK(false);      // Not implemented.
#endif
}

void OSEventPrimitive::Close() {
    if (IsValid()) {
#if DAWN_PLATFORM_IS(WINDOWS)
        CloseHandle(v);
#elif DAWN_PLATFORM_IS(POSIX)
        close(v);
#else
        CHECK(false);  // Not implemented.
#endif
        static constexpr T kInvalid = OSEventPrimitive{}.v;
        v = kInvalid;
    }
}

// OSEventReceiver

OSEventReceiver OSEventReceiver::CreateAlreadySignaled() {
#if DAWN_PLATFORM_IS(WINDOWS)
    // TODO(crbug.com/dawn/1987): Implement this.
    CHECK(false);
#elif DAWN_PLATFORM_IS(POSIX)
    int status;

    int pipeFds[2];
    status = pipe(pipeFds);
    CHECK(status >= 0);

    OSEventReceiver receiver{pipeFds[0]};

    int sender = pipeFds[1];

    char zero[1] = {0};
    status = write(sender, zero, 1);
    CHECK(status >= 0);

    status = close(sender);
    CHECK(status >= 0);

    return receiver;
#else
    CHECK(false);      // Not implemented.
#endif
}

bool OSEventReceiver::WaitAny(size_t count, TrackedFutureWaitInfo* futures, Nanoseconds timeout) {
#if DAWN_PLATFORM_IS(WINDOWS)
    // TODO(crbug.com/dawn/1987): Implement this.
    CHECK(false);
#elif DAWN_PLATFORM_IS(POSIX)
    std::vector<pollfd> pollfds(count);
    for (size_t i = 0; i < count; ++i) {
        pollfds[i] = pollfd{futures[i].event->GetPrimitive(), POLLIN, 0};
    }

    int status = poll(pollfds.data(), pollfds.size(), ToMilliseconds(timeout));

    CHECK(status >= 0);
    if (status == 0) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        int revents = pollfds[i].revents;
        static constexpr int kAllowedEvents = POLLIN | POLLHUP;
        CHECK((revents & kAllowedEvents) == revents);
    }

    for (size_t i = 0; i < count; ++i) {
        bool ready = (pollfds[i].revents & POLLIN) != 0;
        futures[i].ready = ready;
    }

    return true;
#else
    CHECK(false);      // Not implemented.
#endif
}

OSEventReceiver::OSEventReceiver(OSEventPrimitive::T primitive) : mPrimitive{primitive} {}

OSEventReceiver::OSEventReceiver(OSEventReceiver&& rhs) {
    *this = std::move(rhs);
}

OSEventReceiver& OSEventReceiver::operator=(OSEventReceiver&& rhs) {
    mPrimitive = rhs.mPrimitive;
    rhs.mPrimitive = OSEventPrimitive{};
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
