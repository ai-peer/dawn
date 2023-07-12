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

#include "src/tint/ast/transform/texture_builtins_from_uniform.h"

#include <utility>

#include "src/tint/ast/transform/simplify_pointers.h"
#include "src/tint/ast/transform/test_helper.h"
#include "src/tint/ast/transform/unshadow.h"

namespace tint::ast::transform {
namespace {

using TextureBuiltinsFromUniformTest = TransformTest;

TEST_F(TextureBuiltinsFromUniformTest, ShouldRunEmptyModule) {
    auto* src = R"()";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});
    cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 0}, 0);

    Transform::DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    EXPECT_FALSE(ShouldRun<TextureBuiltinsFromUniform>(src, data));
}

TEST_F(TextureBuiltinsFromUniformTest, ShouldRunNoTextureNumLevels) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});
    cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 0}, 0);

    Transform::DataMap data;
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
    cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 0}, 0);

    Transform::DataMap data;
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

TEST_F(TextureBuiltinsFromUniformTest, Basic) {
    auto* src = R"(
@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = textureNumLevels(t);
}
)";

    auto* expect = R"(
struct tint_symbol {
  texture_num_levels : array<vec4<u32>, 1u>,
}

@group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = tint_symbol_1.texture_num_levels[0u][0u];
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});
    cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 0}, 0);

    Transform::DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    auto got = Run<Unshadow, TextureBuiltinsFromUniform>(src, data);

    EXPECT_EQ(expect, str(got));
    auto* val = got.data.Get<TextureBuiltinsFromUniform::Result>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(std::unordered_set<uint32_t>({0}), val->used_size_indices);
}

TEST_F(TextureBuiltinsFromUniformTest, Test) {
    auto* src = R"(
struct tint_symbol {
  texture_num_samples : array<vec4<u32>, 1u>,
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
  texture_num_samples : array<vec4<u32>, 1u>,
  texture_num_levels : array<vec4<u32>, 1u>,
}

struct tint_symbol {
  texture_num_samples : array<vec4<u32>, 1u>,
}

@group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol_2;

@group(0) @binding(0) var t : texture_2d<f32>;

@compute @workgroup_size(1)
fn main() {
  var len : u32 = tint_symbol_1.texture_num_levels[0u][0u];
}
)";

    TextureBuiltinsFromUniform::Config cfg({0, 30u});
    cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 0}, 0);

    Transform::DataMap data;
    data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

    auto got = Run<Unshadow, TextureBuiltinsFromUniform>(src, data);

    EXPECT_EQ(expect, str(got));
    auto* val = got.data.Get<TextureBuiltinsFromUniform::Result>();
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(std::unordered_set<uint32_t>({0}), val->used_size_indices);
}

// TEST_F(TextureBuiltinsFromUniformTest, BasicInStruct) {
//     auto* src = R"(
// struct SB {
//   x : i32,
//   arr : array<i32>,
// };

// @group(0) @binding(0) var<storage, read> sb : SB;

// @compute @workgroup_size(1)
// fn main() {
//   var len : u32 = arrayLength(&sb.arr);
// }
// )";

//     auto* expect = R"(
// struct tint_symbol {
//   buffer_size : array<vec4<u32>, 1u>,
// }

// @group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

// struct SB {
//   x : i32,
//   arr : array<i32>,
// }

// @group(0) @binding(0) var<storage, read> sb : SB;

// @compute @workgroup_size(1)
// fn main() {
//   var len : u32 = ((tint_symbol_1.buffer_size[0u][0u] - 4u) / 4u);
// }
// )";

//     TextureBuiltinsFromUniform::Config cfg({0, 30u});
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 0}, 0);

//     Transform::DataMap data;
//     data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

//     auto got = Run<Unshadow, SimplifyPointers, TextureBuiltinsFromUniform>(src, data);

//     EXPECT_EQ(expect, str(got));
//     EXPECT_EQ(std::unordered_set<uint32_t>({0}),
//               got.data.Get<TextureBuiltinsFromUniform::Result>()->used_size_indices);
// }

// TEST_F(TextureBuiltinsFromUniformTest, MultipleStorageBuffers) {
//     auto* src = R"(
// struct SB1 {
//   x : i32,
//   arr1 : array<i32>,
// };
// struct SB2 {
//   x : i32,
//   arr2 : array<vec4<f32>>,
// };
// struct SB4 {
//   x : i32,
//   arr4 : array<vec4<f32>>,
// };

// @group(0) @binding(2) var<storage, read> sb1 : SB1;
// @group(1) @binding(2) var<storage, read> sb2 : SB2;
// @group(2) @binding(2) var<storage, read> sb3 : array<vec4<f32>>;
// @group(3) @binding(2) var<storage, read> sb4 : SB4;
// @group(4) @binding(2) var<storage, read> sb5 : array<vec4<f32>>;

