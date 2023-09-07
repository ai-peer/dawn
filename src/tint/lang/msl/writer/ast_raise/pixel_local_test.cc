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

#include "src/tint/lang/msl/writer/ast_raise/pixel_local.h"

#include "src/tint/lang/wgsl/ast/transform/helper_test.h"

namespace tint::msl::writer {
namespace {

using PixelLocalTest = ast::transform::TransformTest;

TEST_F(PixelLocalTest, EmptyModule) {
    auto* src = "";

    EXPECT_FALSE(ShouldRun<PixelLocal>(src));
}

TEST_F(PixelLocalTest, DirectUse) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

var<pixel_local> P : u32;
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

var<private> P : u32;
)";

    auto got = Run<PixelLocal>(src);

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::msl::writer
