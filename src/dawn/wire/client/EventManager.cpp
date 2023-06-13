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

#include <unordered_map>
#include <utility>

#include "dawn/wire/ObjectHandle.h"
#include "dawn/wire/client/Client.h"
#include "dawn/wire/client/EventManager.h"

namespace dawn::wire::client {

// EventManager

EventManager::EventManager(Client* client) : mClient(client) {}

FutureID EventManager::TrackEvent(WGPUCallbackModeFlags mode, std::function<void()>&& callback) {
    std::lock_guard lock(mTrackedEventsMutex);

    FutureID futureID = mNextFutureID++;
    auto [it, inserted] = mTrackedEvents.emplace(futureID, TrackedEvent(mode, std::move(callback)));
    ASSERT(inserted);
    if (mClient->IsDisconnected()) {
        it->second.SetReady();
    }

    return futureID;
}

void EventManager::SetFutureReady(FutureID futureID) {
    std::lock_guard lock(mTrackedEventsMutex);
    TrackedEvent& trackedEvent = mTrackedEvents.at(futureID);  // Asserts futureID is in the map.
    trackedEvent.SetReady();
}

void EventManager::ProcessPollEvents() {
    std::lock_guard lock(mTrackedEventsMutex);
    for (auto& [futureID, trackedEvent] : mTrackedEvents) {
        if (trackedEvent.mMode & WGPUCallbackMode_ProcessEvents) {
            if (trackedEvent.CheckReadyAndCompleteIfNeeded()) {
                mTrackedEvents.erase(futureID);
            }
        }
    }
}

WGPUWaitStatus EventManager::WaitAny(size_t count, WGPUFutureWaitInfo* infos, uint64_t timeoutNS) {
    if (count == 0) {
        return WGPUWaitStatus_Success;
    }

    std::lock_guard lock(mTrackedEventsMutex);

    bool anyCompleted = false;
    for (size_t i = 0; i < count; ++i) {
        FutureID futureID = infos[i].future.id;
        ASSERT(futureID < mNextFutureID);

        auto it = mTrackedEvents.find(futureID);
        if (it == mTrackedEvents.end()) {
            infos[i].completed = true;
            anyCompleted = true;
        } else {
            TrackedEvent& event = it->second;
            ASSERT(event.mMode & WGPUCallbackMode_Future);
            infos[i].completed = false;
        }
    }
    if (anyCompleted) {
        return WGPUWaitStatus_Success;
    }

    // Validate for feature support.
    // Note this is after .completed fields get set, so they'll be correct even if there's an error.
    if (timeoutNS > 0) {
        // Wire doesn't support timedWaitEnable (yet).
        // TODO(crbug.com/dawn/1987): CreateInstance needs to validate that it wasn't requested.
        return WGPUWaitStatus_UnsupportedTimeout;

        // There's no UnsupportedCount validation here because that only applies to timed waits.
    }
    // TODO(crbug.com/dawn/1987): Implement UnsupportedMixedSources validation. This requires
    // knowing the Device for a given event.

    for (size_t i = 0; i < count; ++i) {
        FutureID futureID = infos[i].future.id;
        // We know here that the future must be tracked, since anyCompleted is false.
        TrackedEvent& event = mTrackedEvents.at(futureID);

        // TODO(crbug.com/dawn/1987): Guarantee the event ordering from the JS spec.
        bool ready = event.CheckReadyAndCompleteIfNeeded();
        if (ready) {
            infos[i].completed = true;
            mTrackedEvents.erase(futureID);
            anyCompleted = true;
        }
    }

    return anyCompleted ? WGPUWaitStatus_Success : WGPUWaitStatus_TimedOut;
}

// EventManager::TrackedEvent

EventManager::TrackedEvent::TrackedEvent(WGPUCallbackModeFlags mode,
                                         std::function<void()>&& callback)
    : mMode(mode), mCallback(callback) {}

void EventManager::TrackedEvent::SetReady() {
    mReady = true;
    if ((mMode & WGPUCallbackMode_Spontaneous) && mCallback) {
        mCallback();
        mCallback = {};
    }
}

bool EventManager::TrackedEvent::CheckReadyAndCompleteIfNeeded() {
    if (mReady && mCallback) {
        mCallback();
        mCallback = {};
    }
    return mReady;
}

}  // namespace dawn::wire::client
