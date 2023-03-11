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

#include "gtest/gtest.h"

#include "tint/interp/invocation.h"
#include "tint/interp/memory.h"
#include "tint/interp/shader_executor.h"
#include "tint/lang/core/constant/scalar.h"
#include "tint/lang/wgsl/ast/module.h"
#include "tint/tint.h"
#include "tint/utils/text/styled_text_printer.h"

namespace tint::interp {
namespace {

class InvocationTest : public testing::Test {
  protected:
    void Init(std::string source) {
        // Parse the source file to produce a Tint program object.
        wgsl::reader::Options options;
        options.allowed_features = wgsl::AllowedFeatures::Everything();
        file_ = std::make_unique<Source::File>("test.wgsl", source);
        program_ = std::make_unique<Program>(wgsl::reader::Parse(file_.get(), options));
        auto diag_printer = StyledTextPrinter::Create(stderr);
        diag::Formatter diag_formatter;
        ASSERT_TRUE(program_) << "Failed to run WGSL parser.";
        if (program_->Diagnostics().Count() > 0) {
            diag_printer->Print(diag_formatter.Format(program_->Diagnostics()));
        }
        ASSERT_TRUE(program_->IsValid()) << "Source WGSL was invalid.";

        // Create the shader executor and a single invocation.
        auto result = ShaderExecutor::Create(*program_, "main", {});
        ASSERT_EQ(result, Success) << result.Failure();
        executor_ = result.Move();
        ASSERT_NE(executor_, nullptr);
        executor_->AddErrorCallback([&](auto&& error) { errors_ += error.Str(); });
        invocation_ = std::make_unique<Invocation>(*executor_, UVec3(0, 0, 0), UVec3(0, 0, 0));
    }

    /// Step the invocation over one expression.
    void StepExpr() { invocation_->Step(); }

    /// Step the invocation over one statement.
    void StepStmt() {
        auto* prev_stmt = invocation_->CurrentStatement();
        while (invocation_->GetState() == Invocation::State::kReady) {
            StepExpr();
            if (!prev_stmt || invocation_->CurrentStatement() != prev_stmt) {
                break;
            }
        }
    }

    /// Step the invocation until it reaches a statement with the type `T`.
    template <typename T>
    void ContinueTo() {
        auto* previous = invocation_->CurrentStatement();
        while (invocation_->GetState() == Invocation::State::kReady) {
            StepStmt();
            auto* current = invocation_->CurrentStatement();
            if (current->Is<T>() && current != previous) {
                break;
            }
        }
    }

    /// Step the invocation until it reaches the closing brace of the entry point.
    void ContinueToEnd() {
        while (invocation_->GetState() == Invocation::State::kReady) {
            if (invocation_->CurrentStatement() == nullptr &&
                invocation_->CurrentBlock() == executor_->EntryPoint()->body) {
                break;
            }
            StepStmt();
        }
    }

    /// Make sure no errors occurred during execution.
    void TearDown() override { EXPECT_TRUE(errors_.empty()) << errors_; }

    std::unique_ptr<Source::File> file_;
    std::unique_ptr<Program> program_;
    std::unique_ptr<ShaderExecutor> executor_;
    std::unique_ptr<Invocation> invocation_;
    std::string errors_;
};

// Check that the value of the identifier `name` matches `value`.
#define CHECK_VALUE(name, value) EXPECT_EQ(invocation_->GetValue(name), value)

TEST_F(InvocationTest, Basic) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  let v = 42;
}
)");
    StepStmt();
    CHECK_VALUE("v", "42");
    StepStmt();
}

TEST_F(InvocationTest, DeclScope) {
    Init(R"(
var<private> a = 1.5;

@compute @workgroup_size(1)
fn main() {
  let v = 42;
  {
    let v = 7;
    {
      let a : i32 = 10;
    }
  }
  a = 0.5;
}
)");
    CHECK_VALUE("v", "<identifier not found>");
    CHECK_VALUE("a", "1.500000");
    StepStmt();
    CHECK_VALUE("v", "42");
    StepStmt();
    CHECK_VALUE("v", "42");
    StepStmt();
    CHECK_VALUE("v", "7");
    StepStmt();
    StepStmt();
    CHECK_VALUE("a", "10");
    StepStmt();
    CHECK_VALUE("a", "1.500000");
    StepStmt();
    CHECK_VALUE("a", "1.500000");
    CHECK_VALUE("v", "42");
    StepStmt();
    CHECK_VALUE("a", "0.500000");
}

TEST_F(InvocationTest, BinaryAdd) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  let a = 42;
  let b = -7;
  let c = a + b;
  let d = 100 + c;
}
)");
    ContinueToEnd();
    CHECK_VALUE("c", "35");
    CHECK_VALUE("d", "135");
}

TEST_F(InvocationTest, FunctionVars) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v1 = 100;
  var v2 = -7;
  v1 = 42;
  var v3 = v1 + v2;
  v2 = 1 + v2;
}
)");
    StepStmt();
    CHECK_VALUE("v1", "100");
    StepStmt();
    CHECK_VALUE("v2", "-7");
    StepStmt();
    CHECK_VALUE("v1", "42");
    StepStmt();
    CHECK_VALUE("v3", "35");
    StepStmt();
    CHECK_VALUE("v2", "-6");
    StepStmt();
}

TEST_F(InvocationTest, PrivateVars) {
    Init(R"(
var<private> v1 = 100;
var<private> v2 = -7;
var<private> v3 : i32;

@compute @workgroup_size(1)
fn main() {
  v1 = 42;
  v3 = v1 + v2;
}
)");
    CHECK_VALUE("v1", "100");
    CHECK_VALUE("v2", "-7");
    CHECK_VALUE("v3", "0");
    StepStmt();
    CHECK_VALUE("v1", "42");
    StepStmt();
    CHECK_VALUE("v3", "35");
    StepStmt();
}

