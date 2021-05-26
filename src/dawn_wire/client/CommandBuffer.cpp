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

#include "dawn_wire/client/CommandBuffer.h"

#include "dawn_wire/client/Client.h"

namespace dawn_wire { namespace client {

    CommandBuffer::~CommandBuffer() {
        // Callbacks need to be fired in all cases, as they can handle freeing resources. So we call
        // them with "Unknown" status.
        for (auto& it : mExecutionTimeRequests) {
            if (it.second.callback) {
                it.second.callback(WGPUExecutionTimeRequestStatus_Unknown, 0.0, it.second.userdata);
            }
        }
        mExecutionTimeRequests.clear();
    }

    void CommandBuffer::GetExecutionTime(WGPUExecutionTimeCallback callback, void* userdata) {
        if (client->IsDisconnected()) {
            callback(WGPUExecutionTimeRequestStatus_DeviceLost, 0.0, userdata);
            return;
        }

        uint64_t serial = mExecutionTimeRequestSerial++;
        CommandBufferGetExecutionTimeCmd cmd;
        cmd.commandBufferId = this->id;
        cmd.requestSerial = serial;

        mExecutionTimeRequests[serial] = {callback, userdata};

        client->SerializeCommand(cmd);
    }

    bool CommandBuffer::GetExecutionTimeCallback(uint64_t requestSerial,
                                                 WGPUExecutionTimeRequestStatus status,
                                                 double time) {
        auto requestIt = mExecutionTimeRequests.find(requestSerial);
        if (requestIt == mExecutionTimeRequests.end()) {
            return false;
        }

        // Remove the request data so that the callback cannot be called again.
        // ex.) inside the callback: if the shader module is deleted, all callbacks reject.
        ExecutionTimeRequest request = std::move(requestIt->second);
        mExecutionTimeRequests.erase(requestIt);

        request.callback(status, time, request.userdata);
        return true;
    }

}}  // namespace dawn_wire::client
