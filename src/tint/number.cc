// Copyright 2022 The Tint Authors.
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

#include "src/tint/number.h"

#include <algorithm>
#include <cstring>
#include <ostream>

namespace tint {

std::ostream& operator<<(std::ostream& out, ConversionFailure failure) {
    switch (failure) {
        case ConversionFailure::kExceedsPositiveLimit:
            return out << "value exceeds positive limit for type";
        case ConversionFailure::kExceedsNegativeLimit:
            return out << "value exceeds negative limit for type";
    }
    return out << "<unknown>";
}

f16::type f16::Quantize(f16::type value) {
    if (value > kHighest) {
        return std::numeric_limits<f16::type>::infinity();
    }
    if (value < kLowest) {
        return -std::numeric_limits<f16::type>::infinity();
    }
    // Below value must be within the finite range of a f16.
    // Assert we use binary32 (i.e. flaot) as underlying type, which has 4 bytes.
    static_assert(std::is_same<f16::type, float>());
    const uint32_t sign_mask = 0x80000000;      // Mask for the sign bit
    const uint32_t exponent_mask = 0x7f800000;  // Mask for 8 exponent bits
    // const uint32_t mantissa_mask = 0x007fffff;  // Mask for 23 mantissa bits

    uint32_t u32;
    memcpy(&u32, &value, 4);

    if ((u32 & ~sign_mask) == 0) {
        return value;                // +/- zero
    }
    if ((u32 & exponent_mask) == exponent_mask) {  // exponent all 1's
        return value;                        // inf or nan
    }

    // The smallest positive normal number representable by f16 is F16::kSmallest = 0x1p-14
    // (0_00001_0, 2^-14 = 1024*2^-24). The largest positive subnormal number representable by f16
    // is 0x1.FF8p-14 (0_00000_1111111111, 2^-14*2^-10*1023=1023*2^-24). The smallest positive
    // subnormal number representable by f16 is 0x1p-24 (binary 0_00000_0000000001,
    // 2^-14*2^-10=2^-24). If value can be represented as normal f16 number, i.e. abs(value) >=
    // 0x1p-14, we can just discard the extra mantissa bits.
    if (value >= kSmallest || value <= -kSmallest) {
        // f32 bits :  1 sign,  8 exponent, 23 mantissa
        // f16 bits :  1 sign,  5 exponent, 10 mantissa
        // Mask the value to preserve the sign, exponent and most-significant 10 mantissa bits.
        u32 = u32 & 0xffffe000u;
    } else {
        // value smaller than F16::kSmallest should be either represented as denormal f16 number, or
        // quantized to zero if too small.
        if (value >= 0x1p-24 || value <= -0x1p-24) {
            // Quantize to denormal f16 number.
            // Get the biased exponent of f32 value, e.g. value 127 representing exponent 2^0.
            uint32_t biased_exponent_original = (u32 & exponent_mask) >> 23;

            // For quantized into denormal f16 number, more mantissa bits will be discarded.
            // E.g., If value is 0x1.ABCDp-15, 10 mantissia bits of the corresponding denormal f16
            // number will be: leading 1 bit of 1, 9 bits from the significant part of ABCD. 23 - 9
            // = 14 mantissa bits in f32 should be discarded. If value is 0x1.ABCDp-20, the 10
            // mantissia bits will be: leading 5 bits of 0, 1 bit of 1, and 4 bits from the
            // significant part of ABCD. 23 - 4 = 19 mantissa bits in f32 should be discarded. If
            // value is 0x1.ABCDp-24, the 10 mantissia bits will be: leading 9 bits of 0, 1 bit of
            // 1, and no bit from ABCD. All 23 mantissa bits in f32 should be discarded. Note that
            // biased exponent is 112 for exponent 2^-15, and 103 for 2^-24.
            uint32_t discard_bits = 126 - biased_exponent_original;
            uint32_t discard_mask = (1u << discard_bits) - 1;
            u32 = u32 & ~discard_mask;
        } else {
            // value is too small that it can't even be represented as denormal f16 number. Quantize
            // to zero.
            return value > 0 ? 0.0 : -0.0;
        }
    }
    memcpy(&value, &u32, 4);
    return value;
}

}  // namespace tint
