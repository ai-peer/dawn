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

#include "absl/strings/str_format.h"

#include <tuple>
#include <utility>

namespace dawn::native {

    namespace detail {

        // Cache key absl::StrFormat format string generation helpers to create the necessary format
        // strings at compile time. The following examples illustrate the generated strings via
        // substring indexing:
        //     CacheKeyFormatGenerator<1>::value -> "{%u:%#s}"
        //     CacheKeyFormatGenerator<2>::value -> "{%u:%#s,%u:%#s}"
        struct CacheKeyFormatBase {
            static constexpr char value[] = "%u:%#s,";
        };
        template <size_t... Is>
        struct CacheKeyFormat {
            static constexpr char value[] = {'{', CacheKeyFormatBase::value[Is]..., '}', 0};
        };
        template <size_t N, size_t... Is>
        struct CacheKeyFormatGenerator
            : CacheKeyFormatGenerator<N - 1, Is..., 0, 1, 2, 3, 4, 5, 6> {};
        template <size_t... Is>
        struct CacheKeyFormatGenerator<1, Is...> : CacheKeyFormat<Is..., 0, 1, 2, 3, 4, 5> {};

        // Wrapper to help fold arbitrary types into statically generated parsed format type. Since
        // the format strings always alternate between %u (member id), and %s (serialized member),
        // the templating determines whether the index is even/odd and produces the necessary
        // arguments to create the absl::ParsedFormat at compile time.
        template <size_t I>
        struct CacheKeyFormatCharWrapper : CacheKeyFormatCharWrapper<I % 2> {};
        template <>
        struct CacheKeyFormatCharWrapper<0> {
            static constexpr char value = 'u';
        };
        template <>
        struct CacheKeyFormatCharWrapper<1> {
            static constexpr char value = 's';
        };

        // Helper enumeration helpers to transform arbitrary packed template arguments to packed
        // arguments prefixed with index each. Example:
        //     <int, float, Class> -> <0, int, 1, float, 2, Class>
        template <size_t I, typename Tuple, bool Even = I % 2 == 0>
        auto EnumerateStep(Tuple&& tup) {
            if constexpr (Even) {
                return I / 2;
            } else {
                return std::get<I / 2>(tup);
            }
        }
        template <typename Tuple, size_t... I>
        auto EnumerateImpl(Tuple&& tup, std::index_sequence<I...>) {
            return std::make_tuple(EnumerateStep<I>(tup)...);
        }
        template <typename... Ts>
        auto Enumerate(const Ts&... inputs) {
            return EnumerateImpl(std::make_tuple(inputs...),
                                 std::make_index_sequence<2 * sizeof...(Ts)>{});
        }

        // Internal cache key generation helper function that takes into account the necessary
        // transformations to prefix each serialized field with a unique member id.
        template <size_t... Is, typename... Ts>
        std::string CacheKeySerializerImpl(std::integer_sequence<size_t, Is...>,
                                           const Ts&... inputs) {
            static const auto* const fmt = [&]() {
                return new absl::ParsedFormat<CacheKeyFormatCharWrapper<Is>::value...>(
                    CacheKeyFormatGenerator<sizeof...(Ts)>::value);
            }();

            return std::apply([](auto... args) { return absl::StrFormat(*fmt, args...); },
                              Enumerate(inputs...));
        }

    }  // namespace detail

    // Given list of arguments, serializes each argument as a string via absl::StrFormat, and
    // returns a fully serialized string such that each argument is prefixed with a unique
    // member id based on the order of which the arguments were specified.
    //
    // Note: Currently non-string types need to either implemnent an AbslFormatConvert overload
    //       or be explicitly converted to a string when passed in. Some built-in types need to be
    //       converted before passing in because formatting string assumes %s for all args.
    template <typename... Ts>
    std::string CacheKeySerializer(const Ts&... inputs) {
        return detail::CacheKeySerializerImpl(std::make_index_sequence<2 * sizeof...(Ts)>{},
                                              inputs...);
    }

}  // namespace dawn::native

#endif  // DAWNNATIVE_CACHE_KEY_SERIALIZER_H_
