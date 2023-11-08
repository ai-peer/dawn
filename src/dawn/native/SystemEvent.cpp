// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/native/SystemEvent.h"
#include <limits>
#include "dawn/common/Assert.h"

#if DAWN_PLATFORM_IS(WINDOWS)
#include <windows.h>
#elif DAWN_PLATFORM_IS(POSIX)
#include <sys/poll.h>
#include <unistd.h>
#endif

#include <tuple>
#include <utility>
#include <vector>

#include "dawn/native/EventManager.h"

namespace dawn::native {

// SystemEventReceiver

SystemEventReceiver SystemEventReceiver::CreateAlreadySignaled() {
    SystemEventPipeSender sender;
    SystemEventReceiver receiver;
    std::tie(sender, receiver) = CreateSystemEventPipe();
    std::move(sender).Signal();
    return receiver;
}

// SystemEventPipeSender

SystemEventPipeSender::~SystemEventPipeSender() {
    // Make sure it's been Signaled (or is empty) before being dropped.
    // Dropping this would "leak" the receiver (it'll never get signalled).
    DAWN_ASSERT(!mPrimitive.IsValid());
}

void SystemEventPipeSender::Signal() && {
    DAWN_ASSERT(mPrimitive.IsValid());
#if DAWN_PLATFORM_IS(WINDOWS)
    // This is not needed on Windows yet. It's implementable using SetEvent().
    DAWN_UNREACHABLE();
#elif DAWN_PLATFORM_IS(POSIX)
    // Send one byte to signal the receiver
    char zero[1] = {0};
    int status = write(mPrimitive.Get(), zero, 1);
    DAWN_CHECK(status >= 0);
#else
    // Not implemented for this platform.
    DAWN_CHECK(false);
#endif

    mPrimitive.Close();
}

std::pair<SystemEventPipeSender, SystemEventReceiver> CreateSystemEventPipe() {
#if DAWN_PLATFORM_IS(WINDOWS)
    // This is not needed on Windows yet. It's implementable using CreateEvent().
    DAWN_UNREACHABLE();
#elif DAWN_PLATFORM_IS(POSIX)
    int pipeFds[2];
    int status = pipe(pipeFds);
    DAWN_CHECK(status >= 0);

    SystemEventReceiver receiver;
    receiver.mPrimitive = SystemHandle::Acquire(pipeFds[0]);

    SystemEventPipeSender sender;
    sender.mPrimitive = SystemHandle::Acquire(pipeFds[1]);

    return std::make_pair(std::move(sender), std::move(receiver));
#else
    // Not implemented for this platform.
    DAWN_CHECK(false);
#endif
}

}  // namespace dawn::native
