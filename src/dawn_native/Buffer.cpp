// Copyright 2017 The Dawn Authors
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

#include "dawn_native/Buffer.h"

#include "common/Assert.h"
#include "dawn_native/Device.h"
#include "dawn_native/ValidationUtils_autogen.h"

#include <cstdio>
#include <utility>

namespace dawn_native {

    namespace {

        class ErrorBuffer : public BufferBase {
          public:
            ErrorBuffer(DeviceBase* device) : BufferBase(device, ObjectBase::kError) {
            }

          private:
            MaybeError SetSubDataImpl(uint32_t start,
                                      uint32_t count,
                                      const uint8_t* data) override {
                UNREACHABLE();
                return {};
            }
            void MapReadAsyncImpl(uint32_t serial) override {
                UNREACHABLE();
            }
            void MapWriteAsyncImpl(uint32_t serial) override {
                UNREACHABLE();
            }
            void UnmapImpl() override {
                UNREACHABLE();
            }
        };

    }  // anonymous namespace

    MaybeError ValidateBufferDescriptor(DeviceBase*, const BufferDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        DAWN_TRY(ValidateBufferUsageBit(descriptor->usage));

        dawn::BufferUsageBit usage = descriptor->usage;

        const dawn::BufferUsageBit kMapWriteAllowedUsages =
            dawn::BufferUsageBit::MapWrite | dawn::BufferUsageBit::TransferSrc;
        if (usage & dawn::BufferUsageBit::MapWrite && (usage & kMapWriteAllowedUsages) != usage) {
            return DAWN_VALIDATION_ERROR("Only TransferSrc is allowed with MapWrite");
        }

        const dawn::BufferUsageBit kMapReadAllowedUsages =
            dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::TransferDst;
        if (usage & dawn::BufferUsageBit::MapRead && (usage & kMapReadAllowedUsages) != usage) {
            return DAWN_VALIDATION_ERROR("Only TransferDst is allowed with MapRead");
        }

        return {};
    }

    // Buffer

    BufferBase::BufferBase(DeviceBase* device, const BufferDescriptor* descriptor)
        : ObjectBase(device),
          mSize(descriptor->size),
          mUsage(descriptor->usage),
          mState(BufferState::Unmapped) {
    }

