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

#include "dawn_wire/client/ApiProcs_autogen.h"
#include "dawn_wire/client/Client.h"
#include "dawn_wire/client/Device.h"

namespace dawn_wire { namespace client {

    namespace {
        template <typename Handle>
        void SerializeBufferMapAsync(const Buffer* buffer, uint32_t serial, Handle* handle) {
            // TODO(enga): Remove the template when Read/Write handles are combined in a tagged
            // pointer.
            constexpr bool isWrite =
                std::is_same<Handle, MemoryTransferService::WriteHandle>::value;

            // Get the serialization size of the handle.
            size_t handleCreateInfoLength = handle->SerializeCreateSize();

            BufferMapAsyncCmd cmd;
            cmd.bufferId = buffer->id;
            cmd.requestSerial = serial;
            cmd.isWrite = isWrite;
            cmd.handleCreateInfoLength = handleCreateInfoLength;
            cmd.handleCreateInfo = nullptr;

            char* writeHandleSpace =
                buffer->device->GetClient()->SerializeCommand(cmd, handleCreateInfoLength);

            // Serialize the handle into the space after the command.
            handle->SerializeCreate(writeHandleSpace);
        }
    }  // namespace

    // static
    WGPUBuffer Buffer::Create(Device* device_, const WGPUBufferDescriptor* descriptor) {
        WGPUDevice cDevice = reinterpret_cast<WGPUDevice>(device_);
        Client* wireClient = device_->GetClient();

        if ((descriptor->usage & (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite)) != 0 &&
            descriptor->size > std::numeric_limits<size_t>::max()) {
            ClientDeviceInjectError(cDevice, WGPUErrorType_OutOfMemory,
                                    "Buffer is too large for map usage");
            return ClientDeviceCreateErrorBuffer(cDevice);
        }

        auto* bufferObjectAndSerial = wireClient->BufferAllocator().New(device_);
        Buffer* buffer = bufferObjectAndSerial->object.get();
        // Store the size of the buffer so that mapping operations can allocate a
        // MemoryTransfer handle of the proper size.
        buffer->mSize = descriptor->size;

        DeviceCreateBufferCmd cmd;
        cmd.self = cDevice;
        cmd.descriptor = descriptor;
        cmd.result = ObjectHandle{buffer->id, bufferObjectAndSerial->generation};

        wireClient->SerializeCommand(cmd);

        return reinterpret_cast<WGPUBuffer>(buffer);
    }

    // static
    WGPUCreateBufferMappedResult Buffer::CreateMapped(Device* device_,
                                                      const WGPUBufferDescriptor* descriptor) {
        WGPUDevice cDevice = reinterpret_cast<WGPUDevice>(device_);
        Client* wireClient = device_->GetClient();

        WGPUCreateBufferMappedResult result;
        result.data = nullptr;
        result.dataLength = 0;

        // This buffer is too large to be mapped and to make a WriteHandle for.
        if (descriptor->size > std::numeric_limits<size_t>::max()) {
            ClientDeviceInjectError(cDevice, WGPUErrorType_OutOfMemory,
                                    "Buffer is too large for mapping");
            result.buffer = ClientDeviceCreateErrorBuffer(cDevice);
            return result;
        }

        // Create a WriteHandle for the map request. This is the client's intent to write GPU
        // memory.
        std::unique_ptr<MemoryTransferService::WriteHandle> writeHandle =
            std::unique_ptr<MemoryTransferService::WriteHandle>(
                wireClient->GetMemoryTransferService()->CreateWriteHandle(descriptor->size));

        if (writeHandle == nullptr) {
            ClientDeviceInjectError(cDevice, WGPUErrorType_OutOfMemory,
                                    "Buffer mapping allocation failed");
            result.buffer = ClientDeviceCreateErrorBuffer(cDevice);
            return result;
        }

        // CreateBufferMapped is synchronous and the staging buffer for upload should be immediately
        // available.
        // Open the WriteHandle. This returns a pointer and size of mapped memory.
        // |result.data| may be null on error.
        std::tie(result.data, result.dataLength) = writeHandle->Open();
        if (result.data == nullptr) {
            ClientDeviceInjectError(cDevice, WGPUErrorType_OutOfMemory,
                                    "Buffer mapping allocation failed");
            result.buffer = ClientDeviceCreateErrorBuffer(cDevice);
            return result;
        }

        auto* bufferObjectAndSerial = wireClient->BufferAllocator().New(device_);
        Buffer* buffer = bufferObjectAndSerial->object.get();
        buffer->mSize = descriptor->size;
        // Successfully created staging memory. The buffer now owns the WriteHandle.
        buffer->mWriteHandle = std::move(writeHandle);
        buffer->mMappedData = result.data;

        result.buffer = reinterpret_cast<WGPUBuffer>(buffer);

        // Get the serialization size of the WriteHandle.
        size_t handleCreateInfoLength = buffer->mWriteHandle->SerializeCreateSize();

        DeviceCreateBufferMappedCmd cmd;
        cmd.device = cDevice;
        cmd.descriptor = descriptor;
        cmd.result = ObjectHandle{buffer->id, bufferObjectAndSerial->generation};
        cmd.handleCreateInfoLength = handleCreateInfoLength;
        cmd.handleCreateInfo = nullptr;

        char* writeHandleSpace =
            buffer->device->GetClient()->SerializeCommand(cmd, handleCreateInfoLength);

        // Serialize the WriteHandle into the space after the command.
        buffer->mWriteHandle->SerializeCreate(writeHandleSpace);

        return result;
    }

