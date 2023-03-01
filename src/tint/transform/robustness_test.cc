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

#include "src/tint/transform/robustness.h"

#include "src/tint/transform/test_helper.h"

namespace tint::transform {

static std::ostream& operator<<(std::ostream& out, Robustness::Action action) {
    switch (action) {
        case Robustness::Action::kIgnore:
            return out << "ignore";
        case Robustness::Action::kClamp:
            return out << "clamp";
        case Robustness::Action::kPredicate:
            return out << "predicate";
    }
    return out << "unknown";
}

namespace {

DataMap Config(Robustness::Action action) {
    Robustness::Config cfg;
    cfg.function_action = action;
    cfg.handle_action = action;
    cfg.private_action = action;
    cfg.push_constant_action = action;
    cfg.storage_action = action;
    cfg.uniform_action = action;
    cfg.workgroup_action = action;
    DataMap data;
    data.Add<Robustness::Config>(cfg);
    return data;
}

const char* Expect(Robustness::Action action,
                   const char* expect_ignore,
                   const char* expect_clamp,
                   const char* expect_predicate) {
    switch (action) {
        case Robustness::Action::kIgnore:
            return expect_ignore;
        case Robustness::Action::kClamp:
            return expect_clamp;
        case Robustness::Action::kPredicate:
            return expect_predicate;
    }
    return "<invalid action>";
}

using RobustnessTest = TransformTestWithParam<Robustness::Action>;

////////////////////////////////////////////////////////////////////////////////
// Constant sized array
////////////////////////////////////////////////////////////////////////////////

TEST_P(RobustnessTest, Read_ConstantSizedArrayVal_IndexWithLiteral) {
    auto* src = R"(
fn f() {
  var b : f32 = array<f32, 3>()[1i];
}
)";

    auto* expect = src;

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayVal_IndexWithConst) {
    auto* src = R"(
const c : u32 = 1u;

fn f() {
  let b : f32 = array<f32, 3>()[c];
}
)";

