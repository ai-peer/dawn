// Copyright 2020 The Tint Authors.
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

#ifndef SRC_TINT_AST_TEST_HELPER_H_
#define SRC_TINT_AST_TEST_HELPER_H_

#include "gtest/gtest.h"
#include "src/tint/program_builder.h"

namespace tint::ast {

/// Helper base class for testing
template <typename BASE>
class TestHelperBase : public BASE, public ProgramBuilder {};

/// Helper class for testing that derives from testing::Test.
using TestHelper = TestHelperBase<testing::Test>;

/// Helper class for testing that derives from `T`.
template <typename T>
using TestParamHelper = TestHelperBase<testing::TestWithParam<T>>;

template <typename... ARGS>
void CheckIdentifier(const SymbolTable& symbols,
                     const ast::Identifier* ident,
                     std::string_view expected,
                     ARGS&&... expected_args) {
    static constexpr uint32_t num_expected_args = sizeof...(expected_args);

    EXPECT_EQ(symbols.NameFor(ident->symbol), expected);

    if constexpr (num_expected_args == 0) {
        EXPECT_FALSE(ident->Is<ast::TemplatedIdentifier>());
    } else {
        ASSERT_TRUE(ident->Is<ast::TemplatedIdentifier>());
        auto* t = ident->As<ast::TemplatedIdentifier>();
        ASSERT_EQ(t->arguments.Length(), num_expected_args);

        size_t arg_idx = 0;
        auto check_arg = [&](auto&& expected) {
            const auto* arg = t->arguments[arg_idx++];

            using T = std::decay_t<decltype(expected)>;
            if constexpr (traits::IsStringLike<T>) {
                ASSERT_TRUE(arg->Is<ast::IdentifierExpression>());
                CheckIdentifier(symbols, arg->As<ast::IdentifierExpression>()->identifier,
                                expected);
            } else {
                FAIL() << "unhandled expected_args type";
            }
        };
        (check_arg(std::forward<ARGS>(expected_args)), ...);
    }
}

}  // namespace tint::ast

#endif  // SRC_TINT_AST_TEST_HELPER_H_
