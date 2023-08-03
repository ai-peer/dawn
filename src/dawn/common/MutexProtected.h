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

#ifndef SRC_DAWN_COMMON_MUTEXPROTECTED_H_
#define SRC_DAWN_COMMON_MUTEXPROTECTED_H_

#include <mutex>
#include <utility>

#include "dawn/common/Mutex.h"
#include "dawn/common/Ref.h"

namespace dawn {

template <typename T>
class MutexProtected;

namespace detail {

template <typename T>
struct MutexProtectedTraits {
    using MutexType = std::mutex;
    using LockType = std::lock_guard<std::mutex>;
    using ObjectType = T;

    static constexpr bool copyable = false;

    static void CreateMutex(MutexType*) {}
    static std::mutex& GetMutex(MutexType& m) { return m; }
    static ObjectType* GetObj(T* const obj) { return obj; }
    static const ObjectType* GetObj(const T* const obj) { return obj; }
};

template <typename T>
struct MutexProtectedTraits<Ref<T>> {
    using MutexType = Ref<Mutex>;
    using LockType = Mutex::AutoLock;
    using ObjectType = T;

    static constexpr bool copyable = true;

    static void CreateMutex(MutexType* m) { *m = AcquireRef(new Mutex()); }
    static Mutex* GetMutex(MutexType& m) { return m.Get(); }
    static ObjectType* GetObj(Ref<T>* const obj) { return obj->Get(); }
    static const ObjectType* GetObj(const Ref<T>* const obj) { return obj->Get(); }
};

template <typename T, typename Traits>
class Guard {
  public:
    using ReturnType = typename UnwrapRef<T>::type;

    // It's the programmer's burden to not save the pointer/reference and reuse it without the lock.
    ReturnType* operator->() { return Traits::GetObj(mObj); }
    ReturnType& operator*() { return *Traits::GetObj(mObj); }
    const ReturnType* operator->() const { return Traits::GetObj(mObj); }
    const ReturnType& operator*() const { return *Traits::GetObj(mObj); }

  private:
    friend class MutexProtected<T>;

    Guard(T* obj, Traits::MutexType& mutex) : mLock(Traits::GetMutex(mutex)), mObj(obj) {}

    Traits::LockType mLock;
    T* const mObj;
};

}  // namespace detail

template <typename T>
class MutexProtected {
  public:
    using Traits = detail::MutexProtectedTraits<T>;
    using Usage = detail::Guard<T, Traits>;
    using ConstUsage = detail::Guard<const T, Traits>;

    MutexProtected() { Traits::CreateMutex(&mMutex); }

    template <typename... Args>
    explicit MutexProtected(Args&&... args) : mObj(std::forward<Args>(args)...) {
        Traits::CreateMutex(&mMutex);
    }

    Usage operator->() { return Use(); }
    ConstUsage operator->() const { return Use(); }

    template <typename Fn>
    auto Use(Fn&& fn) {
        return fn(Use());
    }

  private:
    Usage Use() { return Usage(&mObj, mMutex); }
    ConstUsage Use() const { return ConstUsage(&mObj, mMutex); }

    Traits::MutexType mMutex;
    T mObj;
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_MUTEXPROTECTED_H_