    auto* expect = src;

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayVal_IndexWithLet) {
    auto* src = R"(
fn f() {
  let l : u32 = 1u;
  let b : f32 = array<f32, 3>()[l];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
fn f() {
  let l : u32 = 1u;
  let b : f32 = array<f32, 3>()[min(l, 2u)];
}
)",
                          /* predicate */ R"(
fn f() {
  let l : u32 = 1u;
  let index = l;
  let predicate = (u32(index) <= 2u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = array<f32, 3>()[index];
  }
  let b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayVal_IndexWithRuntimeArrayIndex) {
    auto* src = R"(
var<private> i : u32;

fn f() {
  let a = array<f32, 3>();
  let b = array<i32, 5>();
  var c : f32 = a[b[i]];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> i : u32;

fn f() {
  let a = array<f32, 3>();
  let b = array<i32, 5>();
  var c : f32 = a[min(u32(b[min(i, 4u)]), 2u)];
}
)",
                          /* predicate */ R"(
var<private> i : u32;

fn f() {
  let a = array<f32, 3>();
  let b = array<i32, 5>();
  let index = i;
  let predicate = (u32(index) <= 4u);
  var predicated_load : i32;
  if (predicate) {
    predicated_load = b[index];
  }
  let index_1 = predicated_load;
  let predicate_1 = (u32(index_1) <= 2u);
  var predicated_load_1 : f32;
  if (predicate_1) {
    predicated_load_1 = a[index_1];
  }
  var c : f32 = predicated_load_1;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayVal_IndexWithRuntimeExpression) {
    auto* src = R"(
var<private> c : i32;

fn f() {
  var b : f32 = array<f32, 3>()[((c + 2) - 3)];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> c : i32;

fn f() {
  var b : f32 = array<f32, 3>()[min(u32(((c + 2) - 3)), 2u)];
}
)",
                          /* predicate */ R"(
var<private> c : i32;

fn f() {
  let index = ((c + 2) - 3);
  let predicate = (u32(index) <= 2u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = array<f32, 3>()[index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_NestedConstantSizedArraysVal_IndexWithRuntimeExpressions) {
    auto* src = R"(
var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let a = array<array<array<f32, 1>, 2>, 3>();
  var r = a[x][y][z];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let a = array<array<array<f32, 1>, 2>, 3>();
  var r = a[min(u32(x), 2u)][min(u32(y), 1u)][min(u32(z), 0u)];
}
)",
                          /* predicate */ R"(
var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let a = array<array<array<f32, 1>, 2>, 3>();
  let index = x;
  let predicate = (u32(index) <= 2u);
  var predicated_load : array<array<f32, 1u>, 2u>;
  if (predicate) {
    predicated_load = a[index];
  }
  let index_1 = y;
  let predicate_1 = (predicate & (u32(index_1) <= 1u));
  var predicated_load_1 : array<f32, 1u>;
  if (predicate_1) {
    predicated_load_1 = predicated_load[index_1];
  }
  let index_2 = z;
  let predicate_2 = (predicate_1 & (u32(index_2) <= 0u));
  var predicated_load_2 : f32;
  if (predicate_2) {
    predicated_load_2 = predicated_load_1[index_2];
  }
  var r = predicated_load_2;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayVal_IndexWithOverride) {
    auto* src = R"(
@id(1300) override idx : i32;

fn f() {
  let a = array<f32, 4>();
  var b : f32 = a[idx];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@id(1300) override idx : i32;

fn f() {
  let a = array<f32, 4>();
  var b : f32 = a[min(u32(idx), 3u)];
}
)",
                          /* predicate */ R"(
@id(1300) override idx : i32;

fn f() {
  let a = array<f32, 4>();
  let index = idx;
  let predicate = (u32(index) <= 3u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));
    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayRef_IndexWithLiteral) {
    auto* src = R"(
var<private> a : array<f32, 3>;

fn f() {
  var b : f32 = a[1i];
}
)";

    auto* expect = src;

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayRef_IndexWithConst) {
    auto* src = R"(
var<private> a : array<f32, 3>;

const c : u32 = 1u;

fn f() {
  let b : f32 = a[c];
}
)";

    auto* expect = src;

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayRef_IndexWithLet) {
    auto* src = R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let b : f32 = a[l];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let b : f32 = a[min(l, 2u)];
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let index = l;
  let predicate = (u32(index) <= 2u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[index];
  }
  let b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayRef_IndexWithRuntimeArrayIndex) {
    auto* src = R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  var c : f32 = a[b[i]];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  var c : f32 = a[min(u32(b[min(i, 4u)]), 2u)];
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  let index = i;
  let predicate = (u32(index) <= 4u);
  var predicated_load : i32;
  if (predicate) {
    predicated_load = b[index];
  }
  let index_1 = predicated_load;
  let predicate_1 = (u32(index_1) <= 2u);
  var predicated_load_1 : f32;
  if (predicate_1) {
    predicated_load_1 = a[index_1];
  }
  var c : f32 = predicated_load_1;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayRef_IndexWithRuntimeExpression) {
    auto* src = R"(
var<private> a : array<f32, 3>;

var<private> c : i32;

fn f() {
  var b : f32 = a[((c + 2) - 3)];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

var<private> c : i32;

fn f() {
  var b : f32 = a[min(u32(((c + 2) - 3)), 2u)];
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

var<private> c : i32;

fn f() {
  let index = ((c + 2) - 3);
  let predicate = (u32(index) <= 2u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_NestedConstantSizedArraysRef_IndexWithRuntimeExpressions) {
    auto* src = R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  var r = a[x][y][z];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  var r = a[min(u32(x), 2u)][min(u32(y), 1u)][min(u32(z), 0u)];
}
)",
                          /* predicate */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let index = x;
  let predicate = (u32(index) <= 2u);
  let index_1 = y;
  let predicate_1 = (predicate & (u32(index_1) <= 1u));
  let index_2 = z;
  let predicate_2 = (predicate_1 & (u32(index_2) <= 0u));
  var predicated_load : f32;
  if (predicate_2) {
    predicated_load = a[index][index_1][index_2];
  }
  var r = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayRef_IndexWithOverride) {
    auto* src = R"(
@id(1300) override idx : i32;

fn f() {
  var a : array<f32, 4>;
  var b : f32 = a[idx];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@id(1300) override idx : i32;

fn f() {
  var a : array<f32, 4>;
  var b : f32 = a[min(u32(idx), 3u)];
}
)",
                          /* predicate */ R"(
@id(1300) override idx : i32;

fn f() {
  var a : array<f32, 4>;
  let index = idx;
  let predicate = (u32(index) <= 3u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));
    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayPtr_IndexWithLet) {
    auto* src = R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let p = &(a[l]);
  let f : f32 = *(p);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let p = &(a[min(l, 2u)]);
  let f : f32 = *(p);
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let index = l;
  let predicate = (u32(index) <= 2u);
  let p = &(a[index]);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = *(p);
  }
  let f : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArrayPtr_IndexWithRuntimeArrayIndex) {
    auto* src = R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  let pa = &(a);
  let pb = &(b);
  let p0 = &((*(pb))[i]);
  let p1 = &(a[*(p0)]);
  var x : f32 = *(p1);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  let pa = &(a);
  let pb = &(b);
  let p0 = &((*(pb))[min(i, 4u)]);
  let p1 = &(a[min(u32(*(p0)), 2u)]);
  var x : f32 = *(p1);
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  let pa = &(a);
  let pb = &(b);
  let index = i;
  let predicate = (u32(index) <= 4u);
  let p0 = &((*(pb))[index]);
  var predicated_load : i32;
  if (predicate) {
    predicated_load = *(p0);
  }
  let index_1 = predicated_load;
  let predicate_1 = (u32(index_1) <= 2u);
  let p1 = &(a[index_1]);
  var predicated_load_1 : f32;
  if (predicate_1) {
    predicated_load_1 = *(p1);
  }
  var x : f32 = predicated_load_1;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_NestedConstantSizedArraysPtr_IndexWithRuntimeExpressions) {
    auto* src = R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let p0 = &(a);
  let p1 = &((*(p0))[x]);
  let p2 = &((*(p1))[y]);
  let p3 = &((*(p2))[z]);
  var r = *(p3);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let p0 = &(a);
  let p1 = &((*(p0))[min(u32(x), 2u)]);
  let p2 = &((*(p1))[min(u32(y), 1u)]);
  let p3 = &((*(p2))[min(u32(z), 0u)]);
  var r = *(p3);
}
)",
                          /* predicate */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let p0 = &(a);
  let index = x;
  let predicate = (u32(index) <= 2u);
  let p1 = &((*(p0))[index]);
  let index_1 = y;
  let predicate_1 = (predicate & (u32(index_1) <= 1u));
  let p2 = &((*(p1))[index_1]);
  let index_2 = z;
  let predicate_2 = (predicate_1 & (u32(index_2) <= 0u));
  let p3 = &((*(p2))[index_2]);
  var predicated_load : f32;
  if (predicate_2) {
    predicated_load = *(p3);
  }
  var r = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_NestedConstantSizedArrays_MixedAccess) {
    auto* src = R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

const y = 1;

override z : i32;

fn f() {
  let p0 = &(a);
  let p1 = &((*(p0))[x]);
  let p2 = &((*(p1))[y]);
  let p3 = &((*(p2))[z]);
  var r = *(p3);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

const y = 1;

override z : i32;

fn f() {
  let p0 = &(a);
  let p1 = &((*(p0))[min(u32(x), 2u)]);
  let p2 = &((*(p1))[y]);
  let p3 = &((*(p2))[min(u32(z), 0u)]);
  var r = *(p3);
}
)",
                          /* predicate */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

const y = 1;

override z : i32;

fn f() {
  let p0 = &(a);
  let index = x;
  let predicate = (u32(index) <= 2u);
  let p1 = &((*(p0))[index]);
  let p2 = &((*(p1))[y]);
  let index_1 = z;
  let predicate_1 = (predicate & (u32(index_1) <= 0u));
  let p3 = &((*(p2))[index_1]);
  var predicated_load : f32;
  if (predicate_1) {
    predicated_load = *(p3);
  }
  var r = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Assign_ConstantSizedArray_IndexWithLet) {
    auto* src = R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  a[l] = 42.0f;
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  a[min(l, 2u)] = 42.0f;
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let index = l;
  let predicate = (u32(index) <= 2u);
  if (predicate) {
    a[index] = 42.0f;
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Assign_ConstantSizedArrayPtr_IndexWithLet) {
    auto* src = R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let p = &(a[l]);
  *(p) = 42.0f;
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let p = &(a[min(l, 2u)]);
  *(p) = 42.0f;
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let index = l;
  let predicate = (u32(index) <= 2u);
  let p = &(a[index]);
  if (predicate) {
    *(p) = 42.0f;
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Assign_ConstantSizedArrayPtr_IndexWithRuntimeArrayIndex) {
    auto* src = R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  let pa = &(a);
  let pb = &(b);
  let p0 = &((*(pb))[i]);
  let p1 = &(a[*(p0)]);
  *(p1) = 42.0f;
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  let pa = &(a);
  let pb = &(b);
  let p0 = &((*(pb))[min(i, 4u)]);
  let p1 = &(a[min(u32(*(p0)), 2u)]);
  *(p1) = 42.0f;
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

var<private> b : array<i32, 5>;

var<private> i : u32;

fn f() {
  let pa = &(a);
  let pb = &(b);
  let index = i;
  let predicate = (u32(index) <= 4u);
  let p0 = &((*(pb))[index]);
  var predicated_load : i32;
  if (predicate) {
    predicated_load = *(p0);
  }
  let index_1 = predicated_load;
  let predicate_1 = (u32(index_1) <= 2u);
  let p1 = &(a[index_1]);
  if (predicate_1) {
    *(p1) = 42.0f;
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Assign_NestedConstantSizedArraysPtr_IndexWithRuntimeExpressions) {
    auto* src = R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let p0 = &(a);
  let p1 = &((*(p0))[x]);
  let p2 = &((*(p1))[y]);
  let p3 = &((*(p2))[z]);
  *(p3) = 42.0f;
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let p0 = &(a);
  let p1 = &((*(p0))[min(u32(x), 2u)]);
  let p2 = &((*(p1))[min(u32(y), 1u)]);
  let p3 = &((*(p2))[min(u32(z), 0u)]);
  *(p3) = 42.0f;
}
)",
                          /* predicate */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

var<private> y : i32;

var<private> z : i32;

fn f() {
  let p0 = &(a);
  let index = x;
  let predicate = (u32(index) <= 2u);
  let p1 = &((*(p0))[index]);
  let index_1 = y;
  let predicate_1 = (predicate & (u32(index_1) <= 1u));
  let p2 = &((*(p1))[index_1]);
  let index_2 = z;
  let predicate_2 = (predicate_1 & (u32(index_2) <= 0u));
  let p3 = &((*(p2))[index_2]);
  if (predicate_2) {
    *(p3) = 42.0f;
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Assign_NestedConstantSizedArrays_MixedAccess) {
    auto* src = R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

const y = 1;

override z : i32;

fn f() {
  let p0 = &(a);
  let p1 = &((*(p0))[x]);
  let p2 = &((*(p1))[y]);
  let p3 = &((*(p2))[z]);
  *(p3) = 42.0f;
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

const y = 1;

override z : i32;

fn f() {
  let p0 = &(a);
  let p1 = &((*(p0))[min(u32(x), 2u)]);
  let p2 = &((*(p1))[y]);
  let p3 = &((*(p2))[min(u32(z), 0u)]);
  *(p3) = 42.0f;
}
)",
                          /* predicate */ R"(
var<private> a : array<array<array<f32, 1>, 2>, 3>;

var<private> x : i32;

const y = 1;

override z : i32;

fn f() {
  let p0 = &(a);
  let index = x;
  let predicate = (u32(index) <= 2u);
  let p1 = &((*(p0))[index]);
  let p2 = &((*(p1))[y]);
  let index_1 = z;
  let predicate_1 = (predicate & (u32(index_1) <= 0u));
  let p3 = &((*(p2))[index_1]);
  if (predicate_1) {
    *(p3) = 42.0f;
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, CompoundAssign_ConstantSizedArray_IndexWithLet) {
    auto* src = R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  a[l] += 42.0f;
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  a[min(l, 2u)] += 42.0f;
}
)",
                          /* predicate */ R"(
var<private> a : array<f32, 3>;

fn f() {
  let l : u32 = 1u;
  let index = l;
  let predicate = (u32(index) <= 2u);
  if (predicate) {
    a[index] += 42.0f;
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

////////////////////////////////////////////////////////////////////////////////
// Runtime sized array
////////////////////////////////////////////////////////////////////////////////

TEST_P(RobustnessTest, Read_RuntimeArray_IndexWithLiteral) {
    auto* src = R"(
struct S {
  a : f32,
  b : array<f32>,
}

@group(0) @binding(0) var<storage, read> s : S;

fn f() {
  var d : f32 = s.b[25];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
struct S {
  a : f32,
  b : array<f32>,
}

@group(0) @binding(0) var<storage, read> s : S;

fn f() {
  var d : f32 = s.b[min(u32(25), (arrayLength(&(s.b)) - 1u))];
}
)",
                          /* predicate */ R"(
struct S {
  a : f32,
  b : array<f32>,
}

@group(0) @binding(0) var<storage, read> s : S;

fn f() {
  let index = 25;
  let predicate = (u32(index) <= (arrayLength(&(s.b)) - 1u));
  var predicated_load : f32;
  if (predicate) {
    predicated_load = s.b[index];
  }
  var d : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

////////////////////////////////////////////////////////////////////////////////
// Vector
////////////////////////////////////////////////////////////////////////////////

TEST_P(RobustnessTest, Read_Vector_IndexWithLiteral) {
    auto* src = R"(
var<private> a : vec3<f32>;

fn f() {
  var b : f32 = a[1i];
}
)";

    auto* expect = src;
    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Vector_IndexWithConst) {
    auto* src = R"(
var<private> a : vec3<f32>;

fn f() {
  const i = 1;
  var b : f32 = a[i];
}
)";

    auto* expect = src;
    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Vector_IndexWithLet) {
    auto* src = R"(
fn f() {
  let i = 99;
  let v = vec4<f32>()[i];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
fn f() {
  let i = 99;
  let v = vec4<f32>()[min(u32(i), 3u)];
}
)",
                          /* predicate */ R"(
fn f() {
  let i = 99;
  let index = i;
  let predicate = (u32(index) <= 3u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = vec4<f32>()[index];
  }
  let v = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Vector_IndexWithRuntimeExpression) {
    auto* src = R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a[((c + 2) - 3)];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a[min(u32(((c + 2) - 3)), 2u)];
}
)",
                          /* predicate */ R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  let index = ((c + 2) - 3);
  let predicate = (u32(index) <= 2u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Vector_SwizzleIndexWithGlobalVar) {
    auto* src = R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a.xy[c];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a.xy[min(u32(c), 1u)];
}
)",
                          /* predicate */ R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  let index = c;
  let predicate = (u32(index) <= 1u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a.xy[index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Vector_SwizzleIndexWithRuntimeExpression) {
    auto* src = R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a.xy[((c + 2) - 3)];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a.xy[min(u32(((c + 2) - 3)), 1u)];
}
)",
                          /* predicate */ R"(
var<private> a : vec3<f32>;

var<private> c : i32;

fn f() {
  let index = ((c + 2) - 3);
  let predicate = (u32(index) <= 1u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a.xy[index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Vector_IndexWithOverride) {
    auto* src = R"(
@id(1300) override idx : i32;

fn f() {
  var a : vec3<f32>;
  var b : f32 = a[idx];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@id(1300) override idx : i32;

fn f() {
  var a : vec3<f32>;
  var b : f32 = a[min(u32(idx), 2u)];
}
)",
                          /* predicate */ R"(
@id(1300) override idx : i32;

fn f() {
  var a : vec3<f32>;
  let index = idx;
  let predicate = (u32(index) <= 2u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));
    EXPECT_EQ(expect, str(got));
}

////////////////////////////////////////////////////////////////////////////////
// Matrix
////////////////////////////////////////////////////////////////////////////////

TEST_P(RobustnessTest, Read_Matrix_IndexingWithLiterals) {
    auto* src = R"(
var<private> a : mat3x2<f32>;

fn f() {
  var b : f32 = a[2i][1i];
}
)";

    auto* expect = src;

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Matrix_IndexWithRuntimeExpressionThenLiteral) {
    auto* src = R"(
var<private> a : mat3x2<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a[((c + 2) - 3)][1];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : mat3x2<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a[min(u32(((c + 2) - 3)), 2u)][1];
}
)",
                          /* predicate */ R"(
var<private> a : mat3x2<f32>;

var<private> c : i32;

fn f() {
  let index = ((c + 2) - 3);
  let predicate = (u32(index) <= 2u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[index][1];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Matrix_IndexWithLiteralThenRuntimeExpression) {
    auto* src = R"(
var<private> a : mat3x2<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a[1][((c + 2) - 3)];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> a : mat3x2<f32>;

var<private> c : i32;

fn f() {
  var b : f32 = a[1][min(u32(((c + 2) - 3)), 1u)];
}
)",
                          /* predicate */ R"(
var<private> a : mat3x2<f32>;

var<private> c : i32;

fn f() {
  let index = ((c + 2) - 3);
  let predicate = (u32(index) <= 1u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[1][index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Matrix_IndexWithOverrideThenLiteral) {
    auto* src = R"(
@id(1300) override idx : i32;

fn f() {
  var a : mat3x2<f32>;
  var b : f32 = a[idx][1];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@id(1300) override idx : i32;

fn f() {
  var a : mat3x2<f32>;
  var b : f32 = a[min(u32(idx), 2u)][1];
}
)",
                          /* predicate */ R"(
@id(1300) override idx : i32;

fn f() {
  var a : mat3x2<f32>;
  let index = idx;
  let predicate = (u32(index) <= 2u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[index][1];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));
    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_Matrix_IndexWithLiteralThenOverride) {
    auto* src = R"(
@id(1300) override idx : i32;

fn f() {
  var a : mat3x2<f32>;
  var b : f32 = a[1][idx];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@id(1300) override idx : i32;

fn f() {
  var a : mat3x2<f32>;
  var b : f32 = a[1][min(u32(idx), 1u)];
}
)",
                          /* predicate */ R"(
@id(1300) override idx : i32;

fn f() {
  var a : mat3x2<f32>;
  let index = idx;
  let predicate = (u32(index) <= 1u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = a[1][index];
  }
  var b : f32 = predicated_load;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));
    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Assign_Matrix_IndexWithLet) {
    auto* src = R"(
var<private> m : mat3x4f;

fn f() {
  let c = 1;
  m[c] = vec4f(1);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> m : mat3x4f;

fn f() {
  let c = 1;
  m[min(u32(c), 2u)] = vec4f(1);
}
)",
                          /* predicate */ R"(
var<private> m : mat3x4f;

fn f() {
  let c = 1;
  let index = c;
  let predicate = (u32(index) <= 2u);
  if (predicate) {
    m[index] = vec4f(1);
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, CompoundAssign_Matrix_IndexWithLet) {
    auto* src = R"(
var<private> m : mat3x4f;

fn f() {
  let c = 1;
  m[c] += vec4f(1);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
var<private> m : mat3x4f;

fn f() {
  let c = 1;
  m[min(u32(c), 2u)] += vec4f(1);
}
)",
                          /* predicate */ R"(
var<private> m : mat3x4f;

fn f() {
  let c = 1;
  let index = c;
  let predicate = (u32(index) <= 2u);
  if (predicate) {
    m[index] += vec4f(1);
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

////////////////////////////////////////////////////////////////////////////////
// Texture
////////////////////////////////////////////////////////////////////////////////

TEST_P(RobustnessTest, Load_Texture1D) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_1d<f32>;

fn load_signed(coords : i32, level : i32) {
  let l = textureLoad(t, coords, level);
}

fn load_unsigned(coords : u32, level : u32) {
  let l = textureLoad(t, coords, level);
}

fn load_mixed(coords : i32, level : u32) {
  let l = textureLoad(t, coords, level);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_1d<f32>;

fn load_signed(coords : i32, level : i32) {
  let level_1 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, clamp(coords, 0, i32((textureDimensions(t, level_1) - 1))), level_1);
}

fn load_unsigned(coords : u32, level : u32) {
  let level_2 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_2) - 1)), level_2);
}

fn load_mixed(coords : i32, level : u32) {
  let level_3 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, clamp(coords, 0, i32((textureDimensions(t, level_3) - 1))), level_3);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_1d<f32>;

fn load_signed(coords : i32, level : i32) {
  let level_1 = u32(level);
  let level_clamped = min(level_1, (textureNumLevels(t) - 1));
  let coords_1 = coords;
  var texture_load : vec4<f32>;
  if ((all((u32(coords_1) < textureDimensions(t, level_clamped))) & (level_1 < textureNumLevels(t)))) {
    texture_load = textureLoad(t, coords_1, level_1);
  }
  let l = texture_load;
}

fn load_unsigned(coords : u32, level : u32) {
  let level_2 = u32(level);
  let level_clamped_1 = min(level_2, (textureNumLevels(t) - 1));
  let coords_2 = coords;
  var texture_load_1 : vec4<f32>;
  if ((all((u32(coords_2) < textureDimensions(t, level_clamped_1))) & (level_2 < textureNumLevels(t)))) {
    texture_load_1 = textureLoad(t, coords_2, level_2);
  }
  let l = texture_load_1;
}

fn load_mixed(coords : i32, level : u32) {
  let level_3 = u32(level);
  let level_clamped_2 = min(level_3, (textureNumLevels(t) - 1));
  let coords_3 = coords;
  var texture_load_2 : vec4<f32>;
  if ((all((u32(coords_3) < textureDimensions(t, level_clamped_2))) & (level_3 < textureNumLevels(t)))) {
    texture_load_2 = textureLoad(t, coords_3, level_3);
  }
  let l = texture_load_2;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Load_Texture2D) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

fn load_signed(coords : vec2i, level : i32) {
  let l = textureLoad(t, coords, level);
}

fn load_unsigned(coords : vec2u, level : u32) {
  let l = textureLoad(t, coords, level);
}

fn load_mixed(coords : vec2u, level : i32) {
  let l = textureLoad(t, coords, level);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_2d<f32>;

fn load_signed(coords : vec2i, level : i32) {
  let level_1 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t, level_1) - vec2(1)))), level_1);
}

fn load_unsigned(coords : vec2u, level : u32) {
  let level_2 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_2) - vec2(1))), level_2);
}

fn load_mixed(coords : vec2u, level : i32) {
  let level_3 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_3) - vec2(1))), level_3);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_2d<f32>;

