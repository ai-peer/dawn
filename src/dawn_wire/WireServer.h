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

#ifndef DAWNWIRE_SERVER_WIRESERVER_H_
#define DAWNWIRE_SERVER_WIRESERVER_H_

#include <dawn_wire/Server.h>

#include "dawn_wire/WireDeserializeAllocator.h"
#include "dawn_wire/WireServerBase_autogen.h"

namespace dawn_wire {
    namespace server {
        class WireServer;

        struct MapUserdata {
            WireServer* server;
            ObjectHandle buffer;
            uint32_t requestSerial;
            uint32_t size;
            bool isWrite;
        };

        struct FenceCompletionUserdata {
            WireServer* server;
            ObjectHandle fence;
            uint64_t value;
        };

        class WireServer : public WireServerBase {
          public:
            WireServer(dawnDevice device,
                       const dawnProcTable& procs,
                       CommandSerializer* serializer);
            const char* HandleCommands(const char* commands, size_t size);
            virtual ~WireServer();

          private:
            void* GetCmdSpace(size_t size) {
                return mSerializer->GetCmdSpace(size);
            }

            static void ForwardDeviceErrorToClient(const char* message,
                                                   dawnCallbackUserdata userdata);
            static void ForwardBufferMapReadAsync(dawnBufferMapAsyncStatus status,
                                                  const void* ptr,
                                                  dawnCallbackUserdata userdata);
            static void ForwardBufferMapWriteAsync(dawnBufferMapAsyncStatus status,
                                                   void* ptr,
                                                   dawnCallbackUserdata userdata);
            static void ForwardFenceCompletedValue(dawnFenceCompletionStatus status,
                                                   dawnCallbackUserdata userdata);

            void OnDeviceError(const char* message);
            void OnMapReadAsyncCallback(dawnBufferMapAsyncStatus status,
                                        const void* ptr,
                                        MapUserdata* data);
            void OnMapWriteAsyncCallback(dawnBufferMapAsyncStatus status,
                                         void* ptr,
                                         MapUserdata* data);
            void OnFenceCompletedValueUpdated(FenceCompletionUserdata* data);

#include "dawn_wire/WireServerPrototypes_autogen.inl"

            CommandSerializer* mSerializer = nullptr;
            WireDeserializeAllocator mAllocator;
            dawnProcTable mProcs;
        };
    }  // namespace server

    class Server::Impl : public server::WireServer {
      public:
        Impl(dawnDevice device, const dawnProcTable& procs, CommandSerializer* serializer)
            : server::WireServer(device, procs, serializer) {
        }
    };
}  // namespace dawn_wire

#endif  // DAWNWIRE_SERVER_WIRESERVER_H_
