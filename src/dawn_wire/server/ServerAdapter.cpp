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
        struct AdapterRequestDeviceUserdata {
            Server* server;
            ObjectHandle adapter;
            uint64_t requestSerial;
            ObjectId deviceId;
        };
    }  // namespace

    bool Server::DoAdapterRequestDevice(ObjectId adapterId,
                                        uint64_t requestSerial,
                                        const WGPUDeviceDescriptor* descriptor,
                                        ObjectHandle deviceHandle) {
        if (adapterId == 0) {
            return false;
        }

        auto* adapterData = AdapterObjects().Get(adapterId);
        if (adapterData == nullptr) {
            return false;
        }

        auto* resultData = DeviceObjects().Allocate(deviceHandle.id);
        if (resultData == nullptr) {
            return false;
        }
        resultData->generation = deviceHandle.generation;

        AdapterRequestDeviceUserdata* userdata = new AdapterRequestDeviceUserdata;
        userdata->server = this;
        userdata->adapter = ObjectHandle{adapterId, adapterData->generation};
        userdata->requestSerial = requestSerial;
        userdata->deviceId = deviceHandle.id;

        mProcs.adapterRequestDevice(
            adapterData->handle, descriptor,
            [](WGPURequestDeviceStatus status, WGPUDevice device, void* userdata) {
                auto data = std::unique_ptr<AdapterRequestDeviceUserdata>(
                    reinterpret_cast<AdapterRequestDeviceUserdata*>(userdata));

                auto* deviceData = data->server->DeviceObjects().Get(data->deviceId);
                ASSERT(deviceData != nullptr);

                deviceData->handle = device;
                if (status != WGPURequestDeviceStatus_Success || device == nullptr) {
                    data->server->DeviceObjects().Free(data->deviceId);
                }

                if (device != nullptr) {
                    deviceData->id = data->deviceId;
                    deviceData->server = data->server;

                    data->server->mProcs.deviceSetUncapturedErrorCallback(
                        device, Server::ForwardUncapturedError, deviceData);
                    data->server->mProcs.deviceSetDeviceLostCallback(
                        device, Server::ForwardDeviceLost, deviceData);
                }

                ReturnAdapterRequestDeviceCallbackCmd cmd;
                cmd.adapter = data->adapter;
                cmd.requestSerial = data->requestSerial;
                cmd.status = status;
                cmd.isNull = device == nullptr;

                data->server->SerializeCommand(cmd);
            },
            userdata);
        return true;
    }

}}  // namespace dawn_wire::server
