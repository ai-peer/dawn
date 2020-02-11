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

#include "dawn_native/ObjectHandle.h"

#include "common/Assert.h"
#include "dawn_native/Device.h"

namespace dawn_native {

    // static
    void* ObjectHandleBase::Allocate(DeviceBase* device) {
        if (ObjectHandleBase* handle = device->GetObjectHandlePool()->Pop()) {
            return handle;
        }
        return ::operator new(sizeof(ObjectHandleBase));
    }

    ObjectHandleBase::ObjectHandleBase(DeviceBase* device, void* storage)
        : ObjectBase(device), mStorage(storage), mNextHandle(nullptr) {
    }

    ObjectHandleBase::ObjectHandleBase(DeviceBase* device, ErrorTag tag)
        : ObjectBase(device, tag), mStorage(nullptr), mNextHandle(nullptr) {
    }

    ObjectHandleBase::~ObjectHandleBase() {
        GetDevice()->GetObjectHandlePool()->Push(this);
    }

    void ObjectHandleBase::Free() {
        ASSERT(mStorage == nullptr);
        ::operator delete(this);
    }

}  // namespace dawn_native
