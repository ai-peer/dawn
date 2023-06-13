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
#include <vector>

#include "dawn/wire/ObjectHandle.h"
#include "dawn/wire/client/Client.h"
#include "dawn/wire/client/EventManager.h"

namespace dawn::wire::client {

// EventManager

EventManager::EventManager(Client* client) : mClient(client) {}

FutureID EventManager::TrackEvent(WGPUCallbackModeFlags mode, EventCallback&& callback) {
    DAWN_UNUSED(ValidateAndFlattenCallbackMode(mode));

    FutureID futureID = mNextFutureID++;

    if (mClient->IsDisconnected()) {
        callback(EventStatus::Shutdown);
        return futureID;
    }

    mTrackedEvents.Use([&](auto trackedEvents) {
        auto [it, inserted] =
            trackedEvents->emplace(futureID, TrackedEvent(mode, std::move(callback)));
        ASSERT(inserted);
    });

    return futureID;
}

void EventManager::Shutdown() {
    // Call any outstanding callbacks before destruction.
    while (true) {
        std::unordered_map<FutureID, TrackedEvent> movedEvents;
        mTrackedEvents.Use([&](auto trackedEvents) { movedEvents = std::move(*trackedEvents); });

        if (movedEvents.empty()) {
            break;
        }

        for (auto& [futureID, trackedEvent] : movedEvents) {
            // Event should be already marked Ready since events are actually driven by
            // RequestTrackers (at the time of this writing), which all shut down before this.
            ASSERT(trackedEvent.mReady);
            trackedEvent.mCallback(EventStatus::Shutdown);
            trackedEvent.mCallback = nullptr;
        }
    }
}

void EventManager::SetFutureReady(FutureID futureID) {
    ASSERT(futureID > 0);
    mTrackedEvents.Use([&](auto trackedEvents) {
        TrackedEvent& trackedEvent = trackedEvents->at(futureID);  // Asserts futureID is in the map
        trackedEvent.mReady = true;
    });
    // TODO(crbug.com/dawn/1987): Handle spontaneous completions.
}

void EventManager::ProcessPollEvents() {
    std::vector<TrackedEvent> eventsToCompleteNow;

    mTrackedEvents.Use([&](auto trackedEvents) {
        std::erase_if(*trackedEvents, [&](auto& item) {
            TrackedEvent& event = item.second;
            // TODO(crbug.com/dawn/1987): ProcessEvents should track separately so we don't have
            // to filter through everything here.
            bool shouldRemove = (event.mMode & WGPUCallbackMode_ProcessEvents) && event.mReady;
            // mCallback may still be null if it's stale (was already spontaneously completed).
            if (shouldRemove && event.mCallback) {
                eventsToCompleteNow.emplace_back(std::move(event));
            }
            return shouldRemove;
        });
    });

    for (TrackedEvent& event : eventsToCompleteNow) {
        ASSERT(event.mReady && event.mCallback);
        event.mCallback(EventStatus::Ready);
        event.mCallback = nullptr;
    }
}

WGPUWaitStatus EventManager::WaitAny(size_t count, WGPUFutureWaitInfo* infos, uint64_t timeoutNS) {
    // Validate for feature support.
    if (timeoutNS > 0) {
        // Wire doesn't support timedWaitEnable (for now). (There's no UnsupportedCount or
        // UnsupportedMixedSources validation here, because those only apply to timed waits.)
        //
        // TODO(crbug.com/dawn/1987): CreateInstance needs to validate timedWaitEnable was false.
        return WGPUWaitStatus_UnsupportedTimeout;
    }

    if (count == 0) {
        return WGPUWaitStatus_Success;
    }

    std::vector<TrackedEvent> eventsToCompleteNow;
    bool anyCompleted = false;
    mTrackedEvents.Use([&](auto trackedEvents) {
        for (size_t i = 0; i < count; ++i) {
            FutureID futureID = infos[i].future.id;
            ASSERT(futureID < mNextFutureID);

            auto it = trackedEvents->find(futureID);
            if (it == trackedEvents->end()) {
                infos[i].completed = true;
                anyCompleted = true;
            } else {
                TrackedEvent& event = it->second;
                ASSERT(event.mMode & WGPUCallbackMode_Future);
                infos[i].completed = event.mReady;
                if (event.mReady) {
                    anyCompleted = true;
                    if (event.mCallback) {
                        eventsToCompleteNow.emplace_back(std::move(event));
                    }
                    trackedEvents->erase(it);
                }
            }
        }
    });

    // TODO(crbug.com/dawn/1987): Guarantee the event ordering from the JS spec.
    for (TrackedEvent& event : eventsToCompleteNow) {
        ASSERT(event.mReady && event.mCallback);
        // .completed has already been set to true (before the callback, per API contract).
        event.mCallback(EventStatus::Ready);
        event.mCallback = nullptr;
    }

    return anyCompleted ? WGPUWaitStatus_Success : WGPUWaitStatus_TimedOut;
}

// EventManager::TrackedEvent

EventManager::TrackedEvent::TrackedEvent(WGPUCallbackModeFlags mode, EventCallback&& callback)
    : mMode(mode), mCallback(callback) {}

EventManager::TrackedEvent::~TrackedEvent() {
    // Make sure we're not dropping a callback on the floor.
    ASSERT(!mCallback);
}

}  // namespace dawn::wire::client
