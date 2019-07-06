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

#ifndef DAWNWIRE_WIRESERVER_H_
#define DAWNWIRE_WIRESERVER_H_

#include <memory>

#include "dawn_wire/Wire.h"

namespace dawn_wire {

    namespace server {
        class Server;

        class DAWN_WIRE_EXPORT MemoryTransfer {
          public:
            class ReadHandle;
            class WriteHandle;

            virtual ~MemoryTransfer() = default;

            // Deserialize data to create Read/Write handles. These handles are for the client
            // to Read/Write data.
            virtual ReadHandle* DeserializeReadHandle(const void* deserializePointer,
                                                      uint32_t size) = 0;
            virtual WriteHandle* DeserializeWriteHandle(const void* deserializePointer,
                                                        uint32_t size) = 0;

            class ReadHandle {
              public:
                // Initialize the handle data.
                // Serialize into |serializePointer| so the client can update handle data.
                // If |serializePointer| is nullptr, this returns the required serialization space.
                virtual uint32_t SerializeInitialize(const void* data,
                                                     size_t dataLength,
                                                     void* serializePointer = nullptr) = 0;

                virtual void Close() = 0;
                virtual ~ReadHandle() = default;
            };

            class WriteHandle {
              public:
                // Set the target for writes from the client. DeserializeClose should copy data
                // into the target.
                virtual void SetTarget(void* data, size_t dataLength) = 0;

                // This function takes in the serialized result of
                // client::MemoryTransfer::WriteHandle::SerializeClose.
                virtual bool DeserializeClose(const void* deserializePointer, uint32_t size) = 0;
                virtual ~WriteHandle() = default;
            };
        };
    }

    class DAWN_WIRE_EXPORT WireServer : public CommandHandler {
      public:
        WireServer(DawnDevice device,
                   const DawnProcTable& procs,
                   CommandSerializer* serializer,
                   server::MemoryTransfer* memoryTransfer = nullptr);
        ~WireServer();

        const char* HandleCommands(const char* commands, size_t size) override final;

        bool InjectTexture(DawnTexture texture, uint32_t id, uint32_t generation);

      private:
        std::unique_ptr<server::Server> mImpl;
    };

}  // namespace dawn_wire

#endif  // DAWNWIRE_WIRESERVER_H_
