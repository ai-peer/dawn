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
#include "src/tint/sem/test_helper.h"

#include "gmock/gmock.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::resolver {
namespace {

using AFloatV = builder::vec<3, AFloat>;
using AIntV = builder::vec<3, AInt>;
using f32V = builder::vec<3, f32>;
using f16V = builder::vec<3, f16>;
using i32V = builder::vec<3, i32>;
using u32V = builder::vec<3, u32>;

////////////////////////////////////////////////////////////////////////////////
// MaterializeTests
////////////////////////////////////////////////////////////////////////////////
namespace MaterializeTests {

// How should the materialization occur?
enum class Method {
    // var a : T = literal;
    kVar,

    // let a : T = literal;
    kLet,

    // fn F(v : T) {}
    // fn x() {
    //   F(literal);
    // }
    kFnArg,

    // min(target_expr, literal);
    kBuiltinArg,

    // fn F() : T {
    //   return literal;
    // }
    kReturn,

    // array<T, 1>(literal);
    kArray,

    // struct S {
    //   v : T
    // };
    // fn x() {
    //   _ = S(literal)
    // }
    kStruct,

    // target_expr + literal
    kBinaryOp,

    // switch (literal) {
    //   case target_expr: {}
    //   default: {}
    // }
    kSwitchCond,

    // switch (target_expr) {
    //   case literal: {}
    //   default: {}
    // }
    kSwitchCase,

    // switch (literal) {
    //   case 123: {}
    //   case target_expr: {}
    //   default: {}
    // }
    kSwitchCondWithAbstractCase,

    // switch (target_expr) {
    //   case 123: {}
    //   case literal: {}
    //   default: {}
    // }
    kSwitchCaseWithAbstractCase,
};

constexpr Method kCoreMethods[] = {
    Method::kLet,         //
    Method::kVar,         //
    Method::kFnArg,       //
    Method::kBuiltinArg,  //
    Method::kReturn,      //
    Method::kArray,       //
    Method::kStruct,      //
    Method::kBinaryOp,    //
};

static std::ostream& operator<<(std::ostream& o, Method m) {
    switch (m) {
        case Method::kVar:
            return o << "var";
        case Method::kLet:
            return o << "let";
        case Method::kFnArg:
            return o << "fn-arg";
        case Method::kBuiltinArg:
            return o << "builtin-arg";
        case Method::kReturn:
            return o << "return";
        case Method::kArray:
            return o << "array";
        case Method::kStruct:
            return o << "struct";
        case Method::kBinaryOp:
            return o << "binary-op";
        case Method::kSwitchCond:
            return o << "switch-cond";
        case Method::kSwitchCase:
            return o << "switch-case";
        case Method::kSwitchCondWithAbstractCase:
            return o << "switch-cond-with-abstract";
        case Method::kSwitchCaseWithAbstractCase:
            return o << "switch-case-with-abstract";
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
            builder::DataType<TARGET_TYPE>::Expr,     //
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
            builder::DataType<TARGET_TYPE>::Expr,     //
            builder::DataType<LITERAL_TYPE>::Name(),  //
            builder::DataType<LITERAL_TYPE>::Expr,    //
            0_a,
        };
    }

