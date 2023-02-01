// Copyright 2023 The Tint Authors.
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

#include "gtest/gtest-spi.h"

#include "src/tint/ast/test_helper.h"

namespace tint::ast {
namespace {

using TemplatedIdentifierExpressionTest = TestHelper;

TEST_F(TemplatedIdentifierExpressionTest, Creation) {
    auto* i = Expr("ident", 1, 2.0, false);
    EXPECT_EQ(i->symbol, Symbols().Get("ident"));
    ASSERT_EQ(i->arguments.Length(), 3u);
    EXPECT_TRUE(i->arguments[0]->Is<ast::IntLiteralExpression>());
    EXPECT_TRUE(i->arguments[1]->Is<ast::FloatLiteralExpression>());
    EXPECT_TRUE(i->arguments[2]->Is<ast::BoolLiteralExpression>());
}

TEST_F(TemplatedIdentifierExpressionTest, Creation_WithSource) {
    auto* i = Expr(Source{Source::Location{20, 2}}, "ident", 1, 2.0, false);
    EXPECT_EQ(i->symbol, Symbols().Get("ident"));
    ASSERT_EQ(i->arguments.Length(), 3u);
    EXPECT_TRUE(i->arguments[0]->Is<ast::IntLiteralExpression>());
    EXPECT_TRUE(i->arguments[1]->Is<ast::FloatLiteralExpression>());
    EXPECT_TRUE(i->arguments[2]->Is<ast::BoolLiteralExpression>());

    auto src = i->source;
    EXPECT_EQ(src.range.begin.line, 20u);
    EXPECT_EQ(src.range.begin.column, 2u);
}

TEST_F(TemplatedIdentifierExpressionTest, Assert_InvalidSymbol) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b;
            b.Expr("");
        },
        "internal compiler error");
}

TEST_F(TemplatedIdentifierExpressionTest, Assert_DifferentProgramID_Symbol) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b1;
            ProgramBuilder b2;
            b1.Expr(b2.Sym("b2"));
        },
        "internal compiler error");
}

TEST_F(TemplatedIdentifierExpressionTest, Assert_DifferentProgramID_TemplateArg) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b1;
            ProgramBuilder b2;
            b1.Expr("b1", b2.Expr("b2"));
        },
        "internal compiler error");
}

}  // namespace
}  // namespace tint::ast
