// Copyright 2020 The Dawn Authors
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

#ifndef DAWNWIRE_CLIENT_ADAPTER_H_
#define DAWNWIRE_CLIENT_ADAPTER_H_

#include <dawn/webgpu.h>

#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/client/ObjectBase.h"

#include <map>

namespace dawn_wire { namespace client {

    class Client;
    class Adapter final : public ObjectBaseTmpl<Adapter, Client> {
      public:
        using ObjectBaseTmpl::ObjectBaseTmpl;

        ~Adapter();

        void RequestDevice(const WGPUDeviceDescriptor2* descriptor,
                           WGPURequestDeviceCallback callback,
                           void* userdata);

        bool OnRequestDeviceCallback(uint64_t requestSerial,
                                     WGPURequestDeviceStatus status,
                                     bool isNull);

      private:
        struct RequestDeviceRequest {
            ObjectId deviceId;
            WGPURequestDeviceCallback callback = nullptr;
            void* userdata = nullptr;
        };
        uint64_t mRequestDeviceSerial = 0;
        std::map<uint64_t, RequestDeviceRequest> mRequestDeviceRequests;
    };

}}  // namespace dawn_wire::client

#endif  // DAWNWIRE_CLIENT_ADAPTER_H_
