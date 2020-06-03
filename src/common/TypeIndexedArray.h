// Copyright 2020 The Dawn Authors
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

#ifndef COMMON_TYPEINDEXEDARRAY_H_
#define COMMON_TYPEINDEXEDARRAY_H_

#include "common/TypedInteger.h"
#include "common/UnderlyingType.h"

#include <array>
#include <type_traits>

// TypeIndexedArray is a helper class that wraps std::array with the restriction that
// indices must match a particular TypedInteger type. Dawn uses multiple flat maps of
// index-->data, and this class helps ensure an indices cannot be passed interchangably
// to a flat map of a different type.
template <typename Index, typename Value, size_t Size>
class TypeIndexedArray : private std::array<Value, Size> {
    using I = typename UnderlyingType<Index>::type;

    static_assert(Size <=
                      std::max(std::numeric_limits<I>::max() + 1, std::numeric_limits<I>::max()),
                  "");
    using Base = std::array<Value, Size>;

  public:
    Value& operator[](Index i) {
        return Base::operator[](static_cast<I>(i));
    }

    constexpr const Value& operator[](Index i) const {
        return Base::operator[](static_cast<I>(i));
    }

    Value& at(Index i) {
        return Base::at(static_cast<I>(i));
    }

    constexpr const Value& at(Index i) const {
        return Base::at(static_cast<I>(i));
    }

    Value* begin() noexcept {
        return Base::begin();
    }

    const Value* begin() const noexcept {
        return Base::begin();
    }

    Value* end() noexcept {
        return Base::end();
    }

    const Value* end() const noexcept {
        return Base::end();
    }

    constexpr Index size() const {
        return Index(static_cast<I>(Size));
    }

    using Base::back;
    using Base::data;
    using Base::empty;
    using Base::front;
};

#endif  // COMMON_TYPEINDEXEDARRAY_H_
