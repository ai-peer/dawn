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

#include "src/tint/reader/wgsl/parser_impl_test_helper.h"

namespace tint::reader::wgsl {
namespace {

TEST_F(ParserImplTest, Callable_Array) {
    auto p = parser("array");
    auto t = p->callable();
    ASSERT_TRUE(p->peek_is(Token::Type::kEOF));
    EXPECT_TRUE(t.matched);
    EXPECT_FALSE(t.errored);
    ASSERT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(t.value, nullptr);
    ASSERT_TRUE(t.value->Is<ast::Array>());

    auto* a = t.value->As<ast::Array>();
    EXPECT_FALSE(a->IsRuntimeArray());
    EXPECT_EQ(a->type, nullptr);
    EXPECT_EQ(a->count, nullptr);
}

TEST_F(ParserImplTest, Callable_VecPrefix) {
    auto p = parser("vec3");
    auto t = p->callable();
    ASSERT_TRUE(p->peek_is(Token::Type::kEOF));
    EXPECT_TRUE(t.matched);
    EXPECT_FALSE(t.errored);
    ASSERT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(t.value, nullptr);
    ASSERT_TRUE(t.value->Is<ast::Vector>());

    auto* v = t.value->As<ast::Vector>();
    EXPECT_EQ(v->type, nullptr);
    EXPECT_EQ(v->width, 3u);
}

TEST_F(ParserImplTest, Callable_MatPrefix) {
    auto p = parser("mat3x2");
    auto t = p->callable();
    ASSERT_TRUE(p->peek_is(Token::Type::kEOF));
    EXPECT_TRUE(t.matched);
    EXPECT_FALSE(t.errored);
    ASSERT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(t.value, nullptr);

    auto& sym = p->builder().Symbols();

    auto* mat_type_name = t->As<ast::TypeName>();
    ASSERT_NE(mat_type_name, nullptr);
    EXPECT_EQ(sym.NameFor(mat_type_name->name->symbol), "mat3x2");

    EXPECT_FALSE(mat_type_name->name->Is<ast::TemplatedIdentifier>());
}

TEST_F(ParserImplTest, Callable_TypeDecl_Array) {
    auto p = parser("array<f32, 2>");
    auto t = p->callable();
    ASSERT_TRUE(p->peek_is(Token::Type::kEOF));
    EXPECT_TRUE(t.matched);
    EXPECT_FALSE(t.errored);
    ASSERT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(t.value, nullptr);
    ASSERT_TRUE(t.value->Is<ast::Array>());

    auto& sym = p->builder().Symbols();

    auto* a = t.value->As<ast::Array>();
    EXPECT_FALSE(a->IsRuntimeArray());

    auto* type_name = a->type->As<ast::TypeName>();
    ASSERT_NE(type_name, nullptr);
    EXPECT_EQ(sym.NameFor(type_name->name->symbol), "f32");

    auto* size = a->count->As<ast::IntLiteralExpression>();
    ASSERT_NE(size, nullptr);
    EXPECT_EQ(size->value, 2);
    EXPECT_EQ(size->suffix, ast::IntLiteralExpression::Suffix::kNone);
}

TEST_F(ParserImplTest, Callable_TypeDecl_Array_Runtime) {
    auto p = parser("array<f32>");
    auto t = p->callable();
    ASSERT_TRUE(p->peek_is(Token::Type::kEOF));
    EXPECT_TRUE(t.matched);
    EXPECT_FALSE(t.errored);
    ASSERT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(t.value, nullptr);
    ASSERT_TRUE(t.value->Is<ast::Array>());

    auto& sym = p->builder().Symbols();

    auto* a = t.value->As<ast::Array>();
    EXPECT_TRUE(a->IsRuntimeArray());

    auto* type_name = a->type->As<ast::TypeName>();
    ASSERT_NE(type_name, nullptr);
    EXPECT_EQ(sym.NameFor(type_name->name->symbol), "f32");

    ASSERT_EQ(a->count, nullptr);
}

TEST_F(ParserImplTest, Callable_TypeDecl_VecPrefix) {
    auto p = parser("vec3<f32>");
    auto t = p->callable();
    ASSERT_TRUE(p->peek_is(Token::Type::kEOF));
    EXPECT_TRUE(t.matched);
    EXPECT_FALSE(t.errored);
    ASSERT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(t.value, nullptr);
    ASSERT_TRUE(t.value->Is<ast::Vector>());

    auto& sym = p->builder().Symbols();

    auto* v = t.value->As<ast::Vector>();

    ASSERT_TRUE(v->type->Is<ast::TypeName>());
    EXPECT_EQ(sym.NameFor(v->type->As<ast::TypeName>()->name->symbol), "f32");

    EXPECT_EQ(v->width, 3u);
}

TEST_F(ParserImplTest, Callable_TypeDecl_MatPrefix) {
    auto p = parser("mat3x2<f32>");
    auto t = p->callable();
    ASSERT_TRUE(p->peek_is(Token::Type::kEOF));
    EXPECT_TRUE(t.matched);
    EXPECT_FALSE(t.errored);
    ASSERT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(t.value, nullptr);

    auto& sym = p->builder().Symbols();

    auto* mat_type_name = t->As<ast::TypeName>();
    ASSERT_NE(mat_type_name, nullptr);
    EXPECT_EQ(sym.NameFor(mat_type_name->name->symbol), "mat3x2");

    auto* ident = mat_type_name->name->As<ast::TemplatedIdentifier>();
    ASSERT_NE(ident, nullptr);
    ASSERT_EQ(ident->arguments.Length(), 1u);

    auto* el_type_name = ident->arguments[0]->As<ast::IdentifierExpression>();
    ASSERT_NE(el_type_name, nullptr);
    EXPECT_EQ(sym.NameFor(el_type_name->identifier->symbol), "f32");
}

TEST_F(ParserImplTest, Callable_NoMatch) {
    auto p = parser("ident");
    auto t = p->callable();
    EXPECT_FALSE(t.matched);
    EXPECT_FALSE(t.errored);
    ASSERT_FALSE(p->has_error()) << p->error();
    EXPECT_EQ(nullptr, t.value);
}

}  // namespace
}  // namespace tint::reader::wgsl
