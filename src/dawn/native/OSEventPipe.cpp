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

#include "dawn/native/OSEventPipe.h"

#if DAWN_PLATFORM_IS(WINDOWS)
#include <windows.h>
#elif DAWN_PLATFORM_IS(POSIX)
#include <unistd.h>
#endif

#include <utility>

namespace dawn::native {

// OSEventPipe

// static
std::pair<OSEventPipe, OSEventReceiver> OSEventPipe::CreateEventPipe() {
#if DAWN_PLATFORM_IS(WINDOWS)
    // This is not needed on Windows yet. It's implementable using CreateEvent().
    UNREACHABLE();
#elif DAWN_PLATFORM_IS(POSIX)
    int pipeFds[2];
    int status = pipe(pipeFds);
    CHECK(status >= 0);

    OSEventReceiver receiver{pipeFds[0]};

    OSEventPipe sender;
    sender.mPrimitive = {pipeFds[1]};

    return std::make_pair(std::move(sender), std::move(receiver));
#else
    // Not implemented for this platform.
    CHECK(false);
#endif
}

OSEventPipe::OSEventPipe(OSEventPipe&& rhs) {
    *this = std::move(rhs);
}

OSEventPipe& OSEventPipe::operator=(OSEventPipe&& rhs) {
    mPrimitive = rhs.mPrimitive;
    rhs.mPrimitive = OSEventPrimitive{};
    return *this;
}

OSEventPipe::~OSEventPipe() {
    ASSERT(!mPrimitive.IsValid());
}

void OSEventPipe::Signal() {
    ASSERT(mPrimitive.IsValid());
#if DAWN_PLATFORM_IS(WINDOWS)
    // This is not needed on Windows yet. It's implementable using SetEvent().
    UNREACHABLE();
#elif DAWN_PLATFORM_IS(POSIX)
    // Send one byte to signal the receiver
    char zero[1] = {0};
    int status = write(mPrimitive.v, zero, 1);
    CHECK(status >= 0);
#else
    // Not implemented for this platform.
    CHECK(false);
#endif

    mPrimitive.Close();
}

}  // namespace dawn::native
