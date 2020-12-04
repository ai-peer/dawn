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

#include "dawn_wire/client/Adapter.h"

#include "dawn_wire/client/Client.h"

namespace dawn_wire { namespace client {

    Adapter::~Adapter() {
        for (auto& it : mRequestDeviceRequests) {
            if (it.second.callback) {
                it.second.callback(WGPURequestDeviceStatus_Unknown, nullptr, it.second.userdata);
            }
        }
        mRequestDeviceRequests.clear();
    }

    void Adapter::RequestDevice(const WGPUDeviceDescriptor2* descriptor,
                                WGPURequestDeviceCallback callback,
                                void* userdata) {
        if (GetClient()->IsDisconnected()) {
            return callback(WGPURequestDeviceStatus_Error, nullptr, userdata);
        }

        uint64_t requestSerial = mRequestDeviceSerial++;
        ASSERT(mRequestDeviceRequests.find(requestSerial) == mRequestDeviceRequests.end());

        auto* allocation = GetClient()->DeviceAllocator().New(GetClient());

        AdapterRequestDeviceCmd cmd;
        cmd.adapterId = this->id;
        cmd.requestSerial = requestSerial;
        cmd.descriptor = descriptor;
        cmd.deviceHandle = ObjectHandle{allocation->object->id, allocation->generation};

        mRequestDeviceRequests[requestSerial] = {allocation->object->id, callback, userdata};
        GetClient()->SerializeCommand(cmd);
    }

    bool Adapter::OnRequestDeviceCallback(uint64_t requestSerial,
                                          WGPURequestDeviceStatus status,
                                          bool isNull) {
        auto requestIt = mRequestDeviceRequests.find(requestSerial);
        if (requestIt == mRequestDeviceRequests.end()) {
            return false;
        }

        RequestDeviceRequest request = std::move(requestIt->second);
        mRequestDeviceRequests.erase(requestIt);

        Device* device = GetClient()->DeviceAllocator().GetObject(request.deviceId);

        if (status != WGPURequestDeviceStatus_Success || isNull) {
            GetClient()->DeviceAllocator().Free(device);
            request.callback(status, nullptr, request.userdata);
            return true;
        }

        request.callback(status, reinterpret_cast<WGPUDevice>(device), request.userdata);
        return true;
    }

}}  // namespace dawn_wire::client
