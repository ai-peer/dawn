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

#include "dawn_wire/server/ServerMockMemoryTransferService.h"

namespace dawn_wire { namespace server {

    MockMemoryTransferService::MockReadHandle::MockReadHandle(MockMemoryTransferService* service)
        : ReadHandle(), mService(service) {
    }

    MockMemoryTransferService::MockReadHandle::~MockReadHandle() {
        mService->OnReadHandleDestroy(this);
    }

    size_t MockMemoryTransferService::MockReadHandle::SerializeInitialData(const void* data,
                                                                           size_t dataLength,
                                                                           void* serializePointer) {
        return mService->OnReadHandleSerializeInitialData(this, data, dataLength, serializePointer);
    }

    MockMemoryTransferService::MockWriteHandle::MockWriteHandle(MockMemoryTransferService* service)
        : WriteHandle(), mService(service) {
    }

    MockMemoryTransferService::MockWriteHandle::~MockWriteHandle() {
        mService->OnWriteHandleDestroy(this);
    }

    bool MockMemoryTransferService::MockWriteHandle::DeserializeFlush(
        const void* deserializePointer,
        size_t deserializeSize) {
        return mService->OnWriteHandleDeserializeFlush(this, deserializePointer, deserializeSize);
    }

    MockMemoryTransferService::MockMemoryTransferService() = default;
    MockMemoryTransferService::~MockMemoryTransferService() = default;

    bool MockMemoryTransferService::DeserializeReadHandle(const void* deserializePointer,
                                                          size_t deserializeSize,
                                                          ReadHandle** readHandle) {
        return OnDeserializeReadHandle(deserializePointer, deserializeSize, readHandle);
    }

    bool MockMemoryTransferService::DeserializeWriteHandle(const void* deserializePointer,
                                                           size_t deserializeSize,
                                                           WriteHandle** writeHandle) {
        return OnDeserializeWriteHandle(deserializePointer, deserializeSize, writeHandle);
    }

    MemoryTransferService::ReadHandle* MockMemoryTransferService::NewReadHandle() {
        return new MockReadHandle(this);
    }

    MemoryTransferService::WriteHandle* MockMemoryTransferService::NewWriteHandle() {
        return new MockWriteHandle(this);
    }

}}  //  namespace dawn_wire::server
