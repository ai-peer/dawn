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

#include "dawn_wire/server/WireServer.h"

namespace dawn_wire { namespace server {

    void WireServer::ForwardDeviceErrorToClient(const char* message,
                                                dawnCallbackUserdata userdata) {
        auto server = reinterpret_cast<WireServer*>(static_cast<intptr_t>(userdata));
        server->OnDeviceError(message);
    }

    void WireServer::ForwardBufferMapReadAsync(dawnBufferMapAsyncStatus status,
                                               const void* ptr,
                                               dawnCallbackUserdata userdata) {
        auto data = reinterpret_cast<MapUserdata*>(static_cast<uintptr_t>(userdata));
        data->server->OnMapReadAsyncCallback(status, ptr, data);
    }

    void WireServer::ForwardBufferMapWriteAsync(dawnBufferMapAsyncStatus status,
                                                void* ptr,
                                                dawnCallbackUserdata userdata) {
        auto data = reinterpret_cast<MapUserdata*>(static_cast<uintptr_t>(userdata));
        data->server->OnMapWriteAsyncCallback(status, ptr, data);
    }

    void WireServer::ForwardFenceCompletedValue(dawnFenceCompletionStatus status,
                                                dawnCallbackUserdata userdata) {
        auto data = reinterpret_cast<FenceCompletionUserdata*>(static_cast<uintptr_t>(userdata));
        if (status == DAWN_FENCE_COMPLETION_STATUS_SUCCESS) {
            data->server->OnFenceCompletedValueUpdated(data);
        }
    }

    void WireServer::OnDeviceError(const char* message) {
        ReturnDeviceErrorCallbackCmd cmd;
        cmd.message = message;

        size_t requiredSize = cmd.GetRequiredSize();
        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
        cmd.Serialize(allocatedBuffer);
    }

    void WireServer::OnMapReadAsyncCallback(dawnBufferMapAsyncStatus status,
                                            const void* ptr,
                                            MapUserdata* data) {
        // Skip sending the callback if the buffer has already been destroyed.
        auto* bufferData = BufferObjects().Get(data->buffer.id);
        if (bufferData == nullptr || bufferData->serial != data->buffer.serial) {
            return;
        }

        ReturnBufferMapReadAsyncCallbackCmd cmd;
        cmd.buffer = data->buffer;
        cmd.requestSerial = data->requestSerial;
        cmd.status = status;
        cmd.dataLength = 0;
        cmd.data = reinterpret_cast<const uint8_t*>(ptr);

        if (status == DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS) {
            cmd.dataLength = data->size;
        }

        size_t requiredSize = cmd.GetRequiredSize();
        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
        cmd.Serialize(allocatedBuffer);

        delete data;
    }

    void WireServer::OnMapWriteAsyncCallback(dawnBufferMapAsyncStatus status,
                                             void* ptr,
                                             MapUserdata* data) {
        // Skip sending the callback if the buffer has already been destroyed.
        auto* bufferData = BufferObjects().Get(data->buffer.id);
        if (bufferData == nullptr || bufferData->serial != data->buffer.serial) {
            return;
        }

        ReturnBufferMapWriteAsyncCallbackCmd cmd;
        cmd.buffer = data->buffer;
        cmd.requestSerial = data->requestSerial;
        cmd.status = status;

        size_t requiredSize = cmd.GetRequiredSize();
        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
        cmd.Serialize(allocatedBuffer);

        if (status == DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS) {
            auto* selfData = BufferObjects().Get(data->buffer.id);
            ASSERT(selfData != nullptr);

            selfData->mappedData = ptr;
            selfData->mappedDataSize = data->size;
        }

        delete data;
    }

    void WireServer::OnFenceCompletedValueUpdated(FenceCompletionUserdata* data) {
        ReturnFenceUpdateCompletedValueCmd cmd;
        cmd.fence = data->fence;
        cmd.value = data->value;

        size_t requiredSize = cmd.GetRequiredSize();
        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
        cmd.Serialize(allocatedBuffer);

        delete data;
    }
}}  // namespace dawn_wire::server
