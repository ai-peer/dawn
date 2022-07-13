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

#ifndef SRC_DAWN_NATIVE_SERDE_SERDE_H_
#define SRC_DAWN_NATIVE_SERDE_SERDE_H_

#include "dawn/common/TypedInteger.h"
#include "dawn/native/Error.h"
#include "dawn/native/serde/Sink.h"
#include "dawn/native/serde/Source.h"

namespace dawn::native::serde {

// Helpers to implement HasSerialize and HasDeserialize
namespace detail {
struct CheckSerde {
    template <typename> class Delegate;
    template <typename Fn, Fn>
    struct CheckFn;

    template <typename Cls, typename T>
    static std::true_type HasSerialize(CheckFn<void (*)(Sink*, const T&), &Cls::template Serialize<Delegate>>*);

    template <typename Cls, typename T>
    static std::false_type HasSerialize(...);

    template <typename Cls, typename T>
    static std::true_type HasDeserialize(CheckFn<MaybeError (*)(Source*, T*), &Cls::template Deserialize<Delegate>>*);

    template <typename Cls, typename T>
    static std::false_type HasDeserialize(...);
};
}  // namespace detail

// HasSerialize is true if Delegate<T> has a static Serialize method.
template <template <typename...> class Delegate, typename T>
constexpr bool HasSerialize = decltype(detail::CheckSerde::HasSerialize<Delegate<T>, T>(0))::value;

// HasDeserialize is true if Delegate<T> has a static Deserialize method.
template <template <typename...> class Delegate, typename T>
constexpr bool HasDeserialize =
    decltype(detail::CheckSerde::HasDeserialize<Delegate<T>, T>(0))::value;

// Base implementation for Serialize.
// Delegate should be a template class<T> that provides a static Serialize method.
// Example usage: `Serialize<Serde>(sink, 42);`
template <template <typename...> class Delegate,
          typename T,
          typename SFINAE = std::enable_if_t<HasSerialize<Delegate, T>>>
void Serialize(Sink* s, const T& v) {
    Delegate<T>::template Serialize<Delegate>(s, v);
}

// Base implementation for Deserialize.
// Delegate should be a template class<T> that provides a static Deserialize method.
// Example usage: `int value; Deserialize<Serde>(sink, &value);`
template <template <typename...> class Delegate,
          typename T,
          typename SFINAE = std::enable_if_t<HasDeserialize<Delegate, T>>>
MaybeError Deserialize(Source* s, T* v) {
    return Delegate<T>::template Deserialize<Delegate>(s, v);
}

// // Serialize overload for std::vector<T>
// template <template <typename...> class Delegate,
//           typename T,
//           typename SFINAE = std::enable_if_t<HasSerialize<Delegate, T>>>
// void Serialize(Sink* s, const std::vector<T>& v) {
//     Serialize<Delegate>(s, v.size());
//     for (const T& it : v) {
//         Serialize<Delegate>(s, it);
//     }
// }

// // Deserialize overload for std::vector<T>
// template <template <typename...> class Delegate,
//           typename T,
//           typename SFINAE = std::enable_if_t<HasDeserialize<Delegate, T>>>
// MaybeError Deserialize(Source* s, std::vector<T>* v) {
//     using SizeT = decltype(std::declval<std::vector<T>>().size());
//     SizeT size;
//     DAWN_TRY(Deserialize<Delegate>(s, &size));
//     *v = {};
//     v->reserve(size);
//     for (SizeT i = 0; i < size; ++i) {
//         T el;
//         DAWN_TRY(Deserialize<Delegate>(s, &el));
//         v->push_back(std::move(el));
//     }
//     return {};
// }

// // Serialize overload for fixed arrays of non-fundamental types
// template <template <typename...> class Delegate,
//           typename T,
//           size_t N,
//           typename SFINAE =
//               std::enable_if_t<HasSerialize<Delegate, T> && !std::is_fundamental_v<T>>>
// void Serialize(Sink* s, const T (&t)[N]) {
//     static_assert(N > 0);
//     Serialize<Delegate>(s, N);
//     for (size_t i = 0; i < N; i++) {
//         Serialize<Delegate>(s, t[i]);
//     }
// }

// // Deserialize overload for fixed arrays of non-fundamental types
// template <template <typename...> class Delegate,
//           typename T,
//           size_t N,
//           typename SFINAE =
//               std::enable_if_t<HasDeserialize<Delegate, T> && !std::is_fundamental_v<T>>>
// MaybeError Deserialize(Sink* s, T (*t)[N]) {
//     static_assert(N > 0);
//     size_t count;
//     DAWN_TRY(Deserialize<Delegate>(s, &count));
//     DAWN_INVALID_IF(count != N, "Expected deserialized count (%u) to be %u.", count, N);
//     for (size_t i = 0; i < N; i++) {
//         DAWN_TRY(Deserialize<Delegate>(s, &(*t)[i]));
//     }
//     return {};
// }

// // Serialize overload for std::pair<A, B>
// template <template <typename...> class Delegate,
//           typename A,
//           typename B,
//           typename SFINAE =
//               std::enable_if_t<HasSerialize<Delegate, A> && HasSerialize<Delegate, B>>>
// void Serialize(Sink* s, const std::pair<A, B>& v) {
//     Serialize<Delegate>(s, v.first);
//     Serialize<Delegate>(s, v.second);
// }

// // Deserialize overload for std::pair<A, B>
// template <template <typename...> class Delegate,
//           typename A,
//           typename B,
//           typename SFINAE =
//               std::enable_if_t<HasSerialize<Delegate, A> && HasSerialize<Delegate, B>>>
// MaybeError Deserialize(Source* s, std::pair<A, B>* v) {
//     DAWN_TRY(Deserialize<Delegate>(s, &v->first));
//     DAWN_TRY(Deserialize<Delegate>(s, &v->second));
//     return {};
// }

// Base Serde template for specialization. Specializations should optionally,
// define static methods:
//   static void Serialize(Sink* s, const T& v);
//   static MaybeError Deserialize(Source* s, T* v);
template <typename T, typename SFINAE = void>
class Serde {};

#define SERDE_LEAF(T)                                                        \
  private:                                                                   \
    using _T = T;                                                            \
  public:                                                                    \
    template <template <typename...> class Delegate>                         \
    static void Serialize(::dawn::native::serde::Sink* s, const _T& v) {     \
        Serialize(s, v);                                                     \
    }                                                                        \
    template <template <typename...> class Delegate>                         \
    static MaybeError Deserialize(::dawn::native::serde::Source* s, _T* v) { \
        return Deserialize(s, v);                                            \
    }

// Helper macro for declaring specializations of Serde.
#define DECL_SERDE_LEAF(T)                              \
    template <>                                         \
    class Serde<T> {                                    \
        SERDE_LEAF(T)                                   \
      public:                                           \
        static void Serialize(Sink* s, const T& v);     \
        static MaybeError Deserialize(Source* s, T* v); \
    }

DECL_SERDE_LEAF(std::string);
DECL_SERDE_LEAF(std::string_view);

#undef DECL_SERDE_LEAF

// Serde specialization for fundamental types.
template <typename T>
class Serde<T, std::enable_if_t<std::is_fundamental_v<T>>> {
    SERDE_LEAF(T)
  public:
    static void Serialize(Sink* s, const T& v) { memcpy(s->GetSpace(sizeof(T)), &v, sizeof(T)); }
    static MaybeError Deserialize(Source* s, T* v) {
        const void* ptr;
        DAWN_TRY_ASSIGN(ptr, s->Read(sizeof(T)));
        memcpy(v, ptr, sizeof(T));
        return {};
    }
};

// Serde specialization for fixed arrays of fundamental types.
template <typename T, size_t N>
class Serde<T[N], std::enable_if_t<std::is_fundamental_v<T>>> {
    SERDE_LEAF(T[N])
  public:
    static void Serialize(Sink* s, const T (&t)[N]) {
        static_assert(N > 0);
        void* ptr = s->GetSpace(sizeof(size_t) + sizeof(t));
        size_t size = N;
        memcpy(ptr, &size, sizeof(size_t));
        ptr = static_cast<uint8_t*>(ptr) + sizeof(size_t);
        memcpy(ptr, &t, sizeof(t));
    }

