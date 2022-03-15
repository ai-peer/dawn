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

#ifndef DAWNNATIVE_CACHE_KEY_H_
#define DAWNNATIVE_CACHE_KEY_H_

#include <string>
#include <vector>

namespace dawn::native {

    // Inherit instead of using statement to allow for class specialization.
    class CacheKey : public std::vector<uint8_t> {};

    // Forwarding template called in details to forward serialization for overloaded types.
    // This function should NOT be called by external users, but cannot live inside the
    // 'detail' namespace because of the forward declaration.
    template <typename T>
    void SerializeIntoForwarder(CacheKey* key, const T& t) {
        // Non-builtin types should implement an overload, do the forward declaration. Note that
        // MSVC seems to have a bug when linking and is unable to find the function without explicit
        // namespace, but other compilers do not have this issue, and actually fail to build with
        // the namespace, hence the ifdef.
#if defined(DAWN_COMPILER_MSVC)
        void ::dawn::native::SerializeInto(CacheKey*, const T&);
#else
        void SerializeInto(CacheKey*, const T&);
#endif
        SerializeInto(key, t);
    }

    // Explicitly forward declare the SerializeInto overload for CacheKey's since it is used
    // for nested data structure often.
    void SerializeInto(CacheKey* key, const CacheKey&);

    namespace detail {

        // Base helper handles some builtin types manually, otherwise forwards to user implemented
        // overload.
        //     Types:
        //         - integral/floats: Serialized as a string to avoid handling 0 potentially being
        //                            null terminator in string form. For floats, the default is
        //                            a precision of 6 decimals.
        template <typename T>
        void SerializeIntoHelper(CacheKey* key, const T& t) {
            if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
                std::string str = std::to_string(t);
                key->insert(key->end(), str.begin(), str.end());
            } else {
                SerializeIntoForwarder(key, t);
            }
        }

        // Specialized overload for string literals. Note we drop the null-terminator.
        template <size_t N>
        void SerializeIntoHelper(CacheKey* key, const char (&t)[N]) {
            std::string len = std::to_string(N - 1);
            key->insert(key->end(), len.begin(), len.end());
            key->push_back('"');
            key->insert(key->end(), t, t + N - 1);
            key->push_back('"');
        }

    }  // namespace detail

    // Given list of arguments of types with a free implementation of SerializeIntoImpl in the
    // dawn::native namespace, serializes each argument and appends them to the CacheKey while
    // prepending member ids before each argument.
    template <typename... Ts>
    CacheKey GetCacheKey(const Ts&... inputs) {
        CacheKey key;
        key.push_back('{');
        int memberId = 0;
        auto Serialize = [&](const auto& input) {
            std::string memberIdStr = (memberId == 0 ? "" : ",") + std::to_string(memberId) + ":";
            key.insert(key.end(), memberIdStr.begin(), memberIdStr.end());
            detail::SerializeIntoHelper(&key, input);
            memberId++;
        };
        (Serialize(inputs), ...);
        key.push_back('}');
        return key;
    }

}  // namespace dawn::native

#endif  // DAWNNATIVE_CACHE_KEY_H_
