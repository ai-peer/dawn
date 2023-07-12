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

#include "src/tint/lang/wgsl/ast/transform/texture_builtins_from_uniform.h"

#include <utility>

#include "src/tint/lang/wgsl/ast/transform/simplify_pointers.h"
#include "src/tint/lang/wgsl/ast/transform/test_helper.h"
#include "src/tint/lang/wgsl/ast/transform/unshadow.h"

namespace tint::ast::transform {
namespace {

using TextureBuiltinsFromUniformTest = TransformTest;

TEST_F(TextureBuiltinsFromUniformTest, ShouldRunEmptyModule) {
    auto* src = R"()";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    EXPECT_FALSE(ShouldRun<TextureBuiltinsFromUniform>(src, data));
}

TEST_F(TextureBuiltinsFromUniformTest, ShouldRunNoTextureNumLevels) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  _ = textureDimensions(t);
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    EXPECT_FALSE(ShouldRun<TextureBuiltinsFromUniform>(src, data));
}

TEST_F(TextureBuiltinsFromUniformTest, ShouldRunWithTextureNumLevels) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = textureNumLevels(t);
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    EXPECT_TRUE(ShouldRun<TextureBuiltinsFromUniform>(src, data));
}

TEST_F(TextureBuiltinsFromUniformTest, Error_MissingTransformData) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = textureNumLevels(t);
}
)";

    auto* expect =
        "error: missing transform data for tint::ast::transform::TextureBuiltinsFromUniform";

    auto got = Run<Unshadow, SimplifyPointers, TextureBuiltinsFromUniform>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_F(TextureBuiltinsFromUniformTest, BasicTextureNumLevels) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = textureNumLevels(t);
}
)";

    auto* expect = R"(
struct tint_symbol {
  texture_builtin_values : array<vec4<u32>, 1u>,
}

@group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = tint_symbol_1.texture_builtin_values[0u][0u];
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    auto got = Run<Unshadow, TextureBuiltinsFromUniform>(src, data);

    EXPECT_EQ(expect, str(got));
    auto* val = got.data.Get<TextureBuiltinsFromUniform::Result>();
    ASSERT_NE(val, nullptr);
    // Note: Using the following direct EXPECT_EQ seems to cause compiler to hang.
    // EXPECT_EQ(
    //     TextureBuiltinsFromUniformOptions::BindingPointDataInfo{
    //         {BindgPoint{0u, 0u},
    //          std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 0u)}},
    //     val->bindpoint_to_data);
    EXPECT_EQ(1u, val->bindpoint_to_data.size());
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 0u),
              val->bindpoint_to_data.at(BindingPoint{0, 0}));
}

TEST_F(TextureBuiltinsFromUniformTest, BasicTextureNumSamples) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_multisampled_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var samples : u32 = textureNumSamples(t);
}
)";

    auto* expect = R"(
struct tint_symbol {
  texture_builtin_values : array<vec4<u32>, 1u>,
}

@group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

@group(0) @binding(0) var t : texture_multisampled_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var samples : u32 = tint_symbol_1.texture_builtin_values[0u][0u];
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    auto got = Run<Unshadow, TextureBuiltinsFromUniform>(src, data);

    EXPECT_EQ(expect, str(got));
    auto* val = got.data.Get<TextureBuiltinsFromUniform::Result>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(1u, val->bindpoint_to_data.size());
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumSamples, 0u),
              val->bindpoint_to_data.at(BindingPoint{0, 0}));
}

TEST_F(TextureBuiltinsFromUniformTest, SameBuiltinCalledMultipleTimes) {
    auto* src = R"(
@group(0) @binding(0) var tex : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = textureNumLevels(tex);
  len = textureNumLevels(tex);
}
)";

    auto* expect = R"(
struct tint_symbol {
  texture_builtin_values : array<vec4<u32>, 1u>,
}

@group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

@group(0) @binding(0) var tex : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = tint_symbol_1.texture_builtin_values[0u][0u];
  len = tint_symbol_1.texture_builtin_values[0u][0u];
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    auto got = Run<Unshadow, TextureBuiltinsFromUniform>(src, data);

    EXPECT_EQ(expect, str(got));
    auto* val = got.data.Get<TextureBuiltinsFromUniform::Result>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(1u, val->bindpoint_to_data.size());
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 0u),
              val->bindpoint_to_data.at(BindingPoint{0, 0}));
}

