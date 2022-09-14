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

#include <mutex>

#include "dawn/native/Device.h"
#include "dawn/native/ObjectBase.h"

namespace dawn::native {

static constexpr uint64_t kErrorPayload = 0;
static constexpr uint64_t kNotErrorPayload = 1;

ErrorMonad::ErrorMonad() : RefCounted(kNotErrorPayload) {}
ErrorMonad::ErrorMonad(ErrorTag) : RefCounted(kErrorPayload) {}

bool ErrorMonad::IsError() const {
    return GetRefCountPayload() == kErrorPayload;
}

ObjectBase::ObjectBase(DeviceBase* device) : ErrorMonad(), mDevice(device) {}

ObjectBase::ObjectBase(DeviceBase* device, ErrorTag) : ErrorMonad(kError), mDevice(device) {}

DeviceBase* ObjectBase::GetDevice() const {
    return mDevice.Get();
}

void ApiObjectList::Track(ApiObjectBase* object) {
    std::lock_guard<std::mutex> lock(mMutex);
    mObjects.Prepend(object);
    if (mMarkedDestroyed) {
        object->Destroy();
    }
}

bool ApiObjectList::Untrack(ApiObjectBase* object) {
    if (mMarkedDestroyed) {
        // If we are marked destroyed, we do not need to acquire the lock because we are one of the
        // following re-entrant cases (both which would already be holding the lock):
        //   - ApiObjectList::Destroy->ApiObjectBase::Destroy->ApiObjectList::Untrack
        //   - ApiObjectList::Track->ApiObjectBase::Destroy->ApiObjectList::Untrack
        return object->RemoveFromList();
    }
    std::lock_guard<std::mutex> lock(mMutex);
    return object->RemoveFromList();
}

void ApiObjectList::Destroy() {
    std::lock_guard<std::mutex> lock(mMutex);
    mMarkedDestroyed = true;
    while (!mObjects.empty()) {
        mObjects.head()->value()->Destroy();
    }
}

ApiObjectBase::ApiObjectBase(DeviceBase* device, const char* label) : ObjectBase(device) {
    if (label) {
        mLabel = label;
    }
}

ApiObjectBase::ApiObjectBase(DeviceBase* device, ErrorTag tag) : ObjectBase(device, tag) {}

ApiObjectBase::ApiObjectBase(DeviceBase* device, LabelNotImplementedTag tag) : ObjectBase(device) {}

ApiObjectBase::~ApiObjectBase() {
    ASSERT(!IsAlive());
}

void ApiObjectBase::APISetLabel(const char* label) {
    mLabel = label;
    SetLabelImpl();
}

const std::string& ApiObjectBase::GetLabel() const {
    return mLabel;
}

void ApiObjectBase::SetLabelImpl() {}

bool ApiObjectBase::IsAlive() const {
    return IsInList();
}

void ApiObjectBase::DeleteThis() {
    Destroy();
    RefCounted::DeleteThis();
}

ApiObjectList* ApiObjectBase::GetObjectTrackingList() {
    ASSERT(GetDevice() != nullptr);
    return GetDevice()->GetObjectTrackingList(GetType());
}

void ApiObjectBase::Destroy() {
    if (!IsAlive()) {
        return;
    }
    ApiObjectList* list = GetObjectTrackingList();
    ASSERT(list != nullptr);
    if (list->Untrack(this)) {
        DestroyImpl();
    }
}

}  // namespace dawn::native
