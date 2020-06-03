// Copyright 2020 The Dawn Authors
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

#ifndef COMMON_TYPEINDEXEDSPAN_H_
#define COMMON_TYPEINDEXEDSPAN_H_

#include "common/TypedInteger.h"

#include <type_traits>

// TypedIndexSpan is a helper class that wraps an unowned packed array of type |Value|.
// It stores the size and pointer to first element. This provides a type-safe way to index
// raw pointers.
template <typename Index, typename Value>
class TypeIndexedSpan {
    using I = std::underlying_type_t<Index>;

  public:
    constexpr TypeIndexedSpan() : mData(nullptr), mSize(0) {
    }
    constexpr TypeIndexedSpan(Value* data, Index size) : mData(data), mSize(size) {
    }

    constexpr Value& operator[](Index i) const {
        ASSERT(i < mSize);
        return mData[static_cast<I>(i)];
    }

    Value* begin() noexcept {
        return mData;
    }

    const Value* begin() const noexcept {
        return mData;
    }

    Value* end() noexcept {
        return mData + mSize;
    }

    const Value* end() const noexcept {
        return mData + mSize;
    }

    Index size() const {
        return mSize;
    }

  private:
    Value* mData;
    Index mSize;
};

#endif  // COMMON_TYPEINDEXEDSPAN_H_
