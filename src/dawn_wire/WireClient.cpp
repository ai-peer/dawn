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

#include "dawn_wire/WireClient.h"

namespace dawn_wire {

    Client::Client(dawnProcTable* procs, dawnDevice* device, CommandSerializer* serializer)
        : mImpl(new Impl(procs, device, serializer)) {
    }

    Client::~Client() {
        mImpl.reset();
    }

    const char* Client::HandleCommands(const char* commands, size_t size) {
        return mImpl->HandleCommands(commands, size);
    }

    namespace client {

        WireClient::WireClient(dawnProcTable* procs,
                               dawnDevice* device,
                               CommandSerializer* serializer)
            : mDevice(DeviceAllocator().New()->object.get()), mSerializer(serializer) {
            *device = reinterpret_cast<dawnDeviceImpl*>(mDevice);
            *procs = GetProcs();
        }

        WireClient::~WireClient() {
            DeviceAllocator().Free(mDevice);
        }

        void WireClient::BufferMapReadAsync(dawnBuffer cBuffer,
                                            uint32_t start,
                                            uint32_t size,
                                            dawnBufferMapReadCallback callback,
                                            dawnCallbackUserdata userdata) {
            Buffer* buffer = reinterpret_cast<Buffer*>(cBuffer);

            uint32_t serial = buffer->requestSerial++;
            ASSERT(buffer->requests.find(serial) == buffer->requests.end());

            Buffer::MapRequestData request;
            request.readCallback = callback;
            request.userdata = userdata;
            request.size = size;
            request.isWrite = false;
            buffer->requests[serial] = request;

            BufferMapAsyncCmd cmd;
            cmd.bufferId = buffer->id;
            cmd.requestSerial = serial;
            cmd.start = start;
            cmd.size = size;
            cmd.isWrite = false;

            size_t requiredSize = cmd.GetRequiredSize();
            char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
            cmd.Serialize(allocatedBuffer);
        }

        void WireClient::BufferMapWriteAsync(dawnBuffer cBuffer,
                                             uint32_t start,
                                             uint32_t size,
                                             dawnBufferMapWriteCallback callback,
                                             dawnCallbackUserdata userdata) {
            Buffer* buffer = reinterpret_cast<Buffer*>(cBuffer);

            uint32_t serial = buffer->requestSerial++;
            ASSERT(buffer->requests.find(serial) == buffer->requests.end());

            Buffer::MapRequestData request;
            request.writeCallback = callback;
            request.userdata = userdata;
            request.size = size;
            request.isWrite = true;
            buffer->requests[serial] = request;

            BufferMapAsyncCmd cmd;
            cmd.bufferId = buffer->id;
            cmd.requestSerial = serial;
            cmd.start = start;
            cmd.size = size;
            cmd.isWrite = true;

            size_t requiredSize = cmd.GetRequiredSize();
            char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
            cmd.Serialize(allocatedBuffer);
        }

        uint64_t WireClient::FenceGetCompletedValue(dawnFence cSelf) {
            auto fence = reinterpret_cast<Fence*>(cSelf);
            return fence->completedValue;
        }

        void WireClient::FenceOnCompletion(dawnFence cFence,
                                           uint64_t value,
                                           dawnFenceOnCompletionCallback callback,
                                           dawnCallbackUserdata userdata) {
            Fence* fence = reinterpret_cast<Fence*>(cFence);
            if (value > fence->signaledValue) {
                mDevice->HandleError("Value greater than fence signaled value");
                callback(DAWN_FENCE_COMPLETION_STATUS_ERROR, userdata);
                return;
            }

            if (value <= fence->completedValue) {
                callback(DAWN_FENCE_COMPLETION_STATUS_SUCCESS, userdata);
                return;
            }

            Fence::OnCompletionData request;
            request.completionCallback = callback;
            request.userdata = userdata;
            fence->requests.Enqueue(std::move(request), value);
        }

        void WireClient::BufferUnmap(dawnBuffer cBuffer) {
            Buffer* buffer = reinterpret_cast<Buffer*>(cBuffer);

            // Invalidate the local pointer, and cancel all other in-flight requests that would
            // turn into errors anyway (you can't double map). This prevents race when the following
            // happens, where the application code would have unmapped a buffer but still receive a
            // callback:
            //   - Client -> Server: MapRequest1, Unmap, MapRequest2
            //   - Server -> Client: Result of MapRequest1
            //   - Unmap locally on the client
            //   - Server -> Client: Result of MapRequest2
            if (buffer->mappedData) {
                // If the buffer was mapped for writing, send the update to the data to the server
                if (buffer->isWriteMapped) {
                    BufferUpdateMappedDataCmd cmd;
                    cmd.bufferId = buffer->id;
                    cmd.count = static_cast<uint32_t>(buffer->mappedDataSize);
                    cmd.data = static_cast<const uint8_t*>(buffer->mappedData);

                    size_t requiredSize = cmd.GetRequiredSize();
                    char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
                    cmd.Serialize(allocatedBuffer);
                }

                free(buffer->mappedData);
                buffer->mappedData = nullptr;
            }
            buffer->ClearMapRequests(DAWN_BUFFER_MAP_ASYNC_STATUS_UNKNOWN);

            BufferUnmapCmd cmd;
            cmd.self = cBuffer;
            size_t requiredSize = cmd.GetRequiredSize();
            char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
            cmd.Serialize(allocatedBuffer, *this);
        }

        dawnFence WireClient::DeviceCreateFence(dawnDevice cSelf,
                                                dawnFenceDescriptor const* descriptor) {
            DeviceCreateFenceCmd cmd;
            cmd.self = cSelf;
            auto* allocation = FenceAllocator().New();
            cmd.result = allocation->GetHandle();
            cmd.descriptor = descriptor;

            size_t requiredSize = cmd.GetRequiredSize();
            char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
            cmd.Serialize(allocatedBuffer, *this);

            dawnFence cFence = reinterpret_cast<dawnFence>(allocation->object.get());

            Fence* fence = reinterpret_cast<Fence*>(cFence);
            fence->signaledValue = descriptor->initialValue;
            fence->completedValue = descriptor->initialValue;
            return cFence;
        }

        void WireClient::QueueSignal(dawnQueue cQueue, dawnFence cFence, uint64_t signalValue) {
            Fence* fence = reinterpret_cast<Fence*>(cFence);
            if (signalValue <= fence->signaledValue) {
                mDevice->HandleError("Fence value less than or equal to signaled value");
                return;
            }
            fence->signaledValue = signalValue;

            QueueSignalCmd cmd;
            cmd.self = cQueue;
            cmd.fence = cFence;
            cmd.signalValue = signalValue;

            size_t requiredSize = cmd.GetRequiredSize();
            char* allocatedBuffer = static_cast<char*>(GetCmdSpace(requiredSize));
            cmd.Serialize(allocatedBuffer, *this);
        }

        void WireClient::DeviceReference(dawnDevice) {
        }

        void WireClient::DeviceRelease(dawnDevice) {
        }

        void WireClient::DeviceSetErrorCallback(dawnDevice cSelf,
                                                dawnDeviceErrorCallback callback,
                                                dawnCallbackUserdata userdata) {
            Device* self = reinterpret_cast<Device*>(cSelf);
            self->errorCallback = callback;
            self->errorUserdata = userdata;
        }
    }  // namespace client
}  // namespace dawn_wire
