// Copyright 2023 The Dawn & Tint Authors
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

#include <utility>

#include "src/tint/lang/hlsl/writer/ast_raise/pixel_local.h"

#include "src/tint/lang/wgsl/ast/transform/helper_test.h"

namespace tint::hlsl::writer {

struct Binding {
    uint32_t field_index;
    uint32_t register_index;
};

ast::transform::DataMap Bindings(std::initializer_list<Binding> bindings, uint32_t groupIndex) {
    PixelLocal::Config cfg;
    cfg.rovGroupIndex = groupIndex;
    for (auto& binding : bindings) {
        cfg.pixelLocalStructureMemberIndexToROVRegister.Add(binding.field_index,
                                                            binding.register_index);
    }
    ast::transform::DataMap data;
    data.Add<PixelLocal::Config>(std::move(cfg));
    return data;
}

using HLSLPixelLocalTest = ast::transform::TransformTest;

TEST_F(HLSLPixelLocalTest, UseInEntryPoint_NoPosition) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
}

var<pixel_local> P : PixelLocal;

@fragment
fn F() -> @location(0) vec4f {
  P.a += 42;
  return vec4f(1, 0, 0, 1);
}
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

@binding(1) @group(0) @internal(rov) var pixel_local_a : texture_storage_2d<r32uint, read_write>;

fn load_from_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  P.a = textureLoad(pixel_local_a, rov_texcoord_0).x;
}

fn store_into_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  textureStore(pixel_local_a, rov_texcoord_0, vec4u(P.a));
}

struct F_res {
  @location(0)
  output_0 : vec4<f32>,
}

@fragment
fn F(@builtin(position) my_pos : vec4<f32>) -> F_res {
  let hlsl_sv_position = my_pos;
  load_from_pixel_local_storage(hlsl_sv_position);
  let result = F_inner();
  store_into_pixel_local_storage(hlsl_sv_position);
  return F_res(result);
}

struct PixelLocal {
  a : u32,
}

var<private> P : PixelLocal;

fn F_inner() -> vec4f {
  P.a += 42;
  return vec4f(1, 0, 0, 1);
}
)";

    auto got = Run<PixelLocal>(src, Bindings({{0, 1}}, 0));
    EXPECT_EQ(expect, str(got));
}

TEST_F(HLSLPixelLocalTest, UseInEntryPoint_SeparatePosition) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
}

var<pixel_local> P : PixelLocal;

@fragment
fn F(@builtin(position) pos : vec4f) -> @location(0) vec4f {
  P.a += 42;
  return pos;
}
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

@binding(1) @group(0) @internal(rov) var pixel_local_a : texture_storage_2d<r32uint, read_write>;

fn load_from_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  P.a = textureLoad(pixel_local_a, rov_texcoord_0).x;
}

fn store_into_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  textureStore(pixel_local_a, rov_texcoord_0, vec4u(P.a));
}

struct F_res {
  @location(0)
  output_0 : vec4<f32>,
}

@fragment
fn F(@builtin(position) pos : vec4f) -> F_res {
  let hlsl_sv_position = pos;
  load_from_pixel_local_storage(hlsl_sv_position);
  let result = F_inner(pos);
  store_into_pixel_local_storage(hlsl_sv_position);
  return F_res(result);
}

struct PixelLocal {
  a : u32,
}

var<private> P : PixelLocal;

fn F_inner(pos : vec4f) -> vec4f {
  P.a += 42;
  return pos;
}
)";

    auto got = Run<PixelLocal>(src, Bindings({{0, 1}}, 0));
    EXPECT_EQ(expect, str(got));
}

TEST_F(HLSLPixelLocalTest, UseInEntryPoint_PositionInStruct) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
}

var<pixel_local> P : PixelLocal;

struct FragmentInput {
  @location(0) input : vec4f,
  @builtin(position) pos : vec4f,
}

@fragment
fn F(fragmentInput : FragmentInput) -> @location(0) vec4f {
  P.a += 42;
  return fragmentInput.input + fragmentInput.pos;
}
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

@binding(1) @group(0) @internal(rov) var pixel_local_a : texture_storage_2d<r32uint, read_write>;

fn load_from_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  P.a = textureLoad(pixel_local_a, rov_texcoord_0).x;
}

fn store_into_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  textureStore(pixel_local_a, rov_texcoord_0, vec4u(P.a));
}

struct F_res {
  @location(0)
  output_0 : vec4<f32>,
}

@fragment
fn F(fragmentInput : FragmentInput) -> F_res {
  let hlsl_sv_position = fragmentInput.pos;
  load_from_pixel_local_storage(hlsl_sv_position);
  let result = F_inner(fragmentInput);
  store_into_pixel_local_storage(hlsl_sv_position);
  return F_res(result);
}

