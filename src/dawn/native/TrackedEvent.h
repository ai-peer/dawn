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

#ifndef SRC_DAWN_NATIVE_TRACKEDEVENT_H_
#define SRC_DAWN_NATIVE_TRACKEDEVENT_H_

#include <atomic>
#include <optional>

#include "dawn/common/FutureUtils.h"
#include "dawn/common/Ref.h"
#include "dawn/common/RefCounted.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/OSEventReceiver.h"
#include "dawn/webgpu_cpp.h"

namespace dawn::native {

class DeviceBase;
class InstanceBase;
class QueueBase;

// Base class for the objects that back WGPUFutures. A map of FutureID->TrackedEvent is stored in
// the Instance. That's the primary owner, but it's also refcounted so that WaitAny can take a ref
// while it's running and not hold an instance-global lock for the entire duration of the wait (in
// case it `Spontaneous`ly completes during the wait). In some cases it may also be useful to hold
// a ref somewhere else (like on the thread doing async pipeline creation, so we can signal it
// directly instead of through an OS event, as an optimization).
class TrackedEvent : public RefCounted {
  protected:
    TrackedEvent(InstanceBase* instance,
                 WGPUCallbackModeFlags callbackMode,
                 OSEventReceiver&& receiver);

  public:
    ~TrackedEvent() override;

    OSEventPrimitive::T GetPrimitive() const;
    void EnsureCompleteFromProcessEvents();
    void EnsureCompleteFromWaitAny();

    virtual DeviceBase* GetWaitDevice() const = 0;

    class WaitRef;
    WaitRef TakeWaitRef();

    // A Ref<TrackedEvent>, but ASSERTing that a future isn't used concurrently in multiple
    // WaitAny/ProcessEvents call (by checking that there's never more than one WaitRef for a
    // TrackedEvent). For WaitAny, this checks the embedder's behavior, but for ProcessEvents this
    // is only an internal ASSERT (it's supposed to be synchronized so that this never happens).
    class WaitRef {
      public:
        WaitRef(WaitRef&& rhs) = default;
        WaitRef& operator=(WaitRef&& rhs) = default;

        explicit WaitRef(TrackedEvent* future);
        ~WaitRef();

        TrackedEvent* operator->();
        const TrackedEvent* operator->() const;

      private:
        Ref<TrackedEvent> mRef;
    };

  protected:
    void CompleteIfSpontaneous();

    virtual void Complete() = 0;

    Ref<InstanceBase> mInstance;
    WGPUCallbackModeFlags mCallbackMode;

#if DAWN_ENABLE_ASSERTS
    std::atomic<bool> mCurrentlyBeingWaited;
#endif

  private:
    void EnsureComplete();

    // TODO(crbug.com/dawn/1987): Optimize by creating an OSEventReceiver only once actually needed
    // (the user asks for a timed wait or an OS event handle). This should be generally achievable:
    // - For thread-driven events (async pipeline compilation and Metal queue events), use a mutex
    //   or atomics to atomically:
    //   - On wait: { check if mKnownReady. if not, create the OSEventPipe }
    //   - On signal: { check if there's an OSEventPipe. if not, set mKnownReady }
    // - For D3D12/Vulkan fences, on timed waits, first use GetCompletedValue/GetFenceStatus, then
    //   create an OS event if it's not ready yet (and we don't have one yet).
    OSEventReceiver mReceiver;
    // Callback has been called.
    std::atomic<bool> mCompleted = false;
};

// TrackedEvent::WaitRef plus a few extra fields needed for some implementations.
// Sometimes they'll be unused, but that's OK; it simplifies code reuse.
struct TrackedFutureWaitInfo {
    FutureID futureID;
    TrackedEvent::WaitRef event;
    // Used by EventManager::ProcessPollEvents
    size_t indexInInfos;
    // Used by EventManager::ProcessPollEvents and ::WaitAny
    bool ready;
};

class WorkDoneEvent final : public TrackedEvent {
  public:
    static FutureID Create(QueueBase* queue, const WGPUQueueWorkDoneCallbackInfo& callbackInfo);

    DeviceBase* GetWaitDevice() const override;

  private:
    static MaybeError Validate(QueueBase* queue, WGPUQueueWorkDoneStatus* earlyStatus);

    // Create an event that's ready at creation (for errors, etc.)
    WorkDoneEvent(QueueBase* queue,
                  const WGPUQueueWorkDoneCallbackInfo& callbackInfo,
                  WGPUQueueWorkDoneStatus earlyStatus);
    // Create an event backed by the given OSEventReceiver.
    WorkDoneEvent(QueueBase* queue,
                  const WGPUQueueWorkDoneCallbackInfo& callbackInfo,
                  OSEventReceiver&& receiver);
    ~WorkDoneEvent() override = default;

    void Complete() override;

    Ref<QueueBase> mQueue;
    std::optional<WGPUQueueWorkDoneStatus> mEarlyStatus;
    WGPUQueueWorkDoneCallback mCallback;
    void* mUserdata;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_TRACKEDEVENT_H_
