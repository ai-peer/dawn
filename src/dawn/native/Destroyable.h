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

#ifndef SRC_DAWN_NATIVE_DESTROYABLE_H_
#define SRC_DAWN_NATIVE_DESTROYABLE_H_

#include <atomic>
#include <mutex>

#include "dawn/common/LinkedList.h"
#include "dawn/common/Log.h"
#include "dawn/common/RefCounted.h"

namespace dawn::native {

template <class T, class RefCountedType>
class Destroyable;
template <class RefCountedType>
class Owner;
template <class T>
class Owns;

namespace detail {

class DestroyableBase;
class OwnerBase;
class OwnsBase;

class DestroyableBase : public LinkNode<DestroyableBase> {
  public:
    explicit DestroyableBase(OwnsBase* own = nullptr);

    void Destroy();

    // Tracks whether the object has been destroyed already, and prevents it from being destroyed
    // again. By default this just checks whether the object is tracked by an owner's linked list.
    virtual bool IsAlive() { return IsInList(); }

  protected:
    virtual void DestroyImpl() = 0;

  private:
    OwnsBase* mOwn = nullptr;
};

class OwnerBase : public DestroyableBase {
  public:
    explicit OwnerBase(OwnsBase* own = nullptr);
    void Register(detail::OwnsBase* owns);

  private:
    void DestroyImpl() override;

    std::mutex mMutex;
    LinkedList<detail::OwnsBase> mOwns;

    // Owners put themselves in a list so that they can be destroyed.
    LinkedList<DestroyableBase> mSelf;
};

class OwnsBase : public LinkNode<OwnsBase> {
  protected:
    std::mutex mMutex;
    LinkedList<DestroyableBase> mChildren;

  private:
    friend class DestroyableBase;
    friend class OwnerBase;
};

}  // namespace detail

// Provides an interface for classes that require capabilities for a one-time state transition into
// a 'destroyed' state rendering capabilites unusable without actually calling the class's
// destructor.
template <class T, class RefCountedType = RefCounted>
class Destroyable : public RefCountedType, public detail::DestroyableBase {
  public:
    // using RefCountedType::APIReference;
    // using RefCountedType::APIRelease;
    // using RefCountedType::Reference;
    // using RefCountedType::Release;

    explicit Destroyable(Owns<T>* own = nullptr) : DestroyableBase(own) {
        if (own) {
            own->Track(this);
        }
    }

  protected:
    // Overriding of the RefCounted's DeleteThis function ensures that instances of objects
    // always call their derived class implementation of Destroy prior to the derived
    // class being destroyed. We cannot naively put the call to Destroy in the destructor of this
    // class because it calls DestroyImpl which is a virtual function implemented in the Derived
    // class which would already  have been destroyed by the time this destructor is called by C++'s
    // destruction order. Note that some classes derived classes may may override the DeleteThis
    // function again, and they should ensure that their overriding versions call this version
    // somewhere.
    void DeleteThis() override {
        DAWN_DEBUG() << "DeleteThis called";
        Destroy();
        RefCountedType::DeleteThis();
    }
};

template <class RefCountedType = RefCounted>
class Owner : public RefCountedType, public detail::OwnerBase {
  protected:
    void DeleteThis() override {
        Destroy();
        RefCountedType::DeleteThis();
    }
};

template <class T>
class Owns : public detail::OwnsBase {
  public:
    explicit Owns(detail::OwnerBase* owner) { owner->Register(this); }

    void Track(Destroyable<T>* child) {
        std::lock_guard<std::mutex> lock(mMutex);
        child->InsertBefore(mChildren.head());
    }
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_DESTROYABLE_H_
