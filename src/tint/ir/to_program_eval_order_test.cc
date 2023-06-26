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

#include <string>

#include "src/tint/ir/disassembler.h"
#include "src/tint/ir/to_program.h"
#include "src/tint/ir/to_program_test.h"
#include "src/tint/utils/string.h"
#include "src/tint/writer/wgsl/generator.h"

namespace tint::ir::test {
namespace {

using namespace tint::number_suffixes;        // NOLINT
using namespace tint::builtin::fluent_types;  // NOLINT

using IRToProgramEvalOrderTest = IRToProgramTest;

TEST_F(IRToProgramEvalOrderTest, BinaryOpUnsequencedLHSThenUnsequencedRHS) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] {
        auto* lhs = b.Add(ty.i32(), 1_i, 2_i);
        auto* rhs = b.Add(ty.i32(), 3_i, 4_i);
        auto* bin = b.Add(ty.i32(), lhs, rhs);
        b.Return(fn_b, bin);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b() -> i32 {
  return ((1i + 2i) + (3i + 4i));
}
)");
}

TEST_F(IRToProgramEvalOrderTest, BinaryOpSequencedLHSThenUnsequencedRHS) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] {
        auto* lhs = b.Call(ty.i32(), fn_a, 1_i);
        auto* rhs = b.Add(ty.i32(), 2_i, 3_i);
        auto* bin = b.Add(ty.i32(), lhs, rhs);
        b.Return(fn_b, bin);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b() -> i32 {
  return (a(1i) + (2i + 3i));
}
)");
}

TEST_F(IRToProgramEvalOrderTest, BinaryOpUnsequencedLHSThenSequencedRHS) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] {
        auto* lhs = b.Add(ty.i32(), 1_i, 2_i);
        auto* rhs = b.Call(ty.i32(), fn_a, 3_i);
        auto* bin = b.Add(ty.i32(), lhs, rhs);
        b.Return(fn_b, bin);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b() -> i32 {
  return ((1i + 2i) + a(3i));
}
)");
}

TEST_F(IRToProgramEvalOrderTest, BinaryOpSequencedLHSThenSequencedRHS) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] {
        auto* lhs = b.Call(ty.i32(), fn_a, 1_i);
        auto* rhs = b.Call(ty.i32(), fn_a, 2_i);
        auto* bin = b.Add(ty.i32(), lhs, rhs);
        b.Return(fn_b, bin);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b() -> i32 {
  return (a(1i) + a(2i));
}
)");
}

TEST_F(IRToProgramEvalOrderTest, BinaryOpUnsequencedRHSThenUnsequencedLHS) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] {
        auto* rhs = b.Add(ty.i32(), 3_i, 4_i);
        auto* lhs = b.Add(ty.i32(), 1_i, 2_i);
        auto* bin = b.Add(ty.i32(), lhs, rhs);
        b.Return(fn_b, bin);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b() -> i32 {
  return ((1i + 2i) + (3i + 4i));
}
)");
}

TEST_F(IRToProgramEvalOrderTest, BinaryOpSequencedRHSThenUnsequencedLHS) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] {
        auto* rhs = b.Add(ty.i32(), 2_i, 3_i);
        auto* lhs = b.Call(ty.i32(), fn_a, 1_i);
        auto* bin = b.Add(ty.i32(), lhs, rhs);
        b.Return(fn_b, bin);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b() -> i32 {
  return (a(1i) + (2i + 3i));
}
)");
}

TEST_F(IRToProgramEvalOrderTest, BinaryOpUnsequencedRHSThenSequencedLHS) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] {
        auto* rhs = b.Call(ty.i32(), fn_a, 3_i);
        auto* lhs = b.Add(ty.i32(), 1_i, 2_i);
        auto* bin = b.Add(ty.i32(), lhs, rhs);
        b.Return(fn_b, bin);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b() -> i32 {
  return ((1i + 2i) + a(3i));
}
)");
}

TEST_F(IRToProgramEvalOrderTest, BinaryOpSequencedRHSThenSequencedLHS) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] {
        auto* rhs = b.Call(ty.i32(), fn_a, 2_i);
        auto* lhs = b.Call(ty.i32(), fn_a, 1_i);
        auto* bin = b.Add(ty.i32(), lhs, rhs);
        b.Return(fn_b, bin);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b() -> i32 {
  let v_1 = a(2i);
  return (a(1i) + v_1);
}
)");
}

TEST_F(IRToProgramEvalOrderTest, CallSequencedXYZ) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] { b.Return(fn_b, 0_i); });
    fn_b->SetParams(
        {b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32())});

    auto* fn_c = b.Function("c", ty.i32());
    b.With(fn_c->Block(), [&] {
        auto* x = b.Call(ty.i32(), fn_a, 1_i);
        auto* y = b.Call(ty.i32(), fn_a, 2_i);
        auto* z = b.Call(ty.i32(), fn_a, 3_i);
        auto* call = b.Call(ty.i32(), fn_b, x, y, z);
        b.Return(fn_c, call);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);
    mod.functions.Push(fn_c);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b(v_1 : i32, v_2 : i32, v_3 : i32) -> i32 {
  return 0i;
}

fn c() -> i32 {
  return b(a(1i), a(2i), a(3i));
}
)");
}


