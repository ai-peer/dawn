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

#include "dawn_wire/client/Buffer.h"

#include "dawn_wire/BufferConsumer_impl.h"
#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/client/Client.h"
#include "dawn_wire/client/Device.h"

namespace dawn_wire { namespace client {

    // static
    WGPUBuffer Buffer::Create(Device* device, const WGPUBufferDescriptor* descriptor) {
        Client* wireClient = device->client;

        bool mappable =
            (descriptor->usage & (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite)) != 0 ||
            descriptor->mappedAtCreation;
        if (mappable && descriptor->size >= std::numeric_limits<size_t>::max()) {
            device->InjectError(WGPUErrorType_OutOfMemory, "Buffer is too large for map usage");
            return device->CreateErrorBuffer();
        }

        std::unique_ptr<MemoryTransferService::ReadHandle> readHandle = nullptr;
        std::unique_ptr<MemoryTransferService::WriteHandle> writeHandle = nullptr;
        void* writeData = nullptr;

        DeviceCreateBufferCmd cmd;
        cmd.deviceId = device->id;
        cmd.descriptor = descriptor;
        cmd.readHandleCreateInfoLength = 0;
        cmd.readHandleCreateInfo = nullptr;
        cmd.writeHandleCreateInfoLength = 0;
        cmd.writeHandleCreateInfo = nullptr;

        // Params later assign to buffer
        bool isCurrentlyMappedForWriting = false;  // set to true for mappedAtCreation
        bool bufferSetDestructWriteHandleOnUnmap =
            false;  // set to true for mappedAtCreation and no MapWrite usage

        if (mappable) {
            if ((descriptor->usage & WGPUBufferUsage_MapRead) != 0) {
                // Create the read handle on buffer creation.
                readHandle.reset(
                    wireClient->GetMemoryTransferService()->CreateReadHandle(descriptor->size));
                if (readHandle == nullptr) {
                    if (!device->GetAliveWeakPtr().expired()) {
                        device->InjectError(WGPUErrorType_OutOfMemory,
                                            "Failed to create buffer mapping");
                        return device->CreateErrorBuffer();
                    }
                }

                cmd.readHandleCreateInfoLength = readHandle->SerializeCreateSize();
            }

            if ((descriptor->usage & WGPUBufferUsage_MapWrite) != 0 ||
                descriptor->mappedAtCreation) {
                // Create the write handle on buffer creation.
                writeHandle.reset(
                    wireClient->GetMemoryTransferService()->CreateWriteHandle(descriptor->size));
                if (writeHandle == nullptr) {
                    if (!device->GetAliveWeakPtr().expired()) {
                        device->InjectError(WGPUErrorType_OutOfMemory,
                                            "Failed to create buffer mapping");
                        return device->CreateErrorBuffer();
                    }
                }

                // If the buffer is mapped at creation, a write handle is created and will be
                // destructed on unmap if the buffer doesn't have MapWrite usage
                if (descriptor->mappedAtCreation) {
                    writeData = writeHandle->GetData();
                    // If allocation failed the writeHandle would be nullptr and rejected earlier
                    ASSERT(writeData != nullptr);
                    isCurrentlyMappedForWriting = true;
                }

                // This flag is for write handle created by mappedAtCreation
                // instead of MapWrite usage. We don't have such a case for read handle
                bufferSetDestructWriteHandleOnUnmap =
                    descriptor->mappedAtCreation &&
                    (descriptor->usage & WGPUBufferUsage_MapWrite) == 0;

                // Get the serialization size of the write handle.
                cmd.writeHandleCreateInfoLength = writeHandle->SerializeCreateSize();
            }
        }

        // Create the buffer and send the creation command.
        // This must happen after any potential device->CreateErrorBuffer()
        // as server expects allocating ids to be monotonically increasing
        auto* bufferObjectAndSerial = wireClient->BufferAllocator().New(wireClient);
        Buffer* buffer = bufferObjectAndSerial->object.get();
        buffer->mDevice = device;
        buffer->mDeviceIsAlive = device->GetAliveWeakPtr();
        buffer->mSize = descriptor->size;
        buffer->mIsMappingWrite = isCurrentlyMappedForWriting;
        buffer->mDestructWriteHandleOnUnmap = bufferSetDestructWriteHandleOnUnmap;

        cmd.result = ObjectHandle{buffer->id, bufferObjectAndSerial->generation};

        wireClient->SerializeCommand(
            cmd, cmd.readHandleCreateInfoLength + cmd.writeHandleCreateInfoLength,
            [&](SerializeBuffer* serializeBuffer) {
                if (readHandle != nullptr) {
                    char* readHandleBuffer;
                    WIRE_TRY(
                        serializeBuffer->NextN(cmd.readHandleCreateInfoLength, &readHandleBuffer));
                    // Serialize the ReadHandle into the space after the command.
                    readHandle->SerializeCreate(readHandleBuffer);
                    buffer->mReadHandle = std::move(readHandle);
                    buffer->mMappedData = nullptr;
                    buffer->mMapSize = buffer->mSize;
                }
                if (writeHandle != nullptr) {
                    char* writeHandleBuffer;
                    WIRE_TRY(serializeBuffer->NextN(cmd.writeHandleCreateInfoLength,
                                                    &writeHandleBuffer));
                    // Serialize the WriteHandle into the space after the command.
                    writeHandle->SerializeCreate(writeHandleBuffer);
                    buffer->mWriteHandle = std::move(writeHandle);
                    buffer->mMappedData = writeData;
                    buffer->mMapSize = buffer->mSize;
                }
                buffer->mMapOffset = 0;

                return WireResult::Success;
            });
        return ToAPI(buffer);
    }

