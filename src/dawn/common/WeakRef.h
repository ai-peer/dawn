// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_COMMON_WEAKREF_H_
#define SRC_DAWN_COMMON_WEAKREF_H_

#include "dawn/common/RefBase.h"
#include "dawn/common/WeakRefCounted.h"

namespace dawn {

template <typename T>
class WeakRef {
  public:
    WeakRef() {}

    Ref<T> Get() {
        if (mWeakRef != nullptr) {
            return TryGetRef(mWeakRef.Get());
        }
        return nullptr;
    }

    bool IsValid() const { return mWeakRef != nullptr && mWeakRef->IsValid(); }

  private:
    friend class WeakRefCounted;

    explicit WeakRef(detail::WeakRefData* weakRef) : mWeakRef(weakRef) {}

    Ref<detail::WeakRefData> mWeakRef = nullptr;
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_WEAKREFCOUNTED_H_
