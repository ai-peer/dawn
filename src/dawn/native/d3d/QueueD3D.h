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

#ifndef SRC_DAWN_NATIVE_D3D_QUEUED3D_H_
#define SRC_DAWN_NATIVE_D3D_QUEUED3D_H_

#include "dawn/common/MutexProtected.h"
#include "dawn/common/SerialMap.h"
#include "dawn/common/windows_with_undefs.h"
#include "dawn/native/Queue.h"
#include "dawn/native/SystemEvent.h"

namespace dawn::native::d3d {

class SharedFence;

class Queue : public QueueBase {
  public:
    using QueueBase::QueueBase;
    ~Queue() override;

    virtual ResultOrError<Ref<SharedFence>> GetOrCreateSharedFence() = 0;

    void RegisterSpontaneousEvent(Ref<EventManager::TrackedEvent> event,
                                  ExecutionSerial completionSerial) override;

  private:
    // SpontaneousEventTracker is registered with the Windows OS and receives
    // callbacks when OS events complete. These are forwarded to resolve
    // spontaneous-mode Futures.
    class SpontaneousEventTracker {
      public:
        explicit SpontaneousEventTracker(SystemHandle fenceHandle);

        // Unregister the tracker with the OS. This is done inside the OS
        // callback once it is called, or if the Queue is destructed before
        // we receive the OS callbacks.
        void Unregister();

        // Add a spontaneous-mode tracked event to this tracker. May call
        // the Future callback immediately if the event has already completed.
        void AddEvent(Ref<EventManager::TrackedEvent> event);

      private:
        // Called by the OS. Unregisters the tracker and calls the
        // spontaneous-mode Future callbacks.
        static void Callback(void* userdata, unsigned char timedOut);

        std::atomic<bool> mActive;
        SystemHandle mFenceHandle;
        SystemHandle mWaitHandle;
        MutexProtected<std::vector<Ref<EventManager::TrackedEvent>>> mEvents;
    };

    virtual void SetEventOnCompletion(ExecutionSerial serial, HANDLE event) = 0;

    ResultOrError<bool> WaitForQueueSerial(ExecutionSerial serial, Nanoseconds timeout) override;

    MutexProtected<SerialMap<ExecutionSerial, SystemEventReceiver>> mSystemEventReceivers;
    MutexProtected<SerialMap<ExecutionSerial, std::unique_ptr<SpontaneousEventTracker>>>
        mSpontaneousEventTrackers;
};

}  // namespace dawn::native::d3d

#endif  // SRC_DAWN_NATIVE_D3D_QUEUED3D_H_
