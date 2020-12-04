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

#include "common/Assert.h"
#include "dawn_wire/server/Server.h"

namespace dawn_wire { namespace server {

    namespace {
        struct InstanceRequestAdapterUserdata {
            Server* server;
            ObjectHandle instance;
            uint64_t requestSerial;
            ObjectId adapterId;
        };
    }  // namespace

    bool Server::DoInstanceRequestAdapter(ObjectId instanceId,
                                          uint64_t requestSerial,
                                          const WGPURequestAdapterOptions* options,
                                          ObjectHandle adapterHandle) {
        if (instanceId == 0) {
            return false;
        }

        auto* instanceData = InstanceObjects().Get(instanceId);
        if (instanceData == nullptr) {
            return false;
        }

        auto* resultData = AdapterObjects().Allocate(adapterHandle.id);
        if (resultData == nullptr) {
            return false;
        }
        resultData->generation = adapterHandle.generation;

        InstanceRequestAdapterUserdata* userdata = new InstanceRequestAdapterUserdata;
        userdata->server = this;
        userdata->instance = ObjectHandle{instanceId, instanceData->generation};
        userdata->requestSerial = requestSerial;
        userdata->adapterId = adapterHandle.id;

        mProcs.instanceRequestAdapter(
            instanceData->handle, options,
            [](WGPURequestAdapterStatus status, WGPUAdapter adapter, void* userdata) {
                auto data = std::unique_ptr<InstanceRequestAdapterUserdata>(
                    reinterpret_cast<InstanceRequestAdapterUserdata*>(userdata));

                auto* adapterObject = data->server->AdapterObjects().Get(data->adapterId);
                ASSERT(adapterObject != nullptr);

                adapterObject->handle = adapter;
                if (status != WGPURequestAdapterStatus_Success || adapter == nullptr) {
                    data->server->AdapterObjects().Free(data->adapterId);
                }

                ReturnInstanceRequestAdapterCallbackCmd cmd;
                cmd.instance = data->instance;
                cmd.requestSerial = data->requestSerial;
                cmd.status = status;
                cmd.isNull = adapter == nullptr;

                data->server->SerializeCommand(cmd);
            },
            userdata);
        return true;
    }

}}  // namespace dawn_wire::server
