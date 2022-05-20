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

#ifndef SRC_DAWN_NATIVE_CACHEREQUEST_BUILDER_H_
#define SRC_DAWN_NATIVE_CACHEREQUEST_BUILDER_H_

#include <tuple>
#include <utility>

#include "dawn/common/Assert.h"
#include "dawn/native/CacheKey.h"

// Helper for X macro to declare a struct member.
#define DAWN_INTERNAL_CACHE_REQUEST_DECL_STRUCT_MEMBER(type, name) type name;

// Helper for X macro to declare a setter for this cache request field.
// It statically asserts that the same field is not specified more than once.
#define DAWN_INTERNAL_CACHE_REQUEST_DFN_SETTER(type, name)                                       \
    auto name(type name##Value) {                                                                \
        constexpr bool kFoundMember =                                                            \
            IndexOfMemberInParameterPack<&Struct::name, Members...>::value < sizeof...(Members); \
        static_assert(!kFoundMember, #name " cannot be specified more than once.");              \
        return Set<&Struct::name>(std::move(name##Value));                                       \
    }

// Helper for X macro for statically checking that all fields in the cache request have been
// specified. Also returns a boolean status to short-circuit code that would cause a cascading
// failure in
#define DAWN_INTERNAL_CACHE_REQUEST_CHECK_COMPLETE(type, name)                                   \
    {                                                                                            \
        constexpr bool kFoundMember =                                                            \
            IndexOfMemberInParameterPack<&Struct::name, Members...>::value < sizeof...(Members); \
        static_assert(kFoundMember, #name " missing from cache request.");                       \
        if (!kFoundMember) {                                                                     \
            return false;                                                                        \
        }                                                                                        \
    }

// Helper for X macro for recording cache request fields into a CacheKey. This helper macro finds
// the index of &Struct::name in mValues, pulls it out, and records it into the cache key. The
// reason for this is so we can record in the order of the fields in the X macro definition, instead
// of the order in which the fields were set.
#define DAWN_INTERNAL_CACHE_REQUEST_RECORD_KEY(type, name) \
    key.Record(std::get<IndexOfMemberInParameterPack<&Struct::name, Members...>::value>(mValues));

// Helper for X macro for populating fields of a cache request struct. This helper macro finds the
// index of &Struct::name in mValues, pulls it out, and moves the value into struct. The reason for
// this is so we can specify struct fields in the same order the struct fields were defined,
// allowing aggregate initialization.
#define DAWN_INTERNAL_CACHE_REQUEST_MAKE_STRUCT(type, name) \
    std::move(std::get<IndexOfMemberInParameterPack<&Struct::name, Members...>::value>(mValues)),

// Create a struct named Request to hold data for a cache request, and a Request##Builder
// to help populating that struct. The builder exposes CreateCacheKey for computing a CacheKey from
// the struct members, and a method Call for moving the cache request struct into a free function
// pointer and calling it. For both of these, the builder statically asserts that all of the cache
// request fields were specified exactly once.
//
// For Call, the function is also static_assert'ed to be convertible to a plain-old function
// pointer. If it is a lambda, it means it has nothing bound to it. This is intended so that
// strictly the arguments into |Memoize| are used in the |fn|, so every input to it becomes part of
// the key. Call should only be called once. It is invoked on an rvalue reference to help indicate
// it should only be used once. For example:
//   auto req = MakeCacheRequest().foo(1);
//   std::move(req).Call([](data) { return data.foo; });
//
// MEMBERS is a list of X(type, name)
#define DAWN_MAKE_CACHE_REQUEST_BUILDER(Request, MEMBERS)                                   \
    struct Request {                                                                        \
        MEMBERS(DAWN_INTERNAL_CACHE_REQUEST_DECL_STRUCT_MEMBER)                             \
    };                                                                                      \
    template <auto... Members>                                                              \
    class Request##Builder {                                                                \
        using Struct = Request;                                                             \
        template <auto M>                                                                   \
        using MemberType = decltype(std::declval<Request>().*M);                            \
                                                                                            \
        template <auto M, auto... Ms>                                                       \
        struct IndexOfMemberInParameterPack {                                               \
            static constexpr size_t value = 0u;                                             \
        };                                                                                  \
                                                                                            \
        template <auto M1, auto M2, bool Same = std::is_same_v<decltype(M1), decltype(M2)>> \
        struct PointerToMembersEqual {                                                      \
            static constexpr bool value = M1 == M2;                                         \
        };                                                                                  \
                                                                                            \
        template <auto M1, auto M2>                                                         \
        struct PointerToMembersEqual<M1, M2, false> {                                       \
            static constexpr bool value = false;                                            \
        };                                                                                  \
                                                                                            \
        template <auto M, auto First, auto... Ms>                                           \
        struct IndexOfMemberInParameterPack<M, First, Ms...> {                              \
            static constexpr size_t value =                                                 \
                PointerToMembersEqual<M, First>::value                                      \
                    ? 0u                                                                    \
                    : 1u + IndexOfMemberInParameterPack<M, Ms...>::value;                   \
        };                                                                                  \
                                                                                            \
      public:                                                                               \
        Request##Builder() {}                                                               \
        Request##Builder(std::tuple<MemberType<Members>...> values)                         \
            : mValues(std::move(values)) {}                                                 \
                                                                                            \
        MEMBERS(DAWN_INTERNAL_CACHE_REQUEST_DFN_SETTER)                                     \
                                                                                            \
        static constexpr bool CheckComplete() {                                             \
            MEMBERS(DAWN_INTERNAL_CACHE_REQUEST_CHECK_COMPLETE) return true;                \
        }                                                                                   \
                                                                                            \
        CacheKey CreateCacheKey() const {                                                   \
            CacheKey key;                                                                   \
            if constexpr (CheckComplete()) {                                                \
                MEMBERS(DAWN_INTERNAL_CACHE_REQUEST_RECORD_KEY)                             \
            }                                                                               \
            return key;                                                                     \
        }                                                                                   \
                                                                                            \
        template <typename Fn>                                                              \
        auto Call(Fn fn) && {                                                               \
            ASSERT(!mCalled);                                                               \
            mCalled = true;                                                                 \
            /* Deduce the return type. */                                                   \
            using R = decltype(fn(std::declval<Request>()));                                \
            static_assert(                                                                  \
                std::is_convertible_v<Fn, R (*)(Request)>,                                  \
                "Call function siganature does not match, or it is not a free function.");  \
            return fn(IntoStruct());                                                        \
        }                                                                                   \
                                                                                            \
      private:                                                                              \
        auto IntoStruct() {                                                                 \
            if constexpr (CheckComplete()) {                                                \
                return Request{MEMBERS(DAWN_INTERNAL_CACHE_REQUEST_MAKE_STRUCT)};           \
            }                                                                               \
        }                                                                                   \
                                                                                            \
        template <auto M>                                                                   \
        auto Set(MemberType<M> value) {                                                     \
            return Request##Builder<Members..., M>(                                         \
                std::tuple_cat(std::move(mValues), std::make_tuple(std::move(value))));     \
        }                                                                                   \
        std::tuple<MemberType<Members>...> mValues;                                         \
        bool mCalled = false;                                                               \
    };                                                                                      \
    auto Make##Request() { return Request##Builder<>(); }

#endif  // SRC_DAWN_NATIVE_CACHEREQUEST_BUILDER_H_
