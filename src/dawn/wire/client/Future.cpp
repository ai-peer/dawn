// Copyright 2023 The Dawn Authors
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

#include "dawn/wire/client/Future.h"

#include <set>

#include "dawn/wire/client/Client.h"

namespace dawn::wire::client {

WGPUWaitStatus ClientFuturesWaitAny(size_t* futureCount, WGPUFuture* futures, uint64_t timeout) {
    std::set<Client*> wireClients;
    for (size_t i = 0; i < *futureCount; ++i) {
        Client* wireClient = reinterpret_cast<Future*>(futures[i])->GetClient();
        wireClients.insert(wireClient);
    }
    for (Client* wireClient : wireClients) {
        wireClient->Flush();
        // FIXME: wait s2c somehow??
    }

    WGPUFuture* middlePtr = std::partition(futures, &futures[*futureCount], [](WGPUFuture future) {
        // Pending futures first, then Ready/Observed futures after.
        return reinterpret_cast<Future*>(future)->IsReady();
    });
    ASSERT(middlePtr >= futures);
    static_assert(sizeof(WGPUFuture) <= sizeof(size_t));
    size_t newCount = size_t(middlePtr - futures) / sizeof(WGPUFuture);
    WGPUWaitStatus ret =
        newCount == *futureCount ? WGPUWaitStatus_TimedOut : WGPUWaitStatus_SomeCompleted;
    *futureCount = newCount;
    return ret;
}

void ClientFuturesGetEarliestFds(size_t count, WGPUFuture const* futures, int* fds) {
    UNREACHABLE();  // FIXME
}

Future::Future(const ObjectBaseParams& params) : ObjectBase(params) {}

bool Future::IsReady() const {
    return mReady;
}

void Future::MakeReady() {
    mReady = true;
}

}  // namespace dawn::wire::client
