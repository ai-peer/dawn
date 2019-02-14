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

#include "common/Assert.h"
#include "dawn_wire/server/Server.h"

#include <memory>

namespace dawn_wire { namespace server {

    bool Server::PreHandleBufferUnmap(const BufferUnmapCmd& cmd) {
        auto* selfData = BufferObjects().Get(cmd.selfId);
        DAWN_ASSERT(selfData != nullptr);

        selfData->mappedData = nullptr;

        return true;
    }

    bool Server::DoBufferMapAsync(ObjectId bufferId,
                                  uint32_t requestSerial,
                                  bool isWrite) {
        // These requests are just forwarded to the buffer, with userdata containing what the
        // client will require in the return command.

        // The null object isn't valid as `self`
        if (bufferId == 0) {
            return false;
        }

        auto* buffer = BufferObjects().Get(bufferId);
        if (buffer == nullptr) {
            return false;
        }

        auto* data = new MapUserdata;
        data->server = this;
        data->buffer = ObjectHandle{bufferId, buffer->serial};
        data->requestSerial = requestSerial;
        data->isWrite = isWrite;

        auto userdata = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(data));

        if (!buffer->valid) {
            // Fake the buffer returning a failure, data will be freed in this call.
            if (isWrite) {
                ForwardBufferMapWriteAsync(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, 0,
                                           userdata);
            } else {
                ForwardBufferMapReadAsync(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, 0, userdata);
            }
            return true;
        }

        if (isWrite) {
            mProcs.bufferMapWriteAsync(buffer->handle, ForwardBufferMapWriteAsync, userdata);
        } else {
            mProcs.bufferMapReadAsync(buffer->handle, ForwardBufferMapReadAsync, userdata);
        }

        return true;
    }

    bool Server::DoBufferUpdateMappedData(ObjectId bufferId, uint32_t count, const uint8_t* data) {
        // The null object isn't valid as `self`
        if (bufferId == 0) {
            return false;
        }

        auto* buffer = BufferObjects().Get(bufferId);
        if (buffer == nullptr || !buffer->valid || buffer->mappedData == nullptr ||
            buffer->mappedDataSize != count) {
            return false;
        }

        if (data == nullptr) {
            return false;
        }

        memcpy(buffer->mappedData, data, count);

        return true;
    }

    bool Server::DoCreateBufferMappedAsync(ObjectId deviceId,
                                           const dawnBufferDescriptor* descriptor,
                                           ObjectHandle buffer) {
        // The null object isn't valid as `self`
        if (deviceId == 0) {
            return false;
        }

        auto* device = DeviceObjects().Get(deviceId);
        if (device == nullptr || !device->valid) {
            return false;
        }

        auto* bufferData = BufferObjects().Allocate(buffer.id);
        if (bufferData == nullptr) {
            return false;
        }
        bufferData->serial = buffer.serial;

        auto* data = new CreateBufferMappedAsyncUserdata;
        data->server = this;
        data->device = ObjectHandle{deviceId, device->serial};
        data->buffer = buffer;

        auto userdata = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(data));

