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

#ifndef SRC_DAWN_COMMON_NUMERIC_H_
#define SRC_DAWN_COMMON_NUMERIC_H_

#include <cstdint>
#include <limits>
#include <type_traits>

#include "dawn/common/Assert.h"

namespace detail {

template <typename T>
inline constexpr uint32_t u32_sizeof() {
    static_assert(sizeof(T) <= std::numeric_limits<uint32_t>::max());
    return uint32_t(sizeof(T));
}

template <typename T>
inline constexpr uint32_t u32_alignof() {
    static_assert(alignof(T) <= std::numeric_limits<uint32_t>::max());
    return uint32_t(alignof(T));
}

}  // namespace detail

template <typename T>
inline constexpr uint32_t u32_sizeof = detail::u32_sizeof<T>();

template <typename T>
inline constexpr uint32_t u32_alignof = detail::u32_alignof<T>();

// Only defined for unsigned integers because that is all that is
// needed at the time of writing.
template <typename Dst, typename Src, typename = std::enable_if_t<std::is_unsigned_v<Src>>>
inline Dst checked_cast(const Src& value) {
    ASSERT(value <= std::numeric_limits<Dst>::max());
    return static_cast<Dst>(value);
}

// Check if the value is representable by a given type, following the WebIDL.
// template <typename T>
// bool is_representable(double value);

// template<typename T, typename Enabled = void>
// bool is_double_value_representable(double value);

// Following WebIDL 3.3.6.[EnforceRange]
// template<typename T, typename = std::enable_if_t<std::is_integral<T>>>
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
// inline bool is_double_value_representable(double value) {
bool is_double_value_representable(double value) {
    constexpr double kMin = static_cast<double>(std::numeric_limits<T>::min());
    constexpr double kMax = static_cast<double>(std::numeric_limits<T>::max());
    return kMin <= value && value <= kMax;
}

// Following WebIDL 3.2.5.float
template <typename T, typename = std::enable_if_t<std::is_same_v<T, float> == true>>
// template<typename T = float>
// inline bool is_double_value_representable(double value) {
bool is_double_value_representable(double value) {
    return true;
}

// template<typename T>
// inline bool is_double_value_representable(double value) {
//     static_assert(std::is_same_v<T, float>);
//     return true;
// }

#endif  // SRC_DAWN_COMMON_NUMERIC_H_
