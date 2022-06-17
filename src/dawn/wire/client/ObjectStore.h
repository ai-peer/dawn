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

#ifndef SRC_DAWN_WIRE_CLIENT_OBJECTSTORE_H_
#define SRC_DAWN_WIRE_CLIENT_OBJECTSTORE_H_

#include <memory>
#include <vector>

#include "dawn/wire/client/ObjectBase.h"

namespace dawn::wire::client {

class Client;

// A helper class used in Client, ObjectStore owns the association of some ObjectBase and
// ObjectHandles. The lifetime of the ObjectBase is then owned by the ObjectStore, destruction
// happening when Free is called.
//
// Since the wire has one "ID" namespace per type of object, each ObjectStore should contain a
// single type of objects. However no templates are used because Client wraps ObjectStore and is
// type-generic, so ObjectStore is type-erased to only work on ObjectBase.
class ObjectStore {
  public:
    ObjectStore();

    // ObjectBase's constructor require the ObjectHandle and child classes of ObjectBase have
    // additional constructor argument, so we can't atomically 1) reserve the slot 2) create
    // the ObjectBase and 3) insert it in mObjects (because 2) is made by the user of this class).
    // So instead 1) is done and the data for 3) is returned to the caller so it can insert the
    // constructed object in the correct place immediately.
    // AAAAnd we return a pointer to a vector storage that can be invalidated so it's not great.
    std::pair<ObjectHandle, std::unique_ptr<ObjectBase>*> ReserveSlot();
    void Free(ObjectBase* obj);
    ObjectBase* Get(ObjectId id) const;

  private:
    uint32_t mCurrentId;
    std::vector<ObjectHandle> mFreeHandles;
    std::vector<std::unique_ptr<ObjectBase>> mObjects;
};

}  // namespace dawn::wire::client

#endif  // SRC_DAWN_WIRE_CLIENT_OBJECTSTORE_H_
