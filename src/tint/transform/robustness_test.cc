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

TEST_P(RobustnessTest, Read_ConstantSizedArray_IndexWithLiteral) {
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

TEST_P(RobustnessTest, Read_ConstantSizedArray_IndexWithConst) {
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

TEST_P(RobustnessTest, Read_ConstantSizedArray_IndexWithLet) {
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
  let predicate = (u32(index) < 2u);
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

TEST_P(RobustnessTest, Read_ConstantSizedArray_IndexWithRuntimeArrayIndex) {
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
  let predicate = (u32(index) < 4u);
  var predicated_load : i32;
  if (predicate) {
    predicated_load = b[index];
  }
  let index_1 = b[index];
  let predicate_1 = (u32(index_1) < 2u);
  var predicated_load_1 : f32;
  if (predicate_1) {
    predicated_load_1 = a[predicated_load];
  }
  var c : f32 = predicated_load_1;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

TEST_P(RobustnessTest, Read_ConstantSizedArray_IndexWithRuntimeExpression) {
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
  let predicate = (u32(index) < 2u);
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

TEST_P(RobustnessTest, Read_ConstantSizedArray_IndexWithOverride) {
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
  let predicate = (u32(index) < 3u);
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
  let predicate = (u32(index) < 2u);
  if (predicate) {
    a[index] = 42.0f;
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
  let predicate = (u32(index) < 2u);
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
  let predicate = (u32(index) < (arrayLength(&(s.b)) - 1u));
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
  let predicate = (u32(index) < 2u);
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
  let predicate = (u32(index) < 1u);
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
  let predicate = (u32(index) < 1u);
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
  let predicate = (u32(index) < 2u);
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
  let predicate = (u32(index) < 2u);
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
  let predicate = (u32(index) < 1u);
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
  let predicate = (u32(index) < 2u);
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
  let predicate = (u32(index) < 1u);
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
  let predicate = (u32(index) < 2u);
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
  let predicate = (u32(index) < 2u);
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

// Clamp textureLoad() coord, array_index and level values
TEST_P(RobustnessTest, TextureLoad) {
    auto* src = R"(
@group(0) @binding(0) var tex_1d : texture_1d<f32>;

@group(0) @binding(0) var tex_2d : texture_2d<f32>;

@group(0) @binding(0) var tex_2d_arr : texture_2d_array<f32>;

@group(0) @binding(0) var tex_3d : texture_3d<f32>;

@group(0) @binding(0) var tex_ms_2d : texture_multisampled_2d<f32>;

@group(0) @binding(0) var tex_depth_2d : texture_depth_2d;

@group(0) @binding(0) var tex_depth_2d_arr : texture_depth_2d_array;

@group(0) @binding(0) var tex_external : texture_external;

fn idx_signed() {
  var array_idx : i32;
  var level_idx : i32;
  var sample_idx : i32;
  _ = textureLoad(tex_1d, 1i, level_idx);
  _ = textureLoad(tex_2d, vec2<i32>(1, 2), level_idx);
  _ = textureLoad(tex_2d_arr, vec2<i32>(1, 2), array_idx, level_idx);
  _ = textureLoad(tex_3d, vec3<i32>(1, 2, 3), level_idx);
  _ = textureLoad(tex_ms_2d, vec2<i32>(1, 2), sample_idx);
  _ = textureLoad(tex_depth_2d, vec2<i32>(1, 2), level_idx);
  _ = textureLoad(tex_depth_2d_arr, vec2<i32>(1, 2), array_idx, level_idx);
  _ = textureLoad(tex_external, vec2<i32>(1, 2));
}

fn idx_unsigned() {
  var array_idx : u32;
  var level_idx : u32;
  var sample_idx : u32;
  _ = textureLoad(tex_1d, 1u, level_idx);
  _ = textureLoad(tex_2d, vec2<u32>(1, 2), level_idx);
  _ = textureLoad(tex_2d_arr, vec2<u32>(1, 2), array_idx, level_idx);
  _ = textureLoad(tex_3d, vec3<u32>(1, 2, 3), level_idx);
  _ = textureLoad(tex_ms_2d, vec2<u32>(1, 2), sample_idx);
  _ = textureLoad(tex_depth_2d, vec2<u32>(1, 2), level_idx);
  _ = textureLoad(tex_depth_2d_arr, vec2<u32>(1, 2), array_idx, level_idx);
  _ = textureLoad(tex_external, vec2<u32>(1, 2));
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var tex_1d : texture_1d<f32>;

@group(0) @binding(0) var tex_2d : texture_2d<f32>;

@group(0) @binding(0) var tex_2d_arr : texture_2d_array<f32>;

@group(0) @binding(0) var tex_3d : texture_3d<f32>;

@group(0) @binding(0) var tex_ms_2d : texture_multisampled_2d<f32>;

@group(0) @binding(0) var tex_depth_2d : texture_depth_2d;

@group(0) @binding(0) var tex_depth_2d_arr : texture_depth_2d_array;

@group(0) @binding(0) var tex_external : texture_external;

fn idx_signed() {
  var array_idx : i32;
  var level_idx : i32;
  var sample_idx : i32;
  let level = min(u32(level_idx), (textureNumLevels(tex_1d) - 1));
  _ = textureLoad(tex_1d, clamp(1i, 0, i32((textureDimensions(tex_1d, level) - 1))), level);
  let level_1 = min(u32(level_idx), (textureNumLevels(tex_2d) - 1));
  _ = textureLoad(tex_2d, clamp(vec2<i32>(1, 2), vec2(0), vec2<i32>((textureDimensions(tex_2d, level_1) - vec2(1)))), level_1);
  let level_2 = min(u32(level_idx), (textureNumLevels(tex_2d_arr) - 1));
  _ = textureLoad(tex_2d_arr, clamp(vec2<i32>(1, 2), vec2(0), vec2<i32>((textureDimensions(tex_2d_arr, level_2) - vec2(1)))), clamp(array_idx, 0, i32((textureNumLayers(tex_2d_arr) - 1))), level_2);
  let level_3 = min(u32(level_idx), (textureNumLevels(tex_3d) - 1));
  _ = textureLoad(tex_3d, clamp(vec3<i32>(1, 2, 3), vec3(0), vec3<i32>((textureDimensions(tex_3d, level_3) - vec3(1)))), level_3);
  _ = textureLoad(tex_ms_2d, clamp(vec2<i32>(1, 2), vec2(0), vec2<i32>((textureDimensions(tex_ms_2d) - vec2(1)))), sample_idx);
  let level_4 = min(u32(level_idx), (textureNumLevels(tex_depth_2d) - 1));
  _ = textureLoad(tex_depth_2d, clamp(vec2<i32>(1, 2), vec2(0), vec2<i32>((textureDimensions(tex_depth_2d, level_4) - vec2(1)))), level_4);
  let level_5 = min(u32(level_idx), (textureNumLevels(tex_depth_2d_arr) - 1));
  _ = textureLoad(tex_depth_2d_arr, clamp(vec2<i32>(1, 2), vec2(0), vec2<i32>((textureDimensions(tex_depth_2d_arr, level_5) - vec2(1)))), clamp(array_idx, 0, i32((textureNumLayers(tex_depth_2d_arr) - 1))), level_5);
  _ = textureLoad(tex_external, clamp(vec2<i32>(1, 2), vec2(0), vec2<i32>((textureDimensions(tex_external) - vec2(1)))));
}

fn idx_unsigned() {
  var array_idx : u32;
  var level_idx : u32;
  var sample_idx : u32;
  let level_6 = min(u32(level_idx), (textureNumLevels(tex_1d) - 1));
  _ = textureLoad(tex_1d, min(1u, (textureDimensions(tex_1d, level_6) - 1)), level_6);
  let level_7 = min(u32(level_idx), (textureNumLevels(tex_2d) - 1));
  _ = textureLoad(tex_2d, min(vec2<u32>(1, 2), (textureDimensions(tex_2d, level_7) - vec2(1))), level_7);
  let level_8 = min(u32(level_idx), (textureNumLevels(tex_2d_arr) - 1));
  _ = textureLoad(tex_2d_arr, min(vec2<u32>(1, 2), (textureDimensions(tex_2d_arr, level_8) - vec2(1))), min(array_idx, (textureNumLayers(tex_2d_arr) - 1)), level_8);
  let level_9 = min(u32(level_idx), (textureNumLevels(tex_3d) - 1));
  _ = textureLoad(tex_3d, min(vec3<u32>(1, 2, 3), (textureDimensions(tex_3d, level_9) - vec3(1))), level_9);
  _ = textureLoad(tex_ms_2d, min(vec2<u32>(1, 2), (textureDimensions(tex_ms_2d) - vec2(1))), sample_idx);
  let level_10 = min(u32(level_idx), (textureNumLevels(tex_depth_2d) - 1));
  _ = textureLoad(tex_depth_2d, min(vec2<u32>(1, 2), (textureDimensions(tex_depth_2d, level_10) - vec2(1))), level_10);
  let level_11 = min(u32(level_idx), (textureNumLevels(tex_depth_2d_arr) - 1));
  _ = textureLoad(tex_depth_2d_arr, min(vec2<u32>(1, 2), (textureDimensions(tex_depth_2d_arr, level_11) - vec2(1))), min(array_idx, (textureNumLayers(tex_depth_2d_arr) - 1)), level_11);
  _ = textureLoad(tex_external, min(vec2<u32>(1, 2), (textureDimensions(tex_external) - vec2(1))));
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var tex_1d : texture_1d<f32>;

@group(0) @binding(0) var tex_2d : texture_2d<f32>;

@group(0) @binding(0) var tex_2d_arr : texture_2d_array<f32>;

@group(0) @binding(0) var tex_3d : texture_3d<f32>;

@group(0) @binding(0) var tex_ms_2d : texture_multisampled_2d<f32>;

@group(0) @binding(0) var tex_depth_2d : texture_depth_2d;

@group(0) @binding(0) var tex_depth_2d_arr : texture_depth_2d_array;

@group(0) @binding(0) var tex_external : texture_external;

fn idx_signed() {
  var array_idx : i32;
  var level_idx : i32;
  var sample_idx : i32;
  let level = u32(level_idx);
  let level_clamped = min(level, (textureNumLevels(tex_1d) - 1));
  let coords = 1i;
  var texture_load : vec4<f32>;
  if ((all((u32(coords) < textureDimensions(tex_1d, level_clamped))) & (level < textureNumLevels(tex_1d)))) {
    texture_load = textureLoad(tex_1d, coords, level);
  }
  _ = texture_load;
  let level_1 = u32(level_idx);
  let level_clamped_1 = min(level_1, (textureNumLevels(tex_2d) - 1));
  let coords_1 = vec2<i32>(1, 2);
  var texture_load_1 : vec4<f32>;
  if ((all((vec2<u32>(coords_1) < textureDimensions(tex_2d, level_clamped_1))) & (level_1 < textureNumLevels(tex_2d)))) {
    texture_load_1 = textureLoad(tex_2d, coords_1, level_1);
  }
  _ = texture_load_1;
  let level_2 = u32(level_idx);
  let level_clamped_2 = min(level_2, (textureNumLevels(tex_2d_arr) - 1));
  let coords_2 = vec2<i32>(1, 2);
  let array_idx_1 = array_idx;
  var texture_load_2 : vec4<f32>;
  if (((all((vec2<u32>(coords_2) < textureDimensions(tex_2d_arr, level_clamped_2))) & (level_2 < textureNumLevels(tex_2d_arr))) & (u32(array_idx_1) < textureNumLayers(tex_2d_arr)))) {
    texture_load_2 = textureLoad(tex_2d_arr, coords_2, array_idx_1, level_2);
  }
  _ = texture_load_2;
  let level_3 = u32(level_idx);
  let level_clamped_3 = min(level_3, (textureNumLevels(tex_3d) - 1));
  let coords_3 = vec3<i32>(1, 2, 3);
  var texture_load_3 : vec4<f32>;
  if ((all((vec3<u32>(coords_3) < textureDimensions(tex_3d, level_clamped_3))) & (level_3 < textureNumLevels(tex_3d)))) {
    texture_load_3 = textureLoad(tex_3d, coords_3, level_3);
  }
  _ = texture_load_3;
  let coords_4 = vec2<i32>(1, 2);
  var texture_load_4 : vec4<f32>;
  if (all((vec2<u32>(coords_4) < textureDimensions(tex_ms_2d)))) {
    texture_load_4 = textureLoad(tex_ms_2d, coords_4, sample_idx);
  }
  _ = texture_load_4;
  let level_4 = u32(level_idx);
  let level_clamped_4 = min(level_4, (textureNumLevels(tex_depth_2d) - 1));
  let coords_5 = vec2<i32>(1, 2);
  var texture_load_5 : f32;
  if ((all((vec2<u32>(coords_5) < textureDimensions(tex_depth_2d, level_clamped_4))) & (level_4 < textureNumLevels(tex_depth_2d)))) {
    texture_load_5 = textureLoad(tex_depth_2d, coords_5, level_4);
  }
  _ = texture_load_5;
  let level_5 = u32(level_idx);
  let level_clamped_5 = min(level_5, (textureNumLevels(tex_depth_2d_arr) - 1));
  let coords_6 = vec2<i32>(1, 2);
  let array_idx_2 = array_idx;
  var texture_load_6 : f32;
  if (((all((vec2<u32>(coords_6) < textureDimensions(tex_depth_2d_arr, level_clamped_5))) & (level_5 < textureNumLevels(tex_depth_2d_arr))) & (u32(array_idx_2) < textureNumLayers(tex_depth_2d_arr)))) {
    texture_load_6 = textureLoad(tex_depth_2d_arr, coords_6, array_idx_2, level_5);
  }
  _ = texture_load_6;
  let coords_7 = vec2<i32>(1, 2);
  var texture_load_7 : vec4<f32>;
  if (all((vec2<u32>(coords_7) < textureDimensions(tex_external)))) {
    texture_load_7 = textureLoad(tex_external, coords_7);
  }
  _ = texture_load_7;
}

fn idx_unsigned() {
  var array_idx : u32;
  var level_idx : u32;
  var sample_idx : u32;
  let level_6 = u32(level_idx);
  let level_clamped_6 = min(level_6, (textureNumLevels(tex_1d) - 1));
  let coords_8 = 1u;
  var texture_load_8 : vec4<f32>;
  if ((all((u32(coords_8) < textureDimensions(tex_1d, level_clamped_6))) & (level_6 < textureNumLevels(tex_1d)))) {
    texture_load_8 = textureLoad(tex_1d, coords_8, level_6);
  }
  _ = texture_load_8;
  let level_7 = u32(level_idx);
  let level_clamped_7 = min(level_7, (textureNumLevels(tex_2d) - 1));
  let coords_9 = vec2<u32>(1, 2);
  var texture_load_9 : vec4<f32>;
  if ((all((vec2<u32>(coords_9) < textureDimensions(tex_2d, level_clamped_7))) & (level_7 < textureNumLevels(tex_2d)))) {
    texture_load_9 = textureLoad(tex_2d, coords_9, level_7);
  }
  _ = texture_load_9;
  let level_8 = u32(level_idx);
  let level_clamped_8 = min(level_8, (textureNumLevels(tex_2d_arr) - 1));
  let coords_10 = vec2<u32>(1, 2);
  let array_idx_3 = array_idx;
  var texture_load_10 : vec4<f32>;
  if (((all((vec2<u32>(coords_10) < textureDimensions(tex_2d_arr, level_clamped_8))) & (level_8 < textureNumLevels(tex_2d_arr))) & (u32(array_idx_3) < textureNumLayers(tex_2d_arr)))) {
    texture_load_10 = textureLoad(tex_2d_arr, coords_10, array_idx_3, level_8);
  }
  _ = texture_load_10;
  let level_9 = u32(level_idx);
  let level_clamped_9 = min(level_9, (textureNumLevels(tex_3d) - 1));
  let coords_11 = vec3<u32>(1, 2, 3);
  var texture_load_11 : vec4<f32>;
  if ((all((vec3<u32>(coords_11) < textureDimensions(tex_3d, level_clamped_9))) & (level_9 < textureNumLevels(tex_3d)))) {
    texture_load_11 = textureLoad(tex_3d, coords_11, level_9);
  }
  _ = texture_load_11;
  let coords_12 = vec2<u32>(1, 2);
  var texture_load_12 : vec4<f32>;
  if (all((vec2<u32>(coords_12) < textureDimensions(tex_ms_2d)))) {
    texture_load_12 = textureLoad(tex_ms_2d, coords_12, sample_idx);
  }
  _ = texture_load_12;
  let level_10 = u32(level_idx);
  let level_clamped_10 = min(level_10, (textureNumLevels(tex_depth_2d) - 1));
  let coords_13 = vec2<u32>(1, 2);
  var texture_load_13 : f32;
  if ((all((vec2<u32>(coords_13) < textureDimensions(tex_depth_2d, level_clamped_10))) & (level_10 < textureNumLevels(tex_depth_2d)))) {
    texture_load_13 = textureLoad(tex_depth_2d, coords_13, level_10);
  }
  _ = texture_load_13;
  let level_11 = u32(level_idx);
  let level_clamped_11 = min(level_11, (textureNumLevels(tex_depth_2d_arr) - 1));
  let coords_14 = vec2<u32>(1, 2);
  let array_idx_4 = array_idx;
  var texture_load_14 : f32;
  if (((all((vec2<u32>(coords_14) < textureDimensions(tex_depth_2d_arr, level_clamped_11))) & (level_11 < textureNumLevels(tex_depth_2d_arr))) & (u32(array_idx_4) < textureNumLayers(tex_depth_2d_arr)))) {
    texture_load_14 = textureLoad(tex_depth_2d_arr, coords_14, array_idx_4, level_11);
  }
  _ = texture_load_14;
  let coords_15 = vec2<u32>(1, 2);
  var texture_load_15 : vec4<f32>;
  if (all((vec2<u32>(coords_15) < textureDimensions(tex_external)))) {
    texture_load_15 = textureLoad(tex_external, coords_15);
  }
  _ = texture_load_15;
}
)");

    auto got = Run<Robustness>(src, Config(GetParam()));

    EXPECT_EQ(expect, str(got));
}

// Clamp textureStore() coord, array_index and level values
TEST_P(RobustnessTest, TextureStore) {
    auto* src = R"(
@group(0) @binding(0) var tex1d : texture_storage_1d<rgba8sint, write>;

@group(0) @binding(1) var tex2d : texture_storage_2d<rgba8sint, write>;

@group(0) @binding(2) var tex2d_arr : texture_storage_2d_array<rgba8sint, write>;

@group(0) @binding(3) var tex3d : texture_storage_3d<rgba8sint, write>;

fn idx_signed() {
  textureStore(tex1d, 10i, vec4<i32>());
  textureStore(tex2d, vec2<i32>(10, 20), vec4<i32>());
  textureStore(tex2d_arr, vec2<i32>(10, 20), 50i, vec4<i32>());
  textureStore(tex3d, vec3<i32>(10, 20, 30), vec4<i32>());
}

fn idx_unsigned() {
  textureStore(tex1d, 10u, vec4<i32>());
  textureStore(tex2d, vec2<u32>(10, 20), vec4<i32>());
  textureStore(tex2d_arr, vec2<u32>(10, 20), 50u, vec4<i32>());
  textureStore(tex3d, vec3<u32>(10, 20, 30), vec4<i32>());
}
)";

    auto* expect = Expect(GetParam(),
                          /* ignore */ src,
                          /* clamp */ R"(
@group(0) @binding(0) var tex1d : texture_storage_1d<rgba8sint, write>;

@group(0) @binding(1) var tex2d : texture_storage_2d<rgba8sint, write>;

@group(0) @binding(2) var tex2d_arr : texture_storage_2d_array<rgba8sint, write>;

@group(0) @binding(3) var tex3d : texture_storage_3d<rgba8sint, write>;

fn idx_signed() {
  textureStore(tex1d, clamp(10i, 0, i32((textureDimensions(tex1d) - 1))), vec4<i32>());
  textureStore(tex2d, clamp(vec2<i32>(10, 20), vec2(0), vec2<i32>((textureDimensions(tex2d) - vec2(1)))), vec4<i32>());
  textureStore(tex2d_arr, clamp(vec2<i32>(10, 20), vec2(0), vec2<i32>((textureDimensions(tex2d_arr) - vec2(1)))), clamp(50i, 0, i32((textureNumLayers(tex2d_arr) - 1))), vec4<i32>());
  textureStore(tex3d, clamp(vec3<i32>(10, 20, 30), vec3(0), vec3<i32>((textureDimensions(tex3d) - vec3(1)))), vec4<i32>());
}

fn idx_unsigned() {
  textureStore(tex1d, min(10u, (textureDimensions(tex1d) - 1)), vec4<i32>());
  textureStore(tex2d, min(vec2<u32>(10, 20), (textureDimensions(tex2d) - vec2(1))), vec4<i32>());
  textureStore(tex2d_arr, min(vec2<u32>(10, 20), (textureDimensions(tex2d_arr) - vec2(1))), min(50u, (textureNumLayers(tex2d_arr) - 1)), vec4<i32>());
  textureStore(tex3d, min(vec3<u32>(10, 20, 30), (textureDimensions(tex3d) - vec3(1))), vec4<i32>());
}
)",
                          /* predicate */ R"(
@group(0) @binding(0) var tex1d : texture_storage_1d<rgba8sint, write>;

@group(0) @binding(1) var tex2d : texture_storage_2d<rgba8sint, write>;

@group(0) @binding(2) var tex2d_arr : texture_storage_2d_array<rgba8sint, write>;

@group(0) @binding(3) var tex3d : texture_storage_3d<rgba8sint, write>;

fn idx_signed() {
  let coords = 10i;
  if (all((u32(coords) < textureDimensions(tex1d)))) {
    textureStore(tex1d, coords, vec4<i32>());
  }
  let coords_1 = vec2<i32>(10, 20);
  if (all((vec2<u32>(coords_1) < textureDimensions(tex2d)))) {
    textureStore(tex2d, coords_1, vec4<i32>());
  }
  let coords_2 = vec2<i32>(10, 20);
  let array_idx = 50i;
  if ((all((vec2<u32>(coords_2) < textureDimensions(tex2d_arr))) & (u32(array_idx) < textureNumLayers(tex2d_arr)))) {
    textureStore(tex2d_arr, coords_2, array_idx, vec4<i32>());
  }
  let coords_3 = vec3<i32>(10, 20, 30);
  if (all((vec3<u32>(coords_3) < textureDimensions(tex3d)))) {
    textureStore(tex3d, coords_3, vec4<i32>());
  }
}

fn idx_unsigned() {
  let coords_4 = 10u;
  if (all((u32(coords_4) < textureDimensions(tex1d)))) {
    textureStore(tex1d, coords_4, vec4<i32>());
  }
  let coords_5 = vec2<u32>(10, 20);
  if (all((vec2<u32>(coords_5) < textureDimensions(tex2d)))) {
    textureStore(tex2d, coords_5, vec4<i32>());
  }
  let coords_6 = vec2<u32>(10, 20);
  let array_idx_1 = 50u;
  if ((all((vec2<u32>(coords_6) < textureDimensions(tex2d_arr))) & (u32(array_idx_1) < textureNumLayers(tex2d_arr)))) {
    textureStore(tex2d_arr, coords_6, array_idx_1, vec4<i32>());
  }
  let coords_7 = vec3<u32>(10, 20, 30);
  if (all((vec3<u32>(coords_7) < textureDimensions(tex3d)))) {
    textureStore(tex3d, coords_7, vec4<i32>());
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
    let predicate = (u32(index) < 4u);
    var predicated_load : f32;
    if (predicate) {
      predicated_load = a[index];
    }
    var b : f32 = predicated_load;
  }
  let index_1 = i;
  let predicate_1 = (u32(index_1) < 2u);
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
  let predicate = (u32(index) < (arrayLength(&(s.b)) - 1u));
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