// @compute @workgroup_size(1)
// fn main() {
//   var len1 : u32 = arrayLength(&(sb1.arr1));
//   var len2 : u32 = arrayLength(&(sb2.arr2));
//   var len3 : u32 = arrayLength(&sb3);
//   var len4 : u32 = arrayLength(&(sb4.arr4));
//   var len5 : u32 = arrayLength(&sb5);
//   var x : u32 = (len1 + len2 + len3 + len4 + len5);
// }
// )";

//     auto* expect = R"(
// struct tint_symbol {
//   buffer_size : array<vec4<u32>, 2u>,
// }

// @group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

// struct SB1 {
//   x : i32,
//   arr1 : array<i32>,
// }

// struct SB2 {
//   x : i32,
//   arr2 : array<vec4<f32>>,
// }

// struct SB4 {
//   x : i32,
//   arr4 : array<vec4<f32>>,
// }

// @group(0) @binding(2) var<storage, read> sb1 : SB1;

// @group(1) @binding(2) var<storage, read> sb2 : SB2;

// @group(2) @binding(2) var<storage, read> sb3 : array<vec4<f32>>;

// @group(3) @binding(2) var<storage, read> sb4 : SB4;

// @group(4) @binding(2) var<storage, read> sb5 : array<vec4<f32>>;

// @compute @workgroup_size(1)
// fn main() {
//   var len1 : u32 = ((tint_symbol_1.buffer_size[0u][0u] - 4u) / 4u);
//   var len2 : u32 = ((tint_symbol_1.buffer_size[0u][1u] - 16u) / 16u);
//   var len3 : u32 = (tint_symbol_1.buffer_size[0u][2u] / 16u);
//   var len4 : u32 = ((tint_symbol_1.buffer_size[0u][3u] - 16u) / 16u);
//   var len5 : u32 = (tint_symbol_1.buffer_size[1u][0u] / 16u);
//   var x : u32 = ((((len1 + len2) + len3) + len4) + len5);
// }
// )";

//     TextureBuiltinsFromUniform::Config cfg({0, 30u});
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 2u}, 0);
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{1u, 2u}, 1);
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{2u, 2u}, 2);
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{3u, 2u}, 3);
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{4u, 2u}, 4);

//     Transform::DataMap data;
//     data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

//     auto got = Run<Unshadow, SimplifyPointers, TextureBuiltinsFromUniform>(src, data);

//     EXPECT_EQ(expect, str(got));
//     EXPECT_EQ(std::unordered_set<uint32_t>({0, 1, 2, 3, 4}),
//               got.data.Get<TextureBuiltinsFromUniform::Result>()->used_size_indices);
// }

// TEST_F(TextureBuiltinsFromUniformTest, MultipleUnusedStorageBuffers) {
//     auto* src = R"(
// struct SB1 {
//   x : i32,
//   arr1 : array<i32>,
// };
// struct SB2 {
//   x : i32,
//   arr2 : array<vec4<f32>>,
// };
// struct SB4 {
//   x : i32,
//   arr4 : array<vec4<f32>>,
// };

// @group(0) @binding(2) var<storage, read> sb1 : SB1;
// @group(1) @binding(2) var<storage, read> sb2 : SB2;
// @group(2) @binding(2) var<storage, read> sb3 : array<vec4<f32>>;
// @group(3) @binding(2) var<storage, read> sb4 : SB4;
// @group(4) @binding(2) var<storage, read> sb5 : array<vec4<f32>>;

// @compute @workgroup_size(1)
// fn main() {
//   var len1 : u32 = arrayLength(&(sb1.arr1));
//   var len3 : u32 = arrayLength(&sb3);
//   var x : u32 = (len1 + len3);
// }
// )";

//     auto* expect = R"(
// struct tint_symbol {
//   buffer_size : array<vec4<u32>, 1u>,
// }

// @group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

// struct SB1 {
//   x : i32,
//   arr1 : array<i32>,
// }

// struct SB2 {
//   x : i32,
//   arr2 : array<vec4<f32>>,
// }

// struct SB4 {
//   x : i32,
//   arr4 : array<vec4<f32>>,
// }

// @group(0) @binding(2) var<storage, read> sb1 : SB1;

// @group(1) @binding(2) var<storage, read> sb2 : SB2;

// @group(2) @binding(2) var<storage, read> sb3 : array<vec4<f32>>;

// @group(3) @binding(2) var<storage, read> sb4 : SB4;

// @group(4) @binding(2) var<storage, read> sb5 : array<vec4<f32>>;

// @compute @workgroup_size(1)
// fn main() {
//   var len1 : u32 = ((tint_symbol_1.buffer_size[0u][0u] - 4u) / 4u);
//   var len3 : u32 = (tint_symbol_1.buffer_size[0u][2u] / 16u);
//   var x : u32 = (len1 + len3);
// }
// )";

