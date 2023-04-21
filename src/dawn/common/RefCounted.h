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

#include "dawn/common/RefBase.h"

class RefCount {
  public:
    // Create a refcount with a payload. The refcount starts initially at one.
    explicit RefCount(uint64_t payload = 0);

    uint64_t GetValueForTesting() const;
    uint64_t GetPayload() const;

    // Add a reference.
    void Increment();

    // Perform reference's increase then return true.
    // However if the ref count was already "zero", this will return false and no reference will be
    // added.
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

    // Attempt to increase the ref count of the object then return true.
    // However if the ref count was already "zero", this will return false and no reference will be
    // added.
    // Note: since normally an object whose ref count reaches zero will be deleted immediaty, this
    // method is intended to be used by a cache that stores raw pointer to the object. When being
    // used on multiple threads, the stored object's destruction must have some forms of
    // synchronization with the cache so that it won't try to call WeakReference() on the already
    // deleted object.
    bool TryReference();

    // Release() is called by internal code, so it's assumed that there is already a thread
    // synchronization in place for destruction.
    void Release();

    void APIReference() { Reference(); }
    // APIRelease() can be called without any synchronization guarantees so we need to use a Release
    // method that will call LockAndDeleteThis() on destruction.
    void APIRelease() { ReleaseAndLockBeforeDestroy(); }

  protected:
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
};

template <typename T>
Ref<T> AcquireRef(T* pointee) {
    Ref<T> ref;
    ref.Acquire(pointee);
    return ref;
}

#endif  // SRC_DAWN_COMMON_REFCOUNTED_H_
