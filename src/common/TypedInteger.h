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

#ifndef DAWNNATIVE_TYPEDINTEGER_H_
#define DAWNNATIVE_TYPEDINTEGER_H_

#include "common/Assert.h"

#include <limits>
#include <type_traits>

namespace detail {

    template <typename Tag,
              typename T,
              T MinValue = std::numeric_limits<T>::min(),
              T MaxValue = std::numeric_limits<T>::max()>
    class TypedIntegerImpl {
        static_assert(std::is_integral<T>::value, "TypedInteger must be integral");

      public:
        // Intentionally deleted. In Release builds, this class is just an alias for the primitive
        // type T which does not have a default constructor.
        constexpr TypedIntegerImpl() = delete;

        // Construction from non-narrowing integral types.
        template <typename T2, typename = std::enable_if_t<std::is_integral<T2>::value>>
        constexpr explicit TypedIntegerImpl(T2 value) : mValue(static_cast<T>(value)) {
            static_assert(std::numeric_limits<T2>::max() <= std::numeric_limits<T>::max(),
                          "construction would narrow");
            static_assert(std::numeric_limits<T2>::min() >= std::numeric_limits<T>::min(),
                          "construction would narrow");
            ASSERT(mValue >= MinValue);
            ASSERT(mValue <= MaxValue);
        }

        // Cast to non-narrowing integral types.
        template <typename T2, typename = std::enable_if_t<std::is_integral<T2>::value>>
        constexpr operator T2() const {
            static_assert(std::numeric_limits<T2>::max() >= std::numeric_limits<T>::max(),
                          "cast would narrow");
            static_assert(std::numeric_limits<T2>::min() <= std::numeric_limits<T>::min(),
                          "cast would narrow");
            return mValue;
        }

        constexpr bool operator<(const TypedIntegerImpl& rhs) const {
            return mValue < rhs.mValue;
        }

        constexpr bool operator<=(const TypedIntegerImpl& rhs) const {
            return mValue <= rhs.mValue;
        }

        constexpr bool operator>(const TypedIntegerImpl& rhs) const {
            return mValue > rhs.mValue;
        }

        constexpr bool operator>=(const TypedIntegerImpl& rhs) const {
            return mValue >= rhs.mValue;
        }

        constexpr bool operator==(const TypedIntegerImpl& rhs) const {
            return mValue == rhs.mValue;
        }

        constexpr bool operator!=(const TypedIntegerImpl& rhs) const {
            return mValue != rhs.mValue;
        }

        constexpr TypedIntegerImpl& operator++() {
            ASSERT(mValue < MaxValue);
            ++mValue;
            return *this;
        }

        constexpr TypedIntegerImpl& operator--() {
            ASSERT(mValue > MinValue);
            --mValue;
            return *this;
        }

        constexpr TypedIntegerImpl operator+(const TypedIntegerImpl& rhs) const {
            // TODO(enga): Would be nice to check bounds.
            return TypedIntegerImpl(mValue + rhs.mValue);
        }

        constexpr TypedIntegerImpl operator-(const TypedIntegerImpl& rhs) const {
            // TODO(enga): Would be nice to check bounds.
            return TypedIntegerImpl(mValue - rhs.mValue);
        }

      private:
        T mValue;
    };

}  // namespace detail

template <typename Tag,
          typename T,
          T MinValue = std::numeric_limits<T>::min(),
          T MaxValue = std::numeric_limits<T>::max()>
#if defined(DAWN_ENABLE_ASSERTS)
using TypedInteger = detail::TypedIntegerImpl<Tag, T, MinValue, MaxValue>;
#else
using TypedInteger = T;
#endif

#endif  // COMMON_TYPEDINTEGER_H_
