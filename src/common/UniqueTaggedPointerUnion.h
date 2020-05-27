// Copyright 2019 The Dawn Authors
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

#ifndef COMMON_UNIQUETAGGEDPOINTERUNION_H_
#define COMMON_UNIQUETAGGEDPOINTERUNION_H_

#include "common/TaggedPointerUnion.h"

template <typename... Ts>
class UniqueTaggedPointerUnion : public TaggedPointerUnion<Ts...> {
    using Base = TaggedPointerUnion<Ts...>;

  public:
    UniqueTaggedPointerUnion() : Base() {
    }
    UniqueTaggedPointerUnion(std::nullptr_t) : Base(nullptr) {
    }

    template <typename T>
    UniqueTaggedPointerUnion(T* ptr) : Base(ptr) {
    }

    UniqueTaggedPointerUnion& operator=(std::nullptr_t) {
        this->Reset();
        return *this;
    }

    UniqueTaggedPointerUnion(UniqueTaggedPointerUnion&& rhs) : Base(std::forward<Base>(rhs)) {
    }

    UniqueTaggedPointerUnion& operator=(UniqueTaggedPointerUnion&& rhs) {
        Base::operator=(std::forward<Base>(rhs));
        return *this;
    }

    template <typename T, typename... TArgs>
    static UniqueTaggedPointerUnion Make(TArgs&&... args) {
        return UniqueTaggedPointerUnion(new T(std::forward<TArgs>(args)...));
    }

    UniqueTaggedPointerUnion(const UniqueTaggedPointerUnion&) = delete;
    UniqueTaggedPointerUnion& operator=(const UniqueTaggedPointerUnion&) = delete;

    ~UniqueTaggedPointerUnion() {
        this->Reset();
    }

    template <typename T>
    T* Release() {
        T* ptr = Base::template As<T>();
        *static_cast<Base*>(this) = nullptr;
        return ptr;
    }
};

#endif  // COMMON_UNIQUETAGGEDPOINTERUNION_H_
