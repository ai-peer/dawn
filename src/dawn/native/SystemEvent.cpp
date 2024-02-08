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

SystemEventReceiver::SystemEventReceiver(SystemHandle primitive)
    : mPrimitive(std::move(primitive)) {}

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

bool SystemEventPipeSender::IsValid() const {
    return mPrimitive.IsValid();
}

void SystemEventPipeSender::Signal() && {
    DAWN_ASSERT(mPrimitive.IsValid());
#if DAWN_PLATFORM_IS(WINDOWS)
    DAWN_CHECK(SetEvent(mPrimitive.Get()));
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
    HANDLE eventDup;
    HANDLE event = CreateEvent(nullptr, /*bManualReset=*/true, /*bInitialState=*/false, nullptr);

    DAWN_CHECK(event != nullptr);
    DAWN_CHECK(DuplicateHandle(GetCurrentProcess(), event, GetCurrentProcess(), &eventDup, 0, FALSE,
                               DUPLICATE_SAME_ACCESS));
    DAWN_CHECK(eventDup != nullptr);

    SystemEventReceiver receiver;
    receiver.mPrimitive = SystemHandle::Acquire(event);

    SystemEventPipeSender sender;
    sender.mPrimitive = SystemHandle::Acquire(eventDup);

    return std::make_pair(std::move(sender), std::move(receiver));
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

// SystemEvent

// static
Ref<SystemEvent> SystemEvent::CreateSignaled() {
    auto ev = AcquireRef(new SystemEvent());
    ev->Signal();
    return ev;
}

bool SystemEvent::IsSignaled() const {
    return mSignaled.load(std::memory_order_acquire);
}

void SystemEvent::Signal() {
    if (!mSignaled.exchange(true, std::memory_order_acq_rel)) {
        mPipes.Use([](auto pipes) {
            // Signal all senders.
            for (auto& sender : pipes->senders) {
                std::move(sender).Signal();
            }
            pipes->senders.clear();
        });
    }
}

Ref<SharedSystemEventReceiver> SystemEvent::GetOrCreateSharedSystemEventReceiver() {
    return std::get<Ref<SharedSystemEventReceiver>>(
        GetOrCreateSystemEventReceiver(/*shared=*/true));
}

SystemEventReceiver SystemEvent::GetOrCreateNotSharedSystemEventReceiver() {
    return std::get<SystemEventReceiver>(GetOrCreateSystemEventReceiver(/*shared=*/false));
}

std::variant<SystemEventReceiver, Ref<SharedSystemEventReceiver>>
SystemEvent::GetOrCreateSystemEventReceiver(bool shared) {
    return mPipes.Use(
        [&](auto pipes) -> std::variant<SystemEventReceiver, Ref<SharedSystemEventReceiver>> {
            if (shared && pipes->sharedReceiver) {
                // Return the shared receiver if there is one.
                return {pipes->sharedReceiver};
            }

            SystemEventReceiver receiver;
            if (pipes->receivers.empty()) {
                // Create a new sender/receiver if there are no receivers available.
                SystemEventPipeSender sender{};
                // Check whether the event was marked as completed. This may have happened if
                // this function races with another thread performing Signal. If we won
                // the race, then the pipe we just created will get signaled inside Signal.
                // If we lost the race, then it will not be signaled and we must do it now.
                if (IsSignaled()) {
                    receiver = SystemEventReceiver::CreateAlreadySignaled();
                } else {
                    std::tie(sender, receiver) = CreateSystemEventPipe();
                    pipes->senders.push_back(std::move(sender));
                }
            } else {
                // Acquire a receiver from the list of recycled receivers.
                receiver = std::move(pipes->receivers.back());
                pipes->receivers.pop_back();
            }
            if (!shared) {
                // If we should not share the receiver, return it.
                return {std::move(receiver)};
            }
            // Otherwise, set it as the shared receiver and return it.
            pipes->sharedReceiver = AcquireRef(new SharedSystemEventReceiver(std::move(receiver)));
            return {pipes->sharedReceiver};
        });
}

void SystemEvent::ReturnReceiverToPool(SystemEventReceiver receiver) {
    mPipes.Use([&](auto pipes) { pipes->receivers.push_back(std::move(receiver)); });
}

}  // namespace dawn::native
