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

#include "dawn/common/Ref.h"

namespace dawn {

template <typename T>
class MutexProtected;

namespace detail {

template <typename T>
struct IsMutexProtected {
    static constexpr bool value = false;
};
template <typename T>
struct IsMutexProtected<MutexProtected<T>> {
    static constexpr bool value = true;
};

template <typename T, typename Mutex = std::mutex>
class Guard {
  public:
    using ReturnType = UnwrapRef<T>::type;

    // It's the programmer's burden to not save the pointer/reference and reuse it again without a
    // lock.
    ReturnType* operator->() {
        if constexpr (IsRef<T>::value) {
            return mObj->Get();
        } else {
            return mObj;
        }
    }
    ReturnType& operator*() {
        if constexpr (IsRef<T>::value) {
            return *mObj->Get();
        } else {
            return *mObj;
        }
    }
    const ReturnType* operator->() const {
        if constexpr (IsRef<T>::value) {
            return mObj->Get();
        } else {
            return mObj;
        }
    }
    const ReturnType& operator*() const {
        if constexpr (IsRef<T>::value) {
            return *mObj->Get();
        } else {
            return *mObj;
        }
    }

  private:
    friend class MutexProtected<T>;

    Guard(T* obj, Mutex& mutex) : mLock(mutex), mObj(obj) {}

    std::lock_guard<Mutex> mLock;
    T* const mObj;
};

}  // namespace detail

template <typename T>
class MutexProtected {
  public:
    using Usage = detail::Guard<T>;
    using ConstUsage = detail::Guard<const T>;

    MutexProtected() = default;

    template <typename... Args>
    explicit MutexProtected(Args&&... args) : mObj(std::forward<Args>(args)...) {}

    Usage operator->() { return Use(); }
    ConstUsage operator->() const { return Use(); }

  private:
    Usage Use() { return Usage(&mObj, mMutex); }
    ConstUsage Use() const { return ConstUsage(&mObj, mMutex); }

    template <typename Fn, typename... Args>
    friend auto UseProtected(Fn&& fn, Args&&... args);

    std::mutex mMutex;
    T mObj;
};

template <typename Fn, typename... Args>
auto UseProtected(Fn&& fn, Args&&... args) {
    return fn(args.Use()...);
}

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_MUTEXPROTECTED_H_