TEST_F(InvocationTest, ModuleConstants) {
    Init(R"(
const a = 42;
const b = 10.5;
const c = array<i32, 4>(1, 2, 3, 4);

@compute @workgroup_size(1)
fn main() {
  var v1 = a;
  var v2 = a + b;
  var v3 = c;
}
)");
    CHECK_VALUE("a", "42");
    CHECK_VALUE("b", "10.500000");
    CHECK_VALUE("c", R"(array<i32, 4>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
})");
    ContinueToEnd();
    CHECK_VALUE("v1", "42");
    CHECK_VALUE("v2", "52.500000");
    CHECK_VALUE("v3", R"(array<i32, 4>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
})");
}

TEST_F(InvocationTest, FunctionConstants) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  const a = 42;
  const b = 10.5;
  const c = array<i32, 4>(1, 2, 3, 4);
  var v1 = a;
  var v2 = a + b;
  var v3 = c;
}
)");
    ContinueToEnd();
    CHECK_VALUE("a", "42");
    CHECK_VALUE("b", "10.500000");
    CHECK_VALUE("c", R"(array<i32, 4>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
})");
    CHECK_VALUE("v1", "42");
    CHECK_VALUE("v2", "52.500000");
    CHECK_VALUE("v3", R"(array<i32, 4>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
})");
}

TEST_F(InvocationTest, ConstAssert) {
    Init(R"(
const a = 42;
const b = 10.5;
const_assert(a > b);

@compute @workgroup_size(1)
fn main() {
  const a = 42;
  const b = 10.5;
  const_assert(a != b);
  let c = a;
}
)");
    ContinueToEnd();
    CHECK_VALUE("c", "42");
}

TEST_F(InvocationTest, ZeroInit) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v1 : i32;
  var v2 = 42;
  let result = v1 + v2;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "0");
    CHECK_VALUE("result", "42");
}

TEST_F(InvocationTest, Bool) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v1 : bool;
  var v2 = true;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "false");
    CHECK_VALUE("v2", "true");
}

TEST_F(InvocationTest, U32) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v1 : u32;
  var v2 = 4000000000u;
  var v3 = v2 + 1;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "0");
    CHECK_VALUE("v2", "4000000000");
    CHECK_VALUE("v3", "4000000001");
}

TEST_F(InvocationTest, F32) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v1 : f32;
  var v2 = 12.25;
  var v3 = v2 + 56.25;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "0.000000");
    CHECK_VALUE("v2", "12.250000");
    CHECK_VALUE("v3", "68.500000");
}

TEST_F(InvocationTest, F16) {
    Init(R"(
enable f16;

@compute @workgroup_size(1)
fn main() {
  var v1 : f16;
  var v2 = 12.25h;
  var v3 = v2 + 56.25h;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "0.000000");
    CHECK_VALUE("v2", "12.250000");
    CHECK_VALUE("v3", "68.500000");
}

TEST_F(InvocationTest, Vec) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v1 : vec4<u32>;
  var v2 = vec4<u32>(42);
  let v3 = vec4<u32>(1, 2, 3, 4);
  var v4 = v2 + v3;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "vec4<u32>{0, 0, 0, 0}");
    CHECK_VALUE("v2", "vec4<u32>{42, 42, 42, 42}");
    CHECK_VALUE("v3", "vec4<u32>{1, 2, 3, 4}");
    CHECK_VALUE("v4", "vec4<u32>{43, 44, 45, 46}");
}

TEST_F(InvocationTest, Mat) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v1 : mat2x3<f32>;
  var v2 = mat2x3<f32>(vec3<f32>(42), vec3<f32>(0.5));
  let v3 = mat2x3<f32>(1, 2, 3, 4, 5, 6);
  var v4 = v2 + v3;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", R"(mat2x3<f32>{
  vec3<f32>{0.000000, 0.000000, 0.000000},
  vec3<f32>{0.000000, 0.000000, 0.000000},
})");
    CHECK_VALUE("v2", R"(mat2x3<f32>{
  vec3<f32>{42.000000, 42.000000, 42.000000},
  vec3<f32>{0.500000, 0.500000, 0.500000},
})");
    CHECK_VALUE("v3", R"(mat2x3<f32>{
  vec3<f32>{1.000000, 2.000000, 3.000000},
  vec3<f32>{4.000000, 5.000000, 6.000000},
})");
    CHECK_VALUE("v4", R"(mat2x3<f32>{
  vec3<f32>{43.000000, 44.000000, 45.000000},
  vec3<f32>{4.500000, 5.500000, 6.500000},
})");
}

TEST_F(InvocationTest, Arrays) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr1 = array(1, 2, 3, 4);
  var arr2 = array(array(1, 2, 3, 4), array(5, 6, 7, 8));

  var i = 2;
  var arr3 = array(arr1[0], i, i + 1, 4);
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr1", R"(array<i32, 4>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
})");
    CHECK_VALUE("arr2", R"(array<array<i32, 4>, 2>{
  [0] = array<i32, 4>{
    [0] = 1,
    [1] = 2,
    [2] = 3,
    [3] = 4,
  },
  [1] = array<i32, 4>{
    [0] = 5,
    [1] = 6,
    [2] = 7,
    [3] = 8,
  },
})");
    CHECK_VALUE("arr3", R"(array<i32, 4>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
})");
}

TEST_F(InvocationTest, IndexAccessor_Array) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  let arr1 = array(1, 2, 3, 4);
  var arr2 = array(5, 6, 7, 8);
  let v1 = arr1[0];
  let v3 = arr1[2];
  let v6 = arr2[1];
  let v8 = arr2[3];
  let result = arr1[3] + arr2[0];
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "1");
    CHECK_VALUE("v3", "3");
    CHECK_VALUE("v6", "6");
    CHECK_VALUE("v8", "8");
    CHECK_VALUE("result", "9");
}

TEST_F(InvocationTest, IndexAccessor_NestedArray) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr = array(array(1, 2), array(3, 4), array(5, 6), array(7, 8));
  let v1 = arr[0][0];
  let v3 = arr[1][0];
  let v6 = arr[2][1];
  let v7 = arr[3][0];
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "1");
    CHECK_VALUE("v3", "3");
    CHECK_VALUE("v6", "6");
    CHECK_VALUE("v7", "7");
}

