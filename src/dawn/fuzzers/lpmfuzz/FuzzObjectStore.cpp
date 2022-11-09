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

#include <limits>
#include <utility>

#include "dawn/fuzzers/lpmfuzz/DawnLPMConstants_gen.h"
#include "dawn/fuzzers/lpmfuzz/FuzzObjectStore.h"
#include "dawn/wire/client/ObjectBase.h"

namespace dawn::wire::client {

FuzzObjectStore::FuzzObjectStore() {
    // ObjectId 0 is no object
    mObjects.emplace_back(0);
    mCurrentId = 1;
}

ObjectHandle FuzzObjectStore::ReserveHandle() {
    if (mFreeHandles.empty()) {
        mObjects.emplace_back(mCurrentId);
        return {mCurrentId++, 0};
    }
    ObjectHandle handle = mFreeHandles.back();
    mFreeHandles.pop_back();
    mObjects.emplace_back(handle.id);
    return handle;
}

void FuzzObjectStore::Free(ObjectId id) {
    ObjectId real_id = static_cast<ObjectId>(id + 1);
    for (auto handle : mFreeHandles) {
        if (real_id == handle.id) {
            mFreeHandles.push_back({handle.id, handle.generation + 1});
            mObjects[handle.id] = INVALID_OBJECTID;
        }
    }
}

ObjectId FuzzObjectStore::Get(ObjectId id) const {
    ObjectId real_id = static_cast<ObjectId>(id + 1);

    if (mObjects.size() == 0) {
        return INVALID_OBJECTID;
    }

    return mObjects[real_id % mObjects.size()];
}

size_t FuzzObjectStore::size() const {
    return mObjects.size();
}

}  // namespace dawn::wire::client
