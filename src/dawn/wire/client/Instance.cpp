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

#include <optional>
#include <utility>

#include "dawn/common/FutureUtils.h"
#include "dawn/wire/client/Client.h"
#include "dawn/wire/client/Device.h"

namespace dawn::wire::client {

// Instance

Instance::~Instance() {
    mRequestAdapterRequests.CloseAll([](RequestAdapterData* request) {
        request->callback(WGPURequestAdapterStatus_Unknown, nullptr,
                          "Instance destroyed before callback", request->userdata);
    });
}

void Instance::CancelCallbacksForDisconnect() {
    mRequestAdapterRequests.CloseAll([](RequestAdapterData* request) {
        request->callback(WGPURequestAdapterStatus_Unknown, nullptr, "GPU connection lost",
                          request->userdata);
    });
}

void Instance::RequestAdapter(const WGPURequestAdapterOptions* options,
                              WGPURequestAdapterCallback callback,
                              void* userdata) {
    Client* client = GetClient();
    if (client->IsDisconnected()) {
        callback(WGPURequestAdapterStatus_Error, nullptr, "GPU connection lost", userdata);
        return;
    }

    Adapter* adapter = client->Make<Adapter>();
    uint64_t serial = mRequestAdapterRequests.Add({callback, adapter->GetWireId(), userdata});

    InstanceRequestAdapterCmd cmd;
    cmd.instanceId = GetWireId();
    cmd.requestSerial = serial;
    cmd.adapterObjectHandle = adapter->GetWireHandle();
    cmd.options = options;

    client->SerializeCommand(cmd);
}

bool Client::DoInstanceRequestAdapterCallback(Instance* instance,
                                              uint64_t requestSerial,
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
    return instance->OnRequestAdapterCallback(requestSerial, status, message, properties, limits,
                                              featuresCount, features);
}

bool Instance::OnRequestAdapterCallback(uint64_t requestSerial,
                                        WGPURequestAdapterStatus status,
                                        const char* message,
                                        const WGPUAdapterProperties* properties,
                                        const WGPUSupportedLimits* limits,
                                        uint32_t featuresCount,
                                        const WGPUFeatureName* features) {
    RequestAdapterData request;
    if (!mRequestAdapterRequests.Acquire(requestSerial, &request)) {
        return false;
    }

    Client* client = GetClient();
    Adapter* adapter = client->Get<Adapter>(request.adapterObjectId);

    // If the return status is a failure we should give a null adapter to the callback and
    // free the allocation.
    if (status != WGPURequestAdapterStatus_Success) {
        client->Free(adapter);
        request.callback(status, nullptr, message, request.userdata);
        return true;
    }

    adapter->SetProperties(properties);
    adapter->SetLimits(limits);
    adapter->SetFeatures(features, featuresCount);

    request.callback(status, ToAPI(adapter), message, request.userdata);
    return true;
}

FutureID Instance::TrackEvent(WGPUCallbackMode mode,
                              Device* deviceForQueueEventSource,
                              std::function<void()>&& callback) {
    std::lock_guard lock(mTrackedEventsMutex);

    FutureID futureID = mNextFutureID++;
    mTrackedEvents.emplace(futureID,
                           TrackedEvent(mode, deviceForQueueEventSource, std::move(callback)));

    return (mode & WGPUCallbackMode_Future) ? futureID : 0;
}

WGPUWaitStatus Instance::WaitAny(size_t count, WGPUFutureWaitInfo* infos, uint64_t timeoutNS) {
    if (count == 0) {
        return WGPUWaitStatus_Success;
    }

    std::lock_guard lock(mTrackedEventsMutex);

    // Wire can always handle mixed event sources, but we still have to validate it.
    Device* deviceForQueueEventSource = nullptr;
    bool foundMixedSources = false;

    bool anyCompleted = false;
    for (size_t i = 0; i < count; ++i) {
        auto it = mTrackedEvents.find(infos[i].future.id);
        if (it == mTrackedEvents.end()) {
            infos[i].completed = true;
            anyCompleted = true;
        } else {
            TrackedEvent& event = it->second;
            ASSERT(event.mMode & WGPUCallbackMode_Future);
            infos[i].completed = false;

            if (deviceForQueueEventSource) {
                if (event.mDeviceForQueueEventSource != deviceForQueueEventSource) {
                    foundMixedSources = true;
                }
            } else {
                deviceForQueueEventSource = event.mDeviceForQueueEventSource.Get();
            }
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
    if (foundMixedSources) {
        return WGPUWaitStatus_UnsupportedMixedSources;
    }

    for (size_t i = 0; i < count; ++i) {
        FutureID futureID = infos[i].future.id;
        // We know here that the future must be tracked, since anyCompleted is false.
        TrackedEvent& event = mTrackedEvents[futureID];

        // TODO(crbug.com/dawn/1987): Guarantee the event ordering from the JS spec.
        bool ready = event.CheckReadyAndCompleteIfNeeded();
        if (ready) {
            infos[i].completed = true;
            mTrackedEvents.erase(futureID);
        }
    }

    return WGPUWaitStatus_Success;
}

// Instance::TrackedEvent

Instance::TrackedEvent::TrackedEvent(WGPUCallbackModeFlags mode,
                                     Device* deviceForQueueEventSource,
                                     std::function<void()>&& callback)
    : mMode(mode), mDeviceForQueueEventSource(deviceForQueueEventSource), mCallback(callback) {}

void Instance::TrackedEvent::SetReady() {
    mReady = true;
    if ((mMode & WGPUCallbackMode_Spontaneous) && mCallback) {
        mCallback();
        mCallback = {};
    }
}

bool Instance::TrackedEvent::CheckReadyAndCompleteIfNeeded() {
    if (mReady && mCallback) {
        mCallback();
        mCallback = {};
    }
    return mReady;
}

}  // namespace dawn::wire::client
