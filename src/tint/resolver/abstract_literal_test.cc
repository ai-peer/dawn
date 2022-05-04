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

#include "src/tint/resolver/resolver.h"
#include "src/tint/resolver/resolver_test_helper.h"
#include "src/tint/sem/materialize.h"

#include "gmock/gmock.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::resolver {
namespace {

struct AbstractLiteralTypeTest : public resolver::TestHelper, public testing::Test {};

TEST_F(AbstractLiteralTypeTest, InferLet) {
    // let a = 123;
    auto* expr = Expr(123_ai);
    WrapInFunction(Decl(Let("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::I32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractLiteralTypeTest, InferVar) {
    // var a = 123;
    auto* expr = Expr(123_ai);
    WrapInFunction(Decl(Var("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::I32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractLiteralTypeTest, I32Var) {
    // var a : i32 = 123;
    auto* expr = Expr(123_ai);
    WrapInFunction(Decl(Var("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::I32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractLiteralTypeTest, U32Let) {
    // let a : u32 = 123;
    auto* expr = Expr(123_ai);
    WrapInFunction(Decl(Let("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::U32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::U32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::U32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

}  // namespace
}  // namespace tint::resolver