TEST_F(InvocationTest, IndexAccessor_Mat3x3) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  let mat1 = mat3x3(1, 2, 3, 4, 5, 6, 7, 8, 9);
  var mat2 = mat3x3(9, 8, 7, 6, 5, 4, 3, 2, 1);
  let v123 = mat1[0];
  let v456 = mat1[1];
  let v789 = mat1[2];
  let v987 = mat2[0];
  let v654 = mat2[1];
  let v321 = mat2[2];
  let result = mat1[0] + mat2[1];
}
)");
    ContinueToEnd();
    CHECK_VALUE("v123", "vec3<f32>{1.000000, 2.000000, 3.000000}");
    CHECK_VALUE("v456", "vec3<f32>{4.000000, 5.000000, 6.000000}");
    CHECK_VALUE("v789", "vec3<f32>{7.000000, 8.000000, 9.000000}");
    CHECK_VALUE("v987", "vec3<f32>{9.000000, 8.000000, 7.000000}");
    CHECK_VALUE("v654", "vec3<f32>{6.000000, 5.000000, 4.000000}");
    CHECK_VALUE("v321", "vec3<f32>{3.000000, 2.000000, 1.000000}");
    CHECK_VALUE("result", "vec3<f32>{7.000000, 7.000000, 7.000000}");
}

TEST_F(InvocationTest, Struct) {
    Init(R"(
struct S1 {
  a : i32,
  b : i32,
}

struct S2 {
  c : i32,
  d : i32,
  e : array<S1, 4>,
  f : i32,
}

@compute @workgroup_size(1)
fn main() {
  var s1 = S1(42, -7);
  var s2 = S2(1234, -9876, array(S1(1, 2), S1(3, 4), S1(5, 6), S1(7, 8)), 42);

  var a = 42;
  var s3 = S1(a, s2.e[0].b);
}
)");
    ContinueToEnd();
    CHECK_VALUE("s1", R"(S1{
  .a = 42,
  .b = -7,
})");
    CHECK_VALUE("s2", R"(S2{
  .c = 1234,
  .d = -9876,
  .e = array<S1, 4>{
    [0] = S1{
      .a = 1,
      .b = 2,
    },
    [1] = S1{
      .a = 3,
      .b = 4,
    },
    [2] = S1{
      .a = 5,
      .b = 6,
    },
    [3] = S1{
      .a = 7,
      .b = 8,
    },
  },
  .f = 42,
})");
    CHECK_VALUE("s3", R"(S1{
  .a = 42,
  .b = 2,
})");
}

TEST_F(InvocationTest, MemberAccessor_Struct) {
    Init(R"(
struct S1 {
  a : i32,
  b : i32,
}

struct S2 {
  c : i32,
  d : i32,
  e : array<S1, 4>,
  f : i32,
}

@compute @workgroup_size(1)
fn main() {
  var s1 = S1(42, -7);
  var s2 = S2(1234, -9876, array(S1(1, 2), S1(3, 4), S1(5, 6), S1(7, 8)), 42);
  let result1 = s1.a + s1.b;
  let result2 = s2.c + s2.e[2].b + s2.f;
}
)");
    ContinueToEnd();
    CHECK_VALUE("result1", "35");
    CHECK_VALUE("result2", "1282");
}

TEST_F(InvocationTest, VectorSwizzle_Var) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = vec4(1, 2, 3, 4);
  let result1 = v.x + v.y;
  let result2 = v.wx + v.bg;
  v.y = 42;
}
)");
    ContinueToEnd();
    CHECK_VALUE("result1", "3");
    CHECK_VALUE("result2", "vec2<i32>{7, 3}");
    CHECK_VALUE("v", "vec4<i32>{1, 42, 3, 4}");
}

TEST_F(InvocationTest, VectorSwizzle_Let) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  let v = vec4(1, 2, 3, 4);
  let result1 = v.x + v.y;
  let result2 = v.wx + v.bg;
}
)");
    ContinueToEnd();
    CHECK_VALUE("result1", "3");
    CHECK_VALUE("result2", "vec2<i32>{7, 3}");
}

TEST_F(InvocationTest, VectorConstructor) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var i = 1;
  var v1 = vec4(i, i + 1, i + 2, i + 3);
  var v2 = vec4(v1);
  var v3 = vec4(i);
  var v4 = vec4(v1.zyx, 42);
}
)");
    ContinueToEnd();
    CHECK_VALUE("v1", "vec4<i32>{1, 2, 3, 4}");
    CHECK_VALUE("v2", "vec4<i32>{1, 2, 3, 4}");
    CHECK_VALUE("v3", "vec4<i32>{1, 1, 1, 1}");
    CHECK_VALUE("v4", "vec4<i32>{3, 2, 1, 42}");
}

TEST_F(InvocationTest, MatrixConstructor) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var i = 1.f;
  var m1 = mat2x2(i, i + 1, i + 2, i + 3);
  var m2 = mat2x2(m1[0], m1[1]);
  var m3 = mat2x2(vec2(i + 1, i), m1[1].yx);
}
)");
    ContinueToEnd();
    CHECK_VALUE("m1", R"(mat2x2<f32>{
  vec2<f32>{1.000000, 2.000000},
  vec2<f32>{3.000000, 4.000000},
})");
    CHECK_VALUE("m2", R"(mat2x2<f32>{
  vec2<f32>{1.000000, 2.000000},
  vec2<f32>{3.000000, 4.000000},
})");
    CHECK_VALUE("m3", R"(mat2x2<f32>{
  vec2<f32>{2.000000, 1.000000},
  vec2<f32>{4.000000, 3.000000},
})");
}

