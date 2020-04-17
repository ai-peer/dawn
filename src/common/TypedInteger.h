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

// TypedInteger is helper class that provides additional type safety in Debug.
//  - Integers of different (Tag, BaseIntegerType) may not be used interoperably
//  - Disallows narrowing casts
//  - Has range assertions on construction, assignment, and increment/decrement
//  - Integers of the same (Tag, BaseIntegerType) may be compared or assigned, but range
//    assertions still apply.
// This class helps ensure that the many types of indices in Dawn aren't mixed up and used
// interchangably.

namespace detail {

    template <typename T>
    class TypedIntegerStorage {
        // Base class for TypedIntegerImpl which declares all template instantiations
        // of TypedIntegerImpl as friends so that TypedIntegers of
        // different types may read each other's values.
        template <typename Tag, typename T2, T2 Min, T2 Max>
        friend class TypedIntegerImpl;

      private:
        constexpr TypedIntegerStorage(T value) : mValue(value) {
        }
        T mValue;
    };

    template <typename Tag, typename T, T MinValue, T MaxValue>
    class TypedIntegerImpl : public TypedIntegerStorage<T> {
        static_assert(std::is_integral<T>::value, "TypedInteger must be integral");

      public:
        constexpr TypedIntegerImpl() : TypedIntegerStorage<T>(0) {
        }

        // Construction from non-narrowing integral types.
        // This constructor is explicit as creating a typed integer must be intentional.
        template <typename I, typename = std::enable_if_t<std::is_integral<I>::value>>
        explicit constexpr TypedIntegerImpl(I rhs) : TypedIntegerStorage<T>(static_cast<T>(rhs)) {
            // static_assert(std::numeric_limits<I>::max() <= std::numeric_limits<T>::max(),
            //               "construction would narrow");
            // static_assert(std::numeric_limits<I>::min() >= std::numeric_limits<T>::min(),
            //               "construction would narrow");
            ASSERT(this->mValue >= MinValue);
            ASSERT(this->mValue <= MaxValue);
        }

        // Cast to non-narrowing integral types. This is always legal if the destination
        // is a plain integer large enough to hold the value.
        template <typename I, typename = std::enable_if_t<std::is_integral<I>::value>>
        constexpr operator I() const {
            static_assert(MaxValue <= std::numeric_limits<I>::max(), "cast would narrow");
            static_assert(MinValue >= std::numeric_limits<I>::min(), "cast would narrow");
            return static_cast<I>(this->mValue);
        }

        // Construction from a TypedInteger of the same tag but different min/max
        template <T Min, T Max>
        constexpr TypedIntegerImpl(const TypedIntegerImpl<Tag, T, Min, Max>& rhs)
            : TypedIntegerStorage<T>(rhs.mValue) {
            ASSERT(this->mValue >= MinValue);
            ASSERT(this->mValue <= MaxValue);
        }

        // Assignment from a TypedInteger of the same tag but different min/max
        template <T Min, T Max>
        constexpr TypedIntegerImpl& operator=(const TypedIntegerImpl<Tag, T, Min, Max>& rhs) {
            this->mValue = rhs.mValue;
            ASSERT(this->mValue >= MinValue);
            ASSERT(this->mValue <= MaxValue);
            return *this;
        }

// Same-tag TypedInteger comparison operators
#define TYPED_COMPARISON(op)                                                          \
    template <T Min, T Max>                                                           \
    constexpr bool operator op(const TypedIntegerImpl<Tag, T, Min, Max>& rhs) const { \
        return this->mValue op rhs.mValue;                                            \
    }
        TYPED_COMPARISON(<)
        TYPED_COMPARISON(<=)
        TYPED_COMPARISON(>)
        TYPED_COMPARISON(>=)
        TYPED_COMPARISON(==)
        TYPED_COMPARISON(!=)
#undef TYPED_COMPARISON

// Primitive comparison operators
#define PRIMITIVE_COMPARISON(op)                                                   \
    constexpr bool operator op(const T& rhs) const {                               \
        return this->mValue op rhs;                                                \
    }                                                                              \
                                                                                   \
    friend constexpr bool operator op(const T& lhs, const TypedIntegerImpl& rhs) { \
        return lhs op rhs.mValue;                                                  \
    }
        PRIMITIVE_COMPARISON(<)
        PRIMITIVE_COMPARISON(<=)
        PRIMITIVE_COMPARISON(>)
        PRIMITIVE_COMPARISON(>=)
        PRIMITIVE_COMPARISON(==)
        PRIMITIVE_COMPARISON(!=)
#undef PRIMITIVE_COMPARISON

