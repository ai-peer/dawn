// Copyright 2020 The Dawn Authors
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

#ifndef DAWNNATIVE_OBJECTHANDLE_H_
#define DAWNNATIVE_OBJECTHANDLE_H_

#include "common/PlacementAllocated.h"
#include "dawn_native/ObjectBase.h"

#include <memory>

namespace dawn_native {

    class ObjectHandleBase : public ObjectBase, public PlacementAllocated {
      public:
        static void* Allocate(DeviceBase* device);

        ObjectHandleBase(DeviceBase* device, void* storage);
        ObjectHandleBase(DeviceBase* device, ErrorTag tag);
        ~ObjectHandleBase() override;

      protected:
        void* mStorage;

      private:
        friend class ObjectHandlePool;
        using ObjectBase::ObjectBase;

        void Free();

        ObjectHandleBase* mNextHandle;
    };

    template <typename T>
    class ObjectHandle : public ObjectHandleBase {

      protected:
        using ObjectHandleBase::ObjectHandleBase;

        T* GetStorage() {
            return static_cast<T*>(mStorage);
        }

        const T* GetStorage() const {
            return static_cast<const T*>(mStorage);
        }
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_OBJECTHANDLE_H_
