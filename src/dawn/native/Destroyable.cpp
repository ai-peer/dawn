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

#include "dawn/native/Destroyable.h"

namespace dawn::native::detail {

DestroyableBase::DestroyableBase(OwnsBase* own) : mOwn(own) {}

void DestroyableBase::Destroy() {
    DAWN_DEBUG() << "DestroyableBase::Destroy";
    if (mOwn) {
        DAWN_DEBUG() << "Has owner";
        const std::lock_guard<std::mutex> lock(mOwn->mMutex);
        DAWN_DEBUG() << "Got lock";
        if (IsAlive() && RemoveFromList()) {
            DestroyImpl();
        }
        mOwn = nullptr;
    } else {
        DAWN_DEBUG() << "No owner";
        if (IsAlive() && RemoveFromList()) {
            DestroyImpl();
        }
    }
}

OwnerBase::OwnerBase(OwnsBase* own) : DestroyableBase(own) {
    InsertBefore(mSelf.head());
}

void OwnerBase::Register(detail::OwnsBase* owner) {
    DAWN_DEBUG() << "Registering a Owns";
    std::lock_guard<std::mutex> lock(mMutex);
    owner->InsertBefore(mOwns.head());
}

void OwnerBase::DestroyImpl() {
    DAWN_DEBUG() << "OwnerBase::DestroyImpl";
    // Aggregate all children into a single list to avoid locking the same mutex multiple times.
    LinkedList<detail::DestroyableBase> children;
    for (LinkNode<detail::OwnsBase>* own : mOwns) {
        DAWN_DEBUG() << "Own list loop";
        const std::lock_guard<std::mutex> lock(own->value()->mMutex);
        own->value()->mChildren.MoveInto(&children);
    }

    // Loop over the aggregation list and destroy everything.
    while (!children.empty()) {
        DAWN_DEBUG() << "Own loop";
        children.head()->value()->Destroy();
    }
}

}  // namespace dawn::native::detail
