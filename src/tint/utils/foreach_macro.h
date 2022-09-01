// Copyright 2022 The Tint Authors.
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

#ifndef SRC_TINT_UTILS_FOREACH_MACRO_H_
#define SRC_TINT_UTILS_FOREACH_MACRO_H_

// Macro magic to perform macro variadic dispatch.
// See:
// https://renenyffenegger.ch/notes/development/languages/C-C-plus-plus/preprocessor/macros/__VA_ARGS__/count-arguments
// Note, this doesn't attempt to use the ##__VA_ARGS__ trick to handle 0

// Helper macro to force expanding __VA_ARGS__ to satisfy MSVC compiler.
#define TINT_MSVC_EXPAND_BUG(X) X

/// TINT_COUNT_ARGUMENTS_NTH_ARG is used by TINT_COUNT_ARGUMENTS to get the number of arguments in a
/// variadic macro call.
#define TINT_COUNT_ARGUMENTS_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, \
                                     _15, _16, N, ...)                                            \
    N

/// TINT_COUNT_ARGUMENTS evaluates to the number of arguments passed to the macro
#define TINT_COUNT_ARGUMENTS(...)                                                                 \
    TINT_MSVC_EXPAND_BUG(TINT_COUNT_ARGUMENTS_NTH_ARG(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, \
                                                      8, 7, 6, 5, 4, 3, 2, 1, 0))

// Correctness checks.
static_assert(1 == TINT_COUNT_ARGUMENTS(a), "TINT_COUNT_ARGUMENTS broken");
static_assert(2 == TINT_COUNT_ARGUMENTS(a, b), "TINT_COUNT_ARGUMENTS broken");
static_assert(3 == TINT_COUNT_ARGUMENTS(a, b, c), "TINT_COUNT_ARGUMENTS broken");

/// TINT_FOREACH calls CALLBACK with each of the variadic arguments.
#define TINT_FOREACH(CALLBACK, ...) \
    TINT_MSVC_EXPAND_BUG(           \
        TINT_CONCAT(TINT_FOREACH_, TINT_COUNT_ARGUMENTS(__VA_ARGS__))(CALLBACK, __VA_ARGS__))

#define TINT_FOREACH_1(CALLBACK, _1) CALLBACK(_1)
#define TINT_FOREACH_2(CALLBACK, _1, _2) \
    TINT_FOREACH_1(CALLBACK, _1)         \
    CALLBACK(_2)
#define TINT_FOREACH_3(CALLBACK, _1, _2, _3) \
    TINT_FOREACH_2(CALLBACK, _1, _2)         \
    CALLBACK(_3)
#define TINT_FOREACH_4(CALLBACK, _1, _2, _3, _4) \
    TINT_FOREACH_3(CALLBACK, _1, _2, _3)         \
    CALLBACK(_4)
#define TINT_FOREACH_5(CALLBACK, _1, _2, _3, _4, _5) \
    TINT_FOREACH_4(CALLBACK, _1, _2, _3, _4)         \
    CALLBACK(_5)
#define TINT_FOREACH_6(CALLBACK, _1, _2, _3, _4, _5, _6) \
    TINT_FOREACH_5(CALLBACK, _1, _2, _3, _4, _5)         \
    CALLBACK(_6)
#define TINT_FOREACH_7(CALLBACK, _1, _2, _3, _4, _5, _6, _7) \
    TINT_FOREACH_6(CALLBACK, _1, _2, _3, _4, _5, _6)         \
    CALLBACK(_7)
#define TINT_FOREACH_8(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8) \
    TINT_FOREACH_7(CALLBACK, _1, _2, _3, _4, _5, _6, _7)         \
    CALLBACK(_8)
#define TINT_FOREACH_9(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9) \
    TINT_FOREACH_8(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8)         \
    CALLBACK(_9)
#define TINT_FOREACH_10(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10) \
    TINT_FOREACH_9(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9)           \
    CALLBACK(_10)
#define TINT_FOREACH_11(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11) \
    TINT_FOREACH_10(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10)          \
    CALLBACK(_11)
#define TINT_FOREACH_12(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12) \
    TINT_FOREACH_11(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11)          \
    CALLBACK(_12)
#define TINT_FOREACH_13(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13) \
    TINT_FOREACH_11(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12)          \
    CALLBACK(_13)
#define TINT_FOREACH_14(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14) \
    TINT_FOREACH_11(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13)          \
    CALLBACK(_14)
#define TINT_FOREACH_15(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, \
                        _15)                                                                   \
    TINT_FOREACH_11(CALLBACK, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14)     \
    CALLBACK(_15)

#endif  // SRC_TINT_UTILS_FOREACH_MACRO_H_
