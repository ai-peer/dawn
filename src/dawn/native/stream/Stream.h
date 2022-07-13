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

#ifndef SRC_DAWN_NATIVE_STREAM_STREAM_H_
#define SRC_DAWN_NATIVE_STREAM_STREAM_H_

#include <algorithm>
#include <bitset>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "dawn/common/Platform.h"
#include "dawn/common/TypedInteger.h"
#include "dawn/native/Error.h"
#include "dawn/native/stream/Sink.h"
#include "dawn/native/stream/Source.h"

namespace dawn::native::stream {

// Base Stream template for specialization. Specializations may define static methods:
//   static void Write(Sink* s, const T& v);
//   static MaybeError Read(Source* s, T* v);
template <typename T, typename SFINAE = void>
class Stream {
  public:
    static void Write(Sink* s, const T& v);
    static MaybeError Read(Source* s, T* v);
};

// Helper to call Stream<T>::Write(.
// By default, calling Serialize will call this overload inside the stream namespace.
// Other definitons of Serialize found by ADL may override this one.
template <typename T>
constexpr void StreamIn(Sink* s, const T& v) {
    Stream<T>::Write(s, v);
}

// Helper to call Stream<T>::Read(
// By default, calling Deserialize will call this overload inside the stream namespace.
// Other definitons of Deserialize found by ADL may override this one.
template <typename T>
MaybeError StreamOut(Source* s, T* v) {
    return Stream<T>::Read(s, v);
}

// Helper to call Serialize on a parameter pack.
template <typename T, typename T2, typename... Ts>
constexpr void StreamIn(Sink* s, T&& v, T2&& v2, Ts&&... vs) {
    StreamIn(s, std::forward<T>(v));
    StreamIn(s, std::forward<T2>(v2));
    (StreamIn(s, std::forward<Ts>(vs)), ...);
}

// Helper to call Deserialize on a parameter pack.
template <typename T, typename T2, typename... Ts>
MaybeError StreamOut(Source* s, T* v, T2* v2, Ts*... vs) {
    DAWN_TRY(StreamOut(s, v));
    DAWN_TRY(StreamOut(s, v2));
    return StreamOut(s, vs...);
}

// Stream specialization for fundamental types.
template <typename T>
class Stream<T, std::enable_if_t<std::is_fundamental_v<T>>> {
  public:
    static void Write(Sink* s, const T& v) { memcpy(s->GetSpace(sizeof(T)), &v, sizeof(T)); }
    static MaybeError Read(Source* s, T* v) {
        const void* ptr;
        DAWN_TRY(s->Read(&ptr, sizeof(T)));
        memcpy(v, ptr, sizeof(T));
        return {};
    }
};

// Stream specialization for fixed arrays of fundamental types.
template <typename T, size_t N>
class Stream<T[N], std::enable_if_t<std::is_fundamental_v<T>>> {
  public:
    static void Write(Sink* s, const T (&t)[N]) {
        static_assert(N > 0);
        memcpy(s->GetSpace(sizeof(t)), &t, sizeof(t));
    }

    static MaybeError Read(Source* s, T (*t)[N]) {
        static_assert(N > 0);
        const void* ptr;
        DAWN_TRY(s->Read(&ptr, sizeof(*t)));
        memcpy(*t, ptr, sizeof(*t));
        return {};
    }
};

// Stream specialization for enums.
template <typename T>
class Stream<T, std::enable_if_t<std::is_enum_v<T>>> {
    using U = std::underlying_type_t<T>;

  public:
    static void Write(Sink* s, const T& v) { StreamIn(s, static_cast<U>(v)); }

    static MaybeError Read(Source* s, T* v) {
        U out;
        DAWN_TRY(StreamOut(s, &out));
        *v = static_cast<T>(out);
        return {};
    }
};

// Stream specialization for TypedInteger.
template <typename Tag, typename Integer>
class Stream<::detail::TypedIntegerImpl<Tag, Integer>> {
    using T = ::detail::TypedIntegerImpl<Tag, Integer>;

  public:
    static void Write(Sink* s, const T& t) { StreamIn(s, static_cast<Integer>(t)); }

