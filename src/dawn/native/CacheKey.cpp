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

#include "dawn/native/CacheKey.h"

#include <iomanip>
#include <string>
#include <string_view>

namespace dawn::native {

CacheKey::CacheKey() : CacheKeyRecorder(this) {}

void* CacheKey::GetSpace(size_t bytes) {
    size_t currentSize = this->size();
    this->resize(currentSize + bytes);
    return &this->operator[](currentSize);
}

std::ostream& operator<<(std::ostream& os, const CacheKey& key) {
    os << std::hex;
    for (const int b : key) {
        os << std::setfill('0') << std::setw(2) << b << " ";
    }
    os << std::dec;
    return os;
}

template <>
void CacheKeySerializer<CacheKey>::Serialize(serde::Sink* sink, const CacheKey& t) {
    // For nested cache keys, we do not record the length, and just copy the key so that it
    // appears we just flatten the keys into a single key.
    size_t size = t.size();
    void* ptr = sink->GetSpace(size);
    memcpy(ptr, t.data(), size);
}

}  // namespace dawn::native