        // Increment / decrement operators for for-loop iteration
        constexpr TypedIntegerImpl& operator++() {
            ASSERT(this->mValue < MaxValue);
            ++this->mValue;
            return *this;
        }

        constexpr TypedIntegerImpl operator++(int) {
            TypedIntegerImpl ret = *this;

            ASSERT(this->mValue < MaxValue);
            ++this->mValue;
            return ret;
        }

        constexpr TypedIntegerImpl& operator--() {
            assert(this->mValue > MinValue);
            --this->mValue;
            return *this;
        }

        constexpr TypedIntegerImpl operator--(int) {
            TypedIntegerImpl ret = *this;

            ASSERT(this->mValue > MinValue);
            --this->mValue;
            return ret;
        }

// Primitive binary operations that yield back some integer type
#define PRIMITIVE_BIN_OP(op)                                                       \
    template <typename I, typename = std::enable_if_t<std::is_integral<I>::value>> \
    constexpr auto operator op(const I& rhs) const {                               \
        return this->mValue op rhs;                                                \
    }                                                                              \
    template <typename I, typename = std::enable_if_t<std::is_integral<I>::value>> \
    friend constexpr auto operator op(const I& lhs, const TypedIntegerImpl& rhs) { \
        return lhs op rhs.mValue;                                                  \
    }
        PRIMITIVE_BIN_OP(+)
        PRIMITIVE_BIN_OP(-)
        PRIMITIVE_BIN_OP(*)
        PRIMITIVE_BIN_OP(/)
        PRIMITIVE_BIN_OP(<<)
        PRIMITIVE_BIN_OP(>>)
#undef PRIMITIVE_BIN_OP
    };

}  // namespace detail

template <typename Tag,
          typename T,
          T MinValue = std::numeric_limits<T>::min(),
          T MaxValue = std::numeric_limits<T>::max(),
          typename = std::enable_if_t<std::is_integral<T>::value>>
#if defined(DAWN_ENABLE_ASSERTS)
using TypedInteger = detail::TypedIntegerImpl<Tag, T, MinValue, MaxValue>;
#else
using TypedInteger = T;
#endif

namespace std {
    template <typename Tag, typename T, T MinValue, T MaxValue>
    class numeric_limits<TypedInteger<Tag, T, MinValue, MaxValue>> {
      public:
        static TypedInteger<Tag, T, MinValue, MaxValue> max() noexcept {
            return TypedInteger<Tag, T, MinValue, MaxValue>(MaxValue);
        }
        static TypedInteger<Tag, T, MinValue, MaxValue> min() noexcept {
            return TypedInteger<Tag, T, MinValue, MaxValue>(MinValue);
        }
    };

    template <typename Tag, typename T, T MinValue, T MaxValue>
    class hash<TypedInteger<Tag, T, MinValue, MaxValue>> : public hash<T> {
      public:
        constexpr hash() = default;
    };

}  // namespace std

#include <array>

// TypedIndexedArray is a helper class that wraps std::array with the restriction that
// indices must match a particular TypedInteger type. Dawn uses multiple flat maps of
// index-->data, and this class helps ensure an indices cannot be passed interchangably
// to a flat map of a different type.
template <typename Value, size_t Size, typename Index>
class TypedIndexedArray;

template <typename Value, size_t Size, typename Tag, typename T, T MinValue, T MaxValue>
class TypedIndexedArray<Value, Size, TypedInteger<Tag, T, MinValue, MaxValue>>
    : private std::array<Value, Size> {
    static_assert(Size <= std::max(MaxValue + 1, std::numeric_limits<T>::max()), "");
    using Base = std::array<Value, Size>;

  public:
    using Index = TypedInteger<Tag, T, 0, Size - 1>;
    using Count = TypedInteger<Tag, T, 0, Size>;

    Value& operator[](Index i) {
        return Base::operator[](i);
    }

    constexpr const Value& operator[](Index i) const {
        return Base::operator[](i);
    }

    constexpr Count size() const {
        return Count(static_cast<T>(Base::size()));
    }

    using Base::data;
};

#endif  // COMMON_TYPEDINTEGER_H_
