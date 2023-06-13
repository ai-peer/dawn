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

#include "dawn/common/MutexProtected.h"
#include "dawn/common/NonCopyable.h"
#include "dawn/common/Ref.h"
#include "dawn/native/Error.h"
#include "dawn/native/TrackedEvent.h"

namespace dawn::native {

struct InstanceDescriptor;

// Subcomponent of the Instance which tracks callback events for the Future-based callback
// entrypoints. All events from this instance (regardless of whether from an adapter, device, queue,
// etc.) are tracked here, and used by the instance-wide ProcessEvents and WaitAny entrypoints.
//
// TODO(crbug.com/dawn/1987): Can this eventually replace CallbackTaskManager?
class EventManager final : NonMovable {
  public:
    EventManager() = default;
    // TODO(crbug.com/dawn/1987): Clean up any leftover callbacks in the destructor, which happens
    // on instance destruction, and test this. See also CallbackTaskManager.
    ~EventManager() = default;

    MaybeError Initialize(const InstanceDescriptor*);

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

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_EVENTMANAGER_H_
