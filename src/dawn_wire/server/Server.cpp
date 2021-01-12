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

#include "dawn_wire/server/Server.h"
#include "dawn_wire/WireServer.h"

namespace dawn_wire { namespace server {

    Server::Server(WGPUDevice device,
                   const DawnProcTable& procs,
                   CommandSerializer* serializer,
                   MemoryTransferService* memoryTransferService)
        : mSerializer(serializer),
          mProcs(procs),
          mDeviceOnCreation(device),
          mMemoryTransferService(memoryTransferService),
          mIsAlive(std::make_shared<bool>(true)) {
        if (mMemoryTransferService == nullptr) {
            // If a MemoryTransferService is not provided, fallback to inline memory.
            mOwnedMemoryTransferService = CreateInlineMemoryTransferService();
            mMemoryTransferService = mOwnedMemoryTransferService.get();
        }
        // The client-server knowledge is bootstrapped with device 1.
        auto* deviceData = DeviceObjects().Allocate(1);
        deviceData->handle = device;

        // Take an extra ref. All objects may be freed by the client, but this
        // one is externally owned.
        mProcs.deviceReference(device);

        // Note: these callbacks are manually inlined here since they do not acquire and
        // free their userdata.
        mProcs.deviceSetUncapturedErrorCallback(
            device,
            [](WGPUErrorType type, const char* message, void* userdata) {
                Server* server = static_cast<Server*>(userdata);
                server->OnUncapturedError(type, message);
            },
            this);
        mProcs.deviceSetDeviceLostCallback(
            device,
            [](const char* message, void* userdata) {
                Server* server = static_cast<Server*>(userdata);
                server->OnDeviceLost(message);
            },
            this);
    }

    Server::~Server() {
        // Un-set the error and lost callbacks since we cannot forward them
        // after the server has been destroyed.
        mProcs.deviceSetUncapturedErrorCallback(mDeviceOnCreation, nullptr, nullptr);
        mProcs.deviceSetDeviceLostCallback(mDeviceOnCreation, nullptr, nullptr);

        DestroyAllObjects(mProcs);
    }

    bool Server::InjectTexture(WGPUTexture texture,
                               uint32_t id,
                               uint32_t generation,
                               uint32_t deviceId,
                               uint32_t deviceGeneration) {
        ObjectData<WGPUDevice>* device = DeviceObjects().Get(deviceId);
        if (device == nullptr || device->generation != deviceGeneration) {
            return false;
        }

        ObjectData<WGPUTexture>* data = TextureObjects().Allocate(id);
        if (data == nullptr) {
            return false;
        }

        if (!TrackDeviceChild(device, ObjectType::Texture, id)) {
            return false;
        }

        data->handle = texture;
        data->generation = generation;
        data->allocated = true;
        data->device = device;

        // The texture is externally owned so it shouldn't be destroyed when we receive a destroy
        // message from the client. Add a reference to counterbalance the eventual release.
        mProcs.textureReference(texture);

        return true;
    }

    bool TrackDeviceChild(ObjectDataBase<WGPUDevice>* device, ObjectType type, ObjectId id) {
        auto it = static_cast<ObjectData<WGPUDevice>*>(device)->childObjectTypesAndIds.insert(
            PackObjectTypeAndId(type, id));
        if (!it.second) {
            // An object of this type and id already exists.
            return false;
        }
        return true;
    }

}}  // namespace dawn_wire::server
