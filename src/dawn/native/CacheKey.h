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

#include <limits>
#include <string>
#include <vector>

#include "dawn/common/Assert.h"

namespace dawn::native {

    // Inherit instead of using statement to allow for class specialization.
    class CacheKey : public std::vector<uint8_t> {
        using std::vector<uint8_t>::vector;
    };

    // Overridable serializer struct that should be implemented for cache key serializable
    // types/classes.
    template <typename T, typename SFINAE = void>
    class CacheKeySerializer {
      public:
        static void Serialize(CacheKey* key, const T& t);
    };

    // Helper template function that defers to underlying static functions.
    template <typename T>
    void SerializeInto(CacheKey* key, const T& t) {
        CacheKeySerializer<T>::Serialize(key, t);
    }

    // Specialized overload for integral types.
    template <typename Integer>
    class CacheKeySerializer<Integer, std::enable_if_t<std::is_integral_v<Integer>>> {
      public:
        static void Serialize(CacheKey* key, const Integer t) {
            const char* it = reinterpret_cast<const char*>(&t);
            for (size_t i = 0; i < sizeof(Integer); i++) {
                key->push_back(*(it + i));
            }
        }
    };

    // Specialized overload for floating point types.
    template <typename Float>
    class CacheKeySerializer<Float, std::enable_if_t<std::is_floating_point_v<Float>>> {
      public:
        static void Serialize(CacheKey* key, const Float t) {
            const char* it = reinterpret_cast<const char*>(&t);
            for (size_t i = 0; i < sizeof(Float); i++) {
                key->push_back(*(it + i));
            }
        }
    };

    // Specialized overload for string literals. Note we drop the null-terminator.
    template <size_t N>
    class CacheKeySerializer<char[N]> {
      public:
        static void Serialize(CacheKey* key, const char (&t)[N]) {
            SerializeInto(key, (size_t)(N - 1));
            key->insert(key->end(), t, t + N - 1);
        }
    };

    // Helper class used as a temporary to generate cache keys. The class helps keep track of state
    // such as member ids.
    class CacheKeyGenerator {
      public:
        // Currently we assume that cache keys will not have more than 256 unique flattened members.
        using MemberId = uint8_t;

        CacheKeyGenerator() = default;

        // Sub-generator ctor to serialize into an already existing key to avoid unnecessary copies.
        explicit CacheKeyGenerator(CacheKeyGenerator& parent);

        // Calls into appropriately overwritten serialization function for each argument.
        template <typename T>
        CacheKeyGenerator& Record(const T& value) {
            ASSERT(mMemberId <= std::numeric_limits<MemberId>::max());
            SerializeInto(mKey, mMemberId++);
            SerializeInto(mKey, value);
            return *this;
        }
        template <typename T, typename... Args>
        CacheKeyGenerator& Record(const T& value, const Args&... args) {
            return Record(value).Record(args...);
        }

        // Records iterables by prepending the number of elements. Note that all elements in a
        // single iterable are recorded under one member id. Some common iterables are have a
        // SerializeInto implemented to avoid needing to split them out when recording, i.e.
        // strings and CacheKeys, but they fundamentally do the same as this function.
        template <typename IterableT>
        CacheKeyGenerator& RecordIterable(const IterableT& iterable) {
            SerializeInto(mKey, mMemberId++);
            // Always serialize the size of the iterable as a size_t for now.
            SerializeInto(mKey, (size_t)iterable.size());
            for (auto it = iterable.begin(); it != iterable.end(); ++it) {
                SerializeInto(mKey, *it);
            }
            return *this;
        }

        // Returns the generated cache key iff this is the outer-most generator. Sub generators
        // should not call this function.
        CacheKey GetCacheKey() const;

      private:
        bool mIsSubGenerator = false;
        MemberId mMemberId = 0;
        CacheKey mDefaultKey;
        CacheKey* mKey = &mDefaultKey;
    };

}  // namespace dawn::native

#endif  // DAWNNATIVE_CACHE_KEY_H_
