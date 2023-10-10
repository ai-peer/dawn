// Copyright 2023 The Tint Authors.
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

#ifndef SRC_TINT_UTILS_BYTES_SWAP_H_
#define SRC_TINT_UTILS_BYTES_SWAP_H_

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace tint::bytes {

/// @return the input integer value with all bytes reversed
/// @param value the input value
template <typename T>
[[nodiscard]] inline T Swap(T value) {
    static_assert(std::is_integral_v<T>);
    uint8_t bytes[sizeof(T)];
    memcpy(bytes, &value, sizeof(T));
    for (size_t i = 0; i < sizeof(T) / 2; i++) {
        std::swap(bytes[i], bytes[sizeof(T) - i - 1]);
    }
    T out;
    memcpy(&out, bytes, sizeof(T));
    return out;
}

}  // namespace tint::bytes

#endif  // SRC_TINT_UTILS_BYTES_SWAP_H_