    // static
    WGPUBuffer Buffer::CreateError(Device* device) {
        auto* allocation = device->client->BufferAllocator().New(device->client);
        allocation->object->mDevice = device;
        allocation->object->mDeviceIsAlive = device->GetAliveWeakPtr();

        DeviceCreateErrorBufferCmd cmd;
        cmd.self = ToAPI(device);
        cmd.result = ObjectHandle{allocation->object->id, allocation->generation};
        device->client->SerializeCommand(cmd);

        return ToAPI(allocation->object.get());
    }

    Buffer::~Buffer() {
        // Callbacks need to be fired in all cases, as they can handle freeing resources
        // so we call them with "DestroyedBeforeCallback" status.
        for (auto& it : mRequests) {
            if (it.second.callback) {
                it.second.callback(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback, it.second.userdata);
            }
        }
        mRequests.clear();

        FreeMappedData(true);
    }

    void Buffer::CancelCallbacksForDisconnect() {
        for (auto& it : mRequests) {
            if (it.second.callback) {
                it.second.callback(WGPUBufferMapAsyncStatus_DeviceLost, it.second.userdata);
            }
        }
        mRequests.clear();
    }

    void Buffer::MapAsync(WGPUMapModeFlags mode,
                          size_t offset,
                          size_t size,
                          WGPUBufferMapCallback callback,
                          void* userdata) {
        if (client->IsDisconnected()) {
            return callback(WGPUBufferMapAsyncStatus_DeviceLost, userdata);
        }

        // Handle the defaulting of size required by WebGPU.
        if (size == 0 && offset < mSize) {
            size = mSize - offset;
        }

        wgpu::MapMode type = static_cast<wgpu::MapMode>(mode);

        // Step 1. Do early validation of Read/WriteHandle allocation failure due to OOM
        if (type == wgpu::MapMode::None ||
            (type == wgpu::MapMode::Read && mReadHandle == nullptr) ||
            (type == wgpu::MapMode::Write && mWriteHandle == nullptr)) {
            if (!mDeviceIsAlive.expired()) {
                mDevice->InjectError(WGPUErrorType_Validation, "Buffer failed for map usage");
            }
            if (callback != nullptr) {
                callback(WGPUBufferMapAsyncStatus_Error, userdata);
            }
            return;
        }

        // Step 2. Create the request structure that will hold information while this mapping is
        // in flight.
        uint32_t serial = mRequestSerial++;
        ASSERT(mRequests.find(serial) == mRequests.end());

        Buffer::MapRequestData request = {};
        request.callback = callback;
        request.userdata = userdata;
        request.size = size;
        request.offset = offset;

        request.type = type;

        // Step 3. Serialize the command to send to the server.
        BufferMapAsyncCmd cmd;
        cmd.bufferId = this->id;
        cmd.requestSerial = serial;
        cmd.mode = mode;
        cmd.offset = offset;
        cmd.size = size;

        client->SerializeCommand(cmd);

        // Step 4. Register this request so that we can retrieve it from its serial when the server
        // sends the callback.
        mRequests[serial] = std::move(request);
    }