TEST_F(IRToProgramEvalOrderTest, CallSequencedYXZ) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] { b.Return(fn_b, 0_i); });
    fn_b->SetParams(
        {b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32())});

    auto* fn_c = b.Function("c", ty.i32());
    b.With(fn_c->Block(), [&] {
        auto* y = b.Call(ty.i32(), fn_a, 2_i);
        auto* x = b.Call(ty.i32(), fn_a, 1_i);
        auto* z = b.Call(ty.i32(), fn_a, 3_i);
        auto* call = b.Call(ty.i32(), fn_b, x, y, z);
        b.Return(fn_c, call);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);
    mod.functions.Push(fn_c);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b(v_1 : i32, v_2 : i32, v_3 : i32) -> i32 {
  return 0i;
}

fn c() -> i32 {
  let v_4 = a(2i);
  return b(a(1i), v_4, a(3i));
}
)");
}


TEST_F(IRToProgramEvalOrderTest, CallSequencedXZY) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] { b.Return(fn_b, 0_i); });
    fn_b->SetParams(
        {b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32())});

    auto* fn_c = b.Function("c", ty.i32());
    b.With(fn_c->Block(), [&] {
        auto* x = b.Call(ty.i32(), fn_a, 1_i);
        auto* z = b.Call(ty.i32(), fn_a, 3_i);
        auto* y = b.Call(ty.i32(), fn_a, 2_i);
        auto* call = b.Call(ty.i32(), fn_b, x, y, z);
        b.Return(fn_c, call);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);
    mod.functions.Push(fn_c);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b(v_1 : i32, v_2 : i32, v_3 : i32) -> i32 {
  return 0i;
}

fn c() -> i32 {
  let v_4 = a(1i);
  let v_5 = a(3i);
  return b(v_4, a(2i), v_5);
}
)");
}


TEST_F(IRToProgramEvalOrderTest, CallSequencedZXY) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] { b.Return(fn_b, 0_i); });
    fn_b->SetParams(
        {b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32())});

    auto* fn_c = b.Function("c", ty.i32());
    b.With(fn_c->Block(), [&] {
        auto* z = b.Call(ty.i32(), fn_a, 3_i);
        auto* x = b.Call(ty.i32(), fn_a, 1_i);
        auto* y = b.Call(ty.i32(), fn_a, 2_i);
        auto* call = b.Call(ty.i32(), fn_b, x, y, z);
        b.Return(fn_c, call);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);
    mod.functions.Push(fn_c);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b(v_1 : i32, v_2 : i32, v_3 : i32) -> i32 {
  return 0i;
}

fn c() -> i32 {
  let v_4 = a(3i);
  return b(a(1i), a(2i), v_4);
}
)");
}


TEST_F(IRToProgramEvalOrderTest, CallSequencedYZX) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] { b.Return(fn_b, 0_i); });
    fn_b->SetParams(
        {b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32())});

    auto* fn_c = b.Function("c", ty.i32());
    b.With(fn_c->Block(), [&] {
        auto* y = b.Call(ty.i32(), fn_a, 2_i);
        auto* z = b.Call(ty.i32(), fn_a, 3_i);
        auto* x = b.Call(ty.i32(), fn_a, 1_i);
        auto* call = b.Call(ty.i32(), fn_b, x, y, z);
        b.Return(fn_c, call);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);
    mod.functions.Push(fn_c);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b(v_1 : i32, v_2 : i32, v_3 : i32) -> i32 {
  return 0i;
}

fn c() -> i32 {
  let v_4 = a(2i);
  let v_5 = a(3i);
  return b(a(1i), v_4, v_5);
}
)");
}


TEST_F(IRToProgramEvalOrderTest, CallSequencedZYX) {
    auto* fn_a = b.Function("a", ty.i32());
    b.With(fn_a->Block(), [&] { b.Return(fn_a, 0_i); });
    fn_a->SetParams({b.FunctionParam(ty.i32())});

    auto* fn_b = b.Function("b", ty.i32());
    b.With(fn_b->Block(), [&] { b.Return(fn_b, 0_i); });
    fn_b->SetParams(
        {b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32()), b.FunctionParam(ty.i32())});

    auto* fn_c = b.Function("c", ty.i32());
    b.With(fn_c->Block(), [&] {
        auto* z = b.Call(ty.i32(), fn_a, 3_i);
        auto* y = b.Call(ty.i32(), fn_a, 2_i);
        auto* x = b.Call(ty.i32(), fn_a, 1_i);
        auto* call = b.Call(ty.i32(), fn_b, x, y, z);
        b.Return(fn_c, call);
    });

    mod.functions.Push(fn_a);
    mod.functions.Push(fn_b);
    mod.functions.Push(fn_c);

    EXPECT_WGSL(R"(
fn a(v : i32) -> i32 {
  return 0i;
}

fn b(v_1 : i32, v_2 : i32, v_3 : i32) -> i32 {
  return 0i;
}

fn c() -> i32 {
  let v_4 = a(3i);
  let v_5 = a(2i);
  return b(a(1i), v_5, v_4);
}
)");
}


}  // namespace
}  // namespace tint::ir::test
