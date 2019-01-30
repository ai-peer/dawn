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

#include "dawn_wire/Client.h"
#include "dawn_wire/Server.h"
#include "dawn_wire/client/Client.h"
#include "dawn_wire/server/Server.h"

namespace dawn_wire {

    Client::Client(CommandSerializer* serializer) : mImpl(new client::Client(serializer)) {
    }

    Client::~Client() {
        mImpl.reset();
    }

    dawnDevice Client::GetDevice() const {
        return mImpl->GetDevice();
    }

    dawnProcTable Client::GetProcs() const {
        return client::GetProcs();
    }

    const char* Client::HandleCommands(const char* commands, size_t size) {
        return mImpl->HandleCommands(commands, size);
    }

    Server::Server(dawnDevice device, const dawnProcTable& procs, CommandSerializer* serializer)
        : mImpl(new server::Server(device, procs, serializer)) {
    }

    Server::~Server() {
        mImpl.reset();
    }

    const char* Server::HandleCommands(const char* commands, size_t size) {
        return mImpl->HandleCommands(commands, size);
    }

}  // namespace dawn_wire