struct PixelLocal {
  a : u32,
}

var<private> P : PixelLocal;

struct FragmentInput {
  @location(0)
  input : vec4f,
  @builtin(position)
  pos : vec4f,
}

fn F_inner(fragmentInput : FragmentInput) -> vec4f {
  P.a += 42;
  return (fragmentInput.input + fragmentInput.pos);
}
)";

    auto got = Run<PixelLocal>(src, Bindings({{0, 1}}, 0));
    EXPECT_EQ(expect, str(got));
}

TEST_F(HLSLPixelLocalTest, UseInEntryPoint_NonZeroROVGroupIndex) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
}

var<pixel_local> P : PixelLocal;

struct FragmentInput {
  @location(0) input : vec4f,
  @builtin(position) pos : vec4f,
}

@fragment
fn F(fragmentInput : FragmentInput) -> @location(0) vec4f {
  P.a += 42;
  return fragmentInput.input + fragmentInput.pos;
}
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

@binding(1) @group(3) @internal(rov) var pixel_local_a : texture_storage_2d<r32uint, read_write>;

fn load_from_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  P.a = textureLoad(pixel_local_a, rov_texcoord_0).x;
}

fn store_into_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  textureStore(pixel_local_a, rov_texcoord_0, vec4u(P.a));
}

struct F_res {
  @location(0)
  output_0 : vec4<f32>,
}

@fragment
fn F(fragmentInput : FragmentInput) -> F_res {
  let hlsl_sv_position = fragmentInput.pos;
  load_from_pixel_local_storage(hlsl_sv_position);
  let result = F_inner(fragmentInput);
  store_into_pixel_local_storage(hlsl_sv_position);
  return F_res(result);
}

struct PixelLocal {
  a : u32,
}

var<private> P : PixelLocal;

struct FragmentInput {
  @location(0)
  input : vec4f,
  @builtin(position)
  pos : vec4f,
}

fn F_inner(fragmentInput : FragmentInput) -> vec4f {
  P.a += 42;
  return (fragmentInput.input + fragmentInput.pos);
}
)";
    constexpr uint32_t kROVGroupIndex = 3u;
    auto got = Run<PixelLocal>(src, Bindings({{0, 1}}, kROVGroupIndex));
    EXPECT_EQ(expect, str(got));
}

TEST_F(HLSLPixelLocalTest, UseInEntryPoint_Multiple_PixelLocal_members) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
  d : u32,
}

var<pixel_local> P : PixelLocal;

struct FragmentInput {
  @location(0) input : vec4f,
  @builtin(position) pos : vec4f,
}

@fragment
fn F(fragmentInput : FragmentInput) -> @location(0) vec4f {
  P.a += 42;
  P.b -= 21;
  P.c += 12.5f;
  P.d -= 5;
  return fragmentInput.input + fragmentInput.pos;
}
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

@binding(1) @group(0) @internal(rov) var pixel_local_a : texture_storage_2d<r32uint, read_write>;

@binding(2) @group(0) @internal(rov) var pixel_local_b : texture_storage_2d<r32sint, read_write>;

@binding(3) @group(0) @internal(rov) var pixel_local_c : texture_storage_2d<r32float, read_write>;

@binding(4) @group(0) @internal(rov) var pixel_local_d : texture_storage_2d<r32uint, read_write>;

fn load_from_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  P.a = textureLoad(pixel_local_a, rov_texcoord_0).x;
  let rov_texcoord_1 = vec2u(my_input.xy);
  P.b = textureLoad(pixel_local_b, rov_texcoord_1).x;
  let rov_texcoord_2 = vec2u(my_input.xy);
  P.c = textureLoad(pixel_local_c, rov_texcoord_2).x;
  let rov_texcoord_3 = vec2u(my_input.xy);
  P.d = textureLoad(pixel_local_d, rov_texcoord_3).x;
}

fn store_into_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  textureStore(pixel_local_a, rov_texcoord_0, vec4u(P.a));
  let rov_texcoord_1 = vec2u(my_input.xy);
  textureStore(pixel_local_b, rov_texcoord_1, vec4i(P.b));
  let rov_texcoord_2 = vec2u(my_input.xy);
  textureStore(pixel_local_c, rov_texcoord_2, vec4f(P.c));
  let rov_texcoord_3 = vec2u(my_input.xy);
  textureStore(pixel_local_d, rov_texcoord_3, vec4u(P.d));
}

struct F_res {
  @location(0)
  output_0 : vec4<f32>,
}

