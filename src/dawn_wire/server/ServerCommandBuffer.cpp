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

#include "dawn_wire/server/Server.h"

#include <memory>

namespace dawn_wire { namespace server {

    bool Server::DoCommandBufferGetExecutionTime(ObjectId commandBufferId, uint64_t requestSerial) {
        auto* commandBuffer = CommandBufferObjects().Get(commandBufferId);
        if (commandBuffer == nullptr) {
            return false;
        }

        auto userdata = MakeUserdata<CommandBufferGetExecutionTimeUserdata>();
        userdata->commandBuffer = ObjectHandle{commandBufferId, commandBuffer->generation};
        userdata->requestSerial = requestSerial;

        mProcs.commandBufferGetExecutionTime(
            commandBuffer->handle,
            ForwardToServer<decltype(&Server::OnCommandBufferGetExecutionTime)>::Func<
                &Server::OnCommandBufferGetExecutionTime>(),
            userdata.release());
        return true;
    }

    void Server::OnCommandBufferGetExecutionTime(WGPUExecutionTimeRequestStatus status,
                                                 double time,
                                                 CommandBufferGetExecutionTimeUserdata* data) {
        ReturnCommandBufferGetExecutionTimeCallbackCmd cmd;
        cmd.commandBuffer = data->commandBuffer;
        cmd.requestSerial = data->requestSerial;
        cmd.status = status;
        cmd.time = time;

        SerializeCommand(cmd);
    }

}}  // namespace dawn_wire::server
