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

#include "absl/strings/str_format.h"

namespace dawn_native {

    static constexpr uint64_t kErrorPayload = 0;
    static constexpr uint64_t kNotErrorPayload = 1;

    ObjectBase::ObjectBase(DeviceBase* device, const char* label)
        : RefCounted(kNotErrorPayload), mDevice(device) {
        if (label) {
            mLabel = label;
        }
    }

    ObjectBase::ObjectBase(DeviceBase* device, ErrorTag)
        : RefCounted(kErrorPayload), mDevice(device) {
    }
    ObjectBase::ObjectBase(DeviceBase* device, LabelNotImplementedTag)
        : RefCounted(kNotErrorPayload), mDevice(device) {
    }

    const std::string& ObjectBase::GetLabel() const {
        return mLabel;
    }

    std::string ObjectBase::ValidationLabel() const {
        if (mLabel.empty()) {
            // TODO: In cases where there is no label given it can still be useful to give some sort
            // of unique identifier so that objects can be cross-referenced between multiple
            // validation messages. The pointer address is maybe not the best choice for this.
            return absl::StrFormat("%s %p", ObjectTypeName(), this);
        }
        return absl::StrFormat("%s \"%s\"", ObjectTypeName(), mLabel);
    }

    DeviceBase* ObjectBase::GetDevice() const {
        return mDevice;
    }

    bool ObjectBase::IsError() const {
        return GetRefCountPayload() == kErrorPayload;
    }

    void ObjectBase::APISetLabel(const char* label) {
        mLabel = label;
        SetLabelImpl();
    }

    void ObjectBase::SetLabelImpl() {
    }

    std::string ObjectBase::ObjectTypeName() const {
        return "Object";
    }

}  // namespace dawn_native
