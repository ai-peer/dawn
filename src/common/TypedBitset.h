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

#ifndef COMMON_TYPEDBITSET_H_
#define COMMON_TYPEDBITSET_H_

#include "common/BitSetIterator.h"
#include "common/TypedInteger.h"

template <typename Index, size_t N>
class TypedBitset : private std::bitset<N> {
    using I = typename UnderlyingType<Index>::type;
    static_assert(sizeof(I) <= sizeof(size_t), "");

    constexpr TypedBitset(const std::bitset<N>& rhs) : std::bitset<N>(rhs) {
    }

  public:
    constexpr TypedBitset() noexcept : std::bitset<N>() {
    }

    constexpr bool operator[](Index i) const {
        return std::bitset<N>::operator[](static_cast<I>(i));
    }

    typename std::bitset<N>::reference operator[](Index i) {
        return std::bitset<N>::operator[](static_cast<I>(i));
    }

    bool test(Index i) const {
        return std::bitset<N>::test(static_cast<I>(i));
    }

    using std::bitset<N>::all;
    using std::bitset<N>::any;
    using std::bitset<N>::none;
    using std::bitset<N>::count;
    using std::bitset<N>::size;

    TypedBitset& operator&=(const TypedBitset& other) noexcept {
        return static_cast<TypedBitset&>(
            std::bitset<N>::operator&=(static_cast<const std::bitset<N>&>(other)));
    }

    TypedBitset& operator|=(const TypedBitset& other) noexcept {
        return static_cast<TypedBitset&>(
            std::bitset<N>::operator|=(static_cast<const std::bitset<N>&>(other)));
    }

    TypedBitset& operator^=(const TypedBitset& other) noexcept {
        return static_cast<TypedBitset&>(
            std::bitset<N>::operator^=(static_cast<const std::bitset<N>&>(other)));
    }

    TypedBitset operator~() const noexcept {
        return TypedBitset(*this).flip();
    }

    TypedBitset operator<<(Index i) const noexcept {
        return TypedBitset(std::bitset<N>::operator<<(static_cast<I>(i)));
    }

    TypedBitset& operator<<=(Index i) noexcept {
        return static_cast<TypedBitset&>(std::bitset<N>::operator<<=(static_cast<I>(i)));
    }

    TypedBitset operator>>(Index i) const noexcept {
        return TypedBitset(std::bitset<N>::operator>>(static_cast<I>(i)));
    }

    TypedBitset& operator>>=(Index i) noexcept {
        return static_cast<TypedBitset&>(std::bitset<N>::operator>>=(static_cast<I>(i)));
    }

    TypedBitset& set() noexcept {
        return static_cast<TypedBitset&>(std::bitset<N>::set());
    }

    TypedBitset& set(Index i, bool value = true) {
        return static_cast<TypedBitset&>(std::bitset<N>::set(static_cast<I>(i), value));
    }

    TypedBitset& reset() noexcept {
        return static_cast<TypedBitset&>(std::bitset<N>::reset());
    }

    TypedBitset& reset(Index i) {
        return static_cast<TypedBitset&>(std::bitset<N>::reset(static_cast<I>(i)));
    }

    TypedBitset& flip() noexcept {
        return static_cast<TypedBitset&>(std::bitset<N>::flip());
    }

    TypedBitset& flip(Index i) {
        return static_cast<TypedBitset&>(std::bitset<N>::flip(static_cast<I>(i)));
    }

    using std::bitset<N>::to_string;
    using std::bitset<N>::to_ulong;
    using std::bitset<N>::to_ullong;

    friend TypedBitset operator&(const TypedBitset& lhs, const TypedBitset& rhs) noexcept {
        return TypedBitset(static_cast<const std::bitset<N>&>(lhs) &
                           static_cast<const std::bitset<N>&>(rhs));
    }

    friend TypedBitset operator|(const TypedBitset& lhs, const TypedBitset& rhs) noexcept {
        return TypedBitset(static_cast<const std::bitset<N>&>(lhs) |
                           static_cast<const std::bitset<N>&>(rhs));
    }

    friend TypedBitset operator^(const TypedBitset& lhs, const TypedBitset& rhs) noexcept {
        return TypedBitset(static_cast<const std::bitset<N>&>(lhs) ^
                           static_cast<const std::bitset<N>&>(rhs));
    }

    friend BitSetIterator<N, Index> IterateBitSet(const TypedBitset& bitset) {
        return BitSetIterator<N, Index>(static_cast<const std::bitset<N>&>(bitset));
    }
};

#endif  // COMMON_TYPEDBITSET_H_
