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

#ifndef COMMON_BITSETITERATOR_H_
#define COMMON_BITSETITERATOR_H_

#include "dawn/common/Assert.h"
#include "dawn/common/Math.h"
#include "dawn/common/UnderlyingType.h"

#include <bitset>
#include <limits>

// This is ANGLE's BitSetIterator class with a customizable return type
// TODO(crbug.com/dawn/306): it could be optimized, in particular when N <= 64

template <typename T>
T roundUp(const T value, const T alignment) {
    auto temp = value + alignment - static_cast<T>(1);
    return temp - temp % alignment;
}

template <size_t N, typename T>
class BitSetIterator final {
  public:
    BitSetIterator(const std::bitset<N>& bitset);
    BitSetIterator(const BitSetIterator& other);
    BitSetIterator& operator=(const BitSetIterator& other);

    class Iterator final {
      public:
        Iterator(const std::bitset<N>& bits);
        Iterator& operator++();

        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;

        T operator*() const {
            using U = UnderlyingType<T>;
            ASSERT(static_cast<U>(mCurrentBit) <= std::numeric_limits<U>::max());
            return static_cast<T>(static_cast<U>(mCurrentBit));
        }

      private:
        unsigned long getNextBit();

        static constexpr size_t kBitsPerWord = sizeof(uint32_t) * 8;
        std::bitset<N> mBits;
        unsigned long mCurrentBit;
        unsigned long mOffset;
    };

    Iterator begin() const {
        return Iterator(mBits);
    }
    Iterator end() const {
        return Iterator(std::bitset<N>(0));
    }

    // Assume we have bitset of at most 64 bits
    // Returns i which is the next integer of the index of the highest bit
    // i == 0 if there is no bit is set to true
    // i == 1 if only the least significant bit (at index 0) is the bit set to true with the highest
    // index
    // ...
    // i == 64 if the most significant bit (at index 64) is the bit set to true with the highest
    // index
    uint64_t GetHighestBitIndexExclusive() const {
#if defined(DAWN_COMPILER_MSVC)
#    if defined(DAWN_PLATFORM_64_BIT)
        unsigned long firstBitIndex = 0ul;
        unsigned char ret = _BitScanReverse64(&firstBitIndex, mBits.to_ullong());
        if (ret == 0) {
            return 0;
        }
        return firstBitIndex + 1;
#    else   // defined(DAWN_PLATFORM_64_BIT)
        if (mBits.none()) {
            return 0;
        }
        for (uint32_t i = 0u; i < N; i++) {
            if (mBits[N - 1 - i]) {
                return N - i;
            }
        }
        UNREACHABLE();
#    endif  // defined(DAWN_PLATFORM_64_BIT)
#else       // defined(DAWN_COMPILER_MSVC)
        if (mBits.none()) {
            return 0;
        }
        return 64 - static_cast<uint32_t>(__builtin_clzll(mBits.to_ullong()));
#endif      // defined(DAWN_COMPILER_MSVC)
    }

  private:
    const std::bitset<N> mBits;
};

template <size_t N, typename T>
BitSetIterator<N, T>::BitSetIterator(const std::bitset<N>& bitset) : mBits(bitset) {
}

template <size_t N, typename T>
BitSetIterator<N, T>::BitSetIterator(const BitSetIterator& other) : mBits(other.mBits) {
}

template <size_t N, typename T>
BitSetIterator<N, T>& BitSetIterator<N, T>::operator=(const BitSetIterator& other) {
    mBits = other.mBits;
    return *this;
}

template <size_t N, typename T>
BitSetIterator<N, T>::Iterator::Iterator(const std::bitset<N>& bits)
    : mBits(bits), mCurrentBit(0), mOffset(0) {
    if (bits.any()) {
        mCurrentBit = getNextBit();
    } else {
        mOffset = static_cast<unsigned long>(roundUp(N, kBitsPerWord));
    }
}

template <size_t N, typename T>
typename BitSetIterator<N, T>::Iterator& BitSetIterator<N, T>::Iterator::operator++() {
    DAWN_ASSERT(mBits.any());
    mBits.set(mCurrentBit - mOffset, 0);
    mCurrentBit = getNextBit();
    return *this;
}

template <size_t N, typename T>
bool BitSetIterator<N, T>::Iterator::operator==(const Iterator& other) const {
    return mOffset == other.mOffset && mBits == other.mBits;
}

template <size_t N, typename T>
bool BitSetIterator<N, T>::Iterator::operator!=(const Iterator& other) const {
    return !(*this == other);
}

template <size_t N, typename T>
unsigned long BitSetIterator<N, T>::Iterator::getNextBit() {
    static std::bitset<N> wordMask(std::numeric_limits<uint32_t>::max());

    while (mOffset < N) {
        uint32_t wordBits = static_cast<uint32_t>((mBits & wordMask).to_ulong());
        if (wordBits != 0ul) {
            return ScanForward(wordBits) + mOffset;
        }

        mBits >>= kBitsPerWord;
        mOffset += kBitsPerWord;
    }
    return 0;
}

// Helper to avoid needing to specify the template parameter size
template <size_t N>
BitSetIterator<N, uint32_t> IterateBitSet(const std::bitset<N>& bitset) {
    return BitSetIterator<N, uint32_t>(bitset);
}

#endif  // COMMON_BITSETITERATOR_H_