@fragment
fn F(fragmentInput : FragmentInput) -> F_res {
  let hlsl_sv_position = fragmentInput.pos;
  load_from_pixel_local_storage(hlsl_sv_position);
  let result = F_inner(fragmentInput);
  store_into_pixel_local_storage(hlsl_sv_position);
  return F_res(result);
}

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
  d : u32,
}

var<private> P : PixelLocal;

struct FragmentInput {
  @location(0)
  input : vec4f,
  @builtin(position)
  pos : vec4f,
}

fn F_inner(fragmentInput : FragmentInput) -> vec4f {
  P.a += 42;
  P.b -= 21;
  P.c += 12.5f;
  P.d -= 5;
  return (fragmentInput.input + fragmentInput.pos);
}
)";

    auto got = Run<PixelLocal>(src, Bindings({{0, 1}, {1, 2}, {2, 3}, {3, 4}}, 0));
    EXPECT_EQ(expect, str(got));
}

TEST_F(HLSLPixelLocalTest, UseInEntryPoint_Multiple_PixelLocal_members_And_Fragment_Output) {
    auto* src = R"(
enable chromium_experimental_pixel_local;

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
  d : u32,
}

var<pixel_local> P : PixelLocal;

struct FragmentInput {
  @location(0) input : vec4f,
  @builtin(position) pos : vec4f,
}

struct FragmentOutput {
  @location(0) color0 : vec4f,
  @location(1) color1 : vec4f,
}

@fragment
fn F(fragmentInput : FragmentInput) -> FragmentOutput {
  P.a += 42;
  P.b -= 21;
  P.c += 12.5f;
  P.d -= 5;
  return FragmentOutput(fragmentInput.input, fragmentInput.pos);
}
)";

    auto* expect =
        R"(
enable chromium_experimental_pixel_local;

@binding(1) @group(0) @internal(rov) var pixel_local_a : texture_storage_2d<r32uint, read_write>;

@binding(2) @group(0) @internal(rov) var pixel_local_b : texture_storage_2d<r32sint, read_write>;

@binding(3) @group(0) @internal(rov) var pixel_local_c : texture_storage_2d<r32float, read_write>;

@binding(4) @group(0) @internal(rov) var pixel_local_d : texture_storage_2d<r32uint, read_write>;

fn load_from_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  P.a = textureLoad(pixel_local_a, rov_texcoord_0).x;
  let rov_texcoord_1 = vec2u(my_input.xy);
  P.b = textureLoad(pixel_local_b, rov_texcoord_1).x;
  let rov_texcoord_2 = vec2u(my_input.xy);
  P.c = textureLoad(pixel_local_c, rov_texcoord_2).x;
  let rov_texcoord_3 = vec2u(my_input.xy);
  P.d = textureLoad(pixel_local_d, rov_texcoord_3).x;
}

fn store_into_pixel_local_storage(my_input : vec4<f32>) {
  let rov_texcoord_0 = vec2u(my_input.xy);
  textureStore(pixel_local_a, rov_texcoord_0, vec4u(P.a));
  let rov_texcoord_1 = vec2u(my_input.xy);
  textureStore(pixel_local_b, rov_texcoord_1, vec4i(P.b));
  let rov_texcoord_2 = vec2u(my_input.xy);
  textureStore(pixel_local_c, rov_texcoord_2, vec4f(P.c));
  let rov_texcoord_3 = vec2u(my_input.xy);
  textureStore(pixel_local_d, rov_texcoord_3, vec4u(P.d));
}

struct F_res {
  @location(0)
  output_0 : vec4<f32>,
  @location(1)
  output_1 : vec4<f32>,
}

@fragment
fn F(fragmentInput : FragmentInput) -> F_res {
  let hlsl_sv_position = fragmentInput.pos;
  load_from_pixel_local_storage(hlsl_sv_position);
  let result = F_inner(fragmentInput);
  store_into_pixel_local_storage(hlsl_sv_position);
  return F_res(result.color0, result.color1);
}

struct PixelLocal {
  a : u32,
  b : i32,
  c : f32,
  d : u32,
}

var<private> P : PixelLocal;

struct FragmentInput {
  @location(0)
  input : vec4f,
  @builtin(position)
  pos : vec4f,
}

struct FragmentOutput {
  color0 : vec4f,
  color1 : vec4f,
}

fn F_inner(fragmentInput : FragmentInput) -> FragmentOutput {
  P.a += 42;
  P.b -= 21;
  P.c += 12.5f;
  P.d -= 5;
  return FragmentOutput(fragmentInput.input, fragmentInput.pos);
}
)";

    auto got = Run<PixelLocal>(src, Bindings({{0, 1}, {1, 2}, {2, 3}, {3, 4}}, 0));
    EXPECT_EQ(expect, str(got));
}

}  // namespace tint::hlsl::writer
