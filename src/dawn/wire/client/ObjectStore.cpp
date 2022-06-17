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

#include "dawn/wire/client/ObjectStore.h"

#include <limits>

namespace dawn::wire::client {

ObjectStore::ObjectStore() {
    // ID 0 is nullptr
    mObjects.emplace_back(nullptr);
    mCurrentId = 1;
}

std::pair<ObjectHandle, std::unique_ptr<ObjectBase>*> ObjectStore::ReserveSlot() {
    // TODO comments
    if (mFreeHandles.empty()) {
        ASSERT(mCurrentId == mObjects.size());
        ObjectHandle handle = {mCurrentId++, 0};
        mObjects.emplace_back(nullptr);
        return {handle, &mObjects.back()};
    }

    ObjectHandle handle = mFreeHandles.back();
    mFreeHandles.pop_back();
    return {handle, &mObjects[handle.id]};
}

void ObjectStore::Free(ObjectBase* obj) {
    ASSERT(obj->IsInList());
    ASSERT(mObjects[obj->GetWireId()].get() == obj);
    // The wire reuses ID for objects to keep them in a packed array starting from 0.
    // To avoid issues with asynchronous server->client communication referring to an ID that's
    // already reused, each handle also has a generation that's increment by one on each reuse.
    // Avoid overflows by only reusing the ID if the increment of the generation won't overflow.
    const ObjectHandle& currentHandle = obj->GetWireHandle();
    if (DAWN_LIKELY(currentHandle.generation != std::numeric_limits<ObjectGeneration>::max())) {
        mFreeHandles.push_back({currentHandle.id, currentHandle.generation + 1});
    }
    mObjects[currentHandle.id] = nullptr;
}

ObjectBase* ObjectStore::Get(ObjectId id) const {
    if (id >= mObjects.size()) {
        return nullptr;
    }
    return mObjects[id].get();
}

}  // namespace dawn::wire::client
