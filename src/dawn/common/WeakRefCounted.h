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

#ifndef SRC_DAWN_COMMON_WEAKREFCOUNTED_H_
#define SRC_DAWN_COMMON_WEAKREFCOUNTED_H_

#include <mutex>
#include <type_traits>

#include "dawn/common/RefCounted.h"

namespace dawn {

template <typename T>
class WeakRef;

namespace detail {

class WeakRefData : public RefCounted {
  public:
    explicit WeakRefData(RefCounted* value);

    void Invalidate();
    bool IsValid();

    // Tries to return a valid Ref to `mValue` if it's internal refcount is not already 0. If the
    // internal refcount has already reached 0, returns nullptr instead.
    template <typename T>
    friend Ref<T> TryGetRef(WeakRefData* data) {
        std::lock_guard<std::mutex> lock(data->mMutex);
        if (!data->mIsValid || !data->mValue->mRefCount.TryIncrement()) {
            return nullptr;
        }
        return AcquireRef(static_cast<T*>(data->mValue));
    }

  private:
    std::mutex mMutex;
    bool mIsValid = true;
    RefCounted* mValue;
};

}  // namespace detail

// Class that should be extended to enable WeakRefs for the type.
class WeakRefCounted : public RefCounted {
  public:
    WeakRefCounted();

    template <typename T>
    WeakRef<T> GetWeakRef(T* object) {
        return WeakRef<T>(mWeakRef.Get());
    }

  protected:
    void DeleteThis() override;

  private:
    Ref<detail::WeakRefData> mWeakRef;
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_WEAKREFCOUNTED_H_
