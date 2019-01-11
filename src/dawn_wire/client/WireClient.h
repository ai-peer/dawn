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

#ifndef DAWNWIRE_CLIENT_WIRECLIENT_H_
#define DAWNWIRE_CLIENT_WIRECLIENT_H_

#include <dawn_wire/Client.h>

#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/WireDeserializeAllocator.h"
#include "dawn_wire/client/WireClientBase_autogen.h"

namespace dawn_wire {
    namespace client {

        class WireClient : public WireClientBase<WireClient> {
          public:
            WireClient(dawnProcTable* procs, dawnDevice* device, CommandSerializer* serializer);
            virtual ~WireClient();

            void* GetCmdSpace(size_t size) {
                return mSerializer->GetCmdSpace(size);
            }
            const char* HandleCommands(const char* commands, size_t size);

          private:
#include "dawn_wire/client/WireClientPrototypes_autogen.inl"

            Device* mDevice;
            CommandSerializer* mSerializer;
            WireDeserializeAllocator mAllocator;
        };

        dawnProcTable GetProcs();
    }  // namespace client

    class Client::Impl : public client::WireClient {
      public:
        Impl(dawnProcTable* procs, dawnDevice* device, CommandSerializer* serializer)
            : client::WireClient(procs, device, serializer) {
        }
    };
}  // namespace dawn_wire

#endif  // DAWNWIRE_CLIENT_WIRECLIENT_H_
