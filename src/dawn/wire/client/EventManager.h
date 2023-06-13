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

#ifndef SRC_DAWN_WIRE_CLIENT_EVENTMANAGER_H_
#define SRC_DAWN_WIRE_CLIENT_EVENTMANAGER_H_

#include <cstddef>
#include <functional>
#include <unordered_map>

#include "dawn/common/FutureUtils.h"
#include "dawn/common/Ref.h"
#include "dawn/webgpu.h"
#include "dawn/wire/ObjectHandle.h"

namespace dawn::wire::client {

class Client;

class EventManager {
  public:
    EventManager(EventManager&) = delete;
    EventManager(EventManager&&) = delete;
    EventManager& operator=(EventManager&) = delete;
    EventManager& operator=(EventManager&&) = delete;

    explicit EventManager(Client*);
    ~EventManager() = default;  // FIXME: handle cleanup stuff like RequestTracker

    FutureID TrackEvent(WGPUCallbackModeFlags mode, std::function<void()>&& callback);
    void SetFutureReady(FutureID futureID);
    void ProcessPollEvents();
    WGPUWaitStatus WaitAny(size_t count, WGPUFutureWaitInfo* infos, uint64_t timeoutNS);

  private:
    struct TrackedEvent {
        TrackedEvent(WGPUCallbackModeFlags mode, std::function<void()>&& callback);

        // FIXME: actually call this ever
        void SetReady();
        bool CheckReadyAndCompleteIfNeeded();

        WGPUCallbackModeFlags mMode;
        // Callback. Falsey if already called.
        std::function<void()> mCallback;
        bool mReady = false;
    };

    Client* const mClient;

    // Must be held to use mTrackedEvents.
    std::mutex mTrackedEventsMutex;
    // Tracks all kinds of events (for both WaitAny and ProcessEvents).
    std::unordered_map<FutureID, TrackedEvent> mTrackedEvents;
    std::atomic<FutureID> mNextFutureID;
};

}  // namespace dawn::wire::client

#endif  // SRC_DAWN_WIRE_CLIENT_EVENTMANAGER_H_