fn load_signed(coords : vec2i, level : i32) {
  let level_1 = u32(level);
  let level_clamped = min(level_1, (textureNumLevels(t) - 1));
  let coords_1 = coords;
  var texture_load : vec4<f32>;
  if ((all((vec2<u32>(coords_1) < textureDimensions(t, level_clamped))) & (level_1 < textureNumLevels(t)))) {
    texture_load = textureLoad(t, coords_1, level_1);
  }
  let l = texture_load;
}

fn load_unsigned(coords : vec2u, level : u32) {
  let level_2 = u32(level);
  let level_clamped_1 = min(level_2, (textureNumLevels(t) - 1));
  let coords_2 = coords;
  var texture_load_1 : vec4<f32>;
  if ((all((vec2<u32>(coords_2) < textureDimensions(t, level_clamped_1))) & (level_2 < textureNumLevels(t)))) {
    texture_load_1 = textureLoad(t, coords_2, level_2);
  }
  let l = texture_load_1;
}

fn load_mixed(coords : vec2u, level : i32) {
  let level_3 = u32(level);
  let level_clamped_2 = min(level_3, (textureNumLevels(t) - 1));
  let coords_3 = coords;
  var texture_load_2 : vec4<f32>;
  if ((all((vec2<u32>(coords_3) < textureDimensions(t, level_clamped_2))) & (level_3 < textureNumLevels(t)))) {
    texture_load_2 = textureLoad(t, coords_3, level_3);
  }
  let l = texture_load_2;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Load_Texture2DArray) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_2d_array<f32>;

