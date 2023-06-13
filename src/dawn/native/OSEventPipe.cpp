// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dawn/native/OSEventPipe.h"

#if DAWN_PLATFORM_IS(WINDOWS)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <utility>

namespace dawn::native {

// OSEventPipe

// static
ResultOrError<std::pair<OSEventPipe, OSEventReceiver>> OSEventPipe::CreateEventPipe() {
#if DAWN_PLATFORM_IS(WINDOWS)
    // This is not needed on Windows currently.
    UNREACHABLE();
#else
    int pipeFds[2];
    int status = pipe(pipeFds);
    if (status == -1) {
        return DAWN_INTERNAL_ERROR("Failed to create POSIX pipe");
    }

    OSEventReceiver receiver{pipeFds[0]};

    OSEventPipe sender;
    sender.mPrimitive = {pipeFds[1]};

    return std::make_pair(std::move(sender), std::move(receiver));
#endif
}

OSEventPipe::OSEventPipe(OSEventPipe&& other) {
    *this = std::move(other);
}

OSEventPipe& OSEventPipe::operator=(OSEventPipe&& other) {
    mPrimitive = other.mPrimitive;
    other.mPrimitive = OSEventPrimitive{};
    return *this;
}

OSEventPipe::~OSEventPipe() {
    ASSERT(!mPrimitive.IsValid());
}

MaybeError OSEventPipe::Signal() {
    ASSERT(mPrimitive.IsValid());
#if DAWN_PLATFORM_IS(WINDOWS)
    // This is not needed on Windows currently.
    UNREACHABLE();
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
