// Copyright 2019 The Dawn Authors
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

#ifndef DAWNWIRE_CLIENT_APIOBJECTS_H_
#define DAWNWIRE_CLIENT_APIOBJECTS_H_

#include <dawn/dawn.h>

#include "common/SerialMap.h"

#include <map>

namespace dawn_wire { namespace client {

    class Device;
    class ClientImpl;

    struct BuilderCallbackData {
        bool Call(dawnBuilderErrorStatus status, const char* message) {
            if (canCall && callback != nullptr) {
                canCall = true;
                callback(status, message, userdata1, userdata2);
                return true;
            }

            return false;
        }

        // For help with development, prints all builder errors by default.
        dawnBuilderErrorCallback callback = nullptr;
        dawnCallbackUserdata userdata1 = 0;
        dawnCallbackUserdata userdata2 = 0;
        bool canCall = true;
    };

    struct ObjectBase {
        ObjectBase(Device* device, uint32_t refcount, uint32_t id)
            : device(device), refcount(refcount), id(id) {
        }

        ClientImpl* GetClient();

        Device* device;
        uint32_t refcount;
        uint32_t id;

        BuilderCallbackData builderCallback;
    };

    class Device : public ObjectBase {
      public:
        Device(ClientImpl* client, uint32_t refcount, uint32_t id);

        ClientImpl* GetClient();
        void HandleError(const char* message);
        void SetErrorCallback(dawnDeviceErrorCallback errorCallback,
                              dawnCallbackUserdata errorUserdata);

      private:
        ClientImpl* mClient = nullptr;
        dawnDeviceErrorCallback mErrorCallback = nullptr;
        dawnCallbackUserdata mErrorUserdata;
    };

    struct Buffer : ObjectBase {
        using ObjectBase::ObjectBase;

        ~Buffer();
        void ClearMapRequests(dawnBufferMapAsyncStatus status);

        // We want to defer all the validation to the server, which means we could have multiple
        // map request in flight at a single time and need to track them separately.
        // On well-behaved applications, only one request should exist at a single time.
        struct MapRequestData {
            dawnBufferMapReadCallback readCallback = nullptr;
            dawnBufferMapWriteCallback writeCallback = nullptr;
            dawnCallbackUserdata userdata = 0;
            uint32_t size = 0;
            bool isWrite = false;
        };
        std::map<uint32_t, MapRequestData> requests;
        uint32_t requestSerial = 0;

        // Only one mapped pointer can be active at a time because Unmap clears all the in-flight
        // requests.
        void* mappedData = nullptr;
        size_t mappedDataSize = 0;
        bool isWriteMapped = false;
    };

    struct Fence : ObjectBase {
        using ObjectBase::ObjectBase;

        ~Fence();
        void CheckPassedFences();

        struct OnCompletionData {
            dawnFenceOnCompletionCallback completionCallback = nullptr;
            dawnCallbackUserdata userdata = 0;
        };
        uint64_t signaledValue = 0;
        uint64_t completedValue = 0;
        SerialMap<OnCompletionData> requests;
    };

}}  // namespace dawn_wire::client

#include "dawn_wire/client/ApiObjects_autogen.h"

#endif  // DAWNWIRE_CLIENT_APIOBJECTS_H_