fn load_signed(coords : vec2i, array : i32, level : i32) {
  let l = textureLoad(t, coords, array, level);
}

fn load_unsigned(coords : vec2u, array : u32, level : u32) {
  let l = textureLoad(t, coords, array, level);
}

fn load_mixed(coords : vec2u, array : i32, level : u32) {
  let l = textureLoad(t, coords, array, level);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_2d_array<f32>;

fn load_signed(coords : vec2i, array : i32, level : i32) {
  let level_1 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t, level_1) - vec2(1)))), clamp(array, 0, i32((textureNumLayers(t) - 1))), level_1);
}

fn load_unsigned(coords : vec2u, array : u32, level : u32) {
  let level_2 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_2) - vec2(1))), min(array, (textureNumLayers(t) - 1)), level_2);
}

fn load_mixed(coords : vec2u, array : i32, level : u32) {
  let level_3 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_3) - vec2(1))), clamp(array, 0, i32((textureNumLayers(t) - 1))), level_3);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_2d_array<f32>;

fn load_signed(coords : vec2i, array : i32, level : i32) {
  let level_1 = u32(level);
  let level_clamped = min(level_1, (textureNumLevels(t) - 1));
  let coords_1 = coords;
  let array_idx = array;
  var texture_load : vec4<f32>;
  if (((all((vec2<u32>(coords_1) < textureDimensions(t, level_clamped))) & (level_1 < textureNumLevels(t))) & (u32(array_idx) < textureNumLayers(t)))) {
    texture_load = textureLoad(t, coords_1, array_idx, level_1);
  }
  let l = texture_load;
}