    static MaybeError Read(Source* s, T* v) {
        Integer out;
        DAWN_TRY(StreamOut(s, &out));
        *v = T(out);
        return {};
    }
};

namespace detail {
// NOLINTNEXTLINE(runtime/int) Alias "long" type to match std::bitset to_ulong
using BitsetUlong = unsigned long;
// NOLINTNEXTLINE(runtime/int) Alias "long" type to match std::bitset to_ullong
using BitsetUllong = unsigned long long;
}  // namespace detail

// Stream specialization for bitsets that are smaller than ulong.
template <size_t N>
class Stream<std::bitset<N>,
             std::enable_if_t<std::less_equal<size_t>{}(N, 8 * sizeof(detail::BitsetUlong))>> {
  public:
    static void Write(Sink* s, const std::bitset<N>& t) { StreamIn(s, t.to_ulong()); }
    static MaybeError Read(Source* s, std::bitset<N>* v) {
        detail::BitsetUlong value;
        DAWN_TRY(StreamOut(s, &value));
        *v = std::bitset<N>(value);
        return {};
    }
};

// Stream specialization for bitsets that are larger than ulong, but smaller than ullong.
template <size_t N>
class Stream<std::bitset<N>,
             std::enable_if_t<std::less<size_t>{}(8 * sizeof(detail::BitsetUlong), N) &&
                              std::less_equal<size_t>{}(N, 8 * sizeof(detail::BitsetUllong))>> {
  public:
    static void Write(Sink* s, const std::bitset<N>& t) { StreamIn(s, t.to_ullong()); }

    static MaybeError Read(Source* s, std::bitset<N>* v) {
        detail::BitsetUllong value;
        DAWN_TRY(StreamOut(s, &value));
        *v = std::bitset<N>(value);
        return {};
    }
};

// Stream specialization for bitsets since using the built-in to_ullong has a size limit.
template <size_t N>
class Stream<std::bitset<N>,
             std::enable_if_t<std::greater<size_t>{}(N, 8 * sizeof(detail::BitsetUllong))>> {
  public:
    static void Write(Sink* s, const std::bitset<N>& t) {
        uint8_t byte = 0;
        // Iterate in 8-bit chunks.
        for (size_t i = 0; i < N; i += 8, byte = 0) {
            // Fill out an 8-bit value.
            for (size_t j = i; j < i + 8 && j < N; j++) {
                byte <<= 1;
                byte |= t[j] ? 1 : 0;
            }
            // Serialize the value.
            StreamIn(s, byte);
        }
    }

    static MaybeError Read(Source* s, std::bitset<N>* v) {
        static_assert(N > 0);
        *v = {};
        // Deserialize 8-bit chunks.
        for (size_t i = 0; i < N; i += 8) {
            uint8_t byte;
            DAWN_TRY(StreamOut(s, &byte));

            // Set the 8 bits in the bitset.
            for (size_t j = i; j < i + 8 && j < N; ++j) {
                v->set(j, byte & 0b10000000);
                byte <<= 1;
            }
        }
        return {};
    }
};

// Stream specialization for std::vector.
template <typename T>
class Stream<std::vector<T>> {
  public:
    static void Write(Sink* s, const std::vector<T>& v) {
        StreamIn(s, v.size());
        for (const T& it : v) {
            StreamIn(s, it);
        }
    }

    static MaybeError Read(Source* s, std::vector<T>* v) {
        using SizeT = decltype(std::declval<std::vector<T>>().size());
        SizeT size;
        DAWN_TRY(StreamOut(s, &size));
        *v = {};
        v->reserve(size);
        for (SizeT i = 0; i < size; ++i) {
            T el;
            DAWN_TRY(StreamOut(s, &el));
            v->push_back(std::move(el));
        }
        return {};
    }
};

// Specialization for C arrays of non-fundamental types.
template <typename T, size_t N>
class Stream<T[N], std::enable_if_t<!std::is_fundamental_v<T>>> {
  public:
    static void Write(Sink* s, const T (&t)[N]) {
        static_assert(N > 0);
        for (size_t i = 0; i < N; i++) {
            StreamIn(s, t[i]);
        }
    }

