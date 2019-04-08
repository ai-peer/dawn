// Copyright 2017 The Dawn Authors
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

#ifndef UTILS_MATH_H_
#define UTILS_MATH_H_

#include <limits>
#include <type_traits>

namespace utils {

    template <typename T>
    float Normalize(T value) {
        static_assert(std::is_integral<T>::value, "Integer required.");
        if (std::is_signed<T>::value) {
            typedef typename std::make_unsigned<T>::type unsigned_type;
            return (2.0f * static_cast<float>(value) + 1.0f) /
                   static_cast<float>(std::numeric_limits<unsigned_type>::max());
        } else {
            return static_cast<float>(value) / static_cast<float>(std::numeric_limits<T>::max());
        }
    }

    template <typename destType, typename sourceType>
    destType bitCast(const sourceType& source) {
        size_t copySize = std::min(sizeof(destType), sizeof(sourceType));
        destType output;
        memcpy(&output, &source, copySize);
        return output;
    }

    unsigned short float32ToFloat16(float fp32) {
        unsigned int fp32i = bitCast<unsigned int>(fp32);
        unsigned int sign = (fp32i & 0x80000000) >> 16;
        unsigned int abs = fp32i & 0x7FFFFFFF;

        if (abs > 0x47FFEFFF) {  // Infinity
            return static_cast<unsigned short>(sign | 0x7FFF);
        } else if (abs < 0x38800000) {  // Denormal
            unsigned int mantissa = (abs & 0x007FFFFF) | 0x00800000;
            int e = 113 - (abs >> 23);

            if (e < 24) {
                abs = mantissa >> e;
            } else {
                abs = 0;
            }

            return static_cast<unsigned short>(sign | (abs + 0x00000FFF + ((abs >> 13) & 1)) >> 13);
        } else {
            return static_cast<unsigned short>(
                sign | (abs + 0xC8000000 + 0x00000FFF + ((abs >> 13) & 1)) >> 13);
        }
    }
}  // namespace utils

#endif  // UTILS_MATH_H_