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

#ifndef DAWN_COMMON_BLOB_H_
#define DAWN_COMMON_BLOB_H_

#include <memory>

namespace dawn {

    class Blob {
      public:
        Blob() : mData(nullptr), mSize(0) {
        }
        Blob(std::unique_ptr<char[]> data, size_t size) : mData(std::move(data)), mSize(size) {
        }

        Blob(Blob&& rhs) = default;
        Blob& operator=(Blob&& rhs) = default;

        Blob(const Blob& rhs);
        Blob& operator=(const Blob& rhs);

        char* Data() const {
            return mData.get();
        }
        size_t Size() const {
            return mSize;
        }

        bool operator==(const Blob& rhs) const;

        struct HashFunc {
            size_t operator()(const Blob& blob) const;
        };

      private:
        std::unique_ptr<char[]> mData;
        size_t mSize;
    };

}  // namespace dawn

#endif  // DAWN_COMMON_BLOB_H_
