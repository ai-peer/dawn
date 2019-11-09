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

#include "dawn_wire/client/ObjectAllocatorBase.h"

#include "dawn_wire/client/Client.h"

namespace dawn_wire { namespace client {

    ObjectAllocatorBase::ObjectAllocatorBase(ClientBase* client)
        : mClient(static_cast<Client*>(client)) {
    }

    uint32_t ObjectAllocatorBase::GetNewId() {
        if (mFreeIds.empty()) {
            return mCurrentId++;
        }
        uint32_t id = mFreeIds.back();
        mFreeIds.pop_back();
        return id;
    }

    void ObjectAllocatorBase::FreeId(uint32_t id) {
        mFreeIds.push_back(id);
    }

    void ObjectAllocatorBase::EnqueueDestroy(ObjectType objectType, uint32_t id) {
        mClient->EnqueueDestroy(objectType, id);
    }

}}  // namespace dawn_wire::client