TEST_F(InvocationTest, Bitcast) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  let a : i32 = 0x40000042;
  let b : u32 = 0xc2478000;
  let c : f32 = -798.25;

  let a_i = bitcast<i32>(a);
  let a_u = bitcast<u32>(a);
  let a_f = bitcast<f32>(a);

  let b_i = bitcast<i32>(b);
  let b_u = bitcast<u32>(b);
  let b_f = bitcast<f32>(b);

  let c_i = bitcast<i32>(c);
  let c_u = bitcast<u32>(c);
  let c_f = bitcast<f32>(c);

  let v = vec2<u32>(0x40000042, 0xc2478000);
  let v_i = bitcast<vec2<i32>>(v);
  let v_u = bitcast<vec2<u32>>(v);
  let v_f = bitcast<vec2<f32>>(v);
}
)");
    ContinueToEnd();
    CHECK_VALUE("a_i", std::to_string(core::i32(0x40000042)));
    CHECK_VALUE("a_u", std::to_string(core::u32(0x40000042)));
    CHECK_VALUE("a_f", "2.000016");
    CHECK_VALUE("b_i", std::to_string(core::i32(0xc2478000)));
    CHECK_VALUE("b_u", std::to_string(core::u32(0xc2478000)));
    CHECK_VALUE("b_f", "-49.875000");
    CHECK_VALUE("c_i", std::to_string(core::i32(0xc4479000)));
    CHECK_VALUE("c_u", std::to_string(core::u32(0xc4479000)));
    CHECK_VALUE("c_f", "-798.250000");
    CHECK_VALUE("v_i", "vec2<i32>{1073741890, -1035501568}");
    CHECK_VALUE("v_u", "vec2<u32>{1073741890, 3259465728}");
    CHECK_VALUE("v_f", "vec2<f32>{2.000016, -49.875000}");
}

TEST_F(InvocationTest, AddressOf) {
    Init(R"(
struct S {
  a : i32,
  b : bool,
}

var<private> v4 : i32;
var<private> v5 : S;

@compute @workgroup_size(1)
fn main() {
  var v1 : i32;
  var v2 : vec4<f32>;
  var v3 : array<array<u32, 4>, 4>;
  let p1 = &v1;
  let p2 = &v2;
  let p3 = &(v3[2][1]);
  let p4 = &v4;
  let p5 = &(v5.b);
}
)");
    ContinueToEnd();
    CHECK_VALUE("p1", "ptr<function, i32>");
    CHECK_VALUE("p2", "ptr<function, vec4<f32>>");
    CHECK_VALUE("p3", "ptr<function, u32>");
    CHECK_VALUE("p4", "ptr<private, i32>");
    CHECK_VALUE("p5", "ptr<private, bool>");
}

TEST_F(InvocationTest, Deref) {
    Init(R"(
struct S {
  a : i32,
  b : bool,
}

var<private> v3 = S(10, false);

fn foo(p1 : ptr<function, i32>, p2 : ptr<function, i32>, p3 : ptr<private, S>) -> i32 {
  return *p1 + *p2 + (*p3).a;
}

@compute @workgroup_size(1)
fn main() {
  var v1 : i32 = 7;
  var v2 = S(42, true);
  let result = foo(&v1, &(v2.a), &v3);
}
)");
    ContinueToEnd();
    CHECK_VALUE("result", "59");
}

TEST_F(InvocationTest, PointerMemberAccess_ImplicitDeref) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
}

fn foo(p : ptr<function, S>) -> i32 {
  return p.a + p.b;
}

@compute @workgroup_size(1)
fn main() {
  var v = S(42, 1);
  let result = foo(&v);
}
)");
    ContinueToEnd();
    CHECK_VALUE("result", "43");
}

TEST_F(InvocationTest, PointerSwizzle_ImplicitDeref) {
    Init(R"(
fn foo(p : ptr<function, vec2i>) -> i32 {
  return p.x + p.y;
}

@compute @workgroup_size(1)
fn main() {
  var v = vec2i(42, 1);
  let result = foo(&v);
}
)");
    ContinueToEnd();
    CHECK_VALUE("result", "43");
}

TEST_F(InvocationTest, PointerArrayAccessor_ImplicitDeref) {
    Init(R"(
fn foo(p : ptr<function, array<i32, 2>>) -> i32 {
  return p[0] + p[1];
}

@compute @workgroup_size(1)
fn main() {
  var v = array(42i, 1i);
  let result = foo(&v);
}
)");
    ContinueToEnd();
    CHECK_VALUE("result", "43");
}

TEST_F(InvocationTest, Assign_EvaluationOrder) {
    Init(R"(
var<private> v = 0;

fn foo() -> i32 {
  v = v + 1;
  return v;
}

@compute @workgroup_size(1)
fn main() {
  var x = array<i32, 4>(1, 2, 3, 4);
  x[foo()] = x[foo()];
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "2");
    CHECK_VALUE("x", R"(array<i32, 4>{
  [0] = 1,
  [1] = 3,
  [2] = 3,
  [3] = 4,
})");
}

TEST_F(InvocationTest, PhonyAssignment) {
    Init(R"(
var<private> v = 42;

fn bar() -> i32 {
  return v;
}

fn foo() -> i32 {
  v = bar() + 1;
  return 0;
}

@compute @workgroup_size(1)
fn main() {
  _ = foo();
  let result = v + 1;
}
)");
    ContinueToEnd();
    CHECK_VALUE("result", "44");
}

TEST_F(InvocationTest, Increment_Basic) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var x = 7;
  x++;
  x++;
}
)");
    StepStmt();
    CHECK_VALUE("x", "7");
    StepStmt();
    CHECK_VALUE("x", "8");
    StepStmt();
    CHECK_VALUE("x", "9");
}

TEST_F(InvocationTest, Decrement_Basic) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var x = 7;
  x--;
  x--;
}
)");
    StepStmt();
    CHECK_VALUE("x", "7");
    StepStmt();
    CHECK_VALUE("x", "6");
    StepStmt();
    CHECK_VALUE("x", "5");
}

TEST_F(InvocationTest, Increment_ComplexLHS) {
    Init(R"(
var<private> v = 0;

fn foo() -> i32 {
  v = v + 1;
  return v;
}

@compute @workgroup_size(1)
fn main() {
  var x = array<i32, 4>();
  x[foo()]++;
  x[foo()]++;
}
)");
    ContinueTo<ast::IncrementDecrementStatement>();
    CHECK_VALUE("x", R"(array<i32, 4>{
  [0] = 0,
  [1] = 0,
  [2] = 0,
  [3] = 0,
})");
    ContinueTo<ast::IncrementDecrementStatement>();
    CHECK_VALUE("v", "1");
    CHECK_VALUE("x", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 0,
  [3] = 0,
})");
    ContinueToEnd();
    CHECK_VALUE("v", "2");
    CHECK_VALUE("x", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 1,
  [3] = 0,
})");
}

