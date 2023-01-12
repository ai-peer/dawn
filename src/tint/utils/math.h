// Copyright 2021 The Tint Authors.
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

#ifndef SRC_TINT_UTILS_MATH_H_
#define SRC_TINT_UTILS_MATH_H_

#include <sstream>
#include <string>
#include <type_traits>

namespace tint::utils {

/// @param alignment the next multiple to round `value` to
/// @param value the value to round to the next multiple of `alignment`
/// @return `value` rounded to the next multiple of `alignment`
/// @note `alignment` must be positive. An alignment of zero will cause a DBZ.
template <typename T>
inline constexpr T RoundUp(T alignment, T value) {
    return ((value + alignment - 1) / alignment) * alignment;
}

/// @param value the value to check whether it is a power-of-two
/// @returns true if `value` is a power-of-two
/// @note `value` must be positive if `T` is signed
template <typename T>
inline constexpr bool IsPowerOfTwo(T value) {
    return (value & (value - 1)) == 0;
}

/// @param value the input value
/// @returns the base-2 logarithm of @p value
inline constexpr uint32_t Log2(uint64_t value) {
#if defined(_MSC_VER) && !defined(__clang__)  // MSVC
    if constexpr (sizeof(unsigned long) == 8) {
        // NOLINTNEXTLINE(runtime/int)
        unsigned long first_bit_index = 0;
        _BitScanReverse64(&first_bit_index, value);
        return first_bit_index;
    } else {
        // NOLINTNEXTLINE(runtime/int)
        unsigned long first_bit_index = 0;
        if (_BitScanReverse(&first_bit_index, value >> 32)) {
            return first_bit_index + 32;
        }
        _BitScanReverse(&first_bit_index, value & 0xffffffff);
        return first_bit_index;
    }
#elif defined(__clang__) || defined(__GNUC__)
    return 63 - static_cast<uint32_t>(__builtin_clzll(value));
#else
    value--;
    value |= v >> 1;
    value |= v >> 2;
    value |= v >> 4;
    value |= v >> 8;
    value |= v >> 16;
    value |= v >> 32;
    value++;
    return value;
#endif
}

/// @param value the input value
/// @returns the next power of two number greater or equal to @p value
inline constexpr uint64_t NextPowerOfTwo(uint64_t n) {
    if (n <= 1) {
        return 1;
    }
    return static_cast<uint64_t>(1) << (Log2(n - 1) + 1);
}

/// @param value the input value
/// @returns the largest power of two that `value` is a multiple of
template <typename T>
inline std::enable_if_t<std::is_unsigned<T>::value, T> MaxAlignOf(T value) {
    T pot = 1;
    while (value && ((value & 1u) == 0)) {
        pot <<= 1;
        value >>= 1;
    }
    return pot;
}

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_MATH_H_
