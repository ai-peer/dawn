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

namespace dawn_wire { namespace server {

    bool Server::DoBufferMapAsync(ObjectId bufferId,
                                  uint32_t requestSerial,
                                  uint32_t start,
                                  uint32_t size,
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
        data->size = size;
        data->isWrite = isWrite;

        auto userdata = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(data));

        if (!buffer->valid) {
            // Fake the buffer returning a failure, data will be freed in this call.
            if (isWrite) {
                ForwardBufferMapWriteAsync(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, userdata);
            } else {
                ForwardBufferMapReadAsync(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, userdata);
            }
            return true;
        }

        if (isWrite) {
            mProcs.bufferMapWriteAsync(buffer->handle, start, size, ForwardBufferMapWriteAsync,
                                       userdata);
        } else {
            mProcs.bufferMapReadAsync(buffer->handle, start, size, ForwardBufferMapReadAsync,
                                      userdata);
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

    bool Server::DoQueueSignal(dawnQueue cSelf, dawnFence cFence, uint64_t signalValue) {
        if (cFence == nullptr) {
            return false;
        }

        mProcs.queueSignal(cSelf, cFence, signalValue);

        ObjectId fenceId = FenceObjectIdTable().Get(cFence);
        ASSERT(fenceId != 0);
        auto* fence = FenceObjects().Get(fenceId);
        ASSERT(fence != nullptr);

        auto* data = new FenceCompletionUserdata;
        data->server = this;
        data->fence = ObjectHandle{fenceId, fence->serial};
        data->value = signalValue;

        auto userdata = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(data));
        mProcs.fenceOnCompletion(cFence, signalValue, ForwardFenceCompletedValue, userdata);
        return true;
    }

}}  // namespace dawn_wire::server