fn load_unsigned(coords : vec2u, array : u32, level : u32) {
  let level_2 = u32(level);
  let level_clamped_1 = min(level_2, (textureNumLevels(t) - 1));
  let coords_2 = coords;
  let array_idx_1 = array;
  var texture_load_1 : vec4<f32>;
  if (((all((vec2<u32>(coords_2) < textureDimensions(t, level_clamped_1))) & (level_2 < textureNumLevels(t))) & (u32(array_idx_1) < textureNumLayers(t)))) {
    texture_load_1 = textureLoad(t, coords_2, array_idx_1, level_2);
  }
  let l = texture_load_1;
}

fn load_mixed(coords : vec2u, array : i32, level : u32) {
  let level_3 = u32(level);
  let level_clamped_2 = min(level_3, (textureNumLevels(t) - 1));
  let coords_3 = coords;
  let array_idx_2 = array;
  var texture_load_2 : vec4<f32>;
  if (((all((vec2<u32>(coords_3) < textureDimensions(t, level_clamped_2))) & (level_3 < textureNumLevels(t))) & (u32(array_idx_2) < textureNumLayers(t)))) {
    texture_load_2 = textureLoad(t, coords_3, array_idx_2, level_3);
  }
  let l = texture_load_2;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Load_Texture3D) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_3d<f32>;

fn load_signed(coords : vec3i, level : i32) {
  let l = textureLoad(t, coords, level);
}

