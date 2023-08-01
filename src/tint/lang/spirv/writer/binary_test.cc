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

#include "src/tint/lang/spirv/writer/common/helper_test.h"

#include "src/tint/lang/core/ir/binary.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::spirv::writer {
namespace {

/// A parameterized test case.
struct BinaryTestCase {
    /// The element type to test.
    TestElementType type;
    /// The binary operation.
    enum core::BinaryOp kind;
    /// The expected SPIR-V instruction.
    std::string spirv_inst;
    /// The expected SPIR-V result type name.
    std::string spirv_type_name;
};

using Arithmetic_Bitwise = SpirvWriterTestWithParam<BinaryTestCase>;
TEST_P(Arithmetic_Bitwise, Scalar) {
    auto params = GetParam();

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        auto* lhs = MakeScalarValue(params.type);
        auto* rhs = MakeScalarValue(params.type);
        auto* result = b.Binary(params.kind, MakeScalarType(params.type), lhs, rhs);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = " + params.spirv_inst + " %" + params.spirv_type_name);
}
TEST_P(Arithmetic_Bitwise, Vector) {
    auto params = GetParam();

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        auto* lhs = MakeVectorValue(params.type);
        auto* rhs = MakeVectorValue(params.type);
        auto* result = b.Binary(params.kind, MakeVectorType(params.type), lhs, rhs);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = " + params.spirv_inst + " %v2" + params.spirv_type_name);
}
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_I32,
    Arithmetic_Bitwise,
    testing::Values(BinaryTestCase{kI32, core::BinaryOp::kAdd, "OpIAdd", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kSubtract, "OpISub", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kMultiply, "OpIMul", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kDivide, "OpSDiv", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kModulo, "OpSRem", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kAnd, "OpBitwiseAnd", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kOr, "OpBitwiseOr", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kXor, "OpBitwiseXor", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kShiftLeft, "OpShiftLeftLogical", "int"},
                    BinaryTestCase{kI32, core::BinaryOp::kShiftRight, "OpShiftRightArithmetic",
                                   "int"}));
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_U32,
    Arithmetic_Bitwise,
    testing::Values(
        BinaryTestCase{kU32, core::BinaryOp::kAdd, "OpIAdd", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kSubtract, "OpISub", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kMultiply, "OpIMul", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kDivide, "OpUDiv", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kModulo, "OpUMod", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kAnd, "OpBitwiseAnd", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kOr, "OpBitwiseOr", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kXor, "OpBitwiseXor", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kShiftLeft, "OpShiftLeftLogical", "uint"},
        BinaryTestCase{kU32, core::BinaryOp::kShiftRight, "OpShiftRightLogical", "uint"}));
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_F32,
    Arithmetic_Bitwise,
    testing::Values(BinaryTestCase{kF32, core::BinaryOp::kAdd, "OpFAdd", "float"},
                    BinaryTestCase{kF32, core::BinaryOp::kSubtract, "OpFSub", "float"},
                    BinaryTestCase{kF32, core::BinaryOp::kMultiply, "OpFMul", "float"},
                    BinaryTestCase{kF32, core::BinaryOp::kDivide, "OpFDiv", "float"},
                    BinaryTestCase{kF32, core::BinaryOp::kModulo, "OpFRem", "float"}));
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_F16,
    Arithmetic_Bitwise,
    testing::Values(BinaryTestCase{kF16, core::BinaryOp::kAdd, "OpFAdd", "half"},
                    BinaryTestCase{kF16, core::BinaryOp::kSubtract, "OpFSub", "half"},
                    BinaryTestCase{kF16, core::BinaryOp::kMultiply, "OpFMul", "half"},
                    BinaryTestCase{kF16, core::BinaryOp::kDivide, "OpFDiv", "half"},
                    BinaryTestCase{kF16, core::BinaryOp::kModulo, "OpFRem", "half"}));
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_Bool,
    Arithmetic_Bitwise,
    testing::Values(BinaryTestCase{kBool, core::BinaryOp::kAnd, "OpLogicalAnd", "bool"},
                    BinaryTestCase{kBool, core::BinaryOp::kOr, "OpLogicalOr", "bool"}));