TEST_F(InvocationTest, CompoundAssign_Basic) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var x = 7;
  x += 1;
  x *= 2;
  x >>= 2u;
}
)");
    StepStmt();
    CHECK_VALUE("x", "7");
    StepStmt();
    CHECK_VALUE("x", "8");
    StepStmt();
    CHECK_VALUE("x", "16");
    StepStmt();
    CHECK_VALUE("x", "4");
}

TEST_F(InvocationTest, CompoundAssign_ComplexLHS) {
    Init(R"(
var<private> v = 0;

fn foo() -> i32 {
  v = v + 1;
  return v;
}

@compute @workgroup_size(1)
fn main() {
  var x = array<i32, 4>();
  x[foo()] += 1;
  x[foo()] += 1;
}
)");
    ContinueTo<ast::CompoundAssignmentStatement>();
    CHECK_VALUE("x", R"(array<i32, 4>{
  [0] = 0,
  [1] = 0,
  [2] = 0,
  [3] = 0,
})");
    ContinueTo<ast::CompoundAssignmentStatement>();
    CHECK_VALUE("v", "1");
    CHECK_VALUE("x", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 0,
  [3] = 0,
})");
    ContinueToEnd();
    CHECK_VALUE("v", "2");
    CHECK_VALUE("x", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 1,
  [3] = 0,
})");
}

TEST_F(InvocationTest, CompoundAssign_EvaluationOrder) {
    Init(R"(
var<private> v = 0;

fn foo() -> i32 {
  v = v + 1;
  return v;
}

@compute @workgroup_size(1)
fn main() {
  var x = array<i32, 4>(1, 2, 3, 4);
  x[foo()] -= x[foo()];
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "2");
    CHECK_VALUE("x", R"(array<i32, 4>{
  [0] = 1,
  [1] = -1,
  [2] = 3,
  [3] = 4,
})");
}

TEST_F(InvocationTest, CallUserFunction) {
    Init(R"(
var<private> v : i32;

fn foo() {
  v = 42;
}

@compute @workgroup_size(1)
fn main() {
  foo();
}
)");
    CHECK_VALUE("v", "0");
    ContinueToEnd();
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, CallUserFunction_ExplicitReturn) {
    Init(R"(
var<private> v : i32;

fn foo() {
  v = 42;
  return;
}

@compute @workgroup_size(1)
fn main() {
  foo();
}
)");
    CHECK_VALUE("v", "0");
    ContinueToEnd();
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, CallUserFunction_Param) {
    Init(R"(
var<private> v : i32;

fn foo(param : i32) {
  v = param;
}

@compute @workgroup_size(1)
fn main() {
  foo(42);
}
)");
    CHECK_VALUE("v", "0");
    ContinueToEnd();
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, CallUserFunction_ReturnValue) {
    Init(R"(
var<private> v : i32;

fn foo() -> i32 {
  return v;
}

@compute @workgroup_size(1)
fn main() {
  v = 42;
  let result = foo();
}
)");
    CHECK_VALUE("v", "0");
    ContinueToEnd();
    CHECK_VALUE("v", "42");
    CHECK_VALUE("result", "42");
}

TEST_F(InvocationTest, CallUserFunction_NestedInExpressionTree) {
    Init(R"(
var<private> v1 : i32 = 42;
var<private> v2 : i32 = 10;

fn foo(param : i32) -> i32 {
  v2 = -7;
  return param;
}

@compute @workgroup_size(1)
fn main() {
  let result = v1 + (v2 + foo(3)) + v2;
}
)");
    CHECK_VALUE("v1", "42");
    CHECK_VALUE("v2", "10");
    ContinueToEnd();
    CHECK_VALUE("v1", "42");
    CHECK_VALUE("v2", "-7");
    CHECK_VALUE("result", "48");
}

TEST_F(InvocationTest, CallUserFunction_MultipleTimes) {
    Init(R"(
var<private> v : i32;

fn foo(param : i32) {
  let local = param;
  v = local;
}

@compute @workgroup_size(1)
fn main() {
  foo(42);
  foo(43);
  foo(44);
}
)");
    ContinueTo<ast::AssignmentStatement>();
    StepStmt();
    CHECK_VALUE("local", "42");
    CHECK_VALUE("v", "42");
    ContinueTo<ast::AssignmentStatement>();
    StepStmt();
    CHECK_VALUE("local", "43");
    CHECK_VALUE("v", "43");
    ContinueTo<ast::AssignmentStatement>();
    StepStmt();
    CHECK_VALUE("local", "44");
    CHECK_VALUE("v", "44");
}

TEST_F(InvocationTest, CallBuiltinFunction) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var a = -42;
  var b = 90.0;
  var c = 64.0;
  a = abs(a);
  b = sin(radians(b));
  c = sqrt(c);
  var d = select(pow(b, 2), pow(c, 2), a > 0);
}
)");
    ContinueToEnd();
    CHECK_VALUE("a", "42");
    CHECK_VALUE("b", "1.000000");
    CHECK_VALUE("c", "8.000000");
    CHECK_VALUE("d", "64.000000");
}

TEST_F(InvocationTest, If_True) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v < 10) {
    v = 41;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, If_False) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v > 10) {
    v = 41;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "8");
}

TEST_F(InvocationTest, If_True_Else) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v < 10) {
    v = 41;
  } else {
    v = 10;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, If_False_Else) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v > 10) {
    v = 41;
  } else {
    v = 10;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "11");
}

TEST_F(InvocationTest, If_True_ElseIf_True) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v < 10) {
    v = 41;
  } else if (v == 7) {
    v = 10;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, If_False_ElseIf_True) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v > 10) {
    v = 41;
  } else if (v == 7) {
    v = 10;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "11");
}

TEST_F(InvocationTest, If_False_ElseIf_False) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v > 10) {
    v = 41;
  } else if (v == 5) {
    v = 10;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "8");
}

