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

#ifndef SRC_TINT_UTILS_BYTES_ENDIANNESS_H_
#define SRC_TINT_UTILS_BYTES_ENDIANNESS_H_

#include <cstdint>
#include <cstring>

namespace tint::bytes {

enum class Endianness { kBig, kLittle };

inline Endianness NativeEndianness() {
    uint8_t u8[4];
    uint32_t u32 = 0x01020304;
    memcpy(u8, &u32, 4);
    return u8[0] == 1 ? Endianness::kBig : Endianness::kLittle;
}

}  // namespace tint::bytes

#endif  // SRC_TINT_UTILS_BYTES_ENDIANNESS_H_
