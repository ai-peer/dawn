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

#ifndef DAWNWIRE_CLIENT_OBJECTALLOCATOR_H_
#define DAWNWIRE_CLIENT_OBJECTALLOCATOR_H_

#include "common/Assert.h"
#include "dawn_wire/client/ObjectAllocatorBase.h"

#include <memory>
#include <vector>

namespace dawn_wire { namespace client {

    template <typename T>
    class ObjectAllocator : public ObjectAllocatorBase {
        using ObjectOwner =
            typename std::conditional<std::is_same<T, Device>::value, Client, Device>::type;

      static constexpr uint32_t kNeedsDestroyFlag = 0x8000'0000;
      static constexpr uint32_t kMaxSerial = 0x7FFF'FFFF;

      public:
        struct ObjectAndSerial {
            ObjectAndSerial(std::unique_ptr<T> object, uint32_t serial)
                : object(std::move(object)), serial(serial), needsDestroy(0) {
            }
            std::unique_ptr<T> object;
            unsigned serial : 31;
            unsigned needsDestroy : 1;
        };

        ObjectAllocator(ClientBase* client) : ObjectAllocatorBase(client) {
            // ID 0 is nullptr
            mObjects.emplace_back(nullptr, 0);
        }

        ObjectAndSerial* New(ObjectOwner* owner, ObjectHandle* handle) {
            uint32_t id = GetNewId();
            T* result = new T(owner, 1, id);
            auto object = std::unique_ptr<T>(result);

            if (id >= mObjects.size()) {
                ASSERT(id == mObjects.size());
                *handle = ObjectHandle{id, 0};
                mObjects.emplace_back(std::move(object), 0);
            } else {
                ASSERT(mObjects[id].object == nullptr);
                // TODO(cwallez@chromium.org): investigate if overflows could cause bad things to
                // happen

                // Include the needsDestroy flag in the serial. It will be sent to the server.
                uint32_t serial = ++mObjects[id].serial;
                if (mObjects[id].needsDestroy) {
                    serial |= 0x8000'0000;
                }

                *handle = ObjectHandle{id, serial};
                // Now, clear the flag.
                mObjects[id].needsDestroy = 0;
                mObjects[id].object = std::move(object);
            }

            return &mObjects[id];
        }
        void Free(T* obj) {
            if (mObjects[obj->id].serial < kMaxSerial) {
                FreeId(obj->id);
            }

            ASSERT(!mObjects[obj->id].needsDestroy);
            mObjects[obj->id].needsDestroy = 1;

            EnqueueDestroy(GetObjectType(obj), obj->id);

            mObjects[obj->id].object = nullptr;
        }

        bool AcquireNeedsDestroy(uint32_t id) {
            bool needsDestroy = mObjects[id].needsDestroy != 0;
            mObjects[id].needsDestroy = 0;
            return needsDestroy;
        }

        T* GetObject(uint32_t id) {
            if (id >= mObjects.size()) {
                return nullptr;
            }
            return mObjects[id].object.get();
        }

        uint32_t GetSerial(uint32_t id) {
            if (id >= mObjects.size()) {
                return 0;
            }
            return mObjects[id].serial;
        }

      private:
        std::vector<ObjectAndSerial> mObjects;
    };

}}  // namespace dawn_wire::client

#endif  // DAWNWIRE_CLIENT_OBJECTALLOCATOR_H_
