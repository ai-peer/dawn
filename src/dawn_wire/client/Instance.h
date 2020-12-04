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

#ifndef DAWNWIRE_CLIENT_INSTANCE_H_
#define DAWNWIRE_CLIENT_INSTANCE_H_

#include <dawn/webgpu.h>

#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/client/ObjectBase.h"

#include <map>

namespace dawn_wire { namespace client {

    class Client;
    class Instance final : public ObjectBaseTmpl<Instance, Client> {
      public:
        using ObjectBaseTmpl::ObjectBaseTmpl;

        ~Instance();

        void RequestAdapter(const WGPURequestAdapterOptions* options,
                            WGPURequestAdapterCallback callback,
                            void* userdata);

        bool OnRequestAdapterCallback(uint64_t requestSerial,
                                      WGPURequestAdapterStatus status,
                                      bool isNull);

      private:
        struct RequestAdapterRequest {
            ObjectId adapterId;
            WGPURequestAdapterCallback callback = nullptr;
            void* userdata = nullptr;
        };
        uint64_t mRequestAdapterSerial = 0;
        std::map<uint64_t, RequestAdapterRequest> mRequestAdapterRequests;
    };

}}  // namespace dawn_wire::client

#endif  // DAWNWIRE_CLIENT_INSTANCE_H_
