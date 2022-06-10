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

#ifndef SRC_DAWN_WIRE_CLIENT_OBJECTALLOCATOR_H_
#define SRC_DAWN_WIRE_CLIENT_OBJECTALLOCATOR_H_

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "dawn/common/Assert.h"
#include "dawn/common/Compiler.h"
#include "dawn/wire/WireCmd_autogen.h"
#include "dawn/wire/client/ObjectBase.h"

namespace dawn::wire::client {

template <typename T>
class ObjectAllocator {
  public:
    ObjectAllocator() {
        // ID 0 is nullptr
        mObjects.emplace_back(nullptr);
    }

    template <typename Client>
    T* New(Client* client) {
        FreeSlot slot = GetNewId();
        ObjectBaseParams params = {client, slot.id, slot.generation};
        auto object = std::make_unique<T>(params);
        client->TrackObject(object.get());

        if (slot.id >= mObjects.size()) {
            ASSERT(slot.id == mObjects.size());
            mObjects.emplace_back(std::move(object));
        } else {
            // The generation should never overflow. We don't recycle ObjectIds that would
            // overflow their next generation.
            ASSERT(slot.generation != 0);
            ASSERT(mObjects[slot.id] == nullptr);
            mObjects[slot.id] = std::move(object);
        }

        return mObjects[slot.id].get();
    }
    void Free(T* obj) {
        ASSERT(obj->IsInList());
        if (DAWN_LIKELY(obj->generation != std::numeric_limits<uint32_t>::max())) {
            // Only recycle this ObjectId if the generation won't overflow on the next
            // allocation.
            FreeId(obj->id, obj->generation + 1);
        }
        mObjects[obj->id] = nullptr;
    }

    T* GetObject(uint32_t id) {
        if (id >= mObjects.size()) {
            return nullptr;
        }
        return mObjects[id].get();
    }

  private:
    struct FreeSlot {
        uint32_t id;
        uint32_t generation;
    };

    FreeSlot GetNewId() {
        if (mFreeSlots.empty()) {
            return {mCurrentId++, 0};
        }
        FreeSlot slot = mFreeSlots.back();
        mFreeSlots.pop_back();
        return slot;
    }
    void FreeId(uint32_t id, uint32_t generation) { mFreeSlots.push_back({id, generation}); }

    // 0 is an ID reserved to represent nullptr
    uint32_t mCurrentId = 1;
    std::vector<FreeSlot> mFreeSlots;
    std::vector<std::unique_ptr<T>> mObjects;
};
}  // namespace dawn::wire::client

#endif  // SRC_DAWN_WIRE_CLIENT_OBJECTALLOCATOR_H_
