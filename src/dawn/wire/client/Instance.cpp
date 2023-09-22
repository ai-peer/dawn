// Copyright 2021 The Dawn Authors
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

#include "dawn/wire/client/Instance.h"

#include <utility>

#include "dawn/common/Log.h"
#include "dawn/wire/client/Client.h"
#include "dawn/wire/client/EventManager.h"

namespace dawn::wire::client {
namespace {

struct RequestAdapterEventData {
    WGPURequestAdapterStatus status;
    const char* message;
    const WGPUAdapterProperties* properties;
    const WGPUSupportedLimits* limits;
    uint32_t featuresCount;
    const WGPUFeatureName* features;
};

using RequestAdapterEventBase =
    TrackedEvent<EventType::RequestAdapter, WGPURequestAdapterCallback, RequestAdapterEventData>;
class RequestAdapterEvent : public RequestAdapterEventBase {
  public:
    RequestAdapterEvent(const WGPURequestAdapterCallbackInfo& callbackInfo, Adapter* adapter)
        : RequestAdapterEventBase(callbackInfo.mode, callbackInfo.callback, callbackInfo.userdata),
          mAdapter(adapter) {}

  private:
    void CompleteImpl(EventCompletionType completionType) override {
        static constexpr RequestAdapterEventData kShutdownData = {
            WGPURequestAdapterStatus_Unknown, "GPU connection lost", nullptr, nullptr, 0, nullptr};
        if (completionType == EventCompletionType::Shutdown && !mData) {
            mData = kShutdownData;
        }

        if (mData->status != WGPURequestAdapterStatus_Success && mAdapter != nullptr) {
            // If there was a failure, we need to free the adapter before calling the callback.
            mAdapter->GetClient()->Free(mAdapter);
            mAdapter = nullptr;
        } else {
            // Otherwise we set all the fields on the new adapter before calling the adapter.
            mAdapter->SetProperties(mData->properties);
            mAdapter->SetLimits(mData->limits);
            mAdapter->SetFeatures(mData->features, mData->featuresCount);
        }
        if (mCallback) {
            mCallback(mData->status, ToAPI(mAdapter), mData->message, mUserdata);
        }
    }

    Adapter* mAdapter = nullptr;
};

}  // anonymous namespace

// Free-standing API functions

WGPUBool ClientGetInstanceFeatures(WGPUInstanceFeatures* features) {
    if (features->nextInChain != nullptr) {
        return false;
    }

    features->timedWaitAnyEnable = false;
    features->timedWaitAnyMaxCount = kTimedWaitAnyMaxCountDefault;
    return true;
}

WGPUInstance ClientCreateInstance(WGPUInstanceDescriptor const* descriptor) {
    DAWN_UNREACHABLE();
    return nullptr;
}

// Instance

void Instance::RequestAdapter(const WGPURequestAdapterOptions* options,
                              WGPURequestAdapterCallback callback,
                              void* userdata) {
    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    callbackInfo.callback = callback;
    callbackInfo.userdata = userdata;
    RequestAdapterF(options, callbackInfo);
}

WGPUFuture Instance::RequestAdapterF(const WGPURequestAdapterOptions* options,
                                     const WGPURequestAdapterCallbackInfo& callbackInfo) {
    Client* client = GetClient();
    Adapter* adapter = client->Make<Adapter>();
    auto [futureIDInternal, tracked] =
        client->GetEventManager()->TrackEvent(new RequestAdapterEvent(callbackInfo, adapter));
    if (!tracked) {
        return {futureIDInternal};
    }

    InstanceRequestAdapterCmd cmd;
    cmd.instanceId = GetWireId();
    cmd.future = {futureIDInternal};
    cmd.adapterObjectHandle = adapter->GetWireHandle();
    cmd.options = options;

    client->SerializeCommand(cmd);
    return {futureIDInternal};
}

bool Client::DoInstanceRequestAdapterCallback(Instance* instance,
                                              WGPUFuture future,
                                              WGPURequestAdapterStatus status,
                                              const char* message,
                                              const WGPUAdapterProperties* properties,
                                              const WGPUSupportedLimits* limits,
                                              uint32_t featuresCount,
                                              const WGPUFeatureName* features) {
    // May have been deleted or recreated so this isn't an error.
    if (instance == nullptr) {
        return true;
    }
    return instance->OnRequestAdapterCallback(future, status, message, properties, limits,
                                              featuresCount, features);
}

bool Instance::OnRequestAdapterCallback(WGPUFuture future,
                                        WGPURequestAdapterStatus status,
                                        const char* message,
                                        const WGPUAdapterProperties* properties,
                                        const WGPUSupportedLimits* limits,
                                        uint32_t featuresCount,
                                        const WGPUFeatureName* features) {
    RequestAdapterEventData data = {status, message, properties, limits, featuresCount, features};
    return GetClient()->GetEventManager()->SetFutureReady<RequestAdapterEventBase>(
               future.id, std::move(data)) == WireResult::Success;
}

void Instance::ProcessEvents() {
    // TODO(crbug.com/dawn/2061): This should only process events for this Instance, not others
    // on the same client. When EventManager is moved to Instance, this can be fixed.
    GetClient()->GetEventManager()->ProcessPollEvents();

    // TODO(crbug.com/dawn/1987): The responsibility of ProcessEvents here is a bit mixed. It both
    // processes events coming in from the server, and also prompts the server to check for and
    // forward over new events - which won't be received until *after* this client-side
    // ProcessEvents completes.
    //
    // Fixing this nicely probably requires the server to more self-sufficiently
    // forward the events, which is half of making the wire fully invisible to use (which we might
    // like to do, someday, but not soon). This is easy for immediate events (like requestDevice)
    // and thread-driven events (async pipeline creation), but harder for queue fences where we have
    // to wait on the backend and then trigger Dawn code to forward the event.
    //
    // In the meantime, we could maybe do this on client->server flush to keep this concern in the
    // wire instead of in the API itself, but otherwise it's not significantly better so we just
    // keep it here for now for backward compatibility.
    InstanceProcessEventsCmd cmd;
    cmd.self = ToAPI(this);
    GetClient()->SerializeCommand(cmd);
}

WGPUWaitStatus Instance::WaitAny(size_t count, WGPUFutureWaitInfo* infos, uint64_t timeoutNS) {
    // In principle the EventManager should be on the Instance, not the Client.
    // But it's hard to get from an object to its Instance right now, so we can
    // store it on the Client.
    return GetClient()->GetEventManager()->WaitAny(count, infos, timeoutNS);
}

}  // namespace dawn::wire::client
