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
#include <map>
#include <memory>
#include <utility>

#include "dawn/common/FutureUtils.h"
#include "dawn/common/MutexProtected.h"
#include "dawn/common/NonCopyable.h"
#include "dawn/common/Ref.h"
#include "dawn/webgpu.h"
#include "dawn/wire/ObjectHandle.h"
#include "dawn/wire/WireResult.h"
#include "dawn/wire/client/Client.h"

namespace dawn::wire::client {

enum class EventType {
    MapAsync,
    WorkDone,
};

namespace detail {

class TrackedEventBase : NonMovable {
  public:
    explicit TrackedEventBase(WGPUCallbackMode mode);
    virtual ~TrackedEventBase();

    virtual EventType GetEventType() const = 0;
    WGPUCallbackMode GetCallbackMode() const;
    virtual bool IsReady() const = 0;
    virtual void Complete(EventCompletionType type) = 0;

  private:
    const WGPUCallbackMode mMode;
};

}  // namespace detail

template <EventType EType, typename CallbackT, typename DataT>
class TrackedEvent : public detail::TrackedEventBase {
  public:
    static constexpr EventType Type = EType;
    using Data = DataT;

    static TrackedEvent& ToEvent(TrackedEventBase& event) {
        DAWN_ASSERT(event.GetEventType() == Type);
        return static_cast<TrackedEvent&>(event);
    }

    TrackedEvent(WGPUCallbackMode mode, CallbackT callback, void* userdata)
        : TrackedEventBase(mode), mCallback(callback), mUserdata(userdata) {}
    ~TrackedEvent() override { DAWN_ASSERT(mCallback == nullptr); }

    EventType GetEventType() const final { return Type; }
    bool IsReady() const final { return mData.has_value(); }
    void Complete(EventCompletionType type) final {
        DAWN_ASSERT(type == EventCompletionType::Shutdown || IsReady());
        CompleteImpl(type);
        mCallback = nullptr;
    }
    void SetReady(Data&& readyData) { mData = readyData; }

  protected:
    virtual void CompleteImpl(EventCompletionType type) = 0;

    CallbackT mCallback;
    void* mUserdata;
    std::optional<DataT> mData;
};

// Subcomponent which tracks callback events for the Future-based callback
// entrypoints. All events from this instance (regardless of whether from an adapter, device, queue,
// etc.) are tracked here, and used by the instance-wide ProcessEvents and WaitAny entrypoints.
//
// TODO(crbug.com/dawn/2060): This should probably be merged together with RequestTracker.
class EventManager final : NonMovable {
  public:
    explicit EventManager(Client*);
    ~EventManager() = default;

    // Returns a pair of the FutureID and a bool that is true iff the event was successfuly tracked,
    // false otherwise. Events may not be tracked if the client is already disconnected.
    std::pair<FutureID, bool> TrackEvent(detail::TrackedEventBase* event);
    void ShutDown();

    template <typename T>
    void SetFutureReady(FutureID futureID, T::Data&& readyData) {
        DAWN_ASSERT(futureID > 0);
        // If the client was already disconnected, then all the callbacks should already have fired
        // so we don't need to fire the callback anymore.
        if (mClient->IsDisconnected()) {
            return;
        }

        std::optional<std::unique_ptr<detail::TrackedEventBase>> event;
        mTrackedEvents.Use([&](auto trackedEvents) {
            std::unique_ptr<detail::TrackedEventBase>& trackedEvent =
                trackedEvents->at(futureID);  // Asserts futureID is in the map

            // Asserts that the event is the expected type and sets the ready data.
            T::ToEvent(*trackedEvent.get()).SetReady(std::move(readyData));

            // If the event can be spontaneously completed, do so now.
            if (trackedEvent->GetCallbackMode() == WGPUCallbackMode_AllowSpontaneous) {
                event = std::move(trackedEvent);
                trackedEvents->erase(futureID);
            }
        });

        // Handle spontaneous completions.
        if (event.has_value()) {
            std::move(*event)->Complete(EventCompletionType::Ready);
        }
    }

    void ProcessPollEvents();
    WGPUWaitStatus WaitAny(size_t count, WGPUFutureWaitInfo* infos, uint64_t timeoutNS);

  private:
    Client* mClient;

    // Tracks all kinds of events (for both WaitAny and ProcessEvents). We use an ordered map so
    // that in most cases, event ordering is already implicit when we iterate the map. (Not true for
    // WaitAny though because the user could specify the FutureIDs out of order.)
    MutexProtected<std::map<FutureID, std::unique_ptr<detail::TrackedEventBase>>> mTrackedEvents;
    std::atomic<FutureID> mNextFutureID = 1;
};

}  // namespace dawn::wire::client

#endif  // SRC_DAWN_WIRE_CLIENT_EVENTMANAGER_H_
