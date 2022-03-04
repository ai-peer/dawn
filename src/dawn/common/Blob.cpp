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

#include "dawn/common/Blob.h"

#include <functional>
#include <string_view>

namespace dawn {

    Blob::Blob(const Blob& rhs) {
        mSize = rhs.mSize;
        if (rhs.mData) {
            mData = std::unique_ptr<char[]>(new char[rhs.mSize]);
            memcpy(mData.get(), rhs.mData.get(), rhs.mSize);
        } else {
            mData = nullptr;
        }
    }

    Blob& Blob::operator=(const Blob& rhs) {
        if (&rhs == this) {
            return *this;
        }

        mSize = rhs.mSize;
        if (rhs.mData) {
            mData = std::unique_ptr<char[]>(new char[rhs.mSize]);
            memcpy(mData.get(), rhs.mData.get(), rhs.mSize);
        } else {
            mData = nullptr;
        }
        return *this;
    }

    bool Blob::operator==(const Blob& rhs) const {
        return mSize == rhs.mSize && memcmp(mData.get(), rhs.mData.get(), mSize) == 0;
    }

    size_t Blob::HashFunc::operator()(const Blob& blob) const {
        return std::hash<std::string_view>{}(std::string_view(blob.Data(), blob.Size()));
    }

}  // namespace dawn
