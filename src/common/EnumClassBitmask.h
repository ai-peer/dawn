// Copyright 2020 The Dawn Authors
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

#ifndef COMMON_ENUMCLASSBITMASK_H_
#define COMMON_ENUMCLASSBITMASK_H_

#include <type_traits>

namespace detail {

    template <typename T>
    struct EnumBitmaskSize {
        static constexpr unsigned value = 0;
    };

    template <typename T, typename Enable = void>
    struct LowerBitmask {
        static constexpr bool enable = false;
    };

    template <typename T>
    struct LowerBitmask<T, typename std::enable_if<(EnumBitmaskSize<T>::value > 0)>::type> {
        static constexpr bool enable = true;
        using type = T;
        constexpr static T Lower(T t) {
            return t;
        }
    };

    template <typename T>
    struct BoolConvertible {
        using Integral = typename std::underlying_type<T>::type;

        constexpr BoolConvertible(Integral value) : value(value) {
        }
        constexpr operator bool() const {
            return value != 0;
        }
        constexpr operator T() const {
            return static_cast<T>(value);
        }

        Integral value;
    };

    template <typename T>
    struct LowerBitmask<BoolConvertible<T>> {
        static constexpr bool enable = true;
        using type = T;
        static constexpr type Lower(BoolConvertible<T> t) {
            return t;
        }
    };

}  // namespace detail

template <typename T1,
          typename T2,
          typename = typename std::enable_if<detail::LowerBitmask<T1>::enable &&
                                             detail::LowerBitmask<T2>::enable>::type>
constexpr detail::BoolConvertible<typename detail::LowerBitmask<T1>::type> operator|(T1 left,
                                                                                     T2 right) {
    using T = typename detail::LowerBitmask<T1>::type;
    using Integral = typename std::underlying_type<T>::type;
    return static_cast<Integral>(detail::LowerBitmask<T1>::Lower(left)) |
           static_cast<Integral>(detail::LowerBitmask<T2>::Lower(right));
}

template <typename T1,
          typename T2,
          typename = typename std::enable_if<detail::LowerBitmask<T1>::enable &&
                                             detail::LowerBitmask<T2>::enable>::type>
constexpr detail::BoolConvertible<typename detail::LowerBitmask<T1>::type> operator&(T1 left,
                                                                                     T2 right) {
    using T = typename detail::LowerBitmask<T1>::type;
    using Integral = typename std::underlying_type<T>::type;
    return static_cast<Integral>(detail::LowerBitmask<T1>::Lower(left)) &
           static_cast<Integral>(detail::LowerBitmask<T2>::Lower(right));
}

template <typename T1,
          typename T2,
          typename = typename std::enable_if<detail::LowerBitmask<T1>::enable &&
                                             detail::LowerBitmask<T2>::enable>::type>
constexpr detail::BoolConvertible<typename detail::LowerBitmask<T1>::type> operator^(T1 left,
                                                                                     T2 right) {
    using T = typename detail::LowerBitmask<T1>::type;
    using Integral = typename std::underlying_type<T>::type;
    return static_cast<Integral>(detail::LowerBitmask<T1>::Lower(left)) ^
           static_cast<Integral>(detail::LowerBitmask<T2>::Lower(right));
}

template <typename T1>
constexpr detail::BoolConvertible<typename detail::LowerBitmask<T1>::type> operator~(T1 t) {
    using T = typename detail::LowerBitmask<T1>::type;
    using Integral = typename std::underlying_type<T>::type;
    return ~static_cast<Integral>(detail::LowerBitmask<T1>::Lower(t));
}

template <typename T,
          typename T2,
          typename = typename std::enable_if<(detail::EnumBitmaskSize<T>::value > 0) &&
                                             detail::LowerBitmask<T2>::enable>::type>
constexpr T& operator&=(T& l, T2 right) {
    T r = detail::LowerBitmask<T2>::Lower(right);
    l = l & r;
    return l;
}

template <typename T,
          typename T2,
          typename = typename std::enable_if<(detail::EnumBitmaskSize<T>::value > 0) &&
                                             detail::LowerBitmask<T2>::enable>::type>
constexpr T& operator|=(T& l, T2 right) {
    T r = detail::LowerBitmask<T2>::Lower(right);
    l = l | r;
    return l;
}

template <typename T,
          typename T2,
          typename = typename std::enable_if<(detail::EnumBitmaskSize<T>::value > 0) &&
                                             detail::LowerBitmask<T2>::enable>::type>
constexpr T& operator^=(T& l, T2 right) {
    T r = detail::LowerBitmask<T2>::Lower(right);
    l = l ^ r;
    return l;
}

#endif  // COMMON_ENUMCLASSBITMASK_H_
