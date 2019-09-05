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

#include "dawn_native/metal/BufferMTL.h"

#include "common/Math.h"
#include "dawn_native/metal/DeviceMTL.h"

namespace dawn_native { namespace metal {

    Buffer::Buffer(Device* device, const BufferDescriptor* descriptor)
        : BufferBase(device, descriptor) {
        MTLResourceOptions storageMode;
        if (GetUsage() & (dawn::BufferUsage::MapRead | dawn::BufferUsage::MapWrite)) {
            storageMode = MTLResourceStorageModeShared;
        } else {
            storageMode = MTLResourceStorageModePrivate;
        }

        uint32 physicalSize = mVirtualSize = GetSize();
        if (GetUsage() & (dawn::BufferUsage::Uniform | dawn::BufferUsage::Storage)) {
            // Metal requires the size of constant buffer and storage buffer are aligned to the
            // largest base alignment of its members.
            physicalSize = Align(physicalSize, 16);
        }

        mMtlBuffer = [device->GetMTLDevice() newBufferWithLength:physicalSize options:storageMode];
    }

    Buffer::~Buffer() {
        DestroyInternal();
    }

    id<MTLBuffer> Buffer::GetMTLBuffer() const {
        return mMtlBuffer;
    }

    uint32_t Buffer::GetVirtualSize() const {
        return mVirtualSize;
    }

    void Buffer::OnMapCommandSerialFinished(uint32_t mapSerial, bool isWrite) {
        char* data = reinterpret_cast<char*>([mMtlBuffer contents]);
        if (isWrite) {
            CallMapWriteCallback(mapSerial, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, data, GetSize());
        } else {
            CallMapReadCallback(mapSerial, DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, data, GetSize());
        }
    }

    bool Buffer::IsMapWritable() const {
        // TODO(enga): Handle CPU-visible memory on UMA
        return (GetUsage() & (dawn::BufferUsage::MapRead | dawn::BufferUsage::MapWrite)) != 0;
    }

    MaybeError Buffer::MapAtCreationImpl(uint8_t** mappedPointer) {
        *mappedPointer = reinterpret_cast<uint8_t*>([mMtlBuffer contents]);
        return {};
    }

    MaybeError Buffer::MapReadAsyncImpl(uint32_t serial) {
        MapRequestTracker* tracker = ToBackend(GetDevice())->GetMapTracker();
        tracker->Track(this, serial, false);
        return {};
    }

    MaybeError Buffer::MapWriteAsyncImpl(uint32_t serial) {
        MapRequestTracker* tracker = ToBackend(GetDevice())->GetMapTracker();
        tracker->Track(this, serial, true);
        return {};
    }

    void Buffer::UnmapImpl() {
        // Nothing to do, Metal StorageModeShared buffers are always mapped.
    }

    void Buffer::DestroyImpl() {
        [mMtlBuffer release];
        mMtlBuffer = nil;
    }

    MapRequestTracker::MapRequestTracker(Device* device) : mDevice(device) {
    }

    MapRequestTracker::~MapRequestTracker() {
        ASSERT(mInflightRequests.Empty());
    }

    void MapRequestTracker::Track(Buffer* buffer,
                                  uint32_t mapSerial,
                                  bool isWrite) {
        Request request;
        request.buffer = buffer;
        request.mapSerial = mapSerial;
        request.isWrite = isWrite;

        mInflightRequests.Enqueue(std::move(request), mDevice->GetPendingCommandSerial());
    }

    void MapRequestTracker::Tick(Serial finishedSerial) {
        for (auto& request : mInflightRequests.IterateUpTo(finishedSerial)) {
            request.buffer->OnMapCommandSerialFinished(request.mapSerial, request.isWrite);
        }
        mInflightRequests.ClearUpTo(finishedSerial);
    }

}}  // namespace dawn_native::metal