    Expectation expectation;
    std::string target_type_name;
    builder::ast_type_func_ptr target_ast_ty;
    builder::sem_type_func_ptr target_sem_ty;
    builder::ast_expr_func_ptr target_expr;
    std::string literal_type_name;
    builder::ast_expr_func_ptr literal_value;
    std::variant<AInt, AFloat> materialized_value;
};

static std::ostream& operator<<(std::ostream& o, const Case& c) {
    return o << "[" << c.target_type_name << " <- " << c.literal_type_name << "]";
}

using MaterializeAbstractNumeric = resolver::ResolverTestWithParam<std::tuple<Method, Case>>;

TEST_P(MaterializeAbstractNumeric, Test) {
    // Once F16 is properly supported, we'll need to enable this:
    // Enable(ast::Extension::kF16);

    auto& param = GetParam();
    auto& method = std::get<0>(param);
    auto& case_ = std::get<1>(param);
    auto target_ty = [&] { return case_.target_ast_ty(*this); };
    auto target_expr = [&] { return case_.target_expr(*this, 42); };
    auto* literal = case_.literal_value(*this, 1);
    switch (method) {
        case Method::kVar:
            WrapInFunction(Decl(Var("a", target_ty(), literal)));
            break;
        case Method::kLet:
            WrapInFunction(Decl(Let("a", target_ty(), literal)));
            break;
        case Method::kFnArg:
            Func("F", {Param("P", target_ty())}, ty.void_(), {});
            WrapInFunction(CallStmt(Call("F", literal)));
            break;
        case Method::kBuiltinArg:
            WrapInFunction(CallStmt(Call("min", target_expr(), literal)));
            break;
        case Method::kReturn:
            Func("F", {}, target_ty(), {Return(literal)});
            break;
        case Method::kArray:
            WrapInFunction(Construct(ty.array(target_ty(), 1_i), literal));
            break;
        case Method::kStruct:
            Structure("S", {Member("v", target_ty())});
            WrapInFunction(Construct(ty.type_name("S"), literal));
            break;
        case Method::kBinaryOp:
            WrapInFunction(Add(target_expr(), literal));
            break;
        case Method::kSwitchCond:
            WrapInFunction(Switch(literal,                                               //
                                  Case(target_expr()->As<ast::IntLiteralExpression>()),  //
                                  DefaultCase()));
            break;
        case Method::kSwitchCase:
            WrapInFunction(Switch(target_expr(),                                   //
                                  Case(literal->As<ast::IntLiteralExpression>()),  //
                                  DefaultCase()));
            break;
        case Method::kSwitchCondWithAbstractCase:
            WrapInFunction(Switch(literal,                                               //
                                  Case(Expr(123_a)),                                     //
                                  Case(target_expr()->As<ast::IntLiteralExpression>()),  //
                                  DefaultCase()));
            break;
        case Method::kSwitchCaseWithAbstractCase:
            WrapInFunction(Switch(target_expr(),                                   //
                                  Case(Expr(123_a)),                               //
                                  Case(literal->As<ast::IntLiteralExpression>()),  //
                                  DefaultCase()));
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
            std::string expect;
            switch (method) {
                case Method::kBuiltinArg:
                    expect = "error: no matching call to min(" + case_.target_type_name + ", " +
                             case_.literal_type_name + ")";
                    break;
                case Method::kBinaryOp:
                    expect = "error: no matching overload for operator + (" +
                             case_.target_type_name + ", " + case_.literal_type_name + ")";
                    break;
                default:
                    expect = "error: cannot convert value of type '" + case_.literal_type_name +
                             "' to type '" + case_.target_type_name + "'";
                    break;
            }
            EXPECT_THAT(r()->error(), testing::StartsWith(expect));
            break;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(Scalar,
                         MaterializeAbstractNumeric,                                             //
                         testing::Combine(testing::ValuesIn(kCoreMethods),                       //
                                          testing::Values(Case::Pass<f32, AFloat>(1.0_a),        //
                                                          /* Case::Pass<f16, AFloat>(1.0_a), */  //
                                                          Case::Pass<i32, AInt>(1_a),            //
                                                          Case::Pass<u32, AInt>(1_a),            //
                                                          Case::Pass<f32, AFloat>(1.0_a),        //
                                                          /* Case::Pass<f16, AFloat>(1.0_a), */  //
                                                          Case::InvalidCast<i32, AFloat>(),      //
                                                          Case::InvalidCast<u32, AFloat>())));

INSTANTIATE_TEST_SUITE_P(
    Vector,
    MaterializeAbstractNumeric,                                               //
    testing::Combine(testing::ValuesIn(kCoreMethods),                         //
                     testing::Values(Case::Pass<f32V, AFloatV>(1.0_a),        //
                                     /* Case::Pass<f16V, AFloatV>(1.0_a), */  //
                                     Case::Pass<i32V, AIntV>(1_a),            //
                                     Case::Pass<u32V, AIntV>(1_a),            //
                                     Case::Pass<f32V, AFloatV>(1.0_a),        //
                                     /* Case::Pass<f16V, AFloatV>(1.0_a), */  //
                                     Case::InvalidCast<i32V, AFloatV>(),      //
                                     Case::InvalidCast<u32V, AFloatV>())));

INSTANTIATE_TEST_SUITE_P(Switch,
                         MaterializeAbstractNumeric,                                             //
                         testing::Combine(testing::Values(Method::kSwitchCond,                   //
                                                          Method::kSwitchCase,                   //
                                                          Method::kSwitchCondWithAbstractCase,   //
                                                          Method::kSwitchCaseWithAbstractCase),  //
                                          testing::Values(Case::Pass<i32, AInt>(1_a),            //
                                                          Case::Pass<u32, AInt>(1_a))));

}  // namespace MaterializeTests

}  // namespace
}  // namespace tint::resolver
