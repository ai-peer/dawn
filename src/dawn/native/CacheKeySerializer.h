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

#ifndef DAWNNATIVE_CACHE_KEY_SERIALIZER_H_
#define DAWNNATIVE_CACHE_KEY_SERIALIZER_H_

#include <string>
#include <vector>

namespace dawn::native {

    using CacheKey = std::vector<uint8_t>;

    // Templated serializer for integral types.
    template <typename T>
    std::enable_if_t<std::is_integral_v<T>> CacheKeySerialize(CacheKey* key, T t) {
        std::string str = std::to_string(t);
        key->insert(key->end(), str.begin(), str.end());
    }

    // Serializers for other common types.
    void CacheKeySerialize(CacheKey* key, const std::string& t);
    void CacheKeySerialize(CacheKey* key, const CacheKey& t);

    // Given list of arguments of types with a free implementation of CacheKeySerialize in the
    // dawn::native namespace, serializes each argument and appends them to the CacheKey while
    // prepending member ids before each argument.
    template <typename... Ts>
    CacheKey CacheKeySerializer(const Ts&... inputs) {
        CacheKey key;
        key.push_back('{');
        int memberId = 0;
        std::string delim = "";
        auto serialize = [&](auto& input) {
            std::string memberIdStr = delim + std::to_string(memberId++) + ":";
            key.insert(key.end(), memberIdStr.begin(), memberIdStr.end());
            CacheKeySerialize(&key, input);
            delim = ",";
        };
        (serialize(inputs), ...);
        key.push_back('}');
        return key;
    }

}  // namespace dawn::native

#endif  // DAWNNATIVE_CACHE_KEY_SERIALIZER_H_