    BufferBase::BufferBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag), mState(BufferState::Unmapped) {
    }

    BufferBase::~BufferBase() {
        if (mState == BufferState::Mapped) {
            ASSERT(!IsError());
            CallMapReadCallback(mMapSerial, DAWN_BUFFER_MAP_ASYNC_STATUS_UNKNOWN, nullptr, 0u);
            CallMapWriteCallback(mMapSerial, DAWN_BUFFER_MAP_ASYNC_STATUS_UNKNOWN, nullptr, 0u);
        }
    }

    // static
    BufferBase* BufferBase::MakeError(DeviceBase* device) {
        return new ErrorBuffer(device);
    }

    // static
    void BufferBase::CreateMappedCallback(dawnBufferMapAsyncStatus status,
                                          void* pointer,
                                          uint32_t dataLength,
                                          dawnCallbackUserdata userdata) {
        BufferBase* buffer = reinterpret_cast<BufferBase*>(static_cast<intptr_t>(userdata));
        ASSERT(buffer != nullptr);
        if (status == DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS) {
            ASSERT(dataLength == buffer->GetSize());
            buffer->mCreateMappedPointer = reinterpret_cast<uint8_t*>(pointer);
        } else {
            buffer->mStagingData.reset();
        }
    }

    // static
    BufferBase* BufferBase::CreateMapped(DeviceBase* device,
                                         const BufferDescriptor* descriptor,
                                         uint8_t** data,
                                         uint32_t* dataLength) {
        ASSERT(data != nullptr);
        ASSERT(dataLength != nullptr);

        BufferBase* buffer = device->CreateBuffer(descriptor);
        if (buffer->IsError()) {
            return buffer;
        }

        buffer->mStagingData.reset(new uint8_t[buffer->GetSize()]);
        *data = buffer->mStagingData.get();
        *dataLength = buffer->GetSize();

        buffer->MapWriteAsync(CreateMappedCallback, static_cast<dawnCallbackUserdata>(
                                                        reinterpret_cast<intptr_t>(buffer)));
        return buffer;
    }

    // static
    void BufferBase::CreateMappedAsync(DeviceBase* device,
                                       const BufferDescriptor* descriptor,
                                       dawnCreateBufferMappedCallback callback,
                                       dawnCallbackUserdata userdata) {
        // TODO: Optimize per-backend to lazily zero-copy init the buffer when it is unmapped
        // For now, we create a buffer and then call MapWriteAsync. We forward the map write
        // callback to the create buffer mapped callback and pass the buffer to it.
        BufferBase* buffer = device->CreateBuffer(descriptor);
        // Buffer descriptor is validated in the frontend
        ASSERT(!buffer->IsError());

        struct CreateMappedAsyncUserdata {
            BufferBase* buffer;
            dawnCreateBufferMappedCallback callback;
            dawnCallbackUserdata userdata;
        };

        auto* data = new CreateMappedAsyncUserdata{buffer, callback, userdata};

        // Pass a lambda to MapWriteAsync that will forward the buffer passed to this function and
        // the lambda's arguments to the create buffer mapped callback
        auto mapWriteAsyncCallback = [](dawnBufferMapAsyncStatus status, void* ptr,
                                        uint32_t dataLength, dawnCallbackUserdata userdata) {
            const auto* data = reinterpret_cast<const CreateMappedAsyncUserdata*>(
                static_cast<uintptr_t>(userdata));
            ASSERT(data != nullptr);

            dawnBuffer buffer = reinterpret_cast<dawnBuffer>(data->buffer);
            dawnCreateBufferMappedCallback createBufferMappedCallback = data->callback;
            dawnCallbackUserdata createBufferMappedUserdata = data->userdata;
            delete data;
            createBufferMappedCallback(buffer, status, ptr, dataLength, createBufferMappedUserdata);
        };

        buffer->MapWriteAsync(mapWriteAsyncCallback,
                              static_cast<dawnCallbackUserdata>(reinterpret_cast<uintptr_t>(data)));
    }

    uint32_t BufferBase::GetSize() const {
        ASSERT(!IsError());
        return mSize;
    }

    dawn::BufferUsageBit BufferBase::GetUsage() const {
        ASSERT(!IsError());
        return mUsage;
    }

    MaybeError BufferBase::ValidateCanUseInSubmitNow() const {
        ASSERT(!IsError());

        switch (mState) {
            case BufferState::Destroyed:
                return DAWN_VALIDATION_ERROR("Destroyed buffer used in a submit");
            case BufferState::Mapped:
                return DAWN_VALIDATION_ERROR("Buffer used in a submit while mapped");
            case BufferState::Unmapped:
                return {};
        }
    }

    void BufferBase::CallMapReadCallback(uint32_t serial,
                                         dawnBufferMapAsyncStatus status,
                                         const void* pointer,
                                         uint32_t dataLength) {
        ASSERT(!IsError());
        if (mMapReadCallback != nullptr && serial == mMapSerial) {
            ASSERT(mMapWriteCallback == nullptr);
            // Tag the callback as fired before firing it, otherwise it could fire a second time if
            // for example buffer.Unmap() is called inside the application-provided callback.
            dawnBufferMapReadCallback callback = mMapReadCallback;
            mMapReadCallback = nullptr;
            callback(status, pointer, dataLength, mMapUserdata);
        }
    }

    void BufferBase::CallMapWriteCallback(uint32_t serial,
                                          dawnBufferMapAsyncStatus status,
                                          void* pointer,
                                          uint32_t dataLength) {
        ASSERT(!IsError());
        if (mMapWriteCallback != nullptr && serial == mMapSerial) {
            ASSERT(mMapReadCallback == nullptr);
            // Tag the callback as fired before firing it, otherwise it could fire a second time if
            // for example buffer.Unmap() is called inside the application-provided callback.
            dawnBufferMapWriteCallback callback = mMapWriteCallback;
            mMapWriteCallback = nullptr;
            callback(status, pointer, dataLength, mMapUserdata);
        }
    }

    void BufferBase::SetSubData(uint32_t start, uint32_t count, const uint8_t* data) {
        if (GetDevice()->ConsumedError(ValidateSetSubData(start, count))) {
            return;
        }
        ASSERT(!IsError());

        if (GetDevice()->ConsumedError(SetSubDataImpl(start, count, data))) {
            return;
        }
    }

    void BufferBase::MapReadAsync(dawnBufferMapReadCallback callback,
                                  dawnCallbackUserdata userdata) {
        if (GetDevice()->ConsumedError(ValidateMap(dawn::BufferUsageBit::MapRead))) {
            callback(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, 0, userdata);
            return;
        }
        ASSERT(!IsError());

        ASSERT(mMapWriteCallback == nullptr);

        // TODO(cwallez@chromium.org): what to do on wraparound? Could cause crashes.
        mMapSerial++;
        mMapReadCallback = callback;
        mMapUserdata = userdata;
        mState = BufferState::Mapped;

        MapReadAsyncImpl(mMapSerial);
    }

    void BufferBase::MapWriteAsync(dawnBufferMapWriteCallback callback,
                                   dawnCallbackUserdata userdata) {
        if (GetDevice()->ConsumedError(ValidateMap(dawn::BufferUsageBit::MapWrite))) {
            callback(DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR, nullptr, 0, userdata);
            return;
        }
        ASSERT(!IsError());

        ASSERT(mMapReadCallback == nullptr);

        // TODO(cwallez@chromium.org): what to do on wraparound? Could cause crashes.
        mMapSerial++;
        mMapWriteCallback = callback;
        mMapUserdata = userdata;
        mState = BufferState::Mapped;

        MapWriteAsyncImpl(mMapSerial);
    }

    void BufferBase::Destroy() {
        if (GetDevice()->ConsumedError(ValidateDestroy())) {
            return;
        }
        ASSERT(!IsError());

        // The buffer is destroyed so we will never need to upload staging data.
        mStagingData.reset();

        if (mState == BufferState::Mapped) {
            Unmap();
        }
        mState = BufferState::Destroyed;
    }

    void BufferBase::Unmap() {
        if (GetDevice()->ConsumedError(ValidateUnmap())) {
            return;
        }
        ASSERT(!IsError());

        while (mStagingData && mCreateMappedPointer == nullptr) {
            // Buffer is initialized with staging data. If the mapping hasn't finished,
            // we need to wait. If the mapping fails, mStagingData will be reset.
            GetDevice()->Tick();
        }

        if (mCreateMappedPointer != nullptr) {
            ASSERT(mStagingData);
            memcpy(mCreateMappedPointer, mStagingData.get(), GetSize());
            mCreateMappedPointer = nullptr;
            mStagingData.reset();
        }

        // A map request can only be called once, so this will fire only if the request wasn't
        // completed before the Unmap
        CallMapReadCallback(mMapSerial, DAWN_BUFFER_MAP_ASYNC_STATUS_UNKNOWN, nullptr, 0u);
        CallMapWriteCallback(mMapSerial, DAWN_BUFFER_MAP_ASYNC_STATUS_UNKNOWN, nullptr, 0u);
        UnmapImpl();
        mState = BufferState::Unmapped;
        mMapReadCallback = nullptr;
        mMapWriteCallback = nullptr;
        mMapUserdata = 0;
    }

    MaybeError BufferBase::ValidateSetSubData(uint32_t start, uint32_t count) const {
        DAWN_TRY(GetDevice()->ValidateObject(this));

        if (mState == BufferState::Destroyed) {
            return DAWN_VALIDATION_ERROR("Buffer is destroyed");
        }

        if (mState == BufferState::Mapped) {
            return DAWN_VALIDATION_ERROR("Buffer is mapped");
        }

        if (count > GetSize()) {
            return DAWN_VALIDATION_ERROR("Buffer subdata with too much data");
        }

        // Note that no overflow can happen because we already checked for GetSize() >= count
        if (start > GetSize() - count) {
            return DAWN_VALIDATION_ERROR("Buffer subdata out of range");
        }

        if (!(mUsage & dawn::BufferUsageBit::TransferDst)) {
            return DAWN_VALIDATION_ERROR("Buffer needs the transfer dst usage bit");
        }

        return {};
    }

    MaybeError BufferBase::ValidateMap(dawn::BufferUsageBit requiredUsage) const {
        DAWN_TRY(GetDevice()->ValidateObject(this));

        if (mState == BufferState::Destroyed) {
            return DAWN_VALIDATION_ERROR("Buffer is destroyed");
        }

        if (mState == BufferState::Mapped) {
            return DAWN_VALIDATION_ERROR("Buffer already mapped");
        }

        if (!(mUsage & requiredUsage)) {
            return DAWN_VALIDATION_ERROR("Buffer needs the correct map usage bit");
        }

        return {};
    }

    MaybeError BufferBase::ValidateUnmap() const {
        DAWN_TRY(GetDevice()->ValidateObject(this));

        if ((mUsage & (dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::MapWrite)) == 0) {
            return DAWN_VALIDATION_ERROR("Buffer does not have map usage");
        }
        switch (mState) {
            case BufferState::Unmapped:
            case BufferState::Mapped:
                return {};
            case BufferState::Destroyed:
                return DAWN_VALIDATION_ERROR("Buffer is destroyed");
        }
    }

    MaybeError BufferBase::ValidateDestroy() const {
        DAWN_TRY(GetDevice()->ValidateObject(this));
        return {};
    }

}  // namespace dawn_native
