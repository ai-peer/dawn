// Copyright 2019 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "dawn/wire/client/Buffer.h"

#include <functional>
#include <limits>
#include <utility>

#include "dawn/wire/BufferConsumer_impl.h"
#include "dawn/wire/WireCmd_autogen.h"
#include "dawn/wire/client/Client.h"
#include "dawn/wire/client/Device.h"
#include "dawn/wire/client/EventManager.h"

namespace dawn::wire::client {
namespace {

WGPUBuffer CreateErrorBufferOOMAtClient(Device* device, const WGPUBufferDescriptor* descriptor) {
    if (descriptor->mappedAtCreation) {
        return nullptr;
    }
    WGPUBufferDescriptor errorBufferDescriptor = *descriptor;
    WGPUDawnBufferDescriptorErrorInfoFromWireClient errorInfo = {};
    errorInfo.chain.sType = WGPUSType_DawnBufferDescriptorErrorInfoFromWireClient;
    errorInfo.outOfMemory = true;
    errorBufferDescriptor.nextInChain = &errorInfo.chain;
    return GetProcs().deviceCreateErrorBuffer(ToAPI(device), &errorBufferDescriptor);
}

class MapAsyncEvent : public TrackedEvent {
  public:
    static constexpr EventType kType = EventType::MapAsync;

    explicit MapAsyncEvent(const WGPUBufferMapCallbackInfo& callbackInfo, Buffer* buffer)
        : TrackedEvent(callbackInfo.mode),
          mCallback(callbackInfo.callback),
          mUserdata(callbackInfo.userdata),
          mBuffer(buffer) {
        DAWN_ASSERT(buffer != nullptr);
        mBuffer->Reference();
    }

    ~MapAsyncEvent() override { GetProcs().bufferRelease(ToAPI(mBuffer)); }

    EventType GetType() override { return kType; }

    bool IsPendingRequest(FutureID futureID) {
        MapStateData& data = mBuffer->GetMapStateData();
        return data.mPendingRequest && data.mPendingRequest->futureID == futureID;
    }

    void ReadyHook(FutureID futureID,
                   WGPUBufferMapAsyncStatus status,
                   uint64_t readDataUpdateInfoLength = 0,
                   const uint8_t* readDataUpdateInfo = nullptr) {
        // Handling for different statuses.
        MapStateData& data = mBuffer->GetMapStateData();
        switch (status) {
            case WGPUBufferMapAsyncStatus_MappingAlreadyPending: {
                DAWN_ASSERT(!IsPendingRequest(futureID));
                mStatus = status;
                break;
            }

            // For client-side rejection errors, we clear the pending request now since they always
            // take precedence.
            case WGPUBufferMapAsyncStatus_DestroyedBeforeCallback:
            case WGPUBufferMapAsyncStatus_UnmappedBeforeCallback: {
                mStatus = status;
                data.mPendingRequest = std::nullopt;
                break;
            }

            case WGPUBufferMapAsyncStatus_Success: {
                if (!IsPendingRequest(futureID)) {
                    // If a success occurs (which must come from the server), but it does not
                    // correspond to the pending request, the pending request must have been
                    // rejected early and hence the status must be set.
                    DAWN_ASSERT(mStatus);
                    break;
                }
                mStatus = status;
                MapRequestData& pending = data.mPendingRequest.value();
                if (!pending.type) {
                    return;
                }
                switch (*pending.type) {
                    case MapRequestType::Read: {
                        if (readDataUpdateInfoLength > std::numeric_limits<size_t>::max()) {
                            // This is the size of data deserialized from the command stream, which
                            // must be CPU-addressable.
                            mStatus = WGPUBufferMapAsyncStatus_DeviceLost;
                            break;
                        }

                        // Validate to prevent bad map request; buffer destroyed during map request
                        if (data.mReadHandle == nullptr) {
                            mStatus = WGPUBufferMapAsyncStatus_DeviceLost;
                            break;
                        }
                        // Update user map data with server returned data
                        if (!data.mReadHandle->DeserializeDataUpdate(
                                readDataUpdateInfo, static_cast<size_t>(readDataUpdateInfoLength),
                                pending.offset, pending.size)) {
                            mStatus = WGPUBufferMapAsyncStatus_DeviceLost;
                            break;
                        }
                        data.mData = const_cast<void*>(data.mReadHandle->GetData());
                        break;
                    }
                    case MapRequestType::Write: {
                        if (data.mWriteHandle == nullptr) {
                            mStatus = WGPUBufferMapAsyncStatus_DeviceLost;
                            break;
                        }
                        data.mData = data.mWriteHandle->GetData();
                        break;
                    }
                }
                data.mOffset = pending.offset;
                data.mSize = pending.size;
                break;
            }

            // All other statuses are server-side status.
            default: {
                if (!IsPendingRequest(futureID)) {
                    break;
                }
                mStatus = status;
            } break;
        }
    }

