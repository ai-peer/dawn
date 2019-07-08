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
#include "dawn_wire/WireClient.h"
#include "dawn_wire/client/Client.h"

#include <stdint.h>

namespace dawn_wire { namespace client {

    class InlineMemoryTransfer : public MemoryTransfer {
        class ReadHandleImpl : public ReadHandle {
          public:
            explicit ReadHandleImpl(size_t size) : mSize(size) {
            }
            ~ReadHandleImpl() override = default;

            uint32_t SerializeCreate(void*) override {
                return 0;
            }

            std::pair<const void*, size_t> DeserializeOpen(const void* deserializePointer,
                                                           uint32_t deserializeSize) override {
                ASSERT(deserializeSize == mSize);
                ASSERT(deserializePointer != nullptr);

                mStagingData = std::unique_ptr<uint8_t[]>(new uint8_t[mSize]);
                memcpy(mStagingData.get(), deserializePointer, mSize);

                return std::make_pair(mStagingData.get(), mSize);
            }

            void Close() override {
                mStagingData.reset();
            }

          private:
            size_t mSize;
            std::unique_ptr<uint8_t[]> mStagingData;
        };

        class WriteHandleImpl : public WriteHandle {
          public:
            explicit WriteHandleImpl(size_t size) : mSize(size) {
            }
            ~WriteHandleImpl() override = default;

            uint32_t SerializeCreate(void*) override {
                return 0;
            }

            std::pair<void*, size_t> Open() override {
                mStagingData = std::unique_ptr<uint8_t[]>(new uint8_t[mSize]);
                memset(mStagingData.get(), 0, mSize);
                return std::make_pair(mStagingData.get(), mSize);
            }

            uint32_t SerializeClose(void* serializePointer) override {
                ASSERT(mSize <= UINT32_MAX);
                if (serializePointer != nullptr) {
                    ASSERT(mStagingData != nullptr);
                    memcpy(serializePointer, mStagingData.get(), mSize);
                    mStagingData.reset();
                }
                return static_cast<uint32_t>(mSize);
            }

          private:
            size_t mSize;
            std::unique_ptr<uint8_t[]> mStagingData;
        };

      public:
        InlineMemoryTransfer() {
        }
        ~InlineMemoryTransfer() override = default;

        ReadHandle* CreateReadHandle(size_t size) override {
            return new ReadHandleImpl(size);
        }

        WriteHandle* CreateWriteHandle(size_t size) override {
            return new WriteHandleImpl(size);
        }
    };

    MemoryTransfer* Client::InitializeInlineMemoryTransfer() {
        mOwnedMemoryTransfer = std::make_unique<InlineMemoryTransfer>();
        return mOwnedMemoryTransfer.get();
    }

}}  //  namespace dawn_wire::client