    bool Buffer::OnMapAsyncCallback(uint32_t requestSerial,
                                    uint32_t status,
                                    uint64_t readDataUpdateInfoLength,
                                    const uint8_t* readDataUpdateInfo) {
        auto requestIt = mRequests.find(requestSerial);
        if (requestIt == mRequests.end()) {
            return false;
        }

        auto request = std::move(requestIt->second);
        // Delete the request before calling the callback otherwise the callback could be fired a
        // second time. If, for example, buffer.Unmap() is called inside the callback.
        mRequests.erase(requestIt);

        auto FailRequest = [&request]() -> bool {
            if (request.callback != nullptr) {
                request.callback(WGPUBufferMapAsyncStatus_DeviceLost, request.userdata);
            }
            return false;
        };


        // Take into account the client-side status of the request if the server says it is a success.
        if (status == WGPUBufferMapAsyncStatus_Success) {
            status = request.clientStatus;
        }

        const void* mappedData = nullptr;
        if (status == WGPUBufferMapAsyncStatus_Success) {
            if (request.type == wgpu::MapMode::Read) {
                if (readDataUpdateInfoLength > std::numeric_limits<size_t>::max()) {
                    // This is the size of data deserialized from the command stream, which must be
                    // CPU-addressable.
                    return FailRequest();
                }

                // Update user map data with server returned data
                ASSERT(mReadHandle != nullptr);
                if (!mReadHandle->DeserializeDataUpdate(
                        readDataUpdateInfo, static_cast<size_t>(readDataUpdateInfoLength),
                        request.offset, request.size)) {
                    return FailRequest();
                }
                mappedData = mReadHandle->GetData();
                ASSERT(mappedData != nullptr);
            } else {
                // Call GetData of WriteHandle. This returns the base address pointer of the buffer
                // On failure, |mappedData| may be null.
                ASSERT(request.type == wgpu::MapMode::Write);
                ASSERT(mWriteHandle != nullptr);
                mappedData = mWriteHandle->GetData();
                ASSERT(mappedData != nullptr);
            }

            // The MapAsync request was successful. Set the mapping access token to prevent other
            // map operation at the same time.
            mIsMappingRead = request.type == wgpu::MapMode::Read;
            mIsMappingWrite = request.type == wgpu::MapMode::Write;
        }

        mMapOffset = request.offset;
        mMapSize = request.size;
        mMappedData = const_cast<void*>(mappedData);
        if (request.callback) {
            request.callback(static_cast<WGPUBufferMapAsyncStatus>(status), request.userdata);
        }

        return true;
    }

    void* Buffer::GetMappedRange(size_t offset, size_t size) {
        if (!IsMappedForWriting() || !CheckGetMappedRangeOffsetSize(offset, size)) {
            return nullptr;
        }
        return static_cast<uint8_t*>(mMappedData) + offset;
    }

    const void* Buffer::GetConstMappedRange(size_t offset, size_t size) {
        if (!(IsMappedForWriting() || IsMappedForReading()) ||
            !CheckGetMappedRangeOffsetSize(offset, size)) {
            return nullptr;
        }
        return static_cast<uint8_t*>(mMappedData) + offset;
    }