TEST_F(TextureBuiltinsFromUniformTest, TextureAsFunctionParameter) {
    auto* src = R"(
@group(0) @binding(0) var tex : texture_2d<f32>;

fn f(t: texture_2d<f32>) -> u32 {
  return textureNumLevels(t);
}

@compute @workgroup_size(1)
fn main() {
  var len : u32 = f(tex);
  len = textureNumLevels(tex);
}
)";

    auto* expect = R"(
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    auto got = Run<Unshadow, TextureBuiltinsFromUniform>(src, data);

    EXPECT_EQ(expect, str(got));
    auto* val = got.data.Get<TextureBuiltinsFromUniform::Result>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(1u, val->bindpoint_to_data.size());
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 0u),
              val->bindpoint_to_data.at(BindingPoint{0, 0}));
}

TEST_F(TextureBuiltinsFromUniformTest, MultipleTextures) {
    auto* src = R"(
@group(0) @binding(0) var t0 : texture_2d<f32>;
@group(0) @binding(1) var t1 : texture_multisampled_2d<f32>;
@group(0) @binding(2) var t2 : texture_2d_array<f32>;
@group(0) @binding(3) var t3 : texture_cube<f32>;
@group(0) @binding(4) var t4 : texture_depth_2d;
@group(1) @binding(0) var t5 : texture_depth_multisampled_2d;

@compute @workgroup_size(1)
fn main() {
  _ = textureNumLevels(t0);
  _ = textureNumSamples(t1);
  _ = textureNumLevels(t2);
  _ = textureNumLevels(t3);
  _ = textureNumLevels(t4);
  _ = textureNumSamples(t5);
}
)";

    auto* expect = R"(
struct tint_symbol {
  texture_builtin_values : array<vec4<u32>, 2u>,
}

@group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

@group(0) @binding(0) var t0 : texture_2d<f32>;

@group(0) @binding(1) var t1 : texture_multisampled_2d<f32>;

@group(0) @binding(2) var t2 : texture_2d_array<f32>;

@group(0) @binding(3) var t3 : texture_cube<f32>;

@group(0) @binding(4) var t4 : texture_depth_2d;

@group(1) @binding(0) var t5 : texture_depth_multisampled_2d;

@compute @workgroup_size(1)
fn main() {
  _ = tint_symbol_1.texture_builtin_values[0u][0u];
  _ = tint_symbol_1.texture_builtin_values[0u][1u];
  _ = tint_symbol_1.texture_builtin_values[0u][2u];
  _ = tint_symbol_1.texture_builtin_values[0u][3u];
  _ = tint_symbol_1.texture_builtin_values[1u][0u];
  _ = tint_symbol_1.texture_builtin_values[1u][1u];
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    auto got = Run<Unshadow, TextureBuiltinsFromUniform>(src, data);

    EXPECT_EQ(expect, str(got));
    auto* val = got.data.Get<TextureBuiltinsFromUniform::Result>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(6u, val->bindpoint_to_data.size());
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 0u),
              val->bindpoint_to_data.at(BindingPoint{0, 0}));
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumSamples, 4u),
              val->bindpoint_to_data.at(BindingPoint{0, 1}));
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 8u),
              val->bindpoint_to_data.at(BindingPoint{0, 2}));
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 12u),
              val->bindpoint_to_data.at(BindingPoint{0, 3}));
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 16u),
              val->bindpoint_to_data.at(BindingPoint{0, 4}));
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumSamples, 20u),
              val->bindpoint_to_data.at(BindingPoint{1, 0}));
}

TEST_F(TextureBuiltinsFromUniformTest, BindingPointExist) {
    auto* src = R"(
struct tint_symbol {
  foo : array<vec4<u32>, 1u>,
}

@group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = textureNumLevels(t);
}
)";

    auto* expect = R"(
struct tint_symbol_2 {
  foo : array<vec4<u32>, 1u>,
  texture_builtin_values : array<vec4<u32>, 1u>,
}

@group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol_2;

@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = tint_symbol_1.texture_builtin_values[0u][0u];
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});

    DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    auto got = Run<Unshadow, TextureBuiltinsFromUniform>(src, data);

    EXPECT_EQ(expect, str(got));
    auto* val = got.data.Get<TextureBuiltinsFromUniform::Result>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(std::make_pair(TextureBuiltinsFromUniformOptions::DataType::TextureNumLevels, 0u),
              val->bindpoint_to_data.at(BindingPoint{0, 0}));
}

}  // namespace
}  // namespace tint::ast::transform
