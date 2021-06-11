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

#include "common/Alloc.h"
#include "common/Assert.h"
#include "dawn_wire/WireClient.h"
#include "dawn_wire/client/Client.h"

#include <cstring>

namespace dawn_wire { namespace client {

    class InlineMemoryTransferService : public MemoryTransferService {
        class ReadHandleImpl : public ReadHandle {
          public:
            explicit ReadHandleImpl(size_t size) {
                mStagingData = std::unique_ptr<uint8_t[]>(AllocNoThrow<uint8_t>(size));
            }

            ~ReadHandleImpl() override = default;

            size_t SerializeCreateSize() override {
                return 0;
            }

            void SerializeCreate(void*) override {
            }

            bool UpdateMapData(const void* deserializePointer,
                               size_t deserializeSize,
                               size_t size,
                               size_t offset,
                               const void** data,
                               size_t* dataLength) override {
                if (deserializeSize != size || deserializePointer == nullptr) {
                    return false;
                }

                if (!mStagingData) {
                    return false;
                }
                void* start = static_cast<uint8_t*>(mStagingData.get()) + offset;
                memcpy(start, deserializePointer, size);

                ASSERT(data != nullptr);
                ASSERT(dataLength != nullptr);
                *data = start;
                *dataLength = size;

                return true;
            }

          private:
            std::unique_ptr<uint8_t[]> mStagingData;
        };

        class WriteHandleImpl : public WriteHandle {
          public:
            explicit WriteHandleImpl(size_t size) : mSize(size), mOffset(0) {
                mStagingData = std::unique_ptr<uint8_t[]>(AllocNoThrow<uint8_t>(size));
            }

            ~WriteHandleImpl() override = default;

            size_t SerializeCreateSize() override {
                return 0;
            }

            void SerializeCreate(void*) override {
            }

            std::pair<void*, size_t> GetMapData(size_t size, size_t offset) override {
                if (!mStagingData) {
                    return std::make_pair(nullptr, 0);
                }
                mSize = size;
                mOffset = offset;
                void* start = static_cast<uint8_t*>(mStagingData.get()) + offset;
                memset(start, 0, size);
                return std::make_pair(start, size);
            }

            size_t SerializeFlushSize() override {
                return mSize;
            }

            void SerializeFlush(void* serializePointer) override {
                ASSERT(mStagingData != nullptr);
                ASSERT(serializePointer != nullptr);
                memcpy(serializePointer, static_cast<uint8_t*>(mStagingData.get()) + mOffset,
                       mSize);
            }

          private:
            size_t mSize;
            size_t mOffset;
            std::unique_ptr<uint8_t[]> mStagingData;
        };

      public:
        InlineMemoryTransferService() {
        }
        ~InlineMemoryTransferService() override = default;

        ReadHandle* CreateReadHandle(size_t size) override {
            return new ReadHandleImpl(size);
        }

        WriteHandle* CreateWriteHandle(size_t size) override {
            return new WriteHandleImpl(size);
        }
    };

    std::unique_ptr<MemoryTransferService> CreateInlineMemoryTransferService() {
        return std::make_unique<InlineMemoryTransferService>();
    }

}}  //  namespace dawn_wire::client