TEST_F(InvocationTest, If_False_ChainOfElseIf_TrueInMiddle) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v > 10) {
    v = 41;
  } else if (v == 4) {
    v = 10;
  } else if (v == 5) {
    v = 10;
  } else if (v == 6) {
    v = 10;
  } else if (v == 7) {
    v = 20;
  } else if (v == 8) {
    v = 10;
  } else if (v == 9) {
    v = 10;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "21");
}

TEST_F(InvocationTest, If_False_ChainOfElseIf_AllFalse) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v > 10) {
    v = 41;
  } else if (v == 4) {
    v = 10;
  } else if (v == 5) {
    v = 10;
  } else if (v == 6) {
    v = 10;
  } else if (v == 8) {
    v = 10;
  } else if (v == 9) {
    v = 10;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "8");
}

TEST_F(InvocationTest, If_False_ChainOfElseIf_AllFalse_Else) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 7;
  if (v > 10) {
    v = 41;
  } else if (v == 4) {
    v = 10;
  } else if (v == 5) {
    v = 10;
  } else if (v == 6) {
    v = 10;
  } else if (v == 8) {
    v = 10;
  } else if (v == 9) {
    v = 10;
  } else {
    v = 20;
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "21");
}

TEST_F(InvocationTest, ForLoop_Basic) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  for (var i = 0; i < 4; i++) {
    arr[i] = i;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, ForLoop_EmptyInitializer) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  var i = 0;
  for (; i < 4; i++) {
    arr[i] = i;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, ForLoop_EmptyCondition) {
    Init(R"(
var<private> arr : array<i32, 4>;

fn foo() {
  for (var i = 0; ; i++) {
    if (i >= 4) {
      break;
    }
    arr[i] = i + 1;
  }
}

@compute @workgroup_size(1)
fn main() {
  foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
})");
}

TEST_F(InvocationTest, ForLoop_EmptyContinuing) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  for (var i = 0; i < 4;) {
    arr[i] = i;
    i++;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, ForLoop_EmptyEverything) {
    Init(R"(
var<private> arr : array<i32, 4>;

@compute @workgroup_size(1)
fn main() {
  var i = 0;
  for (;;) {
    arr[i] = i;
    i++;
    if (i == 4) {
      break;
    }
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

// TODO(jrprice): Need to execute initializer in its own scope.
TEST_F(InvocationTest, DISABLED_ForLoop_InitializerScope) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var i : f32 = 42.5;
  for (var i = 0; i < 4; i++) {
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("i", "42.5");
}

TEST_F(InvocationTest, ForLoop_ConditionScope) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var end = 4;
  var arr : array<i32, 4>;
  for (var i = 0; i < end; i++) {
    arr[i] = i;
    var end = 1;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, ForLoop_ContinuingScope) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var inc = 1;
  var arr : array<i32, 4>;
  for (var i = 0; i < 4; i = i + inc) {
    arr[i] = i;
    var inc = 2;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, ForLoop_BreakFromNestedBlock) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr = array<i32, 4>(7, 7, 7, 7);
  var i = 0;
  for (; i < 4; i++) {
    arr[i] = i;
    {
      if (i == 2) {
        {
          if (true) {
            break;
          }
        }
        i = 55;
      }
    }
  }
  arr[0] = i;
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 2,
  [1] = 1,
  [2] = 2,
  [3] = 7,
})");
}

TEST_F(InvocationTest, ForLoop_BreakFromNestedLoop) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr = array<i32, 4>(7, 7, 7, 7);
  for (var i = 0; i < 4;) {
    arr[i] = i;
    var inc = 0;
    for (var j = 0; j < 10; j++) {
      if (j > 1) {
        break;
      }
      inc += j;
    }
    i += inc;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, ForLoop_ContinueFromNestedBlock) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr = array<i32, 4>(7, 7, 7, 7);
  for (var i = 0; i < 4; i++) {
    {
      if (i == 2) {
        {
          if (true) {
            continue;
          }
        }
        i = 55;
      }
    }
    arr[i] = i;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 7,
  [3] = 3,
})");
}

TEST_F(InvocationTest, ForLoop_ContinueFromNestedLoop) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr = array<i32, 4>(7, 7, 7, 7);
  for (var i = 0; i < 4;) {
    arr[i] = i;
    var inc = 0;
    for (var j = 0; j < 10; j++) {
      if (j > 1) {
        continue;
      }
      inc += j;
    }
    i += inc;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, Loop_NoContinuing) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  var i = 0;
  loop {
    if (i == 4) {
      break;
    }
    arr[i] = i;
    i++;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, Loop_WithContinuing) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  var i = 0;
  loop {
    if (i == 4) {
      break;
    }
    arr[i] = i;
    continuing {
      i++;
    }
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, Loop_BreakIf) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  var i = 0;
  loop {
    arr[i] = i;
    continuing {
      i++;
      break if i == 4;
    }
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, Loop_Continue_NoContinuing) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  var i = 0;
  loop {
    if (i == 4) {
      break;
    }
    arr[i] = i;
    i++;
    if (true) {
      continue;
    }
    i = 55;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, Loop_Continue_WithContinuing) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  var i = 0;
  loop {
    arr[i] = i;
    if (true) {
      continue;
    }
    i = 55;
    continuing {
      i++;
      break if i == 4;
    }
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, WhileLoop_Basic) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 4>;
  var i = 0;
  while (i < 4) {
    arr[i] = i;
    i++;
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 4>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
})");
}

TEST_F(InvocationTest, Switch_Basic) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 0;
  var condition = 0;
  switch (condition) {
    case 0 {
      v = 42;
    }
    default {
      v = 99;
    }
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "43");
}

TEST_F(InvocationTest, Switch_Basic_Default) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var v = 0;
  var condition = 1;
  switch (condition) {
    case 0 {
      v = 42;
    }
    default {
      v = 99;
    }
  }
  v++;
}
)");
    ContinueToEnd();
    CHECK_VALUE("v", "100");
}