fn load_unsigned(coords : vec3u, level : u32) {
  let l = textureLoad(t, coords, level);
}

fn load_mixed(coords : vec3u, level : i32) {
  let l = textureLoad(t, coords, level);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_3d<f32>;

fn load_signed(coords : vec3i, level : i32) {
  let level_1 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, clamp(coords, vec3(0), vec3<i32>((textureDimensions(t, level_1) - vec3(1)))), level_1);
}

fn load_unsigned(coords : vec3u, level : u32) {
  let level_2 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_2) - vec3(1))), level_2);
}

fn load_mixed(coords : vec3u, level : i32) {
  let level_3 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_3) - vec3(1))), level_3);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_3d<f32>;

fn load_signed(coords : vec3i, level : i32) {
  let level_1 = u32(level);
  let level_clamped = min(level_1, (textureNumLevels(t) - 1));
  let coords_1 = coords;
  var texture_load : vec4<f32>;
  if ((all((vec3<u32>(coords_1) < textureDimensions(t, level_clamped))) & (level_1 < textureNumLevels(t)))) {
    texture_load = textureLoad(t, coords_1, level_1);
  }
  let l = texture_load;
}

fn load_unsigned(coords : vec3u, level : u32) {
  let level_2 = u32(level);
  let level_clamped_1 = min(level_2, (textureNumLevels(t) - 1));
  let coords_2 = coords;
  var texture_load_1 : vec4<f32>;
  if ((all((vec3<u32>(coords_2) < textureDimensions(t, level_clamped_1))) & (level_2 < textureNumLevels(t)))) {
    texture_load_1 = textureLoad(t, coords_2, level_2);
  }
  let l = texture_load_1;
}

fn load_mixed(coords : vec3u, level : i32) {
  let level_3 = u32(level);
  let level_clamped_2 = min(level_3, (textureNumLevels(t) - 1));
  let coords_3 = coords;
  var texture_load_2 : vec4<f32>;
  if ((all((vec3<u32>(coords_3) < textureDimensions(t, level_clamped_2))) & (level_3 < textureNumLevels(t)))) {
    texture_load_2 = textureLoad(t, coords_3, level_3);
  }
  let l = texture_load_2;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Load_TextureMultisampled2D) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_multisampled_2d<f32>;

fn load_signed(coords : vec2i, sample : i32) {
  let l = textureLoad(t, coords, sample);
}

fn load_unsigned(coords : vec2u, sample : u32) {
  let l = textureLoad(t, coords, sample);
}

fn load_mixed(coords : vec2i, sample : u32) {
  let l = textureLoad(t, coords, sample);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_multisampled_2d<f32>;

fn load_signed(coords : vec2i, sample : i32) {
  let l = textureLoad(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t) - vec2(1)))), sample);
}

fn load_unsigned(coords : vec2u, sample : u32) {
  let l = textureLoad(t, min(coords, (textureDimensions(t) - vec2(1))), sample);
}

fn load_mixed(coords : vec2i, sample : u32) {
  let l = textureLoad(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t) - vec2(1)))), sample);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_multisampled_2d<f32>;

fn load_signed(coords : vec2i, sample : i32) {
  let coords_1 = coords;
  var texture_load : vec4<f32>;
  if (all((vec2<u32>(coords_1) < textureDimensions(t)))) {
    texture_load = textureLoad(t, coords_1, sample);
  }
  let l = texture_load;
}

fn load_unsigned(coords : vec2u, sample : u32) {
  let coords_2 = coords;
  var texture_load_1 : vec4<f32>;
  if (all((vec2<u32>(coords_2) < textureDimensions(t)))) {
    texture_load_1 = textureLoad(t, coords_2, sample);
  }
  let l = texture_load_1;
}

fn load_mixed(coords : vec2i, sample : u32) {
  let coords_3 = coords;
  var texture_load_2 : vec4<f32>;
  if (all((vec2<u32>(coords_3) < textureDimensions(t)))) {
    texture_load_2 = textureLoad(t, coords_3, sample);
  }
  let l = texture_load_2;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Load_TextureDepth2D) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_depth_2d;

fn load_signed(coords : vec2i, level : i32) {
  let l = textureLoad(t, coords, level);
}

fn load_unsigned(coords : vec2u, level : u32) {
  let l = textureLoad(t, coords, level);
}

fn load_mixed(coords : vec2i, level : u32) {
  let l = textureLoad(t, coords, level);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_depth_2d;

fn load_signed(coords : vec2i, level : i32) {
  let level_1 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t, level_1) - vec2(1)))), level_1);
}

fn load_unsigned(coords : vec2u, level : u32) {
  let level_2 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_2) - vec2(1))), level_2);
}

fn load_mixed(coords : vec2i, level : u32) {
  let level_3 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t, level_3) - vec2(1)))), level_3);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_depth_2d;

fn load_signed(coords : vec2i, level : i32) {
  let level_1 = u32(level);
  let level_clamped = min(level_1, (textureNumLevels(t) - 1));
  let coords_1 = coords;
  var texture_load : f32;
  if ((all((vec2<u32>(coords_1) < textureDimensions(t, level_clamped))) & (level_1 < textureNumLevels(t)))) {
    texture_load = textureLoad(t, coords_1, level_1);
  }
  let l = texture_load;
}

fn load_unsigned(coords : vec2u, level : u32) {
  let level_2 = u32(level);
  let level_clamped_1 = min(level_2, (textureNumLevels(t) - 1));
  let coords_2 = coords;
  var texture_load_1 : f32;
  if ((all((vec2<u32>(coords_2) < textureDimensions(t, level_clamped_1))) & (level_2 < textureNumLevels(t)))) {
    texture_load_1 = textureLoad(t, coords_2, level_2);
  }
  let l = texture_load_1;
}

fn load_mixed(coords : vec2i, level : u32) {
  let level_3 = u32(level);
  let level_clamped_2 = min(level_3, (textureNumLevels(t) - 1));
  let coords_3 = coords;
  var texture_load_2 : f32;
  if ((all((vec2<u32>(coords_3) < textureDimensions(t, level_clamped_2))) & (level_3 < textureNumLevels(t)))) {
    texture_load_2 = textureLoad(t, coords_3, level_3);
  }
  let l = texture_load_2;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Load_TextureDepth2DArray) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_depth_2d_array;

fn load_signed(coords : vec2i, array : i32, level : i32) {
  let l = textureLoad(t, coords, array, level);
}

