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

#ifndef DAWN_COMMON_TRAITS_SERIALIZE_H_
#define DAWN_COMMON_TRAITS_SERIALIZE_H_

#include "dawn/common/TypeID.h"

#include <unordered_map>

namespace dawn {

    template <typename T>
    struct Unkeyed;

    template <typename T>
    struct SerializeTraits {
        static constexpr size_t Size(const T& value) {
            static_assert(std::is_pod_v<T> && !std::is_pointer_v<T>,
                          "Please specialize SerializeTraits.");
            return sizeof(T);
        }

        static void Write(char** ptr, const T& value) {
            static_assert(std::is_pod_v<T> && !std::is_pointer_v<T>,
                          "Please specialize SerializeTraits.");
            memcpy(*ptr, &value, sizeof(T));
            *ptr += sizeof(T);
        }
    };

    template <typename K, typename V>
    struct SerializeTraits<std::unordered_map<K, V>> {
        static size_t Size(const std::unordered_map<K, V>& map) {
            // unstable ordering - ok?
            size_t size = sizeof(size_t);
            size += sizeof(TypeID_t);
            size += sizeof(TypeID_t);
            for (const auto& [k, v] : map) {
                size += SerializeTraits<K>::Size(k);
                size += SerializeTraits<V>::Size(v);
            }
            return size;
        }

        static void Write(char** ptr, const std::unordered_map<K, V>& map) {
            // unstable ordering - ok?
            SerializeTraits<size_t>::Write(ptr, map.size());
            SerializeTraits<TypeID_t>::Write(ptr, TypeID<K>);
            SerializeTraits<TypeID_t>::Write(ptr, TypeID<V>);
            for (const auto& [k, v] : map) {
                SerializeTraits<K>::Write(ptr, k);
                SerializeTraits<V>::Write(ptr, v);
            }
        }
    };

    template <typename T>
    struct SerializeTraits<std::optional<T>> {
        static constexpr size_t Size(const std::optional<T>& v) {
            if (v) {
                return sizeof(bool) + SerializeTraits<T>::Size(*v);
            } else {
                return sizeof(bool);
            }
        }

        static void Write(char** ptr, const std::optional<T>& v) {
            if (v) {
                SerializeTraits<bool>::Write(ptr, true);
                SerializeTraits<T>::Write(ptr, *v);
            } else {
                SerializeTraits<bool>::Write(ptr, false);
            }
        }
    };

    template <>
    struct SerializeTraits<const char*> {
        static size_t Size(const char* s) {
            return strlen(s) + 1;
        }

        static void Write(char** ptr, const char* s) {
            size_t len = strlen(s);
            memcpy(*ptr, s, len);
            (*ptr)[len] = '\0';
            *ptr += len + 1;
        }
    };

    template <typename T>
    struct SerializeTraits<Unkeyed<T>> {
        static constexpr size_t Size(const Unkeyed<T>&) {
            return 0;
        }

        static void Write(char**, const Unkeyed<T>&) {
        }
    };

}  // namespace dawn

#endif  // DAWN_COMMON_TRAITS_SERIALIZE_H_
