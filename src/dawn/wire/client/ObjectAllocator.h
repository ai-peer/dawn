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
#include "dawn/wire/ObjectType_autogen.h"
#include "dawn/wire/client/ApiObjects_autogen.h"
#include "dawn/wire/client/ObjectBase.h"

namespace dawn::wire::client {

class Client;

class ObjectBaseStore {
  public:
    ObjectBaseStore();

    ObjectHandle ReserveHandle();
    void Insert(std::unique_ptr<ObjectBase> obj);
    void Free(ObjectBase* obj);
    ObjectBase* Get(ObjectId id) const;

  private:
    // 0 is an ID reserved to represent nullptr
    uint32_t mCurrentId = 1;
    std::vector<ObjectHandle> mFreeHandles;
    std::vector<std::unique_ptr<ObjectBase>> mObjects;
};

class ObjectAllocator {
  public:
    ObjectAllocator(Client* client);

    template <typename T, typename... Args>
    T* Make(Args&&... args) {
        constexpr ObjectType type = ObjectTypeToTypeEnum<T>::value;
        ObjectBaseParams params = {mClient, mPerTypeStores[type].ReserveHandle()};

        auto objectOwned = std::make_unique<T>(params, std::forward<Args>(args)...);
        T* object = objectOwned.get();

        Track(std::move(objectOwned), type);
        return object;
    }

    template <typename T>
    void Free(T* obj) {
        Free(obj, ObjectTypeToTypeEnum<T>::value);
    }
    void Free(ObjectBase* obj, ObjectType type);

    template <typename T>
    T* Get(ObjectId id) {
        return static_cast<T*>(mPerTypeStores[ObjectTypeToTypeEnum<T>::value].Get(id));
    }

  private:
    void Track(std::unique_ptr<ObjectBase> obj, ObjectType type);

    dawn::wire::client::Client* mClient;
    PerObjectType<ObjectBaseStore> mPerTypeStores;
};

}  // namespace dawn::wire::client

#endif  // SRC_DAWN_WIRE_CLIENT_OBJECTALLOCATOR_H_
