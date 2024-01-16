// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/wgsl/ast/transform/offset_first_index.h"

#include <memory>
#include <utility>
#include <vector>

#include "src/tint/lang/wgsl/ast/transform/helper_test.h"

namespace tint::ast::transform {
namespace {

using OffsetFirstIndexTest = TransformTest;

TEST_F(OffsetFirstIndexTest, ShouldRunEmptyModule) {
    auto* src = R"()";

    EXPECT_FALSE(ShouldRun<OffsetFirstIndex>(src));
}

TEST_F(OffsetFirstIndexTest, ShouldRunFragmentStage) {
    auto* src = R"(
@fragment
fn entry() {
  return;
}
)";

    EXPECT_FALSE(ShouldRun<OffsetFirstIndex>(src));
}

TEST_F(OffsetFirstIndexTest, ShouldRunVertexStage) {
    auto* src = R"(
@vertex
fn entry() -> @builtin(position) vec4<f32> {
  return vec4<f32>();
}
)";

    EXPECT_TRUE(ShouldRun<OffsetFirstIndex>(src));
}

TEST_F(OffsetFirstIndexTest, EmptyModule) {
    auto* src = "";
    auto* expect = "";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(0, 0);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    EXPECT_EQ(data, nullptr);
}

TEST_F(OffsetFirstIndexTest, BasicVertexShader) {
    auto* src = R"(
@vertex
fn entry() -> @builtin(position) vec4<f32> {
  return vec4<f32>();
}
)";
    auto* expect = R"(
enable chromium_experimental_push_constant;

@vertex
fn entry() -> @builtin(position) vec4<f32> {
  return vec4<f32>();
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(0, 0);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, false);
    EXPECT_EQ(data->has_instance_index, false);
}

TEST_F(OffsetFirstIndexTest, BasicModuleVertexIndex) {
    auto* src = R"(
fn test(vert_idx : u32) -> u32 {
  return vert_idx;
}

@vertex
fn entry(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  test(vert_idx);
  return vec4<f32>();
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(1u) var<push_constant> tint_first_vertex : u32;

fn test(vert_idx : u32) -> u32 {
  return vert_idx;
}

@vertex
fn entry(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  test((vert_idx + tint_first_vertex));
  return vec4<f32>();
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 2);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, true);
    EXPECT_EQ(data->has_instance_index, false);
}

TEST_F(OffsetFirstIndexTest, BasicModuleVertexIndex_OutOfOrder) {
    auto* src = R"(
@vertex
fn entry(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  test(vert_idx);
  return vec4<f32>();
}

fn test(vert_idx : u32) -> u32 {
  return vert_idx;
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(1u) var<push_constant> tint_first_vertex : u32;

@vertex
fn entry(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  test((vert_idx + tint_first_vertex));
  return vec4<f32>();
}

fn test(vert_idx : u32) -> u32 {
  return vert_idx;
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 2);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, true);
    EXPECT_EQ(data->has_instance_index, false);
}

TEST_F(OffsetFirstIndexTest, BasicModuleInstanceIndex) {
    auto* src = R"(
fn test(inst_idx : u32) -> u32 {
  return inst_idx;
}

@vertex
fn entry(@builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  test(inst_idx);
  return vec4<f32>();
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(7u) var<push_constant> tint_first_instance : u32;

fn test(inst_idx : u32) -> u32 {
  return inst_idx;
}

@vertex
fn entry(@builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  test((inst_idx + tint_first_instance));
  return vec4<f32>();
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 7);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, false);
    EXPECT_EQ(data->has_instance_index, true);
}

TEST_F(OffsetFirstIndexTest, BasicModuleInstanceIndex_OutOfOrder) {
    auto* src = R"(
@vertex
fn entry(@builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  test(inst_idx);
  return vec4<f32>();
}

fn test(inst_idx : u32) -> u32 {
  return inst_idx;
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(7u) var<push_constant> tint_first_instance : u32;

@vertex
fn entry(@builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  test((inst_idx + tint_first_instance));
  return vec4<f32>();
}

fn test(inst_idx : u32) -> u32 {
  return inst_idx;
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 7);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, false);
    EXPECT_EQ(data->has_instance_index, true);
}

TEST_F(OffsetFirstIndexTest, BasicModuleBothIndex) {
    auto* src = R"(
fn test(instance_idx : u32, vert_idx : u32) -> u32 {
  return instance_idx + vert_idx;
}

struct Inputs {
  @builtin(instance_index) instance_idx : u32,
  @builtin(vertex_index) vert_idx : u32,
};

@vertex
fn entry(inputs : Inputs) -> @builtin(position) vec4<f32> {
  test(inputs.instance_idx, inputs.vert_idx);
  return vec4<f32>();
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(1u) var<push_constant> tint_first_vertex : u32;

@location(2u) var<push_constant> tint_first_instance : u32;

fn test(instance_idx : u32, vert_idx : u32) -> u32 {
  return (instance_idx + vert_idx);
}

struct Inputs {
  @builtin(instance_index)
  instance_idx : u32,
  @builtin(vertex_index)
  vert_idx : u32,
}

@vertex
fn entry(inputs : Inputs) -> @builtin(position) vec4<f32> {
  test((inputs.instance_idx + tint_first_instance), (inputs.vert_idx + tint_first_vertex));
  return vec4<f32>();
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 2);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, true);
    EXPECT_EQ(data->has_instance_index, true);
}