TEST_F(InvocationTest, Switch_Complex) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var arr : array<i32, 8>;
  for (var i = 0; i < 8; i++) {
    switch (i) {
      case 0, 1, 6, 7 {
        arr[i] = 42;
      }
      case 2, default {
        arr[i] = 99;
      }
      case 4, 5 {
        if (i == 4) {
          arr[i] = 4;
          break;
        }
        arr[i] = 5;
      }
    }
  }
}
)");
    ContinueToEnd();
    CHECK_VALUE("arr", R"(array<i32, 8>{
  [0] = 42,
  [1] = 42,
  [2] = 99,
  [3] = 99,
  [4] = 4,
  [5] = 5,
  [6] = 42,
  [7] = 42,
})");
}

TEST_F(InvocationTest, LogicalAnd_SkipRHS) {
    Init(R"(
var<private> v = 42;

fn foo() -> bool {
  v = -1;
  return true;
}

@compute @workgroup_size(1)
fn main() {
  var x = 1;
  var y = (x > 1) && foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "false");
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, LogicalAnd_EvalRHS) {
    Init(R"(
var<private> v = 42;

fn foo() -> bool {
  v = -1;
  return true;
}

@compute @workgroup_size(1)
fn main() {
  var x = 1;
  var y = (x < 2) && foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "true");
    CHECK_VALUE("v", "-1");
}

TEST_F(InvocationTest, LogicalAnd_ConstantLHS) {
    Init(R"(
var<private> v = 42;

fn foo() -> bool {
  v = -1;
  return true;
}

@compute @workgroup_size(1)
fn main() {
  var y = false && foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "false");
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, LogicalAnd_FuncLHS) {
    Init(R"(
var<private> v = 42;

fn foo() -> bool {
  v = -1;
  return true;
}

fn False() -> bool {
  return false;
}

@compute @workgroup_size(1)
fn main() {
  var y = False() && foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "false");
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, LogicalOr_SkipRHS) {
    Init(R"(
var<private> v = 42;

fn foo() -> bool {
  v = -1;
  return false;
}

@compute @workgroup_size(1)
fn main() {
  var x = 1;
  var y = (x < 2) || foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "true");
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, LogicalOr_EvalRHS) {
    Init(R"(
var<private> v = 42;

fn foo() -> bool {
  v = -1;
  return false;
}

@compute @workgroup_size(1)
fn main() {
  var x = 1;
  var y = (x > 1) || foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "false");
    CHECK_VALUE("v", "-1");
}

TEST_F(InvocationTest, LogicalOr_ConstantLHS) {
    Init(R"(
var<private> v = 42;

fn foo() -> bool {
  v = -1;
  return false;
}

@compute @workgroup_size(1)
fn main() {
  var y = true || foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "true");
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, LogicalOr_FuncLHS) {
    Init(R"(
var<private> v = 42;

fn foo() -> bool {
  v = -1;
  return false;
}

fn True() -> bool {
  return true;
}

@compute @workgroup_size(1)
fn main() {
  var y = True() || foo();
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "true");
    CHECK_VALUE("v", "42");
}

TEST_F(InvocationTest, Logical_Nested) {
    Init(R"(
var<private> foo_count = 0;
var<private> bar_count = 0;

fn foo() -> i32 {
  foo_count++;
  return 1;
}

fn bar() -> i32 {
  bar_count++;
  return 1;
}

@compute @workgroup_size(1)
fn main() {
  var x = 2;
  var y = ((x > foo()) && (x < bar() || (x <= foo()))) && ((x == foo()) || (x != bar()));
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "false");
    CHECK_VALUE("foo_count", "2");
    CHECK_VALUE("bar_count", "1");
}

TEST_F(InvocationTest, Logical_Unevaluated) {
    // Test to ensure that we clear that map of short-circuiting operators in case one never gets
    // evaluated and causes problems for a subsequent statement.
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var x = 1;
  var y = (x > 1) && (x < 3);
  var z = x + x + x + x + x + x + x + x + x + x;
}
)");
    ContinueToEnd();
    CHECK_VALUE("y", "false");
    CHECK_VALUE("z", "10");
}

TEST_F(InvocationTest, Memory_LoadThroughView) {
    Init("@compute @workgroup_size(1) fn main() {}");

    Memory alloc(8);

    int32_t value1 = 42;
    int32_t value2 = -1007361;
    alloc.Store(&value1, 0);
    alloc.Store(&value2, 4);

    auto* i32 = executor_->Builder().create<core::type::I32>();
    auto* view1 = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, i32, 0, 4, {});
    auto* view2 = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, i32, 4, 4, {});
    EXPECT_EQ(view1->Load()->ValueAs<int32_t>(), value1);
    EXPECT_EQ(view2->Load()->ValueAs<int32_t>(), value2);
}

TEST_F(InvocationTest, Memory_StoreThroughView) {
    Init("@compute @workgroup_size(1) fn main() {}");

    Memory alloc(8);

    auto* i32 = executor_->Builder().create<core::type::I32>();
    auto* value1 = executor_->Builder().constants.Get(core::i32(42));
    auto* value2 = executor_->Builder().constants.Get(core::i32(-1007361));
    auto* view1 = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, i32, 0, 4, {});
    auto* view2 = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, i32, 4, 4, {});
    view1->Store(value1);
    view2->Store(value2);

    int32_t load1 = 0;
    int32_t load2 = 0;
    alloc.Load(&load1, 0);
    alloc.Load(&load2, 4);
    EXPECT_EQ(load1, value1->ValueAs<int32_t>());
    EXPECT_EQ(load2, value2->ValueAs<int32_t>());
}

TEST_F(InvocationTest, Memory_ArrayStoreThroughView) {
    Init("@compute @workgroup_size(1) fn main() {}");

    Memory alloc(16);

    auto* i32_ty = executor_->Builder().create<core::type::I32>();
    core::type::ConstantArrayCount count(4);
    auto* arr_ty = executor_->Builder().create<core::type::Array>(i32_ty, &count, 4u, 16u, 4u, 4u);
    tint::Vector<const core::constant::Value*, 4> elements = {
        executor_->Builder().constants.Get(core::i32(42)),
        executor_->Builder().constants.Get(core::i32(-1007361)),
        executor_->Builder().constants.Get(core::i32(20222022)),
        executor_->Builder().constants.Get(core::i32(-1)),
    };
    auto* view = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, arr_ty, {});
    view->Store(executor_->ConstEval().ArrayOrStructCtor(arr_ty, elements).Get());

    int32_t values[4] = {};
    alloc.Load(values, 0, 16);
    EXPECT_EQ(values[0], elements[0]->ValueAs<int32_t>());
    EXPECT_EQ(values[1], elements[1]->ValueAs<int32_t>());
    EXPECT_EQ(values[2], elements[2]->ValueAs<int32_t>());
    EXPECT_EQ(values[3], elements[3]->ValueAs<int32_t>());
}

TEST_F(InvocationTest, Memory_ArrayStoreThroughView_Strided) {
    Init("@compute @workgroup_size(1) fn main() {}");

    Memory alloc(32);

    auto* i32_ty = executor_->Builder().create<core::type::I32>();
    core::type::ConstantArrayCount count(4);
    auto* arr_ty = executor_->Builder().create<core::type::Array>(i32_ty, &count, 4u, 32u, 8u, 4u);
    tint::Vector<const core::constant::Value*, 4> elements = {
        executor_->Builder().constants.Get(core::i32(42)),
        executor_->Builder().constants.Get(core::i32(-1007361)),
        executor_->Builder().constants.Get(core::i32(20222022)),
        executor_->Builder().constants.Get(core::i32(-1)),
    };
    auto* view = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, arr_ty, {});
    view->Store(executor_->ConstEval().ArrayOrStructCtor(arr_ty, elements).Get());

    int32_t values[8] = {};
    alloc.Load(values, 0, 32);
    EXPECT_EQ(values[0], elements[0]->ValueAs<int32_t>());
    EXPECT_EQ(values[2], elements[1]->ValueAs<int32_t>());
    EXPECT_EQ(values[4], elements[2]->ValueAs<int32_t>());
    EXPECT_EQ(values[6], elements[3]->ValueAs<int32_t>());
}

TEST_F(InvocationTest, Memory_ArrayLoadThroughView) {
    Init("@compute @workgroup_size(1) fn main() {}");

    Memory alloc(16);

    int32_t values[4] = {42, -1007361, 20222022, -1};
    alloc.Store(values, 0, 16);

    auto* i32_ty = executor_->Builder().create<core::type::I32>();
    core::type::ConstantArrayCount count(4);
    auto* arr_ty = executor_->Builder().create<core::type::Array>(i32_ty, &count, 4u, 16u, 4u, 4u);
    auto* view = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, arr_ty, {});
    auto* result = view->Load();

    EXPECT_EQ(values[0], result->Index(0)->ValueAs<int32_t>());
    EXPECT_EQ(values[1], result->Index(1)->ValueAs<int32_t>());
    EXPECT_EQ(values[2], result->Index(2)->ValueAs<int32_t>());
    EXPECT_EQ(values[3], result->Index(3)->ValueAs<int32_t>());
}

