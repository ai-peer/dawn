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

#include "dawn_wire/client/Instance.h"

#include "dawn_wire/client/Client.h"

namespace dawn_wire { namespace client {

    Instance::~Instance() {
        for (auto& it : mRequestAdapterRequests) {
            if (it.second.callback) {
                it.second.callback(WGPURequestAdapterStatus_Unknown, nullptr, it.second.userdata);
            }
        }
        mRequestAdapterRequests.clear();
    }

    void Instance::RequestAdapter(const WGPURequestAdapterOptions* options,
                                  WGPURequestAdapterCallback callback,
                                  void* userdata) {
        if (GetClient()->IsDisconnected()) {
            return callback(WGPURequestAdapterStatus_Error, nullptr, userdata);
        }

        uint64_t requestSerial = mRequestAdapterSerial++;
        ASSERT(mRequestAdapterRequests.find(requestSerial) == mRequestAdapterRequests.end());

        auto* allocation = GetClient()->AdapterAllocator().New(GetClient());

        InstanceRequestAdapterCmd cmd;
        cmd.instanceId = this->id;
        cmd.requestSerial = requestSerial;
        cmd.options = options;
        cmd.adapterHandle = ObjectHandle{allocation->object->id, allocation->generation};

        mRequestAdapterRequests[requestSerial] = {allocation->object->id, callback, userdata};
        GetClient()->SerializeCommand(cmd);
    }

    bool Instance::OnRequestAdapterCallback(uint64_t requestSerial,
                                            WGPURequestAdapterStatus status,
                                            bool isNull) {
        auto requestIt = mRequestAdapterRequests.find(requestSerial);
        if (requestIt == mRequestAdapterRequests.end()) {
            return false;
        }

        RequestAdapterRequest request = std::move(requestIt->second);
        mRequestAdapterRequests.erase(requestIt);

        Adapter* adapter = GetClient()->AdapterAllocator().GetObject(request.adapterId);

        if (status != WGPURequestAdapterStatus_Success || isNull) {
            GetClient()->AdapterAllocator().Free(adapter);
            request.callback(status, nullptr, request.userdata);
            return true;
        }

        request.callback(status, reinterpret_cast<WGPUAdapter>(adapter), request.userdata);
        return true;
    }

}}  // namespace dawn_wire::client
