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

#include "src/tint/sem/materialize.h"
#include "src/tint/resolver/resolver.h"
#include "src/tint/resolver/resolver_test_helper.h"

#include "gmock/gmock.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::resolver {
namespace {

using testing::HasSubstr;

using AFloatV = builder::vec<3, AFloat>;
using AIntV = builder::vec<3, AInt>;
using f32V = builder::vec<3, f32>;
using f16V = builder::vec<3, f16>;
using i32V = builder::vec<3, i32>;
using u32V = builder::vec<3, u32>;

////////////////////////////////////////////////////////////////////////////////
// MaterializationTests
////////////////////////////////////////////////////////////////////////////////
namespace MaterializationTests {

// How should the materialization occur?
enum class Method {
    kVar,    // var a : T = literal;
    kLet,    // let a : T = literal;
    kFnArg,  // fn F(v : T) {}   fn x() { F(literal); }
};

static std::ostream& operator<<(std::ostream& o, Method m) {
    switch (m) {
        case Method::kVar:
            return o << "var";
        case Method::kLet:
            return o << "let";
        case Method::kFnArg:
            return o << "fn-arg";
    }
    return o << "<unknown>";
}

struct Case {
    enum class Expectation { kPass, kInvalidCast };

    template <typename TARGET_TYPE, typename LITERAL_TYPE, typename MATERIALIZED_TYPE>
    static Case Pass(MATERIALIZED_TYPE materialized_value) {
        return {
            Expectation::kPass,
            builder::DataType<TARGET_TYPE>::Name(),   //
            builder::DataType<TARGET_TYPE>::AST,      //
            builder::DataType<TARGET_TYPE>::Sem,      //
            builder::DataType<LITERAL_TYPE>::Name(),  //
            builder::DataType<LITERAL_TYPE>::Expr,    //
            materialized_value,
        };
    }

    template <typename TARGET_TYPE, typename LITERAL_TYPE>
    static Case InvalidCast() {
        return {
            Expectation::kInvalidCast,
            builder::DataType<TARGET_TYPE>::Name(),   //
            builder::DataType<TARGET_TYPE>::AST,      //
            builder::DataType<TARGET_TYPE>::Sem,      //
            builder::DataType<LITERAL_TYPE>::Name(),  //
            builder::DataType<LITERAL_TYPE>::Expr,    //
            0_a,
        };
    }

    Expectation expectation;
    std::string target_type_name;
    builder::ast_type_func_ptr target_ast_ty;
    builder::sem_type_func_ptr target_sem_ty;
    std::string literal_type_name;
    builder::ast_expr_func_ptr literal_value;
    std::variant<AInt, AFloat> materialized_value;
};

static std::ostream& operator<<(std::ostream& o, const Case& c) {
    return o << "[" << c.target_type_name << " <- " << c.literal_type_name << "]";
}

using MaterializeAbstractNumericTest = resolver::ResolverTestWithParam<std::tuple<Method, Case>>;

TEST_P(MaterializeAbstractNumericTest, Var) {
    Enable(ast::Extension::kF16);
    auto& param = GetParam();
    auto& method = std::get<0>(param);
    auto& case_ = std::get<1>(param);
    auto* target_ty = case_.target_ast_ty(*this);
    auto* literal = case_.literal_value(*this, 1);
    switch (method) {
        case Method::kVar:
            WrapInFunction(Decl(Var("a", target_ty, literal)));
            break;
        case Method::kLet:
            WrapInFunction(Decl(Let("a", target_ty, literal)));
            break;
        case Method::kFnArg:
            Func("F", {Param("P", target_ty)}, ty.void_(), {});
            WrapInFunction(CallStmt(Call("F", literal)));
            break;
    }
    auto* target_sem_ty = case_.target_sem_ty(*this);
    switch (case_.expectation) {
        case Case::Expectation::kPass: {
            ASSERT_TRUE(r()->Resolve()) << r()->error();
            auto* materialize = Sem().Get<sem::Materialize>(literal);
            ASSERT_NE(materialize, nullptr);
            EXPECT_TYPE(materialize->Type(), target_sem_ty);
            EXPECT_TYPE(materialize->ConstantValue().Type(), target_sem_ty);

            uint32_t num_elems = 0;
            const sem::Type* target_sem_el_ty = sem::Type::ElementOf(target_sem_ty, &num_elems);
            EXPECT_TYPE(materialize->ConstantValue().ElementType(), target_sem_el_ty);
            std::visit(
                [&](auto&& v) {
                    EXPECT_EQ(materialize->ConstantValue().Elements(),
                              sem::Constant::Scalars(num_elems, {v}));
                },
                case_.materialized_value);

            break;
        }
        case Case::Expectation::kInvalidCast: {
            ASSERT_FALSE(r()->Resolve());
            EXPECT_EQ(r()->error(), "error: cannot convert value of type '" +
                                        case_.literal_type_name + "' to type '" +
                                        case_.target_type_name + "'");
            break;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    Scalar,
    MaterializeAbstractNumericTest,                                                //
    testing::Combine(testing::Values(Method::kLet, Method::kVar, Method::kFnArg),  //
                     testing::Values(Case::Pass<f32, AFloat>(1.0_a),               //
                                     Case::Pass<f16, AFloat>(1.0_a),               //
                                     Case::Pass<i32, AInt>(1_a),                   //
                                     Case::Pass<u32, AInt>(1_a),                   //
                                     Case::Pass<f32, AFloat>(1.0_a),               //
                                     Case::Pass<f16, AFloat>(1.0_a),               //
                                     Case::InvalidCast<i32, AFloat>(),             //
                                     Case::InvalidCast<u32, AFloat>())));

INSTANTIATE_TEST_SUITE_P(
    Vector,
    MaterializeAbstractNumericTest,                                                //
    testing::Combine(testing::Values(Method::kLet, Method::kVar, Method::kFnArg),  //
                     testing::Values(Case::Pass<f32V, AFloatV>(1.0_a),             //
                                     Case::Pass<f16V, AFloatV>(1.0_a),             //
                                     Case::Pass<i32V, AIntV>(1_a),                 //
                                     Case::Pass<u32V, AIntV>(1_a),                 //
                                     Case::Pass<f32V, AFloatV>(1.0_a),             //
                                     Case::Pass<f16V, AFloatV>(1.0_a),             //
                                     Case::InvalidCast<i32V, AFloatV>(),           //
                                     Case::InvalidCast<u32V, AFloatV>())));
}  // namespace MaterializationTests

}  // namespace
}  // namespace tint::resolver