    static MaybeError Deserialize(Source* s, T (*t)[N]) {
        static_assert(N > 0);
        const void* ptr;
        DAWN_TRY_ASSIGN(ptr, s->Read(sizeof(size_t)));
        size_t count = *static_cast<const size_t*>(ptr);
        DAWN_INVALID_IF(count != N, "Expected deserialized count (%u) to be %u.", count, N);

        DAWN_TRY_ASSIGN(ptr, s->Read(sizeof(*t)));
        memcpy(*t, ptr, sizeof(*t));
        return {};
    }
};

// Serde specialization for enums.
template <typename T>
class Serde<T, std::enable_if_t<std::is_enum_v<T>>> {
    using U = std::underlying_type_t<T>;

  public:
    template <template <typename...> class Delegate>
    static void Serialize(Sink* s, const T& v) { serde::Serialize<Delegate>(s, static_cast<U>(v)); }

    template <template <typename...> class Delegate>
    static MaybeError Deserialize(Source* s, T* v) {
        return serde::Deserialize<Delegate>(s, static_cast<U*>(v));
    }
};

// Serde specialization for TypedInteger.
template <typename Tag, typename Integer>
class Serde<::detail::TypedIntegerImpl<Tag, Integer>> {
    using T = ::detail::TypedIntegerImpl<Tag, Integer>;

  public:
    template <template <typename...> class Delegate>
    static void Serialize(Sink* s, const T& t) {
        serde::Serialize<Delegate>(s, static_cast<Integer>(t));
    }