fn load_unsigned(coords : vec2u, array : u32, level : u32) {
  let l = textureLoad(t, coords, array, level);
}

fn load_mixed(coords : vec2u, array : i32, level : u32) {
  let l = textureLoad(t, coords, array, level);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_depth_2d_array;

fn load_signed(coords : vec2i, array : i32, level : i32) {
  let level_1 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t, level_1) - vec2(1)))), clamp(array, 0, i32((textureNumLayers(t) - 1))), level_1);
}

fn load_unsigned(coords : vec2u, array : u32, level : u32) {
  let level_2 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_2) - vec2(1))), min(array, (textureNumLayers(t) - 1)), level_2);
}

fn load_mixed(coords : vec2u, array : i32, level : u32) {
  let level_3 = min(u32(level), (textureNumLevels(t) - 1));
  let l = textureLoad(t, min(coords, (textureDimensions(t, level_3) - vec2(1))), clamp(array, 0, i32((textureNumLayers(t) - 1))), level_3);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_depth_2d_array;

fn load_signed(coords : vec2i, array : i32, level : i32) {
  let level_1 = u32(level);
  let level_clamped = min(level_1, (textureNumLevels(t) - 1));
  let coords_1 = coords;
  let array_idx = array;
  var texture_load : f32;
  if (((all((vec2<u32>(coords_1) < textureDimensions(t, level_clamped))) & (level_1 < textureNumLevels(t))) & (u32(array_idx) < textureNumLayers(t)))) {
    texture_load = textureLoad(t, coords_1, array_idx, level_1);
  }
  let l = texture_load;
}

fn load_unsigned(coords : vec2u, array : u32, level : u32) {
  let level_2 = u32(level);
  let level_clamped_1 = min(level_2, (textureNumLevels(t) - 1));
  let coords_2 = coords;
  let array_idx_1 = array;
  var texture_load_1 : f32;
  if (((all((vec2<u32>(coords_2) < textureDimensions(t, level_clamped_1))) & (level_2 < textureNumLevels(t))) & (u32(array_idx_1) < textureNumLayers(t)))) {
    texture_load_1 = textureLoad(t, coords_2, array_idx_1, level_2);
  }
  let l = texture_load_1;
}

fn load_mixed(coords : vec2u, array : i32, level : u32) {
  let level_3 = u32(level);
  let level_clamped_2 = min(level_3, (textureNumLevels(t) - 1));
  let coords_3 = coords;
  let array_idx_2 = array;
  var texture_load_2 : f32;
  if (((all((vec2<u32>(coords_3) < textureDimensions(t, level_clamped_2))) & (level_3 < textureNumLevels(t))) & (u32(array_idx_2) < textureNumLayers(t)))) {
    texture_load_2 = textureLoad(t, coords_3, array_idx_2, level_3);
  }
  let l = texture_load_2;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Load_TextureExternal) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_external;

fn load_signed(coords : vec2i) {
  let l = textureLoad(t, coords);
}

fn load_unsigned(coords : vec2u) {
  let l = textureLoad(t, coords);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_external;

fn load_signed(coords : vec2i) {
  let l = textureLoad(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t) - vec2(1)))));
}

fn load_unsigned(coords : vec2u) {
  let l = textureLoad(t, min(coords, (textureDimensions(t) - vec2(1))));
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_external;

fn load_signed(coords : vec2i) {
  let coords_1 = coords;
  var texture_load : vec4<f32>;
  if (all((vec2<u32>(coords_1) < textureDimensions(t)))) {
    texture_load = textureLoad(t, coords_1);
  }
  let l = texture_load;
}

fn load_unsigned(coords : vec2u) {
  let coords_2 = coords;
  var texture_load_1 : vec4<f32>;
  if (all((vec2<u32>(coords_2) < textureDimensions(t)))) {
    texture_load_1 = textureLoad(t, coords_2);
  }
  let l = texture_load_1;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Store_Texture1D) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_storage_1d<rgba8sint, write>;

fn store_signed(coords : i32, value : vec4i) {
  textureStore(t, coords, value);
}

fn store_unsigned(coords : u32, value : vec4i) {
  textureStore(t, coords, value);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_storage_1d<rgba8sint, write>;

fn store_signed(coords : i32, value : vec4i) {
  textureStore(t, clamp(coords, 0, i32((textureDimensions(t) - 1))), value);
}

fn store_unsigned(coords : u32, value : vec4i) {
  textureStore(t, min(coords, (textureDimensions(t) - 1)), value);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_storage_1d<rgba8sint, write>;

fn store_signed(coords : i32, value : vec4i) {
  let coords_1 = coords;
  if (all((u32(coords_1) < textureDimensions(t)))) {
    textureStore(t, coords_1, value);
  }
}

fn store_unsigned(coords : u32, value : vec4i) {
  let coords_2 = coords;
  if (all((u32(coords_2) < textureDimensions(t)))) {
    textureStore(t, coords_2, value);
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Store_Texture2D) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_storage_2d<rgba8sint, write>;

fn store_signed(coords : vec2i, value : vec4i) {
  textureStore(t, coords, value);
}

fn store_unsigned(coords : vec2u, value : vec4i) {
  textureStore(t, coords, value);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_storage_2d<rgba8sint, write>;

fn store_signed(coords : vec2i, value : vec4i) {
  textureStore(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t) - vec2(1)))), value);
}

fn store_unsigned(coords : vec2u, value : vec4i) {
  textureStore(t, min(coords, (textureDimensions(t) - vec2(1))), value);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_storage_2d<rgba8sint, write>;

fn store_signed(coords : vec2i, value : vec4i) {
  let coords_1 = coords;
  if (all((vec2<u32>(coords_1) < textureDimensions(t)))) {
    textureStore(t, coords_1, value);
  }
}

fn store_unsigned(coords : vec2u, value : vec4i) {
  let coords_2 = coords;
  if (all((vec2<u32>(coords_2) < textureDimensions(t)))) {
    textureStore(t, coords_2, value);
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Store_Texture2DArray) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_storage_2d_array<rgba8sint, write>;

fn store_signed(coords : vec2i, array : i32, value : vec4i) {
  textureStore(t, coords, array, value);
}

fn store_unsigned(coords : vec2u, array : i32, value : vec4i) {
  textureStore(t, coords, array, value);
}

fn store_mixed(coords : vec2i, array : i32, value : vec4i) {
  textureStore(t, coords, array, value);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_storage_2d_array<rgba8sint, write>;

fn store_signed(coords : vec2i, array : i32, value : vec4i) {
  textureStore(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t) - vec2(1)))), clamp(array, 0, i32((textureNumLayers(t) - 1))), value);
}

