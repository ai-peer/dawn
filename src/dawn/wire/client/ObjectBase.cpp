// Copyright 2022 The Dawn Authors
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

#include "dawn/wire/client/ObjectBase.h"

#include "dawn/common/Assert.h"

namespace dawn::wire::client {

ObjectBase::ObjectBase(const ObjectBaseParams& params)
    : mClient(params.client), mHandle(params.handle), mRefcount(1) {}

ObjectBase::~ObjectBase() {
    RemoveFromList();
}

const ObjectHandle& ObjectBase::GetWireHandle() const {
    return mHandle;
}

ObjectId ObjectBase::GetWireId() const {
    return mHandle.id;
}

ObjectGeneration ObjectBase::GetWireGeneration() const {
    return mHandle.generation;
}

Client* ObjectBase::GetClient() const {
    return mClient;
}

void ObjectBase::Reference() {
    mRefcount.fetch_add(1, std::memory_order_relaxed);
}

bool ObjectBase::Release() {
    uint32_t prev = mRefcount.fetch_sub(1, std::memory_order_release);
    ASSERT(prev != 0);
    if (prev == 1) {
        std::atomic_thread_fence(std::memory_order_acquire);
        return true;
    }
    return false;
}

}  // namespace dawn::wire::client