    template <template <typename...> class Delegate>
    static MaybeError Deserialize(Source* s, T* v) {
        Integer out;
        DAWN_TRY(serde::Deserialize<Delegate>(s, &out));
        *v = out;
        return {};
    }
};

// Serde specialization for bitsets that are smaller than ulong.
template <size_t N>
class Serde<std::bitset<N>, std::enable_if_t<std::less_equal<size_t>{}(N, sizeof(unsigned long))>> {
  public:
    template <template <typename...> class Delegate>
    static void Serialize(Sink* s, const std::bitset<N>& t) {
        serde::Serialize<Delegate>(s, t.to_ulong());
    }
};

// Serde specialization for bitsets that are larger than ulong, but smaller than ullong.
template <size_t N>
class Serde<std::bitset<N>,
            std::enable_if_t<std::less<size_t>{}(sizeof(unsigned long), N) &&
                             std::less_equal<size_t>{}(N, sizeof(unsigned long long))>> {
  public:
    template <template <typename...> class Delegate>
    static void Serialize(Sink* s, const std::bitset<N>& t) {
        serde::Serialize<Delegate>(s, t.to_ullong());
    }
};

// Serde specialization for bitsets since using the built-in to_ullong has a size limit.
template <size_t N>
class Serde<std::bitset<N>,
            std::enable_if_t<std::greater<size_t>{}(N, sizeof(unsigned long long))>> {
  public:
    template <template <typename...> class Delegate>
    static void Serialize(Sink* s, const std::bitset<N>& t) {
        // Serializes the bitset into series of uint8_t, along with recording the size.
        static_assert(N > 0);
        serde::Serialize<Delegate>(s, N);
        uint8_t value = 0;
        for (size_t i = 0; i < N; i++) {
            value <<= 1;
            // Explicitly convert to numeric since MSVC doesn't like mixing of bools.
            value |= t[i] ? 1 : 0;
            if (i % 8 == 7) {
                // Whenever we fill an 8 bit value, record it and zero it out.
                serde::Serialize<Delegate>(s, value);
                value = 0;
            }
        }
        // Serialize the last value if we are not a multiple of 8.
        if (N % 8 != 0) {
            serde::Serialize<Delegate>(s, value);
        }
    }
};

template <typename T>
class Serde<std::vector<T>> {
  public:
    template <template <typename...> class Delegate>
    static void Serialize(Sink* s, const std::vector<T>& v) {
        serde::Serialize<Delegate>(s, v.size());
        for (const T& it : v) {
            serde::Serialize<Delegate>(s, it);
        }
    }

    template <template <typename...> class Delegate>
    static MaybeError Deserialize(Source* s, std::vector<T>* v) {
        using SizeT = decltype(std::declval<std::vector<T>>().size());
        SizeT size;
        DAWN_TRY(serde::Deserialize<Delegate>(s, &size));
        *v = {};
        v->reserve(size);
        for (SizeT i = 0; i < size; ++i) {
            T el;
            DAWN_TRY(serde::Deserialize<Delegate>(s, &el));
            v->push_back(std::move(el));
        }
        return {};
    }
};

template <typename T, size_t N>
class Serde<T[N], std::enable_if_t<!std::is_fundamental_v<T>>> {
  public:
    template <template <typename...> class Delegate>
    static void Serialize(Sink* s, const T (&t)[N]) {
        static_assert(N > 0);
        serde::Serialize<Delegate>(s, N);
        for (size_t i = 0; i < N; i++) {
            serde::Serialize<Delegate>(s, t[i]);
        }
    }

    template <template <typename...> class Delegate>
    static MaybeError Deserialize(Source* s, T (*t)[N]) {
        static_assert(N > 0);
        size_t count;
        DAWN_TRY(serde::Deserialize<Delegate>(s, &count));
        DAWN_INVALID_IF(count != N, "Expected deserialized count (%u) to be %u.", count, N);
        for (size_t i = 0; i < N; i++) {
            DAWN_TRY(serde::Deserialize<Delegate>(s, &(*t)[i]));
        }
        return {};
    }
};

template <typename A, typename B>
class Serde<std::pair<A, B>> {
  public:
    template <template <typename...> class Delegate>
    static void Serialize(Sink* s, const std::pair<A, B>& v) {
        serde::Serialize<Delegate>(s, v.first);
        serde::Serialize<Delegate>(s, v.second);
    }

    template <template <typename...> class Delegate>
    static MaybeError Deserialize(Source* s, std::pair<A, B>* v) {
        DAWN_TRY(serde::Deserialize<Delegate>(s, &v->first));
        DAWN_TRY(serde::Deserialize<Delegate>(s, &v->second));
        return {};
    }
};


}  // namespace dawn::native::serde

#endif  // SRC_DAWN_NATIVE_SERDE_SERDE_H_
