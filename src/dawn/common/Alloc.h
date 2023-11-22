// Copyright 2020 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_DAWN_COMMON_ALLOC_H_
#define SRC_DAWN_COMMON_ALLOC_H_

#include <cstddef>
#include <new>
#include <utility>

#include "dawn/common/Assert.h"
#include "dawn/common/NonCopyable.h"

namespace dawn {

namespace detail {

static inline bool NothrowAllocationTooLarge(size_t size) {
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
    if (size >= 0x70000000) {
        // std::nothrow isn't implemented in sanitizers and they often have a 2GB allocation
        // limit. Catch large allocations and error out so fuzzers make progress.
        return true;
    }
#endif
    return false;
}

}  // namespace detail

template <typename T>
T* AllocNoThrow(size_t count) {
    if (detail::NothrowAllocationTooLarge(count * sizeof(T))) {
        return nullptr;
    }
    return new (std::nothrow) T[count];
}

template <size_t Alignment>
class AlignedByteArray : NonCopyable {
  public:
    AlignedByteArray() = default;

    AlignedByteArray(AlignedByteArray&& rhs) {
        mData = rhs.mData;
        rhs.mData = nullptr;
    }

    AlignedByteArray& operator=(AlignedByteArray&& rhs) {
        if (this != &rhs) {
            std::swap(mData, rhs.mData);
        }
        return *this;
    }

    explicit AlignedByteArray(size_t size)
        : mData(new(std::align_val_t(Alignment)) uint8_t[size]) {}
    AlignedByteArray(size_t size, std::nothrow_t) {
        if (detail::NothrowAllocationTooLarge(size)) {
            return;
        }
        mData = new (std::align_val_t(Alignment), std::nothrow) uint8_t[size];
    }

    ~AlignedByteArray() { reset(); }

    operator bool() { return mData != nullptr; }

    uint8_t* get() {
        DAWN_ASSERT(mData != nullptr);
        return mData;
    }

    void reset() {
        if (mData != nullptr) {
            ::operator delete[](mData, std::align_val_t(Alignment));
            mData = nullptr;
        }
    }

  private:
    uint8_t* mData = nullptr;
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_ALLOC_H_
