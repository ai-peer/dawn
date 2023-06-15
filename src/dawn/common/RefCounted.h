// Copyright 2017 The Dawn Authors
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

#ifndef SRC_DAWN_COMMON_REFCOUNTED_H_
#define SRC_DAWN_COMMON_REFCOUNTED_H_

#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>
#include <utility>

#include "dawn/common/Assert.h"
#include "dawn/common/RefBase.h"

namespace dawn {
namespace detail {
class WeakRefData;
}  // namespace detail

class RefCounted;
template <typename T>
class Ref;

class WeakRefCounted;
template <typename T>
class WeakRef;

class RefCount {
  public:
    // Create a refcount with a payload. The refcount starts initially at one.
    explicit RefCount(uint64_t payload = 0);

    uint64_t GetValueForTesting() const;
    uint64_t GetPayload() const;

    // Add a reference.
    void Increment();
    // Tries to add a reference. Returns false if the ref count is already at 0. This is used when
    // operating on a raw pointer to a RefCounted instead of a valid Ref that may be soon deleted.
    bool TryIncrement();

    // Remove a reference. Returns true if this was the last reference.
    bool Decrement();

  private:
    std::atomic<uint64_t> mRefCount;
};

class RefCounted {
  public:
    explicit RefCounted(uint64_t payload = 0);

    uint64_t GetRefCountForTesting() const;
    uint64_t GetRefCountPayload() const;

    void Reference();
    // Release() is called by internal code, so it's assumed that there is already a thread
    // synchronization in place for destruction.
    void Release();

    // Tries to return a valid Ref to `object` if it's internal refcount is not already 0. If the
    // internal refcount has already reached 0, returns nullptr instead.
    template <typename T, typename = typename std::is_convertible<T, RefCounted>>
    friend Ref<T> TryGetRef(T* object) {
        // Since this is called on the RefCounted class directly, and can race with destruction, we
        // verify that we can safely increment the refcount first, create the Ref, then decrement
        // the refcount in that order to ensure that the resultant Ref is a valid Ref.
        if (!object->mRefCount.TryIncrement()) {
            return nullptr;
        }
        return AcquireRef(object);
    }

    void APIReference() { Reference(); }
    // APIRelease() can be called without any synchronization guarantees so we need to use a Release
    // method that will call LockAndDeleteThis() on destruction.
    void APIRelease() { ReleaseAndLockBeforeDestroy(); }

  protected:
    friend class detail::WeakRefData;

    virtual ~RefCounted();

    void ReleaseAndLockBeforeDestroy();

    // A Derived class may override these if they require a custom deleter.
    virtual void DeleteThis();
    // This calls DeleteThis() by default.
    virtual void LockAndDeleteThis();

    RefCount mRefCount;
};

template <typename T>
struct RefCountedTraits {
    static constexpr T* kNullValue = nullptr;
    static void Reference(T* value) { value->Reference(); }
    static void Release(T* value) { value->Release(); }
};

template <typename T>
class Ref : public RefBase<T*, RefCountedTraits<T>> {
  public:
    using RefBase<T*, RefCountedTraits<T>>::RefBase;
    WeakRef<T> GetWeakRef();
};

namespace detail {

class WeakRefData : public RefCounted {
  public:
    explicit WeakRefData(RefCounted* value);

    void Invalidate();
    bool IsValid() const;

    // Tries to return a valid Ref to `mValue` if it's internal refcount is not already 0. If the
    // internal refcount has already reached 0, returns nullptr instead.
    template <typename T>
    friend Ref<T> TryGetRefFromData(WeakRefData* data) {
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
    friend WeakRef<T> GetWeakRef(T* object) {
        return WeakRef<T>(object->mWeakRef.Get());
    }

  protected:
    void DeleteThis() override;

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
    template <typename U, typename = typename std::is_convertible<U, T>::type>
    WeakRef(const WeakRef<U>& other) : mWeakRef(other.mWeakRef) {}
    template <typename U, typename = typename std::is_convertible<U, T>::type>
    WeakRef<T>& operator=(const WeakRef<U>& other) {
        mWeakRef = other.mWeakRef;
        return *this;
    }
    template <typename U, typename = typename std::is_convertible<U, T>::type>
    WeakRef(WeakRef<U>&& other) : mWeakRef(std::move(other.mWeakRef)) {}
    template <typename U, typename = typename std::is_convertible<U, T>::type>
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
            return TryGetRefFromData<T>(mWeakRef.Get());
        }
        return nullptr;
    }

    bool IsValid() const { return mWeakRef != nullptr && mWeakRef->IsValid(); }

  private:
    friend class WeakRefCounted;
    // Friend is needed so that we can access the data ref in conversions.
    template <typename U>
    friend class WeakRef;

    // Constructor from data is only allowed by the WeakRefCounted class because the "original" data
    // is always alive at that point.
    explicit WeakRef(detail::WeakRefData* weakRef) : mWeakRef(weakRef) {}

    Ref<detail::WeakRefData> mWeakRef = nullptr;
};

template <typename T>
Ref<T> AcquireRef(T* pointee) {
    Ref<T> ref;
    ref.Acquire(pointee);
    return ref;
}

template <typename T>
WeakRef<T> Ref<T>::GetWeakRef() {
    if constexpr (std::is_convertible<T*, WeakRefCounted*>::value) {
        return GetWeakRef<T>(this->mValue);
    } else {
        UNREACHABLE();
    }
}

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_REFCOUNTED_H_
