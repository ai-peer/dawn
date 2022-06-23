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

#include <utility>

#include "dawn/common/Assert.h"
#include "dawn/common/Math.h"
#include "dawn/native/Blob.h"

namespace dawn::native {

namespace {

template <typename T>
std::pair<void*, std::function<void()>> AllocDataAsArray(size_t byteLength) {
    static_assert(alignof(T) == sizeof(T));
    T* data = new T[Align(byteLength, alignof(T)) / sizeof(T)];
    std::function<void()> deleter = [=]() { delete[] data; };
    return {data, std::move(deleter)};
}

}  // namespace

Blob CreateBlob(size_t size, size_t alignment) {
    ASSERT(IsPowerOfTwo(alignment));
    ASSERT(alignment != 0);
    if (size > 0) {
        void* ptr;
        std::function<void()> deleter;
        switch (alignment) {
            case 1: {
                uint8_t* data = new uint8_t[size];
                ptr = data;
                deleter = [=]() { delete[] data; };
                break;
            }
            case alignof(uint16_t): {
                std::tie(ptr, deleter) = AllocDataAsArray<uint16_t>(size);
                break;
            }
            case alignof(uint32_t): {
                std::tie(ptr, deleter) = AllocDataAsArray<uint32_t>(size);
                break;
            }
            case alignof(uint64_t): {
                std::tie(ptr, deleter) = AllocDataAsArray<uint64_t>(size);
                break;
            }
            default: {
                // Allocate extra space so that there will be sufficient space for |size| even after
                // the |data| pointer is aligned.
                // TODO(crbug.com/dawn/824): Use aligned_alloc when possible. It should be available
                // with C++17 but on macOS it also requires macOS 10.15 to work.
                size_t allocatedSize = size + alignment - 1;
                uint8_t* data = new uint8_t[allocatedSize];
                ptr = AlignPtr(data, alignment);
                deleter = [=]() { delete[] data; };
            }
        }
        return Blob::UnsafeCreateWithDeleter(static_cast<uint8_t*>(ptr), size, std::move(deleter));
    } else {
        return Blob();
    }
}

// static
Blob Blob::UnsafeCreateWithDeleter(uint8_t* data, size_t size, std::function<void()> deleter) {
    return Blob(data, size, deleter);
}

Blob::Blob() : mData(nullptr), mSize(0), mDeleter({}) {}

Blob::Blob(uint8_t* data, size_t size, std::function<void()> deleter)
    : mData(data), mSize(size), mDeleter(std::move(deleter)) {
    // It is invalid to make a blob that has null data unless its size is also zero.
    ASSERT(data != nullptr || size == 0);
}

Blob::Blob(Blob&& rhs) : mData(rhs.mData), mSize(rhs.mSize) {
    mDeleter = std::move(rhs.mDeleter);
}

Blob& Blob::operator=(Blob&& rhs) {
    mData = rhs.mData;
    mSize = rhs.mSize;
    if (mDeleter) {
        mDeleter();
    }
    mDeleter = std::move(rhs.mDeleter);
    return *this;
}

Blob::~Blob() {
    if (mDeleter) {
        mDeleter();
    }
}

bool Blob::Empty() const {
    return mSize == 0;
}

const uint8_t* Blob::Data() const {
    return mData;
}

uint8_t* Blob::Data() {
    return mData;
}

size_t Blob::Size() const {
    return mSize;
}

void Blob::AlignTo(size_t alignment) {
    if (IsPtrAligned(mData, alignment)) {
        return;
    }

    Blob blob = CreateBlob(mSize, alignment);
    memcpy(blob.Data(), mData, mSize);
    *this = std::move(blob);
}

}  // namespace dawn::native
