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

#ifndef SRC_DAWN_NATIVE_EVENTMANAGER_H_
#define SRC_DAWN_NATIVE_EVENTMANAGER_H_

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "dawn/common/FutureUtils.h"
#include "dawn/common/MutexProtected.h"
#include "dawn/common/NonCopyable.h"
#include "dawn/common/Ref.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/OSEventReceiver.h"

namespace dawn::native {

struct InstanceDescriptor;

// Subcomponent of the Instance which tracks callback events for the Future-based callback
// entrypoints. All events from this instance (regardless of whether from an adapter, device, queue,
// etc.) are tracked here, and used by the instance-wide ProcessEvents and WaitAny entrypoints.
//
// TODO(crbug.com/dawn/1987): Can this eventually replace CallbackTaskManager?
// TODO(crbug.com/dawn/1987): There are various ways to optimize ProcessEvents/WaitAny:
// - Only pay attention to the earliest serial on each queue.
// - Spontaneously set events as "early-ready" in other places when we see serials advance, e.g.
//   Submit, or when checking a later wait before an earlier wait.
// - For thread-driven events (async pipeline compilation and Metal queue events), defer tracking
//   for ProcessEvents until the event is already completed.
// - Avoid creating OS events until they're actually needed (see the todo in TrackedEvent).
class EventManager final : NonMovable {
  public:
    EventManager() = default;
    // TODO(crbug.com/dawn/1987): Clean up any leftover callbacks in the destructor, which happens
    // on instance destruction, and test this. See also CallbackTaskManager.
    ~EventManager() = default;

    MaybeError Initialize(const InstanceDescriptor*);

    class TrackedEvent;
    // Track a TrackedEvent and give it a FutureID.
    [[nodiscard]] FutureID TrackEvent(WGPUCallbackModeFlags mode, TrackedEvent*);
    void ProcessPollEvents();
    [[nodiscard]] wgpu::WaitStatus WaitAny(size_t count,
                                           FutureWaitInfo* infos,
                                           Nanoseconds timeout);

  private:
    bool mTimedWaitAnyEnable = false;
    size_t mTimedWaitAnyMaxCount = kTimedWaitAnyMaxCountDefault;

    // Tracks Futures (used by WaitAny).
    MutexProtected<std::unordered_map<FutureID, Ref<TrackedEvent>>> mTrackedFutures;
    std::atomic<FutureID> mNextFutureID = 1;

    // Tracks events polled by ProcessEvents.
    MutexProtected<std::unordered_map<FutureID, Ref<TrackedEvent>>> mTrackedPollEvents;
};

// Base class for the objects that back WGPUFutures. A map of FutureID->TrackedEvent is stored
// in the Instance. That's the primary owner, but it's also refcounted so that WaitAny can take
// a ref while it's running and not hold an instance-global lock for the entire duration of the
// wait (in case it `Spontaneous`ly completes during the wait). In some cases it may also be
// useful to hold a ref somewhere else (like on the thread doing async pipeline creation, so we
// can signal it directly instead of through an OS event, as an optimization).
class EventManager::TrackedEvent : public RefCounted {
  protected:
    TrackedEvent(InstanceBase* instance,
                 WGPUCallbackModeFlags callbackMode,
                 OSEventReceiver&& receiver);

    friend class EventManager;

  public:
    ~TrackedEvent() override;

    class WaitRef;

    OSEventPrimitive::T GetPrimitive() const;
    virtual DeviceBase* GetWaitDevice() const = 0;

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

// A Ref<TrackedEvent>, but ASSERTing that a future isn't used concurrently in multiple
// WaitAny/ProcessEvents call (by checking that there's never more than one WaitRef for a
// TrackedEvent). For WaitAny, this checks the embedder's behavior, but for ProcessEvents this is
// only an internal ASSERT (it's supposed to be synchronized so that this never happens).
class EventManager::TrackedEvent::WaitRef : dawn::NonCopyable {
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

// TrackedEvent::WaitRef plus a few extra fields needed for some implementations.
// Sometimes they'll be unused, but that's OK; it simplifies code reuse.
struct TrackedFutureWaitInfo {
    FutureID futureID;
    EventManager::TrackedEvent::WaitRef event;
    // Used by EventManager::ProcessPollEvents
    size_t indexInInfos;
    // Used by EventManager::ProcessPollEvents and ::WaitAny
    bool ready;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_EVENTMANAGER_H_
