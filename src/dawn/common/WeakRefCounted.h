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
#include <utility>

#include "dawn/common/RefCounted.h"

namespace dawn {

template <typename T>
class WeakRef;

namespace detail {

class WeakRefData : public RefCounted {
  public:
    // The constructor only takes RefCounted types as a protective layer against misuse where a type
    // tries to extend WeakRefCounted without the class being a RefCounted type.
    explicit WeakRefData(RefCounted* value);
    void Invalidate();

    // Tries to return a valid Ref to `mValue` if it's internal refcount is not already 0. If the
    // internal refcount has already reached 0, returns nullptr instead.
    template <typename T>
    Ref<T> TryGetRef() {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mValue || !mValue->mRefCount.TryIncrement()) {
            return nullptr;
        }
        return AcquireRef(static_cast<T*>(mValue));
    }

  private:
    std::mutex mMutex;
    RefCounted* mValue = nullptr;
};

// Interface base class used to enable compile-time verification of WeakRefCounted functions.
class WeakRefCountedBase {};

}  // namespace detail

// Class that should be extended to enable WeakRefs for the type.
template <typename T>
class WeakRefCounted : public detail::WeakRefCountedBase {
  public:
    WeakRefCounted() : mWeakRef(AcquireRef(new detail::WeakRefData(static_cast<T*>(this)))) {}

    auto GetWeakRef() { return WeakRef<decltype(*this)>(mWeakRef.Get()); }

  protected:
    ~WeakRefCounted() { mWeakRef->Invalidate(); }

  private:
    Ref<detail::WeakRefData> mWeakRef;
};

template <typename T>
class WeakRef {
  public:
    WeakRef() {}

    // Constructors from nullptr.
    // NOLINTNEXTLINE(runtime/explicit)
    constexpr WeakRef(std::nullptr_t) : WeakRef() {}

    // Constructors from a RefBase<U>, where U can also equal T.
    template <typename U, typename = typename std::enable_if<std::is_base_of_v<T, U>>::type>
    WeakRef(const WeakRef<U>& other) : mWeakRef(other.mWeakRef) {}
    template <typename U, typename = typename std::enable_if<std::is_base_of_v<T, U>>::type>
    WeakRef<T>& operator=(const WeakRef<U>& other) {
        mWeakRef = other.mWeakRef;
        return *this;
    }
    template <typename U, typename = typename std::enable_if<std::is_base_of_v<T, U>>::type>
    WeakRef(WeakRef<U>&& other) : mWeakRef(std::move(other.mWeakRef)) {}
    template <typename U, typename = typename std::enable_if<std::is_base_of_v<T, U>>::type>
    WeakRef<T>& operator=(WeakRef<U>&& other) {
        if (&other != this) {
            mWeakRef = std::move(other.mWeakRef);
        }
        return *this;
    }

    // Getting on a WeakRef returns a Ref, not a raw pointer because a raw pointer could become
    // invalid after being retrieved.
    Ref<T> Get() {
        if (mWeakRef != nullptr) {
            return mWeakRef->template TryGetRef<T>();
        }
        return nullptr;
    }

  private:
    // Friend is needed so that we can access the data ref in conversions.
    template <typename U>
    friend class WeakRef;
    template <typename U>
    friend class WeakRefCounted;

    // Constructor from data should only be allowed via the GetWeakRef function.
    explicit WeakRef(detail::WeakRefData* weakRef) : mWeakRef(weakRef) {}

    Ref<detail::WeakRefData> mWeakRef = nullptr;
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_WEAKREFCOUNTED_H_