TEST_F(InvocationTest, Memory_ArrayLoadThroughView_Strided) {
    Init("@compute @workgroup_size(1) fn main() {}");

    Memory alloc(32);

    int32_t values[8] = {42, 0, -1007361, 0, 20222022, 0, -1, 0};
    alloc.Store(values, 0, 32);

    auto* i32_ty = executor_->Builder().create<core::type::I32>();
    core::type::ConstantArrayCount count(4);
    auto* arr_ty = executor_->Builder().create<core::type::Array>(i32_ty, &count, 4u, 32u, 8u, 4u);
    auto* view = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, arr_ty, {});
    auto* result = view->Load();

    EXPECT_EQ(values[0], result->Index(0)->ValueAs<int32_t>());
    EXPECT_EQ(values[2], result->Index(1)->ValueAs<int32_t>());
    EXPECT_EQ(values[4], result->Index(2)->ValueAs<int32_t>());
    EXPECT_EQ(values[6], result->Index(3)->ValueAs<int32_t>());
}

TEST_F(InvocationTest, Memory_MatrixLoadThroughView) {
    Init("@compute @workgroup_size(1) fn main() {}");

    Memory alloc(48);

    float values[] = {
        1, 2, 3, 0,  //
        4, 5, 6, 0,  //
        7, 8, 9, 0,  //
    };
    alloc.Store(values, 0, 48);

    auto* mat3x3 = executor_->Builder().create<core::type::Matrix>(
        executor_->Builder().create<core::type::Vector>(
            executor_->Builder().create<core::type::F32>(), 3u),
        3u);
    auto* view = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, mat3x3, 0, 48, {});
    auto* matrix = view->Load();
    for (uint32_t j = 0; j < 3; j++) {
        for (uint32_t i = 0; i < 3; i++) {
            EXPECT_EQ(matrix->Index(j)->Index(i)->ValueAs<float>(), values[j * 4 + i]);
        }
    }
}

TEST_F(InvocationTest, Memory_MatrixStoreThroughView) {
    Init("@compute @workgroup_size(1) fn main() {}");

    Memory alloc(48);

    auto* f32_ty = executor_->Builder().create<core::type::F32>();
    auto* vec3 = executor_->Builder().create<core::type::Vector>(f32_ty, 3u);
    auto* mat3x3 = executor_->Builder().create<core::type::Matrix>(vec3, 3u);
    tint::Vector<const core::constant::Value*, 4> columns;
    for (uint32_t j = 0; j < 3; j++) {
        tint::Vector<const core::constant::Value*, 4> values;
        for (uint32_t i = 0; i < 3; i++) {
            values.Push(executor_->Builder().constants.Get(core::f32(j * 3 + i)));
        }
        columns.Push(executor_->Builder().constants.Composite(vec3, values));
    }
    auto* matrix = executor_->Builder().constants.Composite(mat3x3, columns);

    auto* view = alloc.CreateView(executor_.get(), core::AddressSpace::kPrivate, mat3x3, 0, 48, {});
    view->Store(matrix);

    float loaded[12] = {};
    alloc.Load(loaded, 0, 48);
    for (uint32_t j = 0; j < 3; j++) {
        for (uint32_t i = 0; i < 3; i++) {
            EXPECT_EQ(loaded[j * 4 + i], j * 3 + i);
        }
    }
}

}  // namespace
}  // namespace tint::interp
