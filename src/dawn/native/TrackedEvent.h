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

class TrackedEvent : public RefCounted {
  protected:
    TrackedEvent(InstanceBase* instance, WGPUCallbackModeFlags callbackMode);

  public:
    ~TrackedEvent() override;

    OSEventPrimitive::T GetPrimitive() const;
    void SetReceiver(OSEventReceiver&&);
    bool IsEarlyReady() const;
    void EnsureCompleteFromProcessEvents();
    void EnsureCompleteFromWaitAny();

    virtual DeviceBase* GetWaitDevice() const = 0;

    class WaitRef;
    WaitRef TakeWaitRef();

    // A Ref<TrackedEvent>, but with extra ASSERTs.
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
    // Override the state to Ready, in early-failure cases.
    void TriggerEarlyReady();

    virtual void Complete() = 0;

    Ref<InstanceBase> mInstance;
    WGPUCallbackModeFlags mCallbackMode;
    OSEventReceiver mReceiver;

    // Marked as early-ready, but may or may not be completed yet.
    std::atomic<bool> mEarlyReady = false;

#if DAWN_ENABLE_ASSERTS
    std::atomic<bool> mCurrentlyBeingWaited;
#endif

  private:
    void EnsureComplete();

    // Callback has been called.
    std::atomic<bool> mCompleted = false;
};

// TrackedEvent::WaitRef plus a few extra fields needed for WaitAny implementations.
struct TrackedFutureWaitInfo {
    FutureID futureID;
    TrackedEvent::WaitRef event;
    size_t indexInInfos;
    bool ready;
};

class WorkDoneEvent final : public TrackedEvent {
  public:
    static FutureID Create(QueueBase* queue, const WGPUQueueWorkDoneCallbackInfo& callbackInfo);

    WorkDoneEvent(QueueBase* queue, const WGPUQueueWorkDoneCallbackInfo& callbackInfo);
    ~WorkDoneEvent() override = default;

    // Mark the event as having become ready early, and return a new
    // already-completed FutureID (if needed by the callback mode).
    void SetEarlyReady(WGPUQueueWorkDoneStatus status);

    DeviceBase* GetWaitDevice() const override;

  private:
    MaybeError Validate(WGPUQueueWorkDoneStatus* earlyStatus);
    void Complete() override;

    Ref<QueueBase> mQueue;
    ExecutionSerial mSerial;

    // The early-ready status (used iff TrackedEvent::mState is EarlyReady).
    WGPUQueueWorkDoneStatus mEarlyStatus = WGPUQueueWorkDoneStatus_Unknown;

    WGPUQueueWorkDoneCallback mCallback;
    void* mUserdata;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_TRACKEDEVENT_H_
