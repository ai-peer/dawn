// Copyright 2021 The Tint Authors.
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

#ifndef SRC_TINT_UTILS_STRING_H_
#define SRC_TINT_UTILS_STRING_H_

#include <array>  // for std::size
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>

namespace tint::utils {

/// @param str the string to apply replacements to
/// @param substr the string to search for
/// @param replacement the replacement string to use instead of `substr`
/// @returns `str` with all occurrences of `substr` replaced with `replacement`
inline std::string ReplaceAll(std::string str,
                              const std::string& substr,
                              const std::string& replacement) {
    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string::npos) {
        str.replace(pos, substr.length(), replacement);
        pos += replacement.length();
    }
    return str;
}

namespace detail {
template <typename T, typename = void>
struct has_size {
    static constexpr bool value = false;
};
template <typename T>
struct has_size<T, std::void_t<decltype(std::size(std::declval<T>()))>> {
    static constexpr bool value = true;
};
template <typename T>
constexpr bool has_size_v = has_size<T>::value;
}  // namespace detail

template <typename Sep, typename Arg1, typename... Args>
std::string JoinWith(Sep&& sep, Arg1&& arg1, Args&&... args) {
    auto str = [&](auto&& a) {
        using T = decltype(a);
        using NoRefT = std::remove_reference_t<T>;
        if constexpr (std::is_same_v<NoRefT, const char*>) {
            return std::forward<T>(a);
        } else if constexpr (detail::has_size_v<NoRefT>) {
            return std::forward<T>(a);
        } else {
            return std::to_string(a);
        }
    };

    auto len = [&](auto& a) {
        // Do not std::decay_t otherise const char[N] will decay to const char*, and strlen will be
        // invoked unnecessarily.
        using T = std::remove_reference_t<decltype(a)>;
        if constexpr (std::is_same_v<T, const char*>) {
            return strlen(a);
        } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
            return std::numeric_limits<T>::max_digits10;
        } else {
            // For const char[N], std::string, and std::string_view
            return std::size(a);
        }
    };

    std::string result;

    if constexpr (std::is_same_v<Sep, std::nullptr_t>) {
        result.reserve(len(arg1) + (len(args) + ...));
        result.append(str(std::forward<Arg1>(arg1)));
        ((result.append(str(std::forward<Args>(args)))), ...);
    } else {
        result.reserve(len(arg1) + (len(args) + ...) + (len(sep) * sizeof...(Args)));
        result.append(str(std::forward<Arg1>(arg1)));
        ((result.append(sep), result.append(str(std::forward<Args>(args)))), ...);
    }
    return result;
}

template <typename... Args>
std::string Join(Args&&... args) {
    return JoinWith(" ", std::forward<Args>(args)...);
}

template <typename... Args>
std::string Concat(Args&&... args) {
    return JoinWith(std::nullptr_t{}, std::forward<Args>(args)...);
}

template <typename T>
std::string Quote(T&& s) {
    return Concat("'", std::forward<T>(s), "'");
}

template <typename T, typename U, typename V>
std::string PrePost(T&& prefix, U&& s, V&& suffix) {
    return Concat(std::forward<T>(prefix), std::forward<U>(s), std::forward<V>(suffix));
}

template <typename T, typename U>
std::string Post(T&& s, U&& suffix) {
    return Concat(std::forward<T>(s), std::forward<U>(suffix));
}

template <typename T, typename U>
std::string Pre(T&& prefix, U&& s) {
    return Concat(std::forward<T>(prefix), std::forward<U>(s));
}

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_STRING_H_