  private:
    void CompleteImpl(FutureID futureID, EventCompletionType completionType) override {
        WGPUBufferMapAsyncStatus status = completionType == EventCompletionType::Shutdown
                                              ? WGPUBufferMapAsyncStatus_DeviceLost
                                              : WGPUBufferMapAsyncStatus_Success;
        if (mStatus) {
            status = *mStatus;
        }

        MapStateData& data = mBuffer->GetMapStateData();
        if (data.mPendingRequest && data.mPendingRequest->futureID == futureID) {
            if (status == WGPUBufferMapAsyncStatus_Success && data.mPendingRequest->type) {
                switch (*data.mPendingRequest->type) {
                    case MapRequestType::Read:
                        data.mState = MapState::MappedForRead;
                        break;
                    case MapRequestType::Write:
                        data.mState = MapState::MappedForWrite;
                        break;
                }
            }
            data.mPendingRequest = std::nullopt;
        }
        if (mCallback) {
            mCallback(status, mUserdata);
        }
    }

    WGPUBufferMapCallback mCallback;
    void* mUserdata;

    std::optional<WGPUBufferMapAsyncStatus> mStatus;

    // Strong reference to the buffer so that when we call the callback we can pass the buffer.
    Buffer* const mBuffer;
};

}  // anonymous namespace

// static
WGPUBuffer Buffer::Create(Device* device, const WGPUBufferDescriptor* descriptor) {
    Client* wireClient = device->GetClient();

    bool mappable =
        (descriptor->usage & (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite)) != 0 ||
        descriptor->mappedAtCreation;
    if (mappable && descriptor->size >= std::numeric_limits<size_t>::max()) {
        return CreateErrorBufferOOMAtClient(device, descriptor);
    }

    std::unique_ptr<MemoryTransferService::ReadHandle> readHandle = nullptr;
    std::unique_ptr<MemoryTransferService::WriteHandle> writeHandle = nullptr;

    DeviceCreateBufferCmd cmd;
    cmd.deviceId = device->GetWireId();
    cmd.descriptor = descriptor;
    cmd.readHandleCreateInfoLength = 0;
    cmd.readHandleCreateInfo = nullptr;
    cmd.writeHandleCreateInfoLength = 0;
    cmd.writeHandleCreateInfo = nullptr;

    size_t readHandleCreateInfoLength = 0;
    size_t writeHandleCreateInfoLength = 0;
    if (mappable) {
        if ((descriptor->usage & WGPUBufferUsage_MapRead) != 0) {
            // Create the read handle on buffer creation.
            readHandle.reset(
                wireClient->GetMemoryTransferService()->CreateReadHandle(descriptor->size));
            if (readHandle == nullptr) {
                return CreateErrorBufferOOMAtClient(device, descriptor);
            }
            readHandleCreateInfoLength = readHandle->SerializeCreateSize();
            cmd.readHandleCreateInfoLength = readHandleCreateInfoLength;
        }

        if ((descriptor->usage & WGPUBufferUsage_MapWrite) != 0 || descriptor->mappedAtCreation) {
            // Create the write handle on buffer creation.
            writeHandle.reset(
                wireClient->GetMemoryTransferService()->CreateWriteHandle(descriptor->size));
            if (writeHandle == nullptr) {
                return CreateErrorBufferOOMAtClient(device, descriptor);
            }
            writeHandleCreateInfoLength = writeHandle->SerializeCreateSize();
            cmd.writeHandleCreateInfoLength = writeHandleCreateInfoLength;
        }
    }

    // Create the buffer and send the creation command.
    // This must happen after any potential error buffer creation
    // as server expects allocating ids to be monotonically increasing
    Buffer* buffer = wireClient->Make<Buffer>(device->GetEventManagerHandle(), descriptor);
    buffer->mDestructWriteHandleOnUnmap = false;

    if (descriptor->mappedAtCreation) {
        // If the buffer is mapped at creation, a write handle is created and will be
        // destructed on unmap if the buffer doesn't have MapWrite usage
        // The buffer is mapped right now.
        buffer->mMapStateData.mState = MapState::MappedAtCreation;

        // This flag is for write handle created by mappedAtCreation
        // instead of MapWrite usage. We don't have such a case for read handle
        buffer->mDestructWriteHandleOnUnmap = (descriptor->usage & WGPUBufferUsage_MapWrite) == 0;

        buffer->mMapStateData.mOffset = 0;
        buffer->mMapStateData.mSize = buffer->mSize;
        DAWN_ASSERT(writeHandle != nullptr);
        buffer->mMapStateData.mData = writeHandle->GetData();
    }

    cmd.result = buffer->GetWireHandle();

    // clang-format off
    // Turning off clang format here because for some reason it does not format the
    // CommandExtensions consistently, making it harder to read.
    wireClient->SerializeCommand(
        cmd,
        CommandExtension{readHandleCreateInfoLength,
                         [&](char* readHandleBuffer) {
                             if (readHandle != nullptr) {
                                 // Serialize the ReadHandle into the space after the command.
                                 readHandle->SerializeCreate(readHandleBuffer);
                                 buffer->mMapStateData.mReadHandle = std::move(readHandle);
                             }
                         }},
        CommandExtension{writeHandleCreateInfoLength,
                         [&](char* writeHandleBuffer) {
                             if (writeHandle != nullptr) {
                                 // Serialize the WriteHandle into the space after the command.
                                 writeHandle->SerializeCreate(writeHandleBuffer);
                                 buffer->mMapStateData.mWriteHandle = std::move(writeHandle);
                             }
                         }});
    // clang-format on
    return ToAPI(buffer);
}

Buffer::Buffer(const ObjectBaseParams& params,
               const ObjectHandle& eventManagerHandle,
               const WGPUBufferDescriptor* descriptor)
    : ObjectWithEventsBase(params, eventManagerHandle),
      mSize(descriptor->size),
      mUsage(static_cast<WGPUBufferUsage>(descriptor->usage)) {}

Buffer::~Buffer() {
    FreeMappedData();
}

void Buffer::SetFutureStatus(WGPUBufferMapAsyncStatus status) {
    if (!mMapStateData.mPendingRequest) {
        return;
    }
    DAWN_UNUSED(GetEventManager().SetFutureReady<MapAsyncEvent>(
        mMapStateData.mPendingRequest->futureID, status));
}

void Buffer::MapAsync(WGPUMapModeFlags mode,
                      size_t offset,
                      size_t size,
                      WGPUBufferMapCallback callback,
                      void* userdata) {
    WGPUBufferMapCallbackInfo callbackInfo = {};
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    callbackInfo.callback = callback;
    callbackInfo.userdata = userdata;
    MapAsyncF(mode, offset, size, callbackInfo);
}

WGPUFuture Buffer::MapAsyncF(WGPUMapModeFlags mode,
                             size_t offset,
                             size_t size,
                             const WGPUBufferMapCallbackInfo& callbackInfo) {
    DAWN_ASSERT(GetRefcount() != 0);

    Client* client = GetClient();
    auto [futureIDInternal, tracked] =
        GetEventManager().TrackEvent(std::make_unique<MapAsyncEvent>(callbackInfo, this));
    if (!tracked) {
        return {futureIDInternal};
    }

    if (mMapStateData.mPendingRequest) {
        DAWN_UNUSED(GetEventManager().SetFutureReady<MapAsyncEvent>(
            futureIDInternal, WGPUBufferMapAsyncStatus_MappingAlreadyPending));
        return {futureIDInternal};
    }

    if (mIsDestroyed) {
        DAWN_UNUSED(GetEventManager().SetFutureReady<MapAsyncEvent>(
            futureIDInternal, WGPUBufferMapAsyncStatus_DestroyedBeforeCallback));
        return {futureIDInternal};
    }

    // Handle the defaulting of size required by WebGPU.
    if ((size == WGPU_WHOLE_MAP_SIZE) && (offset <= mSize)) {
        size = mSize - offset;
    }

    // Set up the request structure that will hold information while this mapping is in flight.
    std::optional<MapRequestType> mapMode;
    if (mode & WGPUMapMode_Read) {
        mapMode = MapRequestType::Read;
    } else if (mode & WGPUMapMode_Write) {
        mapMode = MapRequestType::Write;
    }

    mMapStateData.mPendingRequest = {futureIDInternal, offset, size, mapMode};

    // Serialize the command to send to the server.
    BufferMapAsyncCmd cmd;
    cmd.bufferId = GetWireId();
    cmd.eventManagerHandle = GetEventManagerHandle();
    cmd.future = {futureIDInternal};
    cmd.mode = mode;
    cmd.offset = offset;
    cmd.size = size;

    client->SerializeCommand(cmd);
    return {futureIDInternal};
}

bool Client::DoBufferMapAsyncCallback(ObjectHandle eventManager,
                                      WGPUFuture future,
                                      WGPUBufferMapAsyncStatus status,
                                      uint64_t readDataUpdateInfoLength,
                                      const uint8_t* readDataUpdateInfo) {
    return GetEventManager(eventManager)
               .SetFutureReady<MapAsyncEvent>(future.id, status, readDataUpdateInfoLength,
                                              readDataUpdateInfo) == WireResult::Success;
}

void* Buffer::GetMappedRange(size_t offset, size_t size) {
    if (!IsMappedForWriting() || !CheckGetMappedRangeOffsetSize(offset, size)) {
        return nullptr;
    }
    return static_cast<uint8_t*>(mMapStateData.mData) + offset;
}

const void* Buffer::GetConstMappedRange(size_t offset, size_t size) {
    if (!(IsMappedForWriting() || IsMappedForReading()) ||
        !CheckGetMappedRangeOffsetSize(offset, size)) {
        return nullptr;
    }
    return static_cast<uint8_t*>(mMapStateData.mData) + offset;
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
    Client* client = GetClient();

    // mWriteHandle can still be nullptr if buffer has been destroyed before unmap
    if ((mMapStateData.mState == MapState::MappedForWrite ||
         mMapStateData.mState == MapState::MappedAtCreation) &&
        mMapStateData.mWriteHandle != nullptr) {
        // Writes need to be flushed before Unmap is sent. Unmap calls all associated
        // in-flight callbacks which may read the updated data.

        // Get the serialization size of data update writes.
        size_t writeDataUpdateInfoLength = mMapStateData.mWriteHandle->SizeOfSerializeDataUpdate(
            mMapStateData.mOffset, mMapStateData.mSize);

        BufferUpdateMappedDataCmd cmd;
        cmd.bufferId = GetWireId();
        cmd.writeDataUpdateInfoLength = writeDataUpdateInfoLength;
        cmd.writeDataUpdateInfo = nullptr;
        cmd.offset = mMapStateData.mOffset;
        cmd.size = mMapStateData.mSize;

        client->SerializeCommand(
            cmd, CommandExtension{writeDataUpdateInfoLength, [&](char* writeHandleBuffer) {
                                      // Serialize flush metadata into the space after the command.
                                      // This closes the handle for writing.
                                      mMapStateData.mWriteHandle->SerializeDataUpdate(
                                          writeHandleBuffer, cmd.offset, cmd.size);
                                  }});

        // If mDestructWriteHandleOnUnmap is true, that means the write handle is merely
        // for mappedAtCreation usage. It is destroyed on unmap after flush to server
        // instead of at buffer destruction.
        if (mMapStateData.mState == MapState::MappedAtCreation && mDestructWriteHandleOnUnmap) {
            mMapStateData.mWriteHandle = nullptr;
            if (mMapStateData.mReadHandle) {
                // If it's both mappedAtCreation and MapRead we need to reset
                // mData to readHandle's GetData(). This could be changed to
                // merging read/write handle in future
                mMapStateData.mData = const_cast<void*>(mMapStateData.mReadHandle->GetData());
            }
        }
    }

    // Free map access tokens
    mMapStateData.mState = MapState::Unmapped;
    mMapStateData.mOffset = 0;
    mMapStateData.mSize = 0;

    BufferUnmapCmd cmd;
    cmd.self = ToAPI(this);
    client->SerializeCommand(cmd);

    SetFutureStatus(WGPUBufferMapAsyncStatus_UnmappedBeforeCallback);
}

void Buffer::Destroy() {
    Client* client = GetClient();

    // Remove the current mapping and destroy Read/WriteHandles.
    FreeMappedData();

    BufferDestroyCmd cmd;
    cmd.self = ToAPI(this);
    client->SerializeCommand(cmd);

    mIsDestroyed = true;
    SetFutureStatus(WGPUBufferMapAsyncStatus_DestroyedBeforeCallback);
}

WGPUBufferUsage Buffer::GetUsage() const {
    return mUsage;
}

uint64_t Buffer::GetSize() const {
    return mSize;
}

WGPUBufferMapState Buffer::GetMapState() const {
    switch (mMapStateData.mState) {
        case MapState::MappedForRead:
        case MapState::MappedForWrite:
        case MapState::MappedAtCreation:
            return WGPUBufferMapState_Mapped;
        case MapState::Unmapped:
            if (mMapStateData.mPendingRequest) {
                return WGPUBufferMapState_Pending;
            } else {
                return WGPUBufferMapState_Unmapped;
            }
    }
    DAWN_UNREACHABLE();
}

MapStateData& Buffer::GetMapStateData() {
    return mMapStateData;
}

bool Buffer::IsMappedForReading() const {
    return mMapStateData.mState == MapState::MappedForRead;
}

bool Buffer::IsMappedForWriting() const {
    return mMapStateData.mState == MapState::MappedForWrite ||
           mMapStateData.mState == MapState::MappedAtCreation;
}

bool Buffer::CheckGetMappedRangeOffsetSize(size_t offset, size_t size) const {
    if (offset % 8 != 0 || offset < mMapStateData.mOffset || offset > mSize) {
        return false;
    }

    size_t rangeSize = size == WGPU_WHOLE_MAP_SIZE ? mSize - offset : size;

    if (rangeSize % 4 != 0 || rangeSize > mMapStateData.mSize) {
        return false;
    }

    size_t offsetInMappedRange = offset - mMapStateData.mOffset;
    return offsetInMappedRange <= mMapStateData.mSize - rangeSize;
}

void Buffer::FreeMappedData() {
#if defined(DAWN_ENABLE_ASSERTS)
    // When in "debug" mode, 0xCA-out the mapped data when we free it so that in we can detect
    // use-after-free of the mapped data. This is particularly useful for WebGPU test about the
    // interaction of mapping and GC.
    if (mMapStateData.mData) {
        memset(static_cast<uint8_t*>(mMapStateData.mData) + mMapStateData.mOffset, 0xCA,
               mMapStateData.mSize);
    }
#endif  // defined(DAWN_ENABLE_ASSERTS)

    mMapStateData.mOffset = 0;
    mMapStateData.mSize = 0;
    mMapStateData.mReadHandle = nullptr;
    mMapStateData.mWriteHandle = nullptr;
    mMapStateData.mData = nullptr;
    mMapStateData.mState = MapState::Unmapped;
}

}  // namespace dawn::wire::client
