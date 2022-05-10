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

struct AbstractNumericTypeTest : public resolver::TestHelper, public testing::Test {};

TEST_F(AbstractNumericTypeTest, InferLetWithAbstractInt) {
    // let a = 123;
    auto* expr = Expr(123_a);
    WrapInFunction(Decl(Let("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::I32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractNumericTypeTest, InferVarWithAbstractInt) {
    // var a = 123;
    auto* expr = Expr(123_a);
    WrapInFunction(Decl(Var("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::I32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractNumericTypeTest, InferLetWithAbstractFloat) {
    // let a = 123.0;
    auto* expr = Expr(123.0_a);
    WrapInFunction(Decl(Let("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::F32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::F32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::F32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AFloat(123)});
}

TEST_F(AbstractNumericTypeTest, InferVarWithAbstractFloat) {
    // var a = 123.0;
    auto* expr = Expr(123.0_a);
    WrapInFunction(Decl(Var("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::F32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::F32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::F32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AFloat(123)});
}

TEST_F(AbstractNumericTypeTest, I32Let) {
    // let a : i32 = 123;
    auto* expr = Expr(123_a);
    WrapInFunction(Decl(Let("a", ty.i32(), expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::I32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractNumericTypeTest, I32Var) {
    // var a : i32 = 123;
    auto* expr = Expr(123_a);
    WrapInFunction(Decl(Var("a", nullptr, expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::I32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::I32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractNumericTypeTest, U32Let) {
    // let a : u32 = 123;
    auto* expr = Expr(123_a);
    WrapInFunction(Decl(Let("a", ty.u32(), expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::U32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::U32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::U32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractNumericTypeTest, U32Var) {
    // var a : u32 = 123;
    auto* expr = Expr(123_a);
    WrapInFunction(Decl(Var("a", ty.u32(), expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::U32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::U32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::U32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AInt(123)});
}

TEST_F(AbstractNumericTypeTest, F32Let) {
    // let a : f32 = 123.0;
    auto* expr = Expr(123.0_a);
    WrapInFunction(Decl(Let("a", ty.f32(), expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::F32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::F32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::F32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AFloat(123)});
}

TEST_F(AbstractNumericTypeTest, F32Var) {
    // var a : f32 = 123.0;
    auto* expr = Expr(123.0_a);
    WrapInFunction(Decl(Var("a", ty.f32(), expr)));
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    auto* sem = Sem().Get<sem::Materialize>(expr);
    ASSERT_NE(sem, nullptr);
    EXPECT_TRUE(sem->Type()->Is<sem::F32>());
    EXPECT_TRUE(sem->ConstantValue().Type()->Is<sem::F32>());
    EXPECT_TRUE(sem->ConstantValue().ElementType()->Is<sem::F32>());
    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{AFloat(123)});
}

////////////////////////////////////////////////////////////////////////////////
// MaterializationTests
////////////////////////////////////////////////////////////////////////////////
namespace MaterializationTests {

struct Case {
    enum class Expectation { kPass, kFail };

    template <typename TARGET_TYPE, typename LITERAL_TYPE, typename MATERIALIZED_TYPE>
    static Case Pass(LITERAL_TYPE literal_value, MATERIALIZED_TYPE materialized_value) {
        return {
            Expectation::kPass,
            builder::DataType<TARGET_TYPE>::AST,  //
            builder::DataType<TARGET_TYPE>::Sem,  //
            literal_value,
            materialized_value,
        };
    }

    template <typename TARGET_TYPE, typename LITERAL_TYPE>
    static Case Fail(LITERAL_TYPE literal_value) {
        return {
            Expectation::kFail,
            builder::DataType<TARGET_TYPE>::AST,  //
            builder::DataType<TARGET_TYPE>::Sem,  //
            literal_value,
            0_a,
        };
    }

    Expectation expectation;
    builder::ast_type_func_ptr target_ast_ty;
    builder::sem_type_func_ptr target_sem_ty;
    std::variant<AInt, AFloat> literal_value;
    std::variant<AInt, AFloat> materialized_value;
};

using MaterializeAbstractNumericTest = resolver::ResolverTestWithParam<Case>;

TEST_P(MaterializeAbstractNumericTest, DISABLED_Var) {
    // var a : T = literal;
    Enable(ast::Extension::kF16);
    auto& param = GetParam();
    auto* expr = std::visit([&](auto&& v) { return static_cast<const ast::Expression*>(Expr(v)); },
                            param.literal_value);
    WrapInFunction(Decl(Var("a", param.target_ast_ty(*this), expr)));
    auto* target_sem_ty = param.target_sem_ty(*this);
    switch (param.expectation) {
        case Case::Expectation::kPass: {
            ASSERT_TRUE(r()->Resolve()) << r()->error();
            auto* sem = Sem().Get<sem::Materialize>(expr);
            ASSERT_NE(sem, nullptr);
            EXPECT_TYPE(sem->Type(), target_sem_ty);
            EXPECT_TYPE(sem->ConstantValue().Type(), target_sem_ty);
            EXPECT_TYPE(sem->ConstantValue().ElementType(), target_sem_ty);
            std::visit(
                [&](auto&& v) {
                    EXPECT_EQ(sem->ConstantValue().Elements(), sem::Constant::Scalars{{v}});
                },
                param.materialized_value);

            break;
        }
        case Case::Expectation::kFail: {
            EXPECT_FALSE(r()->Resolve());
            break;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(Scalar,
                         MaterializeAbstractNumericTest,
                         testing::Values(Case::Pass<f32>(1_a, 1.0_a),    //
                                         Case::Pass<f16>(1_a, 1.0_a),    //
                                         Case::Pass<i32>(1_a, 1_a),      //
                                         Case::Pass<u32>(1_a, 1_a),      //
                                         Case::Pass<f32>(1.0_a, 1.0_a),  //
                                         Case::Pass<f16>(1.0_a, 1.0_a),  //
                                         Case::Fail<i32>(1.0_a),         //
                                         Case::Fail<u32>(1.0_a)));

}  // namespace MaterializationTests

}  // namespace
}  // namespace tint::resolver