    static MaybeError Read(Source* s, T (*t)[N]) {
        static_assert(N > 0);
        for (size_t i = 0; i < N; i++) {
            DAWN_TRY(StreamOut(s, &(*t)[i]));
        }
        return {};
    }
};

// Stream specialization for std::pair.
template <typename A, typename B>
class Stream<std::pair<A, B>> {
  public:
    static void Write(Sink* s, const std::pair<A, B>& v) {
        StreamIn(s, v.first);
        StreamIn(s, v.second);
    }

    static MaybeError Read(Source* s, std::pair<A, B>* v) {
        DAWN_TRY(StreamOut(s, &v->first));
        DAWN_TRY(StreamOut(s, &v->second));
        return {};
    }
};

// Stream specialization for std::unordered_map<K, V> which sorts the entries
// to provide a stable ordering.
template <typename K, typename V>
class Stream<std::unordered_map<K, V>> {
  public:
    static void Write(stream::Sink* sink, const std::unordered_map<K, V>& m) {
        std::vector<std::pair<K, V>> ordered(m.begin(), m.end());
        std::sort(ordered.begin(), ordered.end(),
                  [](const std::pair<K, V>& a, const std::pair<K, V>& b) {
                      return std::less<K>{}(a.first, b.first);
                  });
        StreamIn(sink, ordered);
    }
};

// Stream specialization for raw function pointers. Raw function pointers don't contain any bound
// data so they do not contain any serialized data.
template <typename R, typename... Args>
class Stream<R (*)(Args...)> {
    using T = R (*)(Args...);

  public:
    constexpr static void StreamIn(stream::Sink* sink, const T& t) {}
};

#if DAWN_PLATFORM_IS(WINDOWS) && DAWN_PLATFORM_IS_32_BIT
template <typename R, typename... Args>
class Stream<R(__stdcall*)(Args...)> {
    using T = R(__stdcall*)(Args...);

  public:
    constexpr static void StreamIn(stream::Sink* sink, const T& t) {}
};
#endif

// Stream specialization for pointers. We always serialize via value, not by pointer.
// To handle nullptr scenarios, we always serialize whether the pointer was not nullptr,
// followed by the contents if applicable.
template <typename T>
class Stream<T, std::enable_if_t<std::is_pointer_v<T>>> {
  public:
    static void Write(stream::Sink* sink, const T& t) {
        using Pointee = std::decay_t<std::remove_pointer_t<T>>;
        static_assert(!std::is_same_v<char, Pointee> && !std::is_same_v<wchar_t, Pointee> &&
                          !std::is_same_v<char16_t, Pointee> && !std::is_same_v<char32_t, Pointee>,
                      "C-str like type likely has ambiguous serialization. For a string, wrap with "
                      "std::string_view instead.");
        StreamIn(sink, t != nullptr);
        if (t != nullptr) {
            StreamIn(sink, *t);
        }
    }
};

// Helper class to contain the begin/end iterators of an iterable.
namespace detail {
template <typename Iterator>
struct Iterable {
    Iterator begin;
    Iterator end;
};
}  // namespace detail

// Helper for making detail::Iterable from a pointer and count.
template <typename T>
auto Iterable(const T* ptr, size_t count) {
    using Iterator = const T*;
    return detail::Iterable<Iterator>{ptr, ptr + count};
}

// Stream specialization for pointers. We always serialize via value, not by pointer.
// To handle nullptr scenarios, we always serialize whether the pointer was not nullptr,
// followed by the contents if applicable.
template <typename Iterator>
class Stream<detail::Iterable<Iterator>> {
  public:
    static void Write(stream::Sink* sink, const detail::Iterable<Iterator>& iter) {
        StreamIn(sink, std::distance(iter.begin, iter.end));
        for (auto it = iter.begin; it != iter.end; ++it) {
            StreamIn(sink, *it);
        }
    }
};

}  // namespace dawn::native::stream

#endif  // SRC_DAWN_NATIVE_STREAM_STREAM_H_