    Buffer::~Buffer() {
        // Callbacks need to be fired in all cases, as they can handle freeing resources
        // so we call them with "Unknown" status.
        ClearMapRequests(WGPUBufferMapAsyncStatus_Unknown);
    }

    void Buffer::ClearMapRequests(WGPUBufferMapAsyncStatus status) {
        for (auto& it : mRequests) {
            if (it.second.writeHandle) {
                it.second.writeCallback(status, nullptr, 0, it.second.userdata);
            } else {
                it.second.readCallback(status, nullptr, 0, it.second.userdata);
            }
        }
        mRequests.clear();
    }

    void Buffer::MapReadAsync(WGPUBufferMapReadCallback callback, void* userdata) {
        uint32_t serial = mRequestSerial++;
        ASSERT(mRequests.find(serial) == mRequests.end());

        if (mSize > std::numeric_limits<size_t>::max()) {
            // On buffer creation, we check that mappable buffers do not exceed this size.
            // So this buffer must not have mappable usage. Inject a validation error.
            ClientDeviceInjectError(reinterpret_cast<WGPUDevice>(device), WGPUErrorType_Validation,
                                    "Buffer needs the correct map usage bit");
            callback(WGPUBufferMapAsyncStatus_Error, nullptr, 0, userdata);
            return;
        }

        // Create a ReadHandle for the map request. This is the client's intent to read GPU
        // memory.
        MemoryTransferService::ReadHandle* readHandle =
            device->GetClient()->GetMemoryTransferService()->CreateReadHandle(
                static_cast<size_t>(mSize));
        if (readHandle == nullptr) {
            ClientDeviceInjectError(reinterpret_cast<WGPUDevice>(device), WGPUErrorType_OutOfMemory,
                                    "Failed to create buffer mapping");
            callback(WGPUBufferMapAsyncStatus_Error, nullptr, 0, userdata);
            return;
        }

        Buffer::MapRequestData request = {};
        request.readCallback = callback;
        request.userdata = userdata;
        // The handle is owned by the MapRequest until the callback returns.
        request.readHandle = std::unique_ptr<MemoryTransferService::ReadHandle>(readHandle);

        // Store a mapping from serial -> MapRequest. The client can map/unmap before the map
        // operations are returned by the server so multiple requests may be in flight.
        mRequests[serial] = std::move(request);

        SerializeBufferMapAsync(this, serial, readHandle);
    }

