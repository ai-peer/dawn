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

#ifndef COMMON_TYPEDINTEGER_H_
#define COMMON_TYPEDINTEGER_H_

#include "common/Assert.h"

#include <limits>
#include <type_traits>

// TypedInteger is helper class that provides additional type safety in Debug.
//  - Integers of different (Tag, BaseIntegerType) may not be used interoperably
//  - Disallows narrowing casts
//  - Has range assertions on construction, assignment, and increment/decrement
//  - Integers of the same (Tag, BaseIntegerType) may be compared or assigned, but range
//    assertions still apply.
// This class helps ensure that the many types of indices in Dawn aren't mixed up and used
// interchangably.

namespace detail {

    template <typename Tag, typename T>
    class TypedIntegerImpl {
        static_assert(std::is_integral<T>::value, "TypedInteger must be integral");
        T mValue;

      public:
        constexpr TypedIntegerImpl() : mValue(0) {
        }

        // Construction from non-narrowing integral types.
        template <typename I,
                  typename = std::enable_if_t<
                      std::is_integral<I>::value &&
                      std::numeric_limits<I>::max() <= std::numeric_limits<T>::max() &&
                      std::numeric_limits<I>::min() >= std::numeric_limits<T>::min()>>
        explicit constexpr TypedIntegerImpl(I rhs) : mValue(static_cast<T>(rhs)) {
        }

        // Allow explicit casts only to the underlying type. If you're casting out of a
        // TypedInteger, you should know what what you're doing, and exactly what type you expect.
        explicit constexpr operator T() const {
            return static_cast<T>(this->mValue);
        }

// Same-tag TypedInteger comparison operators
#define TYPED_COMPARISON(op)                                        \
    constexpr bool operator op(const TypedIntegerImpl& rhs) const { \
        return mValue op rhs.mValue;                                \
    }
        TYPED_COMPARISON(<)
        TYPED_COMPARISON(<=)
        TYPED_COMPARISON(>)
        TYPED_COMPARISON(>=)
        TYPED_COMPARISON(==)
        TYPED_COMPARISON(!=)
#undef TYPED_COMPARISON

        // Increment / decrement operators for for-loop iteration
        constexpr TypedIntegerImpl& operator++() {
            ASSERT(this->mValue < std::numeric_limits<T>::max());
            ++this->mValue;
            return *this;
        }

        constexpr TypedIntegerImpl operator++(int) {
            TypedIntegerImpl ret = *this;

            ASSERT(this->mValue < std::numeric_limits<T>::max());
            ++this->mValue;
            return ret;
        }

        constexpr TypedIntegerImpl& operator--() {
            assert(this->mValue > std::numeric_limits<T>::min());
            --this->mValue;
            return *this;
        }

        constexpr TypedIntegerImpl operator--(int) {
            TypedIntegerImpl ret = *this;

            ASSERT(this->mValue > std::numeric_limits<T>::min());
            --this->mValue;
            return ret;
        }
    };

}  // namespace detail

template <typename Tag, typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
#if defined(DAWN_ENABLE_ASSERTS)
using TypedInteger = detail::TypedIntegerImpl<Tag, T>;
#else
using TypedInteger = T;
#endif

namespace std {
    template <typename Tag, typename T>
    class numeric_limits<TypedInteger<Tag, T>> : public numeric_limits<T> {
      public:
        static TypedInteger<Tag, T> max() noexcept {
            return TypedInteger<Tag, T>(std::numeric_limits<T>::max());
        }
        static TypedInteger<Tag, T> min() noexcept {
            return TypedInteger<Tag, T>(std::numeric_limits<T>::min());
        }
    };

    template <typename Tag, typename T>
    class hash<TypedInteger<Tag, T>> : private hash<T> {
      public:
        size_t operator()(TypedInteger<Tag, T> value) const {
            return hash<T>::operator()(static_cast<T>(value));
        }
    };

    template <typename Tag, typename T>
    class underlying_type<TypedInteger<Tag, T>> {
      public:
        using type = T;
    };

}  // namespace std

#endif  // COMMON_TYPEDINTEGER_H_
