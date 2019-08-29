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

#include "common/Assert.h"

namespace dawn_wire { namespace server {

    // static
    void Server::ForwardUncapturedError(DawnErrorType type, const char* message, void* userdata) {
        auto* device = static_cast<ObjectData<DawnDevice>*>(userdata);
        ASSERT(device->server != nullptr);
        device->server->OnUncapturedError(device, type, message);
    }

    void Server::OnUncapturedError(ObjectData<DawnDevice>* device,
                                   DawnErrorType type,
                                   const char* message) {
        ObjectId deviceId = DeviceObjectIdTable().Get(device->handle);

        ReturnDeviceUncapturedErrorCallbackCmd cmd;
        cmd.device = ObjectHandle{deviceId, device->serial};
        cmd.type = type;
        cmd.message = message;

        size_t requiredSize = cmd.GetRequiredSize();
        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
        cmd.Serialize(allocatedBuffer);
    }

}}  // namespace dawn_wire::server