    void Buffer::MapWriteAsync(WGPUBufferMapWriteCallback callback, void* userdata) {
        uint32_t serial = mRequestSerial++;
        ASSERT(mRequests.find(serial) == mRequests.end());

        if (mSize > std::numeric_limits<size_t>::max()) {
            // On buffer creation, we check that mappable buffers do not exceed this size.
            // So this buffer must not have mappable usage. Inject a validation error.
            ClientDeviceInjectError(reinterpret_cast<WGPUDevice>(device), WGPUErrorType_Validation,
                                    "Buffer needs the correct map usage bit");
            callback(WGPUBufferMapAsyncStatus_Error, nullptr, 0, userdata);
            return;
        }

        // Create a WriteHandle for the map request. This is the client's intent to write GPU
        // memory.
        MemoryTransferService::WriteHandle* writeHandle =
            device->GetClient()->GetMemoryTransferService()->CreateWriteHandle(
                static_cast<size_t>(mSize));
        if (writeHandle == nullptr) {
            ClientDeviceInjectError(reinterpret_cast<WGPUDevice>(device), WGPUErrorType_OutOfMemory,
                                    "Failed to create buffer mapping");
            callback(WGPUBufferMapAsyncStatus_Error, nullptr, 0, userdata);
            return;
        }

        Buffer::MapRequestData request = {};
        request.writeCallback = callback;
        request.userdata = userdata;
        // The handle is owned by the MapRequest until the callback returns.
        request.writeHandle = std::unique_ptr<MemoryTransferService::WriteHandle>(writeHandle);

        // Store a mapping from serial -> MapRequest. The client can map/unmap before the map
        // operations are returned by the server so multiple requests may be in flight.
        mRequests[serial] = std::move(request);

        SerializeBufferMapAsync(this, serial, writeHandle);
    }

    bool Buffer::OnMapReadAsyncCallback(uint32_t requestSerial,
                                        uint32_t status,
                                        uint64_t initialDataInfoLength,
                                        const uint8_t* initialDataInfo) {
        // The requests can have been deleted via an Unmap so this isn't an error.
        auto requestIt = mRequests.find(requestSerial);
        if (requestIt == mRequests.end()) {
            return true;
        }

        auto request = std::move(requestIt->second);
        // Delete the request before calling the callback otherwise the callback could be fired a
        // second time. If, for example, buffer.Unmap() is called inside the callback.
        mRequests.erase(requestIt);

        size_t mappedDataLength = 0;

        auto GetMappedData = [&]() -> bool {
            // It is an error for the server to call the read callback when we asked for a map write
            if (request.writeHandle) {
                return false;
            }

            if (status == WGPUBufferMapAsyncStatus_Success) {
                if (mReadHandle || mWriteHandle) {
                    // Buffer is already mapped.
                    return false;
                }
                if (initialDataInfoLength > std::numeric_limits<size_t>::max()) {
                    // This is the size of data deserialized from the command stream, which must be
                    // CPU-addressable.
                    return false;
                }
                ASSERT(request.readHandle != nullptr);

                // The server serializes metadata to initialize the contents of the ReadHandle.
                // Deserialize the message and return a pointer and size of the mapped data for
                // reading.
                const void* mappedData = nullptr;
                if (!request.readHandle->DeserializeInitialData(
                        initialDataInfo, static_cast<size_t>(initialDataInfoLength), &mappedData,
                        &mappedDataLength)) {
                    // Deserialization shouldn't fail. This is a fatal error.
                    return false;
                }
                ASSERT(mappedData != nullptr);
                mMappedData = const_cast<void*>(mappedData);

                // The MapRead request was successful. The buffer now owns the ReadHandle until
                // Unmap().
                mReadHandle = std::move(request.readHandle);
            }

            return true;
        };

        if (!GetMappedData()) {
            // Dawn promises that all callbacks are called in finite time. Even if a fatal error
            // occurs, the callback is called.
            request.readCallback(WGPUBufferMapAsyncStatus_DeviceLost, nullptr, 0, request.userdata);
            return false;
        } else {
            request.readCallback(static_cast<WGPUBufferMapAsyncStatus>(status), mMappedData,
                                 static_cast<uint64_t>(mappedDataLength), request.userdata);
            return true;
        }
    }