TEST_F(OffsetFirstIndexTest, BasicModuleBothIndex_OutOfOrder) {
    auto* src = R"(
@vertex
fn entry(inputs : Inputs) -> @builtin(position) vec4<f32> {
  test(inputs.instance_idx, inputs.vert_idx);
  return vec4<f32>();
}

struct Inputs {
  @builtin(instance_index) instance_idx : u32,
  @builtin(vertex_index) vert_idx : u32,
};

fn test(instance_idx : u32, vert_idx : u32) -> u32 {
  return instance_idx + vert_idx;
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(1u) var<push_constant> tint_first_vertex : u32;

@location(2u) var<push_constant> tint_first_instance : u32;

@vertex
fn entry(inputs : Inputs) -> @builtin(position) vec4<f32> {
  test((inputs.instance_idx + tint_first_instance), (inputs.vert_idx + tint_first_vertex));
  return vec4<f32>();
}

struct Inputs {
  @builtin(instance_index)
  instance_idx : u32,
  @builtin(vertex_index)
  vert_idx : u32,
}

fn test(instance_idx : u32, vert_idx : u32) -> u32 {
  return (instance_idx + vert_idx);
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 2);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, true);
    EXPECT_EQ(data->has_instance_index, true);
}

TEST_F(OffsetFirstIndexTest, NestedCalls) {
    auto* src = R"(
fn func1(vert_idx : u32) -> u32 {
  return vert_idx;
}

fn func2(vert_idx : u32) -> u32 {
  return func1(vert_idx);
}

@vertex
fn entry(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  func2(vert_idx);
  return vec4<f32>();
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(1u) var<push_constant> tint_first_vertex : u32;

fn func1(vert_idx : u32) -> u32 {
  return vert_idx;
}

fn func2(vert_idx : u32) -> u32 {
  return func1(vert_idx);
}

@vertex
fn entry(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  func2((vert_idx + tint_first_vertex));
  return vec4<f32>();
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 2);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, true);
    EXPECT_EQ(data->has_instance_index, false);
}

TEST_F(OffsetFirstIndexTest, NestedCalls_OutOfOrder) {
    auto* src = R"(
@vertex
fn entry(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  func2(vert_idx);
  return vec4<f32>();
}

fn func2(vert_idx : u32) -> u32 {
  return func1(vert_idx);
}

fn func1(vert_idx : u32) -> u32 {
  return vert_idx;
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(1u) var<push_constant> tint_first_vertex : u32;

@vertex
fn entry(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  func2((vert_idx + tint_first_vertex));
  return vec4<f32>();
}

fn func2(vert_idx : u32) -> u32 {
  return func1(vert_idx);
}

fn func1(vert_idx : u32) -> u32 {
  return vert_idx;
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 2);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, true);
    EXPECT_EQ(data->has_instance_index, false);
}

TEST_F(OffsetFirstIndexTest, MultipleEntryPoints) {
    auto* src = R"(
fn func(i : u32) -> u32 {
  return i;
}

@vertex
fn entry_a(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  func(vert_idx);
  return vec4<f32>();
}

@vertex
fn entry_b(@builtin(vertex_index) vert_idx : u32, @builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  func(vert_idx + inst_idx);
  return vec4<f32>();
}

@vertex
fn entry_c(@builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  func(inst_idx);
  return vec4<f32>();
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(1u) var<push_constant> tint_first_vertex : u32;

@location(2u) var<push_constant> tint_first_instance : u32;

fn func(i : u32) -> u32 {
  return i;
}

@vertex
fn entry_a(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  func((vert_idx + tint_first_vertex));
  return vec4<f32>();
}

@vertex
fn entry_b(@builtin(vertex_index) vert_idx : u32, @builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  func(((vert_idx + tint_first_vertex) + (inst_idx + tint_first_instance)));
  return vec4<f32>();
}

@vertex
fn entry_c(@builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  func((inst_idx + tint_first_instance));
  return vec4<f32>();
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 2);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, true);
    EXPECT_EQ(data->has_instance_index, true);
}

TEST_F(OffsetFirstIndexTest, MultipleEntryPoints_OutOfOrder) {
    auto* src = R"(
@vertex
fn entry_a(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  func(vert_idx);
  return vec4<f32>();
}

@vertex
fn entry_b(@builtin(vertex_index) vert_idx : u32, @builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  func(vert_idx + inst_idx);
  return vec4<f32>();
}

@vertex
fn entry_c(@builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  func(inst_idx);
  return vec4<f32>();
}

fn func(i : u32) -> u32 {
  return i;
}
)";

    auto* expect = R"(
enable chromium_experimental_push_constant;

@location(1u) var<push_constant> tint_first_vertex : u32;

@location(2u) var<push_constant> tint_first_instance : u32;

@vertex
fn entry_a(@builtin(vertex_index) vert_idx : u32) -> @builtin(position) vec4<f32> {
  func((vert_idx + tint_first_vertex));
  return vec4<f32>();
}

@vertex
fn entry_b(@builtin(vertex_index) vert_idx : u32, @builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  func(((vert_idx + tint_first_vertex) + (inst_idx + tint_first_instance)));
  return vec4<f32>();
}

@vertex
fn entry_c(@builtin(instance_index) inst_idx : u32) -> @builtin(position) vec4<f32> {
  func((inst_idx + tint_first_instance));
  return vec4<f32>();
}

fn func(i : u32) -> u32 {
  return i;
}
)";

    DataMap config;
    config.Add<OffsetFirstIndex::Config>(1, 2);
    auto got = Run<OffsetFirstIndex>(src, std::move(config));

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<OffsetFirstIndex::Data>();

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->has_vertex_index, true);
    EXPECT_EQ(data->has_instance_index, true);
}

}  // namespace
}  // namespace tint::ast::transform
