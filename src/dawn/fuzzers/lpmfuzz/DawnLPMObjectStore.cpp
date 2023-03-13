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

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

#include "dawn/fuzzers/lpmfuzz/DawnLPMConstants_autogen.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMObjectStore.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMSerializer_autogen.h"
#include "dawn/wire/ObjectHandle.h"

namespace dawn::wire {

DawnLPMObjectStore::DawnLPMObjectStore() {
    mObjects.emplace_back(DawnLPMFuzzer::invalid_object_id);
    mCurrentId = 1;
}

ObjectHandle DawnLPMObjectStore::ReserveHandle() {
    if (mFreeHandles.empty()) {
        Insert(mCurrentId);
        return {mCurrentId++, 0};
    }
    ObjectHandle handle = mFreeHandles.back();
    mFreeHandles.pop_back();
    Insert(handle.id);
    return handle;
}

void DawnLPMObjectStore::Insert(ObjectId id) {
    std::vector<ObjectId>::iterator iter =
        std::lower_bound(mObjects.begin(), mObjects.end(), id, std::greater<ObjectId>());
    mObjects.insert(iter, id);
}

void DawnLPMObjectStore::Free(ObjectId id) {
    ASSERT(mObjects.size() >= 1);

    if (id == DawnLPMFuzzer::invalid_object_id) {
        return;
    }

    for (size_t i = 0; i < mObjects.size(); i++) {
        if (mObjects[i] == id) {
            mFreeHandles.push_back({id, 0});
            mObjects.erase(mObjects.begin() + i);
        }
    }
}

ObjectId DawnLPMObjectStore::Get(ObjectId id) const {
    ASSERT(mObjects.size() >= 1);

    // CreateBindGroup relies on sending invalid object ids
    if (id == DawnLPMFuzzer::invalid_object_id) {
        return DawnLPMFuzzer::invalid_object_id;
    }

    if (mObjects.size() == 1) {
        return DawnLPMFuzzer::invalid_object_id;
    }

    auto iter = std::lower_bound(mObjects.begin(), mObjects.end(), id);
    if (iter != mObjects.end()) {
        return *iter;
    }

    // Wrap to 0
    iter = std::lower_bound(mObjects.begin(), mObjects.end(), 0);
    if (iter != mObjects.end()) {
        return *iter;
    }

    return DawnLPMFuzzer::invalid_object_id;
}

size_t DawnLPMObjectStore::size() const {
    return mObjects.size();
}

}  // namespace dawn::wire