    bool Buffer::OnMapWriteAsyncCallback(uint32_t requestSerial, uint32_t status) {
        // The requests can have been deleted via an Unmap so this isn't an error.
        auto requestIt = mRequests.find(requestSerial);
        if (requestIt == mRequests.end()) {
            return true;
        }

        auto request = std::move(requestIt->second);
        // Delete the request before calling the callback otherwise the callback could be fired a
        // second time. If, for example, buffer.Unmap() is called inside the callback.
        mRequests.erase(requestIt);

        size_t mappedDataLength = 0;

        auto GetMappedData = [&]() -> bool {
            // It is an error for the server to call the write callback when we asked for a map read
            if (request.readHandle) {
                return false;
            }

            if (status == WGPUBufferMapAsyncStatus_Success) {
                if (mReadHandle || mWriteHandle) {
                    // Buffer is already mapped.
                    return false;
                }
                ASSERT(request.writeHandle != nullptr);

                // Open the WriteHandle. This returns a pointer and size of mapped memory.
                // On failure, |mappedData| may be null.
                std::tie(mMappedData, mappedDataLength) = request.writeHandle->Open();

                if (mMappedData == nullptr) {
                    return false;
                }

                // The MapWrite request was successful. The buffer now owns the WriteHandle until
                // Unmap().
                mWriteHandle = std::move(request.writeHandle);
            }

            return true;
        };

        if (!GetMappedData()) {
            // Dawn promises that all callbacks are called in finite time. Even if a fatal error
            // occurs, the callback is called.
            request.writeCallback(WGPUBufferMapAsyncStatus_DeviceLost, nullptr, 0,
                                  request.userdata);
            return false;
        } else {
            request.writeCallback(static_cast<WGPUBufferMapAsyncStatus>(status), mMappedData,
                                  static_cast<uint64_t>(mappedDataLength), request.userdata);
            return true;
        }
    }

    void* Buffer::GetMappedRange() {
        if (!IsMappedForWriting()) {
            ClientDeviceInjectError(reinterpret_cast<WGPUDevice>(device), WGPUErrorType_Validation,
                                    "Buffer needs to be mapped for writing");
            return nullptr;
        }
        return mMappedData;
    }

    const void* Buffer::GetConstMappedRange() {
        if (!IsMappedForWriting() && !IsMappedForReading()) {
            ClientDeviceInjectError(reinterpret_cast<WGPUDevice>(device), WGPUErrorType_Validation,
                                    "Buffer needs to be mapped");
            return nullptr;
        }
        return mMappedData;
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
        if (mWriteHandle) {
            // Writes need to be flushed before Unmap is sent. Unmap calls all associated
            // in-flight callbacks which may read the updated data.
            ASSERT(mReadHandle == nullptr);

            // Get the serialization size of metadata to flush writes.
            size_t writeFlushInfoLength = mWriteHandle->SerializeFlushSize();

            BufferUpdateMappedDataCmd cmd;
            cmd.bufferId = id;
            cmd.writeFlushInfoLength = writeFlushInfoLength;
            cmd.writeFlushInfo = nullptr;

            char* writeHandleSpace =
                device->GetClient()->SerializeCommand(cmd, writeFlushInfoLength);

            // Serialize flush metadata into the space after the command.
            // This closes the handle for writing.
            mWriteHandle->SerializeFlush(writeHandleSpace);
            mWriteHandle = nullptr;

        } else if (mReadHandle) {
            mReadHandle = nullptr;
        }
        mMappedData = nullptr;
        ClearMapRequests(WGPUBufferMapAsyncStatus_Unknown);

        BufferUnmapCmd cmd;
        cmd.self = reinterpret_cast<WGPUBuffer>(this);
        device->GetClient()->SerializeCommand(cmd);
    }

    void Buffer::Destroy() {
        // Cancel or remove all mappings
        mWriteHandle = nullptr;
        mReadHandle = nullptr;
        mMappedData = nullptr;
        ClearMapRequests(WGPUBufferMapAsyncStatus_Unknown);

        BufferDestroyCmd cmd;
        cmd.self = reinterpret_cast<WGPUBuffer>(this);
        device->GetClient()->SerializeCommand(cmd);
    }

    void Buffer::SetSubData(uint64_t start, uint64_t count, const void* data) {
        BufferSetSubDataInternalCmd cmd;
        cmd.bufferId = id;
        cmd.start = start;
        cmd.count = count;
        cmd.data = static_cast<const uint8_t*>(data);

        device->GetClient()->SerializeCommand(cmd);
    }

    bool Buffer::IsMappedForReading() const {
        return mReadHandle != nullptr;
    }

    bool Buffer::IsMappedForWriting() const {
        return mWriteHandle != nullptr;
    }

}}  // namespace dawn_wire::client
