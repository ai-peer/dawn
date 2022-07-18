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

#ifndef SRC_TINT_UTILS_BITSET_H_
#define SRC_TINT_UTILS_BITSET_H_

#include <stdint.h>

#include "src/tint/utils/vector.h"

namespace tint::utils {

template <size_t N = 0>
class Bitset {
    using Word = size_t;
    static constexpr size_t kWordBits = sizeof(Word) * 8;
    static constexpr size_t NumWords(size_t num_bits) {
        return ((num_bits + kWordBits - 1) / kWordBits);
    }

  public:
    Bitset() = default;
    ~Bitset() = default;

    struct Bit {
        Word& word;
        Word const mask;

        const Bit& operator=(bool value) const {
            if (value) {
                word = word | mask;
            } else {
                word = word & ~mask;
            }
            return *this;
        }

        operator bool() const { return (word & mask) != 0; }
    };

    void Resize(size_t new_size) { vec_.Resize(new_size); }

    Bit operator[](size_t index) {
        auto& word = vec_[index / kWordBits];
        auto mask = static_cast<Word>(1) << (index % kWordBits);
        return Bit{word, mask};
    }

  private:
    Vector<size_t, NumWords(N)> vec_;
};

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_BITSET_H_
