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

#ifndef DAWNWIRE_CLIENT_OBJECTBASE_H_
#define DAWNWIRE_CLIENT_OBJECTBASE_H_

#include <dawn/webgpu.h>

#include "common/LinkedList.h"
#include "dawn_wire/ObjectType_autogen.h"

namespace dawn_wire { namespace client {

    class Client;

    // All non-Device objects of the client side have:
    //  - A pointer to the device to get where to serialize commands
    //  - The external reference count
    //  - An ID that is used to refer to this object when talking with the server side
    //  - A next/prev pointer. They are part of a linked list of objects of the same type.
    class ObjectBase : public LinkNode<ObjectBase> {
      public:
        ~ObjectBase() {
            RemoveFromList();
        }

        virtual void CancelCallbacksForDisconnect() {
        }

        uint32_t refcount;
        const uint32_t id;

      protected:
        ObjectBase(void* parent, uint32_t refcount, uint32_t id)
            : refcount(refcount), id(id), mParent(parent) {
        }

        void* const mParent;
    };

    template <typename Self, typename P>
    class ObjectBaseTmpl : public ObjectBase {
      public:
        using Parent = P;

        ObjectBaseTmpl(Parent* parent, uint32_t refcount_, uint32_t id_)
            : ObjectBase(parent, refcount_, id_) {
        }

        Parent* GetParent() {
            return static_cast<Parent*>(mParent);
        }

        const Parent* GetParent() const {
            return static_cast<const Parent*>(mParent);
        }

      private:
        template <typename T>
        struct FindAncestorImpl {
            static T* Call(ObjectBaseTmpl* self) {
                return self->GetParent()->template FindAncestor<T>();
            }

            static const T* Call(const ObjectBaseTmpl* self) {
                return self->GetParent()->template FindAncestor<T>();
            }
        };

        template <>
        struct FindAncestorImpl<Self> {
            static Self* Call(ObjectBaseTmpl* self) {
                return static_cast<Self*>(self);
            }

            static const Self* Call(const ObjectBaseTmpl* self) {
                return static_cast<const Self*>(self);
            }
        };

      public:
        template <typename T>
        T* FindAncestor() {
            return FindAncestorImpl<T>::Call(this);
        }

        template <typename T>
        const T* FindAncestor() const {
            return FindAncestorImpl<T>::Call(this);
        }

        Client* GetClient() {
            return FindAncestor<Client>();
        }

        const Client* GetClient() const {
            return FindAncestor<Client>();
        }
    };

}}  // namespace dawn_wire::client

#endif  // DAWNWIRE_CLIENT_OBJECTBASE_H_