//     TextureBuiltinsFromUniform::Config cfg({0, 30u});
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 2u}, 0);
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{1u, 2u}, 1);
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{2u, 2u}, 2);
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{3u, 2u}, 3);
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{4u, 2u}, 4);

//     Transform::DataMap data;
//     data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

//     auto got = Run<Unshadow, SimplifyPointers, TextureBuiltinsFromUniform>(src, data);

//     EXPECT_EQ(expect, str(got));
//     EXPECT_EQ(std::unordered_set<uint32_t>({0, 2}),
//               got.data.Get<TextureBuiltinsFromUniform::Result>()->used_size_indices);
// }

// TEST_F(TextureBuiltinsFromUniformTest, NoArrayLengthCalls) {
//     auto* src = R"(
// struct SB {
//   x : i32,
//   arr : array<i32>,
// }

// @group(0) @binding(0) var<storage, read> sb : SB;

// @compute @workgroup_size(1)
// fn main() {
//   _ = &(sb.arr);
// }
// )";

//     TextureBuiltinsFromUniform::Config cfg({0, 30u});
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 0}, 0);

//     Transform::DataMap data;
//     data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

//     auto got = Run<Unshadow, SimplifyPointers, TextureBuiltinsFromUniform>(src, data);

//     EXPECT_EQ(src, str(got));
//     EXPECT_EQ(got.data.Get<TextureBuiltinsFromUniform::Result>(), nullptr);
// }

// TEST_F(TextureBuiltinsFromUniformTest, MissingBindingPointToIndexMapping) {
//     auto* src = R"(
// struct SB1 {
//   x : i32,
//   arr1 : array<i32>,
// };

// struct SB2 {
//   x : i32,
//   arr2 : array<vec4<f32>>,
// };

// @group(0) @binding(2) var<storage, read> sb1 : SB1;

// @group(1) @binding(2) var<storage, read> sb2 : SB2;

// @compute @workgroup_size(1)
// fn main() {
//   var len1 : u32 = arrayLength(&(sb1.arr1));
//   var len2 : u32 = arrayLength(&(sb2.arr2));
//   var x : u32 = (len1 + len2);
// }
// )";

//     auto* expect = R"(
// struct tint_symbol {
//   buffer_size : array<vec4<u32>, 1u>,
// }

// @group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

// struct SB1 {
//   x : i32,
//   arr1 : array<i32>,
// }

// struct SB2 {
//   x : i32,
//   arr2 : array<vec4<f32>>,
// }

// @group(0) @binding(2) var<storage, read> sb1 : SB1;

// @group(1) @binding(2) var<storage, read> sb2 : SB2;

// @compute @workgroup_size(1)
// fn main() {
//   var len1 : u32 = ((tint_symbol_1.buffer_size[0u][0u] - 4u) / 4u);
//   var len2 : u32 = arrayLength(&(sb2.arr2));
//   var x : u32 = (len1 + len2);
// }
// )";

//     TextureBuiltinsFromUniform::Config cfg({0, 30u});
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 2}, 0);

//     Transform::DataMap data;
//     data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

//     auto got = Run<Unshadow, SimplifyPointers, TextureBuiltinsFromUniform>(src, data);

//     EXPECT_EQ(expect, str(got));
//     EXPECT_EQ(std::unordered_set<uint32_t>({0}),
//               got.data.Get<TextureBuiltinsFromUniform::Result>()->used_size_indices);
// }

// TEST_F(TextureBuiltinsFromUniformTest, OutOfOrder) {
//     auto* src = R"(
// @compute @workgroup_size(1)
// fn main() {
//   var len : u32 = arrayLength(&sb.arr);
// }

// @group(0) @binding(0) var<storage, read> sb : SB;

// struct SB {
//   x : i32,
//   arr : array<i32>,
// };
// )";

//     auto* expect = R"(
// struct tint_symbol {
//   buffer_size : array<vec4<u32>, 1u>,
// }

// @group(0) @binding(30) var<uniform> tint_symbol_1 : tint_symbol;

// @compute @workgroup_size(1)
// fn main() {
//   var len : u32 = ((tint_symbol_1.buffer_size[0u][0u] - 4u) / 4u);
// }

// @group(0) @binding(0) var<storage, read> sb : SB;

// struct SB {
//   x : i32,
//   arr : array<i32>,
// }
// )";

//     TextureBuiltinsFromUniform::Config cfg({0, 30u});
//     cfg.bindpoint_to_size_index.emplace(sem::BindingPoint{0, 0}, 0);

//     Transform::DataMap data;
//     data.Add<TextureBuiltinsFromUniform::Config>(std::move(cfg));

//     auto got = Run<Unshadow, SimplifyPointers, TextureBuiltinsFromUniform>(src, data);

//     EXPECT_EQ(expect, str(got));
//     EXPECT_EQ(std::unordered_set<uint32_t>({0}),
//               got.data.Get<TextureBuiltinsFromUniform::Result>()->used_size_indices);
// }

}  // namespace
}  // namespace tint::ast::transform
