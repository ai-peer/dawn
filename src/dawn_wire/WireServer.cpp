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

#include "dawn_wire/WireServer.h"

namespace dawn_wire {

    Server::Server(dawnDevice device, const dawnProcTable& procs, CommandSerializer* serializer)
        : mImpl(new Impl(device, procs, serializer)) {
    }

    Server::~Server() {
        mImpl.reset();
    }

    const char* Server::HandleCommands(const char* commands, size_t size) {
        return mImpl->HandleCommands(commands, size);
    }

    namespace server {

        WireServer::WireServer(dawnDevice device,
                               const dawnProcTable& procs,
                               CommandSerializer* serializer)
            : WireServerBase(procs), mSerializer(serializer), mProcs(procs) {
            // The client-server knowledge is bootstrapped with device 1.
            auto* deviceData = DeviceObjects().Allocate(1);
            deviceData->handle = device;
            deviceData->valid = true;

            auto userdata = static_cast<dawnCallbackUserdata>(reinterpret_cast<intptr_t>(this));
            procs.deviceSetErrorCallback(device, ForwardDeviceErrorToClient, userdata);
        }

        WireServer::~WireServer() {
        }

        bool WireServer::PreHandleBufferUnmap(const BufferUnmapCmd& cmd) {
            auto* selfData = BufferObjects().Get(cmd.selfId);
            ASSERT(selfData != nullptr);

            selfData->mappedData = nullptr;

            return true;
        }
    }  // namespace server
}  // namespace dawn_wire