    void Buffer::Unmap() {
        // Invalidate the local pointer, and cancel all other in-flight requests that would
        // turn into errors anyway (you can't double map). This prevents race when the following
        // happens, where the application code would have unmapped a buffer but still receive a
        // callback:
        //   - Client -> Server: MapRequest1, Unmap, MapRequest2
        //   - Server -> Client: Result of MapRequest1
        //   - Unmap locally on the client
        //   - Server -> Client: Result of MapRequest2

        // TODO(dawn:608): mDevice->InjectError(WGPUErrorType_Validation) for map oom failure
        // and separate from buffer destroyed before unmap case

        if (mIsMappingWrite && mWriteHandle != nullptr) {
            // Writes need to be flushed before Unmap is sent. Unmap calls all associated
            // in-flight callbacks which may read the updated data.

            // mReadHandle could be non-null if the buffer is mappedAtCreation and has map read mode
            // So we only assert the map read access token to be false.
            ASSERT(!mIsMappingRead);

            // Get the serialization size of data update writes.
            size_t writeDataUpdateInfoLength =
                mWriteHandle->SizeOfSerializeDataUpdate(mMapOffset, mMapSize);

            BufferUpdateMappedDataCmd cmd;
            cmd.bufferId = id;
            cmd.writeDataUpdateInfoLength = writeDataUpdateInfoLength;
            cmd.writeDataUpdateInfo = nullptr;
            cmd.offset = mMapOffset;
            cmd.size = mMapSize;

            client->SerializeCommand(
                cmd, writeDataUpdateInfoLength, [&](SerializeBuffer* serializeBuffer) {
                    char* writeHandleBuffer;
                    WIRE_TRY(serializeBuffer->NextN(writeDataUpdateInfoLength, &writeHandleBuffer));

                    // Serialize flush metadata into the space after the command.
                    // This closes the handle for writing.
                    mWriteHandle->SerializeDataUpdate(writeHandleBuffer, mMapOffset, mMapSize);

                    // If mDestructWriteHandleOnUnmap is true, that means the write handle is merely
                    // for mappedAtCreation usage. It is destroyed on unmap after flush to server
                    // instead of at buffer destruction.
                    if (mDestructWriteHandleOnUnmap) {
                        mWriteHandle = nullptr;
                    }
                    return WireResult::Success;
                });
        }

        // FreeMappedData but don't destroy Read/WriteHandle
        FreeMappedData(false);
        // Free map access tokens
        mIsMappingRead = false;
        mIsMappingWrite = false;

        // Tag all mapping requests still in flight as unmapped before callback.
        for (auto& it : mRequests) {
            if (it.second.clientStatus == WGPUBufferMapAsyncStatus_Success) {
                it.second.clientStatus = WGPUBufferMapAsyncStatus_UnmappedBeforeCallback;
            }
        }

        BufferUnmapCmd cmd;
        cmd.self = ToAPI(this);
        client->SerializeCommand(cmd);
        // TODO(dawn:608): change to return bool for map oom error handling
        return;
    }

    void Buffer::Destroy() {
        // Remove the current mapping and destory Read/WriteHandles.
        FreeMappedData(true);

        // Tag all mapping requests still in flight as destroyed before callback.
        for (auto& it : mRequests) {
            if (it.second.clientStatus == WGPUBufferMapAsyncStatus_Success) {
                it.second.clientStatus = WGPUBufferMapAsyncStatus_DestroyedBeforeCallback;
            }
        }

        BufferDestroyCmd cmd;
        cmd.self = ToAPI(this);
        client->SerializeCommand(cmd);
    }

    bool Buffer::IsMappedForReading() const {
        return mIsMappingRead;
    }

    bool Buffer::IsMappedForWriting() const {
        return mIsMappingWrite;
    }

    bool Buffer::CheckGetMappedRangeOffsetSize(size_t offset, size_t size) const {
        if (offset % 8 != 0 || size % 4 != 0) {
            return false;
        }

        if (size > mMapSize || offset < mMapOffset) {
            return false;
        }

        size_t offsetInMappedRange = offset - mMapOffset;
        return offsetInMappedRange <= mMapSize - size;
    }

    void Buffer::FreeMappedData(bool destruction) {
#if defined(DAWN_ENABLE_ASSERTS)
        // When in "debug" mode, 0xCA-out the mapped data when we free it so that in we can detect
        // use-after-free of the mapped data. This is particularly useful for WebGPU test about the
        // interaction of mapping and GC.
        if (mMappedData && destruction) {
            memset(static_cast<uint8_t*>(mMappedData) + mMapOffset, 0xCA, mMapSize);
        }
#endif  // defined(DAWN_ENABLE_ASSERTS)

        mMapOffset = 0;
        mMapSize = 0;
        if (destruction) {
            mReadHandle = nullptr;
            mWriteHandle = nullptr;
        }
        mMappedData = nullptr;
    }

}}  // namespace dawn_wire::client
