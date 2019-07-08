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
#include "dawn_wire/WireServer.h"
#include "dawn_wire/server/Server.h"

#include <stdint.h>

namespace dawn_wire { namespace server {

    class InlineMemoryTransfer : public MemoryTransfer {
      public:
        class ReadHandleImpl : public ReadHandle {
          public:
            ReadHandleImpl() {
            }
            ~ReadHandleImpl() override = default;

            uint32_t SerializeInitialize(const void* data,
                                         size_t dataLength,
                                         void* serializePointer) override {
                ASSERT(dataLength <= UINT32_MAX);
                if (serializePointer != nullptr && dataLength > 0) {
                    ASSERT(data != nullptr);
                    memcpy(serializePointer, data, dataLength);
                }
                return static_cast<uint32_t>(dataLength);
            }

            void Close() override {
            }
        };

        class WriteHandleImpl : public WriteHandle {
          public:
            WriteHandleImpl() {
            }
            ~WriteHandleImpl() override = default;

            void SetTarget(void* data, size_t dataLength) override {
                mTargetData = data;
                mDataLength = dataLength;
            }

            bool DeserializeClose(const void* deserializePointer,
                                  uint32_t deserializeSize) override {
                if (deserializeSize != mDataLength || mTargetData == nullptr ||
                    deserializePointer == nullptr) {
                    return false;
                }
                memcpy(mTargetData, deserializePointer, mDataLength);
                return true;
            }

          private:
            void* mTargetData = nullptr;
            size_t mDataLength = 0;
        };

        InlineMemoryTransfer() {
        }
        ~InlineMemoryTransfer() override = default;

        ReadHandle* DeserializeReadHandle(const void* ptr, uint32_t size) override {
            ASSERT(size == 0);
            return new ReadHandleImpl();
        }

        WriteHandle* DeserializeWriteHandle(const void* ptr, uint32_t size) override {
            ASSERT(size == 0);
            return new WriteHandleImpl();
        }
    };

    MemoryTransfer* Server::InitializeInlineMemoryTransfer() {
        mOwnedMemoryTransfer = std::make_unique<InlineMemoryTransfer>();
        return mOwnedMemoryTransfer.get();
    }

}}  //  namespace dawn_wire::server
