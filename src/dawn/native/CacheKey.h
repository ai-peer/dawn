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

#ifndef SRC_DAWN_NATIVE_CACHEKEY_H_
#define SRC_DAWN_NATIVE_CACHEKEY_H_

#include <algorithm>
#include <bitset>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "dawn/common/Platform.h"
#include "dawn/common/TypedInteger.h"
#include "dawn/common/ityp_array.h"
#include "dawn/native/serde/Serde.h"

namespace dawn::native {

// Forward declare classes because of co-dependency.
class CacheKey;
class CachedObject;

// Stream operator for CacheKey for debugging.
std::ostream& operator<<(std::ostream& os, const CacheKey& key);

// Overridable serializer struct that should be implemented for cache key serializable
// types/classes.
template <typename T, typename SFINAE = void>
class CacheKeySerializer {
  public:
    static void Serialize(serde::Sink* sink, const T& t);
};

// If serde::Serde provides Serialize, use its implementation instead of our own.
template <typename T>
class CacheKeySerializer<T, std::enable_if_t<serde::HasSerialize<serde::Serde, T>>>
    : public serde::Serde<T> {};

// Wrapper around serde::Sink to help record CacheKeys.
class CacheKeyRecorder {
  public:
    explicit CacheKeyRecorder(serde::Sink* sink) : mSink(sink) {}

    template <typename... Ts>
    CacheKeyRecorder& Record(const Ts&... ts) {
        (serde::Serialize<CacheKeySerializer>(mSink, ts), ...);
        return *this;
    }

    template <typename IterableT>
    CacheKeyRecorder& RecordIterable(const IterableT& iterable) {
        serde::Serialize<CacheKeySerializer>(mSink, iterable.size());
        for (const auto& it = iterable.begin(); it != iterable.end(); ++it) {
            serde::Serialize<CacheKeySerializer>(*it);
        }
        return *this;
    }

    template <typename T>
    CacheKeyRecorder& RecordIterable(const T* ptr, size_t n) {
        serde::Serialize<CacheKeySerializer>(mSink, n);
        for (size_t i = 0; i < n; ++i) {
            serde::Serialize<CacheKeySerializer>(mSink, ptr[i]);
        }
        return *this;
    }

  private:
    serde::Sink* mSink;
};

class CacheKey : public std::vector<uint8_t>, public serde::Sink, public CacheKeyRecorder {
  public:
    using std::vector<uint8_t>::vector;

    enum class Type { ComputePipeline, RenderPipeline, Shader };

    template <typename T>
    class UnsafeUnkeyedValue {
      public:
        UnsafeUnkeyedValue() = default;
        // NOLINTNEXTLINE(runtime/explicit) allow implicit construction to decrease verbosity
        UnsafeUnkeyedValue(T&& value) : mValue(std::forward<T>(value)) {}

        const T& UnsafeGetValue() const { return mValue; }

      private:
        T mValue;
    };

    CacheKey();

    // Implementation of serde::Sink
    void* GetSpace(size_t bytes) override;
};

template <typename T>
CacheKey::UnsafeUnkeyedValue<T> UnsafeUnkeyedValue(T&& value) {
    return CacheKey::UnsafeUnkeyedValue<T>(std::forward<T>(value));
}

// Specialized overload for CacheKey::UnsafeIgnoredValue which does nothing.
template <typename T>
class CacheKeySerializer<CacheKey::UnsafeUnkeyedValue<T>> {
  public:
    constexpr static void Serialize(serde::Sink* key, const CacheKey::UnsafeUnkeyedValue<T>&) {}
};

// Specialized overload for raw function pointers. Raw function pointers don't contain any bound
// data so they do not contribute to the CacheKey.
template <typename R, typename... Args>
class CacheKeySerializer<R (*)(Args...)> {
    using T = R (*)(Args...);

  public:
    constexpr static void Serialize(serde::Sink* sink, const T& t) {}
};

#if DAWN_PLATFORM_IS(WINDOWS) && DAWN_PLATFORM_IS_32_BIT
template <typename R, typename... Args>
class CacheKeySerializer<R(__stdcall*)(Args...)> {
    using T = R(__stdcall*)(Args...);

  public:
    constexpr static void Serialize(serde::Sink* sink, const T& t) {}
};
#endif

// Specialized overload for pointers. Since we are serializing for a cache key, we always
// serialize via value, not by pointer. To handle nullptr scenarios, we always serialize whether
// the pointer was nullptr followed by the contents if applicable.
template <typename T>
class CacheKeySerializer<T, std::enable_if_t<std::is_pointer_v<T>>> {
  public:
    static void Serialize(serde::Sink* sink, const T& t) {
        serde::Serialize<CacheKeySerializer>(sink, t == nullptr);
        if (t != nullptr) {
            serde::Serialize<CacheKeySerializer>(sink, *t);
        }
    }
};

// Specialized overload for CachedObjects.
template <typename T>
class CacheKeySerializer<T, std::enable_if_t<std::is_base_of_v<CachedObject, T>>> {
  public:
    static void Serialize(serde::Sink* sink, const T& t) {
        serde::Serialize<CacheKeySerializer>(sink, t.GetCacheKey());
    }
};

// Specialized overload for std::unordered_map<K, V> which sorts the entries.
// CacheKey requires a stable ordering.
template <typename K, typename V>
class CacheKeySerializer<std::unordered_map<K, V>> {
  public:
    static void Serialize(serde::Sink* sink, const std::unordered_map<K, V>& m) {
        std::vector<std::pair<K, V>> ordered(m.begin(), m.end());
        std::sort(ordered.begin(), ordered.end(),
                  [](const std::pair<K, V>& a, const std::pair<K, V>& b) {
                      return std::less<K>{}(a.first, b.first);
                  });
        serde::Serialize<CacheKeySerializer>(sink, ordered);
    }
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CACHEKEY_H_
