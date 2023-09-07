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

#include <utility>

#include "src/tint/lang/wgsl/ast/transform/helper_test.h"

namespace tint::msl::writer {
namespace {

struct Binding {
    uint32_t field_index;
    uint32_t attachment_index;
};

ast::transform::DataMap Bindings(std::initializer_list<Binding> bindings) {
    PixelLocal::Config cfg;
    for (auto& binding : bindings) {
        cfg.attachments.Add(binding.field_index, binding.attachment_index);
    }
    ast::transform::DataMap data;
    data.Add<PixelLocal::Config>(std::move(cfg));
    return data;
}

using PixelLocalTest = ast::transform::TransformTest;

TEST_F(PixelLocalTest, EmptyModule) {
    auto* src = "";

    EXPECT_FALSE(ShouldRun<PixelLocal>(src, Bindings({})));
}

TEST_F(PixelLocalTest, Var) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : i32,
};

var<pixel_local> P : PixelLocal;
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  @internal(attachment(1)) @internal(disable_validation__entry_point_parameter)
  a : i32,
}

var<private> P : PixelLocal;
)";

    auto got = Run<PixelLocal>(src, Bindings({{0, 1}}));

    EXPECT_EQ(expect, str(got));
}

TEST_F(PixelLocalTest, AssignmentInEntryPoint) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
}

var<pixel_local> P : PixelLocal;

@fragment
fn F() {
  P.a = 42;
}
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

@fragment
fn F(pixel_local_1 : PixelLocal) -> PixelLocal {
  P = pixel_local_1;
  F_inner();
  return P;
}

struct PixelLocal {
  @internal(attachment(1)) @internal(disable_validation__entry_point_parameter)
  a : u32,
}

var<private> P : PixelLocal;

fn F_inner() {
  P.a = 42;
}
)";

    auto got = Run<PixelLocal>(src, Bindings({{0, 1}}));

    EXPECT_EQ(expect, str(got));
}

}  // namespace
}  // namespace tint::msl::writer
