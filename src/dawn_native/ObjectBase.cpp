// Copyright 2018 The Dawn Authors
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

#include "dawn_native/ObjectBase.h"
#include "dawn_native/Device.h"

#include <mutex>

namespace dawn_native {

    static constexpr uint64_t kErrorPayload = 0;
    static constexpr uint64_t kNotErrorPayload = 1;

    ObjectBase::ObjectBase(DeviceBase* device, ObjectType type, const char* label)
        : RefCounted(kNotErrorPayload), mType(type), mDevice(device) {
        if (label) {
            mLabel = label;
        }
    }

    ObjectBase::ObjectBase(DeviceBase* device, ObjectType type, ErrorTag)
        : RefCounted(kErrorPayload), mType(type), mDevice(device) {
    }
    ObjectBase::ObjectBase(DeviceBase* device, ObjectType type, LabelNotImplementedTag)
        : RefCounted(kNotErrorPayload), mType(type), mDevice(device) {
    }

    const std::string& ObjectBase::GetLabel() {
        return mLabel;
    }

    DeviceBase* ObjectBase::GetDevice() const {
        return mDevice;
    }

    bool ObjectBase::IsError() const {
        return GetRefCountPayload() == kErrorPayload;
    }

    void ObjectBase::DestroyObject() {
        if (mState != State::Destroyed) {
            if (mDevice != nullptr) {
                const std::lock_guard<std::mutex> lock(mDevice->GetObjectListMutex(mType));
                RemoveFromList();
            }
            DestroyObjectImpl();
        }
        mState = State::Destroyed;
    }

    void ObjectBase::APISetLabel(const char* label) {
        mLabel = label;
        SetLabelImpl();
    }

    void ObjectBase::SetLabelImpl() {
    }

    void ObjectBase::DestroyObjectImpl() {
    }

}  // namespace dawn_native