TEST_F(SpirvWriterTest, Binary_ScalarTimesVector_F32) {
    auto* scalar = b.FunctionParam("scalar", ty.f32());
    auto* vector = b.FunctionParam("vector", ty.vec4<f32>());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({scalar, vector});
    b.Append(func->Block(), [&] {
        auto* result = b.Multiply(ty.vec4<f32>(), scalar, vector);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = OpVectorTimesScalar %v4float %vector %scalar");
}

TEST_F(SpirvWriterTest, Binary_VectorTimesScalar_F32) {
    auto* scalar = b.FunctionParam("scalar", ty.f32());
    auto* vector = b.FunctionParam("vector", ty.vec4<f32>());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({scalar, vector});
    b.Append(func->Block(), [&] {
        auto* result = b.Multiply(ty.vec4<f32>(), vector, scalar);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = OpVectorTimesScalar %v4float %vector %scalar");
}

TEST_F(SpirvWriterTest, Binary_ScalarTimesMatrix_F32) {
    auto* scalar = b.FunctionParam("scalar", ty.f32());
    auto* matrix = b.FunctionParam("matrix", ty.mat3x4<f32>());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({scalar, matrix});
    b.Append(func->Block(), [&] {
        auto* result = b.Multiply(ty.mat3x4<f32>(), scalar, matrix);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = OpMatrixTimesScalar %mat3v4float %matrix %scalar");
}

TEST_F(SpirvWriterTest, Binary_MatrixTimesScalar_F32) {
    auto* scalar = b.FunctionParam("scalar", ty.f32());
    auto* matrix = b.FunctionParam("matrix", ty.mat3x4<f32>());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({scalar, matrix});
    b.Append(func->Block(), [&] {
        auto* result = b.Multiply(ty.mat3x4<f32>(), matrix, scalar);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = OpMatrixTimesScalar %mat3v4float %matrix %scalar");
}

TEST_F(SpirvWriterTest, Binary_VectorTimesMatrix_F32) {
    auto* vector = b.FunctionParam("vector", ty.vec4<f32>());
    auto* matrix = b.FunctionParam("matrix", ty.mat3x4<f32>());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({vector, matrix});
    b.Append(func->Block(), [&] {
        auto* result = b.Multiply(ty.vec3<f32>(), vector, matrix);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = OpVectorTimesMatrix %v3float %vector %matrix");
}

TEST_F(SpirvWriterTest, Binary_MatrixTimesVector_F32) {
    auto* vector = b.FunctionParam("vector", ty.vec3<f32>());
    auto* matrix = b.FunctionParam("matrix", ty.mat3x4<f32>());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({vector, matrix});
    b.Append(func->Block(), [&] {
        auto* result = b.Multiply(ty.vec4<f32>(), matrix, vector);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = OpMatrixTimesVector %v4float %matrix %vector");
}

TEST_F(SpirvWriterTest, Binary_MatrixTimesMatrix_F32) {
    auto* mat1 = b.FunctionParam("mat1", ty.mat4x3<f32>());
    auto* mat2 = b.FunctionParam("mat2", ty.mat3x4<f32>());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({mat1, mat2});
    b.Append(func->Block(), [&] {
        auto* result = b.Multiply(ty.mat3x3<f32>(), mat1, mat2);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = OpMatrixTimesMatrix %mat3v3float %mat1 %mat2");
}

using Comparison = SpirvWriterTestWithParam<BinaryTestCase>;
TEST_P(Comparison, Scalar) {
    auto params = GetParam();

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        auto* lhs = MakeScalarValue(params.type);
        auto* rhs = MakeScalarValue(params.type);
        auto* result = b.Binary(params.kind, ty.bool_(), lhs, rhs);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = " + params.spirv_inst + " %bool");
}

TEST_P(Comparison, Vector) {
    auto params = GetParam();

    auto* func = b.Function("foo", ty.void_());
    b.Append(func->Block(), [&] {
        auto* lhs = MakeVectorValue(params.type);
        auto* rhs = MakeVectorValue(params.type);
        auto* result = b.Binary(params.kind, ty.vec2<bool>(), lhs, rhs);
        b.Return(func);
        mod.SetName(result, "result");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%result = " + params.spirv_inst + " %v2bool");
}
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_I32,
    Comparison,
    testing::Values(
        BinaryTestCase{kI32, core::BinaryOp::kEqual, "OpIEqual", "bool"},
        BinaryTestCase{kI32, core::BinaryOp::kNotEqual, "OpINotEqual", "bool"},
        BinaryTestCase{kI32, core::BinaryOp::kGreaterThan, "OpSGreaterThan", "bool"},
        BinaryTestCase{kI32, core::BinaryOp::kGreaterThanEqual, "OpSGreaterThanEqual", "bool"},
        BinaryTestCase{kI32, core::BinaryOp::kLessThan, "OpSLessThan", "bool"},
        BinaryTestCase{kI32, core::BinaryOp::kLessThanEqual, "OpSLessThanEqual", "bool"}));
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_U32,
    Comparison,
    testing::Values(
        BinaryTestCase{kU32, core::BinaryOp::kEqual, "OpIEqual", "bool"},
        BinaryTestCase{kU32, core::BinaryOp::kNotEqual, "OpINotEqual", "bool"},
        BinaryTestCase{kU32, core::BinaryOp::kGreaterThan, "OpUGreaterThan", "bool"},
        BinaryTestCase{kU32, core::BinaryOp::kGreaterThanEqual, "OpUGreaterThanEqual", "bool"},
        BinaryTestCase{kU32, core::BinaryOp::kLessThan, "OpULessThan", "bool"},
        BinaryTestCase{kU32, core::BinaryOp::kLessThanEqual, "OpULessThanEqual", "bool"}));
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_F32,
    Comparison,
    testing::Values(
        BinaryTestCase{kF32, core::BinaryOp::kEqual, "OpFOrdEqual", "bool"},
        BinaryTestCase{kF32, core::BinaryOp::kNotEqual, "OpFOrdNotEqual", "bool"},
        BinaryTestCase{kF32, core::BinaryOp::kGreaterThan, "OpFOrdGreaterThan", "bool"},
        BinaryTestCase{kF32, core::BinaryOp::kGreaterThanEqual, "OpFOrdGreaterThanEqual", "bool"},
        BinaryTestCase{kF32, core::BinaryOp::kLessThan, "OpFOrdLessThan", "bool"},
        BinaryTestCase{kF32, core::BinaryOp::kLessThanEqual, "OpFOrdLessThanEqual", "bool"}));
INSTANTIATE_TEST_SUITE_P(
    SpirvWriterTest_Binary_F16,
    Comparison,
    testing::Values(
        BinaryTestCase{kF16, core::BinaryOp::kEqual, "OpFOrdEqual", "bool"},
        BinaryTestCase{kF16, core::BinaryOp::kNotEqual, "OpFOrdNotEqual", "bool"},
        BinaryTestCase{kF16, core::BinaryOp::kGreaterThan, "OpFOrdGreaterThan", "bool"},
        BinaryTestCase{kF16, core::BinaryOp::kGreaterThanEqual, "OpFOrdGreaterThanEqual", "bool"},
        BinaryTestCase{kF16, core::BinaryOp::kLessThan, "OpFOrdLessThan", "bool"},
        BinaryTestCase{kF16, core::BinaryOp::kLessThanEqual, "OpFOrdLessThanEqual", "bool"}));
INSTANTIATE_TEST_SUITE_P(SpirvWriterTest_Binary_Bool,
                         Comparison,
                         testing::Values(BinaryTestCase{kBool, core::BinaryOp::kEqual,
                                                        "OpLogicalEqual", "bool"},
                                         BinaryTestCase{kBool, core::BinaryOp::kNotEqual,
                                                        "OpLogicalNotEqual", "bool"}));

TEST_F(SpirvWriterTest, Binary_Chain) {
    auto* func = b.Function("foo", ty.void_());

    b.Append(func->Block(), [&] {
        auto* sub = b.Subtract(ty.i32(), 1_i, 2_i);
        auto* add = b.Add(ty.i32(), sub, sub);
        b.Return(func);
        mod.SetName(sub, "sub");
        mod.SetName(add, "add");
    });

    ASSERT_TRUE(Generate()) << Error() << output_;
    EXPECT_INST("%sub = OpISub %int %int_1 %int_2");
    EXPECT_INST("%add = OpIAdd %int %sub %sub");
}

}  // namespace
}  // namespace tint::spirv::writer
