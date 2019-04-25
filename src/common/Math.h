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

#ifndef COMMON_MATH_H_
#define COMMON_MATH_H_

#include <cstddef>
#include <cstdint>

#include "common/Assert.h"

// The following are not valid for 0
uint32_t ScanForward(uint32_t bits);
bool IsPowerOfTwo(size_t n);

template <typename T>
T Log2(T value) {
    ASSERT(value != 0);
#if defined(DAWN_COMPILER_MSVC)
    unsigned long firstBitIndex = 0ul;
    unsigned char ret = (sizeof(T) == 4) ? _BitScanReverse(&firstBitIndex, value)
                                         : _BitScanReverse64(&firstBitIndex, value);
    ASSERT(ret != 0);
    return firstBitIndex;
#else
    unsigned long numOfBits = (sizeof(T) * CHAR_BIT) - 1;
    return ((sizeof(T) == 4)) ? numOfBits - static_cast<uint32_t>(__builtin_clz(value))
                              : numOfBits - static_cast<uint32_t>(__builtin_clzll(value));
#endif
}

bool IsPtrAligned(const void* ptr, size_t alignment);
void* AlignVoidPtr(void* ptr, size_t alignment);
bool IsAligned(uint32_t value, size_t alignment);
uint32_t Align(uint32_t value, size_t alignment);

template <typename T>
T* AlignPtr(T* ptr, size_t alignment) {
    return reinterpret_cast<T*>(AlignVoidPtr(ptr, alignment));
}

template <typename T>
const T* AlignPtr(const T* ptr, size_t alignment) {
    return reinterpret_cast<const T*>(AlignVoidPtr(const_cast<T*>(ptr), alignment));
}

#endif  // COMMON_MATH_H_
