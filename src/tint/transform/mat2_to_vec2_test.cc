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

#include "src/tint/transform/mat2_to_vec2.h"

// #include <memory>
// #include <utility>

#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

using Mat2ToVec2Test = TransformTest;

TEST_F(Mat2ToVec2Test, NoOp) {
    auto* src = "";
    auto* expect = "";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

TEST_F(Mat2ToVec2Test, Simple) {
    auto* src = R"(
struct U {
  m : mat2x2<f32>,
}
@group(0) @binding(0) var<uniform> u : U;
)";
    auto* expect = R"(
struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

struct U {
  m0 : vec2<f32>,
  m1 : vec2<f32>,
}

@group(0) @binding(0) var<uniform> u : U;
)";

    DataMap data;
    auto got = Run<Mat2ToVec2>(src, data);

    EXPECT_EQ(expect, str(got));
}

}
}
