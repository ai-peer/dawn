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

#ifndef SRC_DAWN_NATIVE_REFCOUNTEDWITHWEAKREF_H_
#define SRC_DAWN_NATIVE_REFCOUNTEDWITHWEAKREF_H_

#include <atomic>
#include <memory>

#include "dawn/common/RefCounted.h"

namespace dawn::native {

template <typename Object>
class WeakReference : public RefCounted {
  public:
    WeakReference(Object* object, uint64_t strongRefPayload)
        : mObjPtr(object), mStrongRefCount(strongRefPayload) {}

    WeakReference() = delete;
    ~WeakReference() override = default;

    // Convert weak reference to strong reference.
    // Return nullptr if strong reference is already invalidated.
    Ref<Object> GetStrongReference() {
        if (!mStrongRefCount.TryIncrement()) {
            return nullptr;
        }

        return AcquireRef(mObjPtr);
    }

  protected:
    Object* mObjPtr;
    RefCount mStrongRefCount;
};

// RecCountedWithWeakCount is a version of reference counted object which supports weak reference.
// The reference counter is stored in a pointer that can outlive the object itself. This is to make
// sure weak reference can check whether the object is alive or not even after the object is
// deleted.
template <typename Derived>
class RefCountedWithWeakRef {
  public:
    explicit RefCountedWithWeakRef(uint64_t payload = 0) {
        mWeakRef = AcquireRef(new WeakReferenceWithStrongControl(static_cast<Derived*>(this),
                                                                 /*strongRefPayload=*/payload));
    }

    void Reference() { mWeakRef->StrongReferenceAddRef(); }
    void Release() { Release(/*isMultiThreadUnsafe=*/false); }
    Ref<WeakReference<Derived>> GetWeakReference() { return mWeakRef.Get(); }

    void APIReference() { Reference(); }
    void APIRelease() { Release(/*isMultiThreadUnsafe=*/true); }

  protected:
    virtual ~RefCountedWithWeakRef() = default;

    void Release(bool isMultiThreadUnsafe) {
        if (mWeakRef->StrongReferenceRelease()) {
            DeleteThis(isMultiThreadUnsafe);
        }
    }

    // A Derived class may override this if they require a custom deleter.
    virtual void DeleteThis(bool isMultiThreadUnsafe) { delete this; }

  private:
    class WeakReferenceWithStrongControl : public WeakReference<Derived> {
      public:
        WeakReferenceWithStrongControl(Derived* obj, uint64_t strongRefPayload)
            : WeakReference<Derived>(obj, strongRefPayload) {}

        void StrongReferenceAddRef() { this->mStrongRefCount.Increment(); }
        bool StrongReferenceRelease() { return this->mStrongRefCount.Decrement(); }
    };

    Ref<WeakReferenceWithStrongControl> mWeakRef;
};

}  // namespace dawn::native

#endif