fn store_unsigned(coords : vec2u, array : i32, value : vec4i) {
  textureStore(t, min(coords, (textureDimensions(t) - vec2(1))), clamp(array, 0, i32((textureNumLayers(t) - 1))), value);
}

fn store_mixed(coords : vec2i, array : i32, value : vec4i) {
  textureStore(t, clamp(coords, vec2(0), vec2<i32>((textureDimensions(t) - vec2(1)))), clamp(array, 0, i32((textureNumLayers(t) - 1))), value);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_storage_2d_array<rgba8sint, write>;

fn store_signed(coords : vec2i, array : i32, value : vec4i) {
  let coords_1 = coords;
  let array_idx = array;
  if ((all((vec2<u32>(coords_1) < textureDimensions(t))) & (u32(array_idx) < textureNumLayers(t)))) {
    textureStore(t, coords_1, array_idx, value);
  }
}

fn store_unsigned(coords : vec2u, array : i32, value : vec4i) {
  let coords_2 = coords;
  let array_idx_1 = array;
  if ((all((vec2<u32>(coords_2) < textureDimensions(t))) & (u32(array_idx_1) < textureNumLayers(t)))) {
    textureStore(t, coords_2, array_idx_1, value);
  }
}

fn store_mixed(coords : vec2i, array : i32, value : vec4i) {
  let coords_3 = coords;
  let array_idx_2 = array;
  if ((all((vec2<u32>(coords_3) < textureDimensions(t))) & (u32(array_idx_2) < textureNumLayers(t)))) {
    textureStore(t, coords_3, array_idx_2, value);
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Store_Texture3D) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_storage_3d<rgba8sint, write>;

fn store_signed(coords : vec3i, value : vec4i) {
  textureStore(t, coords, value);
}

fn store_unsigned(coords : vec3u, value : vec4i) {
  textureStore(t, coords, value);
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var t : texture_storage_3d<rgba8sint, write>;

fn store_signed(coords : vec3i, value : vec4i) {
  textureStore(t, clamp(coords, vec3(0), vec3<i32>((textureDimensions(t) - vec3(1)))), value);
}

fn store_unsigned(coords : vec3u, value : vec4i) {
  textureStore(t, min(coords, (textureDimensions(t) - vec3(1))), value);
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var t : texture_storage_3d<rgba8sint, write>;

fn store_signed(coords : vec3i, value : vec4i) {
  let coords_1 = coords;
  if (all((vec3<u32>(coords_1) < textureDimensions(t)))) {
    textureStore(t, coords_1, value);
  }
}

fn store_unsigned(coords : vec3u, value : vec4i) {
  let coords_2 = coords;
  if (all((vec3<u32>(coords_2) < textureDimensions(t)))) {
    textureStore(t, coords_2, value);
  }
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, ShadowedVariable) {
    auto* src = R"(
fn f() {
  var a : array<f32, 3>;
  var i : u32;
  {
    var a : array<f32, 5>;
    var b : f32 = a[i];
  }
  var c : f32 = a[i];
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
fn f() {
  var a : array<f32, 3>;
  var i : u32;
  {
    var a : array<f32, 5>;
    var b : f32 = a[min(i, 4u)];
  }
  var c : f32 = a[min(i, 2u)];
}
)",
                          /* predicate */ R"(
fn f() {
  var a : array<f32, 3>;
  var i : u32;
  {
    var a : array<f32, 5>;
    let index = i;
    let predicate = (u32(index) <= 4u);
    var predicated_load : f32;
    if (predicate) {
      predicated_load = a[index];
    }
    var b : f32 = predicated_load;
  }
  let index_1 = i;
  let predicate_1 = (u32(index_1) <= 2u);
  var predicated_load_1 : f32;
  if (predicate_1) {
    predicated_load_1 = a[index_1];
  }
  var c : f32 = predicated_load_1;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));
    EXPECT_EQ(expect, str(got));
}

// Check that existing use of min() and arrayLength() do not get renamed.
TEST_P(RobustnessTest, DontRenameSymbols) {
    auto* src = R"(
struct S {
  a : f32,
  b : array<f32>,
}

@group(0) @binding(0) var<storage, read> s : S;

const c : u32 = 1u;

fn f() {
  let b : f32 = s.b[c];
  let x : i32 = min(1, 2);
  let y : u32 = arrayLength(&(s.b));
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
struct S {
  a : f32,
  b : array<f32>,
}

@group(0) @binding(0) var<storage, read> s : S;

const c : u32 = 1u;

fn f() {
  let b : f32 = s.b[min(c, (arrayLength(&(s.b)) - 1u))];
  let x : i32 = min(1, 2);
  let y : u32 = arrayLength(&(s.b));
}
)",
                          /* predicate */ R"(
struct S {
  a : f32,
  b : array<f32>,
}

@group(0) @binding(0) var<storage, read> s : S;

const c : u32 = 1u;

fn f() {
  let index = c;
  let predicate = (u32(index) <= (arrayLength(&(s.b)) - 1u));
  var predicated_load : f32;
  if (predicate) {
    predicated_load = s.b[index];
  }
  let b : f32 = predicated_load;
  let x : i32 = min(1, 2);
  let y : u32 = arrayLength(&(s.b));
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, WorkgroupOverrideCount) {
    auto* src = R"(
override N = 123;

var<workgroup> w : array<f32, N>;

fn f() {
  var b : f32 = w[1i];
}
)";

    auto* expect =
        Expect(GetParam(),
               /* ignore */ src,
               /* clamp */
               R"(error: array size is an override-expression, when expected a constant-expression.
Was the SubstituteOverride transform run?)",
               /* predicate */
               R"(error: array size is an override-expression, when expected a constant-expression.
Was the SubstituteOverride transform run?)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

INSTANTIATE_TEST_SUITE_P(,
                         RobustnessTest,
                         testing::Values(Robustness::Action::kIgnore,
                                         Robustness::Action::kClamp,
                                         Robustness::Action::kPredicate));
}  // namespace
}  // namespace tint::transform