        mProcs.deviceCreateBufferMappedAsync(device->handle, descriptor,
                                             ForwardCreateBufferMappedAsync, userdata);
        return true;
    }

    void Server::ForwardBufferMapReadAsync(dawnBufferMapAsyncStatus status,
                                           const void* ptr,
                                           uint32_t dataLength,
                                           dawnCallbackUserdata userdata) {
        auto data = reinterpret_cast<MapUserdata*>(static_cast<uintptr_t>(userdata));
        data->server->OnBufferMapReadAsyncCallback(status, ptr, dataLength, data);
    }

    void Server::ForwardBufferMapWriteAsync(dawnBufferMapAsyncStatus status,
                                            void* ptr,
                                            uint32_t dataLength,
                                            dawnCallbackUserdata userdata) {
        auto data = reinterpret_cast<MapUserdata*>(static_cast<uintptr_t>(userdata));
        data->server->OnBufferMapWriteAsyncCallback(status, ptr, dataLength, data);
    }

    void Server::ForwardCreateBufferMappedAsync(dawnBuffer buffer,
                                                dawnBufferMapAsyncStatus status,
                                                void* ptr,
                                                uint32_t dataLength,
                                                dawnCallbackUserdata userdata) {
        auto data =
            reinterpret_cast<CreateBufferMappedAsyncUserdata*>(static_cast<uintptr_t>(userdata));
        data->server->OnDeviceCreateBufferMappedAsyncCallback(buffer, status, ptr, dataLength,
                                                              data);
    }

    void Server::OnBufferMapReadAsyncCallback(dawnBufferMapAsyncStatus status,
                                              const void* ptr,
                                              uint32_t dataLength,
                                              MapUserdata* userdata) {
        std::unique_ptr<MapUserdata> data(userdata);

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
            cmd.dataLength = dataLength;
        }

        size_t requiredSize = cmd.GetRequiredSize();
        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
        cmd.Serialize(allocatedBuffer);
    }

    void Server::OnBufferMapWriteAsyncCallback(dawnBufferMapAsyncStatus status,
                                               void* ptr,
                                               uint32_t dataLength,
                                               MapUserdata* userdata) {
        std::unique_ptr<MapUserdata> data(userdata);

        // Skip sending the callback if the buffer has already been destroyed.
        auto* bufferData = BufferObjects().Get(data->buffer.id);
        if (bufferData == nullptr || bufferData->serial != data->buffer.serial) {
            return;
        }

        ReturnBufferMapWriteAsyncCallbackCmd cmd;
        cmd.buffer = data->buffer;
        cmd.requestSerial = data->requestSerial;
        cmd.status = status;
        cmd.dataLength = dataLength;

        size_t requiredSize = cmd.GetRequiredSize();
        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
        cmd.Serialize(allocatedBuffer);

        if (status == DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS) {
            bufferData->mappedData = ptr;
            bufferData->mappedDataSize = dataLength;
        }
    }

    void Server::OnDeviceCreateBufferMappedAsyncCallback(
        dawnBuffer buffer,
        dawnBufferMapAsyncStatus status,
        void* ptr,
        uint32_t dataLength,
        CreateBufferMappedAsyncUserdata* userdata) {
        std::unique_ptr<CreateBufferMappedAsyncUserdata> data(userdata);

        // Skip sending the callback if the device has already been destroyed.
        auto* deviceData = DeviceObjects().Get(data->device.id);
        if (deviceData == nullptr || deviceData->serial != data->device.serial) {
            return;
        }

        bool bufferExists = true;
        auto* bufferData = BufferObjects().Get(data->buffer.id);
        if (bufferData == nullptr || bufferData->serial != data->buffer.serial) {
            // This would happen if the buffer is destroyed which should never happen
            // because this is the callback in which the client receives the buffer.
            UNREACHABLE();
            bufferExists = false;
            status = DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR;
            dataLength = 0;
        }

        if (bufferExists) {
            bufferData->handle = buffer;
            bufferData->valid = buffer != nullptr;

            if (!bufferData->valid) {
                // The buffer could be invalid if there is a validation error
                status = DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR;
                bufferData->mappedData = nullptr;
                bufferData->mappedDataSize = 0;
            }

            if (status == DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS) {
                bufferData->mappedData = ptr;
                bufferData->mappedDataSize = dataLength;
            }
        }

        ReturnDeviceCreateBufferMappedAsyncCallbackCmd cmd;
        cmd.device = data->device;
        cmd.buffer = data->buffer;
        cmd.status = status;
        cmd.dataLength = dataLength;

        size_t requiredSize = cmd.GetRequiredSize();
        char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
        cmd.Serialize(allocatedBuffer);
    }

}}  // namespace dawn_wire::server
