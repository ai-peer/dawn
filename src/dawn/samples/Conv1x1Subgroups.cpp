// Copyright 2023 The Dawn Authors
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

#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120

#include <iostream>
#include <limits>
#include <memory>
#include <thread>
#include <vector>
#include <regex>

#include "OpenCL.hpp"
#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

ABSL_FLAG(double,
          timestamp_period,
          0.0,
          "Assumed timestamp period. Passing this disables Dawn's timestamp conversion");
ABSL_FLAG(bool, dump_shaders, false, "Pass dump_shaders toggle to Dawn.");
ABSL_FLAG(bool, f16_op, false, "Use f16 operations in the shader");
ABSL_FLAG(bool, f16_data, false, "Load/store f16 data in the shader");

ABSL_FLAG(uint32_t, trials, 10, "Number of separate compute passes or trials to measure.");
ABSL_FLAG(uint32_t, dispatches, 10, "Number of dispatches in each trial.");

// Tests 2D convolution 1x1x128x12288 -> 1x1x128x1536
constexpr uint32_t kSharedDim = 128;  // number of floats
constexpr uint32_t kSrcDim = 12288;   // number of floats
constexpr uint32_t kDstDim = 1536;    // number of floats

constexpr char kWGSLShader[] = R"(
@group(0) @binding(0) var dst_tensor_image2d : storetype;
@group(0) @binding(1) var src_tensor_image2d : texture_2d<f32>;
struct biases_buffer_vector {
  data: array<vec4<fdtype>>,
};
@group(0) @binding(2) var<storage, read> biases_buffer : biases_buffer_vector;
struct weights_buffer_vector {
  data: array<vec4<fdtype>>,
};
@group(0) @binding(3) var<storage, read> weights_buffer : weights_buffer_vector;
struct Scalars {
  i0 : vec4<i32>,
  i1 : vec4<i32>,
};
@group(0) @binding(4) var<uniform> U: Scalars;

override wg_size: u32;
@compute @workgroup_size(wg_size, 1, 1)
fn main(
  @builtin(global_invocation_id) gid : vec3<u32>,
  @builtin(workgroup_id) wid : vec3<u32>,
  @builtin(local_invocation_id) lid : vec3<u32>,
  @builtin(subgroup_invocation_id) subgroup_invocation_id : u32,
  @builtin(subgroup_size) subgroup_size : u32
) {
  var DST_X : i32 = i32(gid.x) % U.i0.w;
  var DST_Y : i32 = (i32(gid.x) / U.i0.w) % U.i1.x;

  var DST_S : i32 = i32(wid.y);
  DST_S *= 4;

  if (DST_S >= U.i0.y) { return; }

  var r_w0_h0_s0 = vec4<foptype>(0.0);
  var r_w0_h0_s1 = vec4<foptype>(0.0);
  var r_w0_h0_s2 = vec4<foptype>(0.0);
  var r_w0_h0_s3 = vec4<foptype>(0.0);

  var filters_offset : u32 = u32(DST_S * 4 * U.i0.z);
  var s : i32 = 0;

  // TODO: make this not hardcoded
  if (subgroup_size != 32u && subgroup_size != 16u) { return; }

  while(true) {
    if (subgroup_size == 16u) {
      let src_w0_h0_s0 = vec4<foptype>(textureLoad(src_tensor_image2d, vec2<i32>((DST_X), ((DST_Y) * U.i0.z + (s))), 0));
      let src_w0_h0_s1 = vec4<foptype>(textureLoad(src_tensor_image2d, vec2<i32>((DST_X), ((DST_Y) * U.i0.z + (s+1))), 0));

      let w0 = vec4<foptype>(weights_buffer.data[filters_offset + subgroup_invocation_id]);
      filters_offset += subgroup_size;
      let w1 = vec4<foptype>(weights_buffer.data[filters_offset + subgroup_invocation_id]);
      filters_offset += subgroup_size;

      var w_0 = subgroupBroadcast4(w0, 0);
      var w_1 = subgroupBroadcast4(w0, 1);
      var w_2 = subgroupBroadcast4(w0, 2);
      var w_3 = subgroupBroadcast4(w0, 3);
      var w_4 = subgroupBroadcast4(w0, 4);
      var w_5 = subgroupBroadcast4(w0, 5);
      var w_6 = subgroupBroadcast4(w0, 6);
      var w_7 = subgroupBroadcast4(w0, 7);
      var w_8 = subgroupBroadcast4(w0, 8);
      var w_9 = subgroupBroadcast4(w0, 9);
      var w_10 = subgroupBroadcast4(w0, 10);
      var w_11 = subgroupBroadcast4(w0, 11);
      var w_12 = subgroupBroadcast4(w0, 12);
      var w_13 = subgroupBroadcast4(w0, 13);
      var w_14 = subgroupBroadcast4(w0, 14);
      var w_15 = subgroupBroadcast4(w0, 15);

      r_w0_h0_s0.x += w_0.x * src_w0_h0_s0.x;
      r_w0_h0_s0.y += w_0.y * src_w0_h0_s0.x;
      r_w0_h0_s0.z += w_0.z * src_w0_h0_s0.x;
      r_w0_h0_s0.w += w_0.w * src_w0_h0_s0.x;
      r_w0_h0_s0.x += w_1.x * src_w0_h0_s0.y;
      r_w0_h0_s0.y += w_1.y * src_w0_h0_s0.y;
      r_w0_h0_s0.z += w_1.z * src_w0_h0_s0.y;
      r_w0_h0_s0.w += w_1.w * src_w0_h0_s0.y;
      r_w0_h0_s0.x += w_2.x * src_w0_h0_s0.z;
      r_w0_h0_s0.y += w_2.y * src_w0_h0_s0.z;
      r_w0_h0_s0.z += w_2.z * src_w0_h0_s0.z;
      r_w0_h0_s0.w += w_2.w * src_w0_h0_s0.z;
      r_w0_h0_s0.x += w_3.x * src_w0_h0_s0.w;
      r_w0_h0_s0.y += w_3.y * src_w0_h0_s0.w;
      r_w0_h0_s0.z += w_3.z * src_w0_h0_s0.w;
      r_w0_h0_s0.w += w_3.w * src_w0_h0_s0.w;
      r_w0_h0_s1.x += w_4.x * src_w0_h0_s0.x;
      r_w0_h0_s1.y += w_4.y * src_w0_h0_s0.x;
      r_w0_h0_s1.z += w_4.z * src_w0_h0_s0.x;
      r_w0_h0_s1.w += w_4.w * src_w0_h0_s0.x;
      r_w0_h0_s1.x += w_5.x * src_w0_h0_s0.y;
      r_w0_h0_s1.y += w_5.y * src_w0_h0_s0.y;
      r_w0_h0_s1.z += w_5.z * src_w0_h0_s0.y;
      r_w0_h0_s1.w += w_5.w * src_w0_h0_s0.y;
      r_w0_h0_s1.x += w_6.x * src_w0_h0_s0.z;
      r_w0_h0_s1.y += w_6.y * src_w0_h0_s0.z;
      r_w0_h0_s1.z += w_6.z * src_w0_h0_s0.z;
      r_w0_h0_s1.w += w_6.w * src_w0_h0_s0.z;
      r_w0_h0_s1.x += w_7.x * src_w0_h0_s0.w;
      r_w0_h0_s1.y += w_7.y * src_w0_h0_s0.w;
      r_w0_h0_s1.z += w_7.z * src_w0_h0_s0.w;
      r_w0_h0_s1.w += w_7.w * src_w0_h0_s0.w;
      r_w0_h0_s2.x += w_8.x * src_w0_h0_s0.x;
      r_w0_h0_s2.y += w_8.y * src_w0_h0_s0.x;
      r_w0_h0_s2.z += w_8.z * src_w0_h0_s0.x;
      r_w0_h0_s2.w += w_8.w * src_w0_h0_s0.x;
      r_w0_h0_s2.x += w_9.x * src_w0_h0_s0.y;
      r_w0_h0_s2.y += w_9.y * src_w0_h0_s0.y;
      r_w0_h0_s2.z += w_9.z * src_w0_h0_s0.y;
      r_w0_h0_s2.w += w_9.w * src_w0_h0_s0.y;
      r_w0_h0_s2.x += w_10.x * src_w0_h0_s0.z;
      r_w0_h0_s2.y += w_10.y * src_w0_h0_s0.z;
      r_w0_h0_s2.z += w_10.z * src_w0_h0_s0.z;
      r_w0_h0_s2.w += w_10.w * src_w0_h0_s0.z;
      r_w0_h0_s2.x += w_11.x * src_w0_h0_s0.w;
      r_w0_h0_s2.y += w_11.y * src_w0_h0_s0.w;
      r_w0_h0_s2.z += w_11.z * src_w0_h0_s0.w;
      r_w0_h0_s2.w += w_11.w * src_w0_h0_s0.w;
      r_w0_h0_s3.x += w_12.x * src_w0_h0_s0.x;
      r_w0_h0_s3.y += w_12.y * src_w0_h0_s0.x;
      r_w0_h0_s3.z += w_12.z * src_w0_h0_s0.x;
      r_w0_h0_s3.w += w_12.w * src_w0_h0_s0.x;
      r_w0_h0_s3.x += w_13.x * src_w0_h0_s0.y;
      r_w0_h0_s3.y += w_13.y * src_w0_h0_s0.y;
      r_w0_h0_s3.z += w_13.z * src_w0_h0_s0.y;
      r_w0_h0_s3.w += w_13.w * src_w0_h0_s0.y;
      r_w0_h0_s3.x += w_14.x * src_w0_h0_s0.z;
      r_w0_h0_s3.y += w_14.y * src_w0_h0_s0.z;
      r_w0_h0_s3.z += w_14.z * src_w0_h0_s0.z;
      r_w0_h0_s3.w += w_14.w * src_w0_h0_s0.z;
      r_w0_h0_s3.x += w_15.x * src_w0_h0_s0.w;
      r_w0_h0_s3.y += w_15.y * src_w0_h0_s0.w;
      r_w0_h0_s3.z += w_15.z * src_w0_h0_s0.w;
      r_w0_h0_s3.w += w_15.w * src_w0_h0_s0.w;

      w_0 = subgroupBroadcast4(w1, 0);
      w_1 = subgroupBroadcast4(w1, 1);
      w_2 = subgroupBroadcast4(w1, 2);
      w_3 = subgroupBroadcast4(w1, 3);
      w_4 = subgroupBroadcast4(w1, 4);
      w_5 = subgroupBroadcast4(w1, 5);
      w_6 = subgroupBroadcast4(w1, 6);
      w_7 = subgroupBroadcast4(w1, 7);
      w_8 = subgroupBroadcast4(w1, 8);
      w_9 = subgroupBroadcast4(w1, 9);
      w_10 = subgroupBroadcast4(w1, 10);
      w_11 = subgroupBroadcast4(w1, 11);
      w_12 = subgroupBroadcast4(w1, 12);
      w_13 = subgroupBroadcast4(w1, 13);
      w_14 = subgroupBroadcast4(w1, 14);
      w_15 = subgroupBroadcast4(w1, 15);

      r_w0_h0_s0.x += w_0.x * src_w0_h0_s1.x;
      r_w0_h0_s0.y += w_0.y * src_w0_h0_s1.x;
      r_w0_h0_s0.z += w_0.z * src_w0_h0_s1.x;
      r_w0_h0_s0.w += w_0.w * src_w0_h0_s1.x;
      r_w0_h0_s0.x += w_1.x * src_w0_h0_s1.y;
      r_w0_h0_s0.y += w_1.y * src_w0_h0_s1.y;
      r_w0_h0_s0.z += w_1.z * src_w0_h0_s1.y;
      r_w0_h0_s0.w += w_1.w * src_w0_h0_s1.y;
      r_w0_h0_s0.x += w_2.x * src_w0_h0_s1.z;
      r_w0_h0_s0.y += w_2.y * src_w0_h0_s1.z;
      r_w0_h0_s0.z += w_2.z * src_w0_h0_s1.z;
      r_w0_h0_s0.w += w_2.w * src_w0_h0_s1.z;
      r_w0_h0_s0.x += w_3.x * src_w0_h0_s1.w;
      r_w0_h0_s0.y += w_3.y * src_w0_h0_s1.w;
      r_w0_h0_s0.z += w_3.z * src_w0_h0_s1.w;
      r_w0_h0_s0.w += w_3.w * src_w0_h0_s1.w;
      r_w0_h0_s1.x += w_4.x * src_w0_h0_s1.x;
      r_w0_h0_s1.y += w_4.y * src_w0_h0_s1.x;
      r_w0_h0_s1.z += w_4.z * src_w0_h0_s1.x;
      r_w0_h0_s1.w += w_4.w * src_w0_h0_s1.x;
      r_w0_h0_s1.x += w_5.x * src_w0_h0_s1.y;
      r_w0_h0_s1.y += w_5.y * src_w0_h0_s1.y;
      r_w0_h0_s1.z += w_5.z * src_w0_h0_s1.y;
      r_w0_h0_s1.w += w_5.w * src_w0_h0_s1.y;
      r_w0_h0_s1.x += w_6.x * src_w0_h0_s1.z;
      r_w0_h0_s1.y += w_6.y * src_w0_h0_s1.z;
      r_w0_h0_s1.z += w_6.z * src_w0_h0_s1.z;
      r_w0_h0_s1.w += w_6.w * src_w0_h0_s1.z;
      r_w0_h0_s1.x += w_7.x * src_w0_h0_s1.w;
      r_w0_h0_s1.y += w_7.y * src_w0_h0_s1.w;
      r_w0_h0_s1.z += w_7.z * src_w0_h0_s1.w;
      r_w0_h0_s1.w += w_7.w * src_w0_h0_s1.w;
      r_w0_h0_s2.x += w_8.x * src_w0_h0_s1.x;
      r_w0_h0_s2.y += w_8.y * src_w0_h0_s1.x;
      r_w0_h0_s2.z += w_8.z * src_w0_h0_s1.x;
      r_w0_h0_s2.w += w_8.w * src_w0_h0_s1.x;
      r_w0_h0_s2.x += w_9.x * src_w0_h0_s1.y;
      r_w0_h0_s2.y += w_9.y * src_w0_h0_s1.y;
      r_w0_h0_s2.z += w_9.z * src_w0_h0_s1.y;
      r_w0_h0_s2.w += w_9.w * src_w0_h0_s1.y;
      r_w0_h0_s2.x += w_10.x * src_w0_h0_s1.z;
      r_w0_h0_s2.y += w_10.y * src_w0_h0_s1.z;
      r_w0_h0_s2.z += w_10.z * src_w0_h0_s1.z;
      r_w0_h0_s2.w += w_10.w * src_w0_h0_s1.z;
      r_w0_h0_s2.x += w_11.x * src_w0_h0_s1.w;
      r_w0_h0_s2.y += w_11.y * src_w0_h0_s1.w;
      r_w0_h0_s2.z += w_11.z * src_w0_h0_s1.w;
      r_w0_h0_s2.w += w_11.w * src_w0_h0_s1.w;
      r_w0_h0_s3.x += w_12.x * src_w0_h0_s1.x;
      r_w0_h0_s3.y += w_12.y * src_w0_h0_s1.x;
      r_w0_h0_s3.z += w_12.z * src_w0_h0_s1.x;
      r_w0_h0_s3.w += w_12.w * src_w0_h0_s1.x;
      r_w0_h0_s3.x += w_13.x * src_w0_h0_s1.y;
      r_w0_h0_s3.y += w_13.y * src_w0_h0_s1.y;
      r_w0_h0_s3.z += w_13.z * src_w0_h0_s1.y;
      r_w0_h0_s3.w += w_13.w * src_w0_h0_s1.y;
      r_w0_h0_s3.x += w_14.x * src_w0_h0_s1.z;
      r_w0_h0_s3.y += w_14.y * src_w0_h0_s1.z;
      r_w0_h0_s3.z += w_14.z * src_w0_h0_s1.z;
      r_w0_h0_s3.w += w_14.w * src_w0_h0_s1.z;
      r_w0_h0_s3.x += w_15.x * src_w0_h0_s1.w;
      r_w0_h0_s3.y += w_15.y * src_w0_h0_s1.w;
      r_w0_h0_s3.z += w_15.z * src_w0_h0_s1.w;
      r_w0_h0_s3.w += w_15.w * src_w0_h0_s1.w;
    } else if (subgroup_size == 32u) {
      let src_w0_h0_s0 = vec4<foptype>(textureLoad(src_tensor_image2d, vec2<i32>((DST_X), ((DST_Y) * U.i0.z + (s))), 0));
      let src_w0_h0_s1 = vec4<foptype>(textureLoad(src_tensor_image2d, vec2<i32>((DST_X), ((DST_Y) * U.i0.z + (s+1))), 0));

      let w0 = vec4<foptype>(weights_buffer.data[filters_offset + subgroup_invocation_id]);
      filters_offset += subgroup_size;

      var w_0 = subgroupBroadcast4(w0, 0);
      var w_1 = subgroupBroadcast4(w0, 1);
      var w_2 = subgroupBroadcast4(w0, 2);
      var w_3 = subgroupBroadcast4(w0, 3);
      var w_4 = subgroupBroadcast4(w0, 4);
      var w_5 = subgroupBroadcast4(w0, 5);
      var w_6 = subgroupBroadcast4(w0, 6);
      var w_7 = subgroupBroadcast4(w0, 7);
      var w_8 = subgroupBroadcast4(w0, 8);
      var w_9 = subgroupBroadcast4(w0, 9);
      var w_10 = subgroupBroadcast4(w0, 10);
      var w_11 = subgroupBroadcast4(w0, 11);
      var w_12 = subgroupBroadcast4(w0, 12);
      var w_13 = subgroupBroadcast4(w0, 13);
      var w_14 = subgroupBroadcast4(w0, 14);
      var w_15 = subgroupBroadcast4(w0, 15);

      r_w0_h0_s0.x += w_0.x * src_w0_h0_s0.x;
      r_w0_h0_s0.y += w_0.y * src_w0_h0_s0.x;
      r_w0_h0_s0.z += w_0.z * src_w0_h0_s0.x;
      r_w0_h0_s0.w += w_0.w * src_w0_h0_s0.x;
      r_w0_h0_s0.x += w_1.x * src_w0_h0_s0.y;
      r_w0_h0_s0.y += w_1.y * src_w0_h0_s0.y;
      r_w0_h0_s0.z += w_1.z * src_w0_h0_s0.y;
      r_w0_h0_s0.w += w_1.w * src_w0_h0_s0.y;
      r_w0_h0_s0.x += w_2.x * src_w0_h0_s0.z;
      r_w0_h0_s0.y += w_2.y * src_w0_h0_s0.z;
      r_w0_h0_s0.z += w_2.z * src_w0_h0_s0.z;
      r_w0_h0_s0.w += w_2.w * src_w0_h0_s0.z;
      r_w0_h0_s0.x += w_3.x * src_w0_h0_s0.w;
      r_w0_h0_s0.y += w_3.y * src_w0_h0_s0.w;
      r_w0_h0_s0.z += w_3.z * src_w0_h0_s0.w;
      r_w0_h0_s0.w += w_3.w * src_w0_h0_s0.w;
      r_w0_h0_s1.x += w_4.x * src_w0_h0_s0.x;
      r_w0_h0_s1.y += w_4.y * src_w0_h0_s0.x;
      r_w0_h0_s1.z += w_4.z * src_w0_h0_s0.x;
      r_w0_h0_s1.w += w_4.w * src_w0_h0_s0.x;
      r_w0_h0_s1.x += w_5.x * src_w0_h0_s0.y;
      r_w0_h0_s1.y += w_5.y * src_w0_h0_s0.y;
      r_w0_h0_s1.z += w_5.z * src_w0_h0_s0.y;
      r_w0_h0_s1.w += w_5.w * src_w0_h0_s0.y;
      r_w0_h0_s1.x += w_6.x * src_w0_h0_s0.z;
      r_w0_h0_s1.y += w_6.y * src_w0_h0_s0.z;
      r_w0_h0_s1.z += w_6.z * src_w0_h0_s0.z;
      r_w0_h0_s1.w += w_6.w * src_w0_h0_s0.z;
      r_w0_h0_s1.x += w_7.x * src_w0_h0_s0.w;
      r_w0_h0_s1.y += w_7.y * src_w0_h0_s0.w;
      r_w0_h0_s1.z += w_7.z * src_w0_h0_s0.w;
      r_w0_h0_s1.w += w_7.w * src_w0_h0_s0.w;
      r_w0_h0_s2.x += w_8.x * src_w0_h0_s0.x;
      r_w0_h0_s2.y += w_8.y * src_w0_h0_s0.x;
      r_w0_h0_s2.z += w_8.z * src_w0_h0_s0.x;
      r_w0_h0_s2.w += w_8.w * src_w0_h0_s0.x;
      r_w0_h0_s2.x += w_9.x * src_w0_h0_s0.y;
      r_w0_h0_s2.y += w_9.y * src_w0_h0_s0.y;
      r_w0_h0_s2.z += w_9.z * src_w0_h0_s0.y;
      r_w0_h0_s2.w += w_9.w * src_w0_h0_s0.y;
      r_w0_h0_s2.x += w_10.x * src_w0_h0_s0.z;
      r_w0_h0_s2.y += w_10.y * src_w0_h0_s0.z;
      r_w0_h0_s2.z += w_10.z * src_w0_h0_s0.z;
      r_w0_h0_s2.w += w_10.w * src_w0_h0_s0.z;
      r_w0_h0_s2.x += w_11.x * src_w0_h0_s0.w;
      r_w0_h0_s2.y += w_11.y * src_w0_h0_s0.w;
      r_w0_h0_s2.z += w_11.z * src_w0_h0_s0.w;
      r_w0_h0_s2.w += w_11.w * src_w0_h0_s0.w;
      r_w0_h0_s3.x += w_12.x * src_w0_h0_s0.x;
      r_w0_h0_s3.y += w_12.y * src_w0_h0_s0.x;
      r_w0_h0_s3.z += w_12.z * src_w0_h0_s0.x;
      r_w0_h0_s3.w += w_12.w * src_w0_h0_s0.x;
      r_w0_h0_s3.x += w_13.x * src_w0_h0_s0.y;
      r_w0_h0_s3.y += w_13.y * src_w0_h0_s0.y;
      r_w0_h0_s3.z += w_13.z * src_w0_h0_s0.y;
      r_w0_h0_s3.w += w_13.w * src_w0_h0_s0.y;
      r_w0_h0_s3.x += w_14.x * src_w0_h0_s0.z;
      r_w0_h0_s3.y += w_14.y * src_w0_h0_s0.z;
      r_w0_h0_s3.z += w_14.z * src_w0_h0_s0.z;
      r_w0_h0_s3.w += w_14.w * src_w0_h0_s0.z;
      r_w0_h0_s3.x += w_15.x * src_w0_h0_s0.w;
      r_w0_h0_s3.y += w_15.y * src_w0_h0_s0.w;
      r_w0_h0_s3.z += w_15.z * src_w0_h0_s0.w;
      r_w0_h0_s3.w += w_15.w * src_w0_h0_s0.w;

      w_0 = subgroupBroadcast4(w0, 16 + 0);
      w_1 = subgroupBroadcast4(w0, 16 + 1);
      w_2 = subgroupBroadcast4(w0, 16 + 2);
      w_3 = subgroupBroadcast4(w0, 16 + 3);
      w_4 = subgroupBroadcast4(w0, 16 + 4);
      w_5 = subgroupBroadcast4(w0, 16 + 5);
      w_6 = subgroupBroadcast4(w0, 16 + 6);
      w_7 = subgroupBroadcast4(w0, 16 + 7);
      w_8 = subgroupBroadcast4(w0, 16 + 8);
      w_9 = subgroupBroadcast4(w0, 16 + 9);
      w_10 = subgroupBroadcast4(w0, 16 + 10);
      w_11 = subgroupBroadcast4(w0, 16 + 11);
      w_12 = subgroupBroadcast4(w0, 16 + 12);
      w_13 = subgroupBroadcast4(w0, 16 + 13);
      w_14 = subgroupBroadcast4(w0, 16 + 14);
      w_15 = subgroupBroadcast4(w0, 16 + 15);

      r_w0_h0_s0.x += w_0.x * src_w0_h0_s1.x;
      r_w0_h0_s0.y += w_0.y * src_w0_h0_s1.x;
      r_w0_h0_s0.z += w_0.z * src_w0_h0_s1.x;
      r_w0_h0_s0.w += w_0.w * src_w0_h0_s1.x;
      r_w0_h0_s0.x += w_1.x * src_w0_h0_s1.y;
      r_w0_h0_s0.y += w_1.y * src_w0_h0_s1.y;
      r_w0_h0_s0.z += w_1.z * src_w0_h0_s1.y;
      r_w0_h0_s0.w += w_1.w * src_w0_h0_s1.y;
      r_w0_h0_s0.x += w_2.x * src_w0_h0_s1.z;
      r_w0_h0_s0.y += w_2.y * src_w0_h0_s1.z;
      r_w0_h0_s0.z += w_2.z * src_w0_h0_s1.z;
      r_w0_h0_s0.w += w_2.w * src_w0_h0_s1.z;
      r_w0_h0_s0.x += w_3.x * src_w0_h0_s1.w;
      r_w0_h0_s0.y += w_3.y * src_w0_h0_s1.w;
      r_w0_h0_s0.z += w_3.z * src_w0_h0_s1.w;
      r_w0_h0_s0.w += w_3.w * src_w0_h0_s1.w;
      r_w0_h0_s1.x += w_4.x * src_w0_h0_s1.x;
      r_w0_h0_s1.y += w_4.y * src_w0_h0_s1.x;
      r_w0_h0_s1.z += w_4.z * src_w0_h0_s1.x;
      r_w0_h0_s1.w += w_4.w * src_w0_h0_s1.x;
      r_w0_h0_s1.x += w_5.x * src_w0_h0_s1.y;
      r_w0_h0_s1.y += w_5.y * src_w0_h0_s1.y;
      r_w0_h0_s1.z += w_5.z * src_w0_h0_s1.y;
      r_w0_h0_s1.w += w_5.w * src_w0_h0_s1.y;
      r_w0_h0_s1.x += w_6.x * src_w0_h0_s1.z;
      r_w0_h0_s1.y += w_6.y * src_w0_h0_s1.z;
      r_w0_h0_s1.z += w_6.z * src_w0_h0_s1.z;
      r_w0_h0_s1.w += w_6.w * src_w0_h0_s1.z;
      r_w0_h0_s1.x += w_7.x * src_w0_h0_s1.w;
      r_w0_h0_s1.y += w_7.y * src_w0_h0_s1.w;
      r_w0_h0_s1.z += w_7.z * src_w0_h0_s1.w;
      r_w0_h0_s1.w += w_7.w * src_w0_h0_s1.w;
      r_w0_h0_s2.x += w_8.x * src_w0_h0_s1.x;
      r_w0_h0_s2.y += w_8.y * src_w0_h0_s1.x;
      r_w0_h0_s2.z += w_8.z * src_w0_h0_s1.x;
      r_w0_h0_s2.w += w_8.w * src_w0_h0_s1.x;
      r_w0_h0_s2.x += w_9.x * src_w0_h0_s1.y;
      r_w0_h0_s2.y += w_9.y * src_w0_h0_s1.y;
      r_w0_h0_s2.z += w_9.z * src_w0_h0_s1.y;
      r_w0_h0_s2.w += w_9.w * src_w0_h0_s1.y;
      r_w0_h0_s2.x += w_10.x * src_w0_h0_s1.z;
      r_w0_h0_s2.y += w_10.y * src_w0_h0_s1.z;
      r_w0_h0_s2.z += w_10.z * src_w0_h0_s1.z;
      r_w0_h0_s2.w += w_10.w * src_w0_h0_s1.z;
      r_w0_h0_s2.x += w_11.x * src_w0_h0_s1.w;
      r_w0_h0_s2.y += w_11.y * src_w0_h0_s1.w;
      r_w0_h0_s2.z += w_11.z * src_w0_h0_s1.w;
      r_w0_h0_s2.w += w_11.w * src_w0_h0_s1.w;
      r_w0_h0_s3.x += w_12.x * src_w0_h0_s1.x;
      r_w0_h0_s3.y += w_12.y * src_w0_h0_s1.x;
      r_w0_h0_s3.z += w_12.z * src_w0_h0_s1.x;
      r_w0_h0_s3.w += w_12.w * src_w0_h0_s1.x;
      r_w0_h0_s3.x += w_13.x * src_w0_h0_s1.y;
      r_w0_h0_s3.y += w_13.y * src_w0_h0_s1.y;
      r_w0_h0_s3.z += w_13.z * src_w0_h0_s1.y;
      r_w0_h0_s3.w += w_13.w * src_w0_h0_s1.y;
      r_w0_h0_s3.x += w_14.x * src_w0_h0_s1.z;
      r_w0_h0_s3.y += w_14.y * src_w0_h0_s1.z;
      r_w0_h0_s3.z += w_14.z * src_w0_h0_s1.z;
      r_w0_h0_s3.w += w_14.w * src_w0_h0_s1.z;
      r_w0_h0_s3.x += w_15.x * src_w0_h0_s1.w;
      r_w0_h0_s3.y += w_15.y * src_w0_h0_s1.w;
      r_w0_h0_s3.z += w_15.z * src_w0_h0_s1.w;
      r_w0_h0_s3.w += w_15.w * src_w0_h0_s1.w;
    }
    s += 2;
    if (s >= U.i0.z) { break; }
  }
  if (DST_Y >= U.i0.x || DST_S >= U.i0.y) {
    return;
  }
  if (DST_S + 0 >= U.i0.y) { return; }
  {
    let bias_val = vec4<foptype>(biases_buffer.data[(DST_S + 0)]);
  {
    let res : vec4<foptype> = r_w0_h0_s0 + bias_val;
    textureStore(dst_tensor_image2d, vec2<i32>((DST_X + 0), ((DST_Y + 0) * U.i0.y + (DST_S + 0))), vec4<f32>(res));
  }
  }
  if (DST_S + 1 >= U.i0.y) { return; }
  {
    let bias_val = vec4<foptype>(biases_buffer.data[(DST_S + 1)]);
  {
    let res : vec4<foptype> = r_w0_h0_s1 + bias_val;
    textureStore(dst_tensor_image2d, vec2<i32>((DST_X + 0), ((DST_Y + 0) * U.i0.y + (DST_S + 1))), vec4<f32>(res));
  }
  }
  if (DST_S + 2 >= U.i0.y) { return; }
  {
    let bias_val = vec4<foptype>(biases_buffer.data[(DST_S + 2)]);
  {
    let res : vec4<foptype> = r_w0_h0_s2 + bias_val;
    textureStore(dst_tensor_image2d, vec2<i32>((DST_X + 0), ((DST_Y + 0) * U.i0.y + (DST_S + 2))), vec4<f32>(res));
  }
  }
  if (DST_S + 3 >= U.i0.y) { return; }
  {
    let bias_val = vec4<foptype>(biases_buffer.data[(DST_S + 3)]);
  {
    let res : vec4<foptype> = r_w0_h0_s3 + bias_val;
    textureStore(dst_tensor_image2d, vec2<i32>((DST_X + 0), ((DST_Y + 0) * U.i0.y + (DST_S + 3))), vec4<f32>(res));
  }
  }
}
)";

constexpr char kOpenCLShader[] = R"(
#define GLOBAL_ID_0 get_global_id(0)
#define GLOBAL_ID_1 get_global_id(1)
#define GLOBAL_ID_2 get_global_id(2)
#define LOCAL_ID_0 get_local_id(0)
#define LOCAL_ID_1 get_local_id(1)
#define LOCAL_ID_2 get_local_id(2)
#define GROUP_ID_0 get_group_id(0)
#define GROUP_ID_1 get_group_id(1)
#define GROUP_ID_2 get_group_id(2)
#define GROUP_SIZE_0 get_local_size(0)
#define GROUP_SIZE_1 get_local_size(1)
#define GROUP_SIZE_2 get_local_size(2)

__constant sampler_t smp_zero = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

__kernel void main_function(__global FLTD4* biases_buffer,
  __global FLTD4* weights_buffer,
  __write_only image2d_t dst_tensor_image2d,
  __read_only image2d_t src_tensor_image2d,
  int4 shared_int4_0,
  int4 shared_int4_1)
{
  int DST_X = GLOBAL_ID_0 % shared_int4_0.w;
  int DST_Y = (GLOBAL_ID_0 / shared_int4_0.w) % shared_int4_1.x;

  int DST_S = GROUP_ID_1;
  DST_S *= 4;

  if (DST_S >= shared_int4_0.y) { return; }

  int lid = LOCAL_ID_0;
  FLTOP4 r_w0_h0_s0 = (FLTOP4)(0.0f);
  FLTOP4 r_w0_h0_s1 = (FLTOP4)(0.0f);
  FLTOP4 r_w0_h0_s2 = (FLTOP4)(0.0f);
  FLTOP4 r_w0_h0_s3 = (FLTOP4)(0.0f);

  int filters_offset = DST_S * 4 * shared_int4_0.z;

  // TODO: make this not hardcoded.
  if (get_sub_group_size() != 16 && get_sub_group_size() != 32) {
    return;
  }

  int s = 0;
  while(true) {
    FLTOP4 src_w0_h0_s0 = convert_FLTOP4(READIMG(src_tensor_image2d, smp_zero, (int2)((DST_X), ((DST_Y) * shared_int4_0.z + (s)))));
    FLTOP4 src_w0_h0_s1 = convert_FLTOP4(READIMG(src_tensor_image2d, smp_zero, (int2)((DST_X), ((DST_Y) * shared_int4_0.z + (s + 1)))));
    if (get_sub_group_size() == 16) {
      FLTOP4 w0 = convert_FLTOP4(weights_buffer[filters_offset + get_sub_group_local_id()]);
      filters_offset += 16;
      FLTOP4 w1 = convert_FLTOP4(weights_buffer[filters_offset + get_sub_group_local_id()]);
      filters_offset += 16;

      FLTOP4 w_0 = sub_group_broadcast(w0, 0);
      FLTOP4 w_1 = sub_group_broadcast(w0, 1);
      FLTOP4 w_2 = sub_group_broadcast(w0, 2);
      FLTOP4 w_3 = sub_group_broadcast(w0, 3);
      FLTOP4 w_4 = sub_group_broadcast(w0, 4);
      FLTOP4 w_5 = sub_group_broadcast(w0, 5);
      FLTOP4 w_6 = sub_group_broadcast(w0, 6);
      FLTOP4 w_7 = sub_group_broadcast(w0, 7);
      FLTOP4 w_8 = sub_group_broadcast(w0, 8);
      FLTOP4 w_9 = sub_group_broadcast(w0, 9);
      FLTOP4 w_10 = sub_group_broadcast(w0, 10);
      FLTOP4 w_11 = sub_group_broadcast(w0, 11);
      FLTOP4 w_12 = sub_group_broadcast(w0, 12);
      FLTOP4 w_13 = sub_group_broadcast(w0, 13);
      FLTOP4 w_14 = sub_group_broadcast(w0, 14);
      FLTOP4 w_15 = sub_group_broadcast(w0, 15);

      r_w0_h0_s0.x += w_0.x * src_w0_h0_s0.x;
      r_w0_h0_s0.y += w_0.y * src_w0_h0_s0.x;
      r_w0_h0_s0.z += w_0.z * src_w0_h0_s0.x;
      r_w0_h0_s0.w += w_0.w * src_w0_h0_s0.x;
      r_w0_h0_s0.x += w_1.x * src_w0_h0_s0.y;
      r_w0_h0_s0.y += w_1.y * src_w0_h0_s0.y;
      r_w0_h0_s0.z += w_1.z * src_w0_h0_s0.y;
      r_w0_h0_s0.w += w_1.w * src_w0_h0_s0.y;
      r_w0_h0_s0.x += w_2.x * src_w0_h0_s0.z;
      r_w0_h0_s0.y += w_2.y * src_w0_h0_s0.z;
      r_w0_h0_s0.z += w_2.z * src_w0_h0_s0.z;
      r_w0_h0_s0.w += w_2.w * src_w0_h0_s0.z;
      r_w0_h0_s0.x += w_3.x * src_w0_h0_s0.w;
      r_w0_h0_s0.y += w_3.y * src_w0_h0_s0.w;
      r_w0_h0_s0.z += w_3.z * src_w0_h0_s0.w;
      r_w0_h0_s0.w += w_3.w * src_w0_h0_s0.w;
      r_w0_h0_s1.x += w_4.x * src_w0_h0_s0.x;
      r_w0_h0_s1.y += w_4.y * src_w0_h0_s0.x;
      r_w0_h0_s1.z += w_4.z * src_w0_h0_s0.x;
      r_w0_h0_s1.w += w_4.w * src_w0_h0_s0.x;
      r_w0_h0_s1.x += w_5.x * src_w0_h0_s0.y;
      r_w0_h0_s1.y += w_5.y * src_w0_h0_s0.y;
      r_w0_h0_s1.z += w_5.z * src_w0_h0_s0.y;
      r_w0_h0_s1.w += w_5.w * src_w0_h0_s0.y;
      r_w0_h0_s1.x += w_6.x * src_w0_h0_s0.z;
      r_w0_h0_s1.y += w_6.y * src_w0_h0_s0.z;
      r_w0_h0_s1.z += w_6.z * src_w0_h0_s0.z;
      r_w0_h0_s1.w += w_6.w * src_w0_h0_s0.z;
      r_w0_h0_s1.x += w_7.x * src_w0_h0_s0.w;
      r_w0_h0_s1.y += w_7.y * src_w0_h0_s0.w;
      r_w0_h0_s1.z += w_7.z * src_w0_h0_s0.w;
      r_w0_h0_s1.w += w_7.w * src_w0_h0_s0.w;
      r_w0_h0_s2.x += w_8.x * src_w0_h0_s0.x;
      r_w0_h0_s2.y += w_8.y * src_w0_h0_s0.x;
      r_w0_h0_s2.z += w_8.z * src_w0_h0_s0.x;
      r_w0_h0_s2.w += w_8.w * src_w0_h0_s0.x;
      r_w0_h0_s2.x += w_9.x * src_w0_h0_s0.y;
      r_w0_h0_s2.y += w_9.y * src_w0_h0_s0.y;
      r_w0_h0_s2.z += w_9.z * src_w0_h0_s0.y;
      r_w0_h0_s2.w += w_9.w * src_w0_h0_s0.y;
      r_w0_h0_s2.x += w_10.x * src_w0_h0_s0.z;
      r_w0_h0_s2.y += w_10.y * src_w0_h0_s0.z;
      r_w0_h0_s2.z += w_10.z * src_w0_h0_s0.z;
      r_w0_h0_s2.w += w_10.w * src_w0_h0_s0.z;
      r_w0_h0_s2.x += w_11.x * src_w0_h0_s0.w;
      r_w0_h0_s2.y += w_11.y * src_w0_h0_s0.w;
      r_w0_h0_s2.z += w_11.z * src_w0_h0_s0.w;
      r_w0_h0_s2.w += w_11.w * src_w0_h0_s0.w;
      r_w0_h0_s3.x += w_12.x * src_w0_h0_s0.x;
      r_w0_h0_s3.y += w_12.y * src_w0_h0_s0.x;
      r_w0_h0_s3.z += w_12.z * src_w0_h0_s0.x;
      r_w0_h0_s3.w += w_12.w * src_w0_h0_s0.x;
      r_w0_h0_s3.x += w_13.x * src_w0_h0_s0.y;
      r_w0_h0_s3.y += w_13.y * src_w0_h0_s0.y;
      r_w0_h0_s3.z += w_13.z * src_w0_h0_s0.y;
      r_w0_h0_s3.w += w_13.w * src_w0_h0_s0.y;
      r_w0_h0_s3.x += w_14.x * src_w0_h0_s0.z;
      r_w0_h0_s3.y += w_14.y * src_w0_h0_s0.z;
      r_w0_h0_s3.z += w_14.z * src_w0_h0_s0.z;
      r_w0_h0_s3.w += w_14.w * src_w0_h0_s0.z;
      r_w0_h0_s3.x += w_15.x * src_w0_h0_s0.w;
      r_w0_h0_s3.y += w_15.y * src_w0_h0_s0.w;
      r_w0_h0_s3.z += w_15.z * src_w0_h0_s0.w;
      r_w0_h0_s3.w += w_15.w * src_w0_h0_s0.w;

      w_0 = sub_group_broadcast(w1, 0);
      w_1 = sub_group_broadcast(w1, 1);
      w_2 = sub_group_broadcast(w1, 2);
      w_3 = sub_group_broadcast(w1, 3);
      w_4 = sub_group_broadcast(w1, 4);
      w_5 = sub_group_broadcast(w1, 5);
      w_6 = sub_group_broadcast(w1, 6);
      w_7 = sub_group_broadcast(w1, 7);
      w_8 = sub_group_broadcast(w1, 8);
      w_9 = sub_group_broadcast(w1, 9);
      w_10 = sub_group_broadcast(w1, 10);
      w_11 = sub_group_broadcast(w1, 11);
      w_12 = sub_group_broadcast(w1, 12);
      w_13 = sub_group_broadcast(w1, 13);
      w_14 = sub_group_broadcast(w1, 14);
      w_15 = sub_group_broadcast(w1, 15);

      r_w0_h0_s0.x += w_0.x * src_w0_h0_s1.x;
      r_w0_h0_s0.y += w_0.y * src_w0_h0_s1.x;
      r_w0_h0_s0.z += w_0.z * src_w0_h0_s1.x;
      r_w0_h0_s0.w += w_0.w * src_w0_h0_s1.x;
      r_w0_h0_s0.x += w_1.x * src_w0_h0_s1.y;
      r_w0_h0_s0.y += w_1.y * src_w0_h0_s1.y;
      r_w0_h0_s0.z += w_1.z * src_w0_h0_s1.y;
      r_w0_h0_s0.w += w_1.w * src_w0_h0_s1.y;
      r_w0_h0_s0.x += w_2.x * src_w0_h0_s1.z;
      r_w0_h0_s0.y += w_2.y * src_w0_h0_s1.z;
      r_w0_h0_s0.z += w_2.z * src_w0_h0_s1.z;
      r_w0_h0_s0.w += w_2.w * src_w0_h0_s1.z;
      r_w0_h0_s0.x += w_3.x * src_w0_h0_s1.w;
      r_w0_h0_s0.y += w_3.y * src_w0_h0_s1.w;
      r_w0_h0_s0.z += w_3.z * src_w0_h0_s1.w;
      r_w0_h0_s0.w += w_3.w * src_w0_h0_s1.w;
      r_w0_h0_s1.x += w_4.x * src_w0_h0_s1.x;
      r_w0_h0_s1.y += w_4.y * src_w0_h0_s1.x;
      r_w0_h0_s1.z += w_4.z * src_w0_h0_s1.x;
      r_w0_h0_s1.w += w_4.w * src_w0_h0_s1.x;
      r_w0_h0_s1.x += w_5.x * src_w0_h0_s1.y;
      r_w0_h0_s1.y += w_5.y * src_w0_h0_s1.y;
      r_w0_h0_s1.z += w_5.z * src_w0_h0_s1.y;
      r_w0_h0_s1.w += w_5.w * src_w0_h0_s1.y;
      r_w0_h0_s1.x += w_6.x * src_w0_h0_s1.z;
      r_w0_h0_s1.y += w_6.y * src_w0_h0_s1.z;
      r_w0_h0_s1.z += w_6.z * src_w0_h0_s1.z;
      r_w0_h0_s1.w += w_6.w * src_w0_h0_s1.z;
      r_w0_h0_s1.x += w_7.x * src_w0_h0_s1.w;
      r_w0_h0_s1.y += w_7.y * src_w0_h0_s1.w;
      r_w0_h0_s1.z += w_7.z * src_w0_h0_s1.w;
      r_w0_h0_s1.w += w_7.w * src_w0_h0_s1.w;
      r_w0_h0_s2.x += w_8.x * src_w0_h0_s1.x;
      r_w0_h0_s2.y += w_8.y * src_w0_h0_s1.x;
      r_w0_h0_s2.z += w_8.z * src_w0_h0_s1.x;
      r_w0_h0_s2.w += w_8.w * src_w0_h0_s1.x;
      r_w0_h0_s2.x += w_9.x * src_w0_h0_s1.y;
      r_w0_h0_s2.y += w_9.y * src_w0_h0_s1.y;
      r_w0_h0_s2.z += w_9.z * src_w0_h0_s1.y;
      r_w0_h0_s2.w += w_9.w * src_w0_h0_s1.y;
      r_w0_h0_s2.x += w_10.x * src_w0_h0_s1.z;
      r_w0_h0_s2.y += w_10.y * src_w0_h0_s1.z;
      r_w0_h0_s2.z += w_10.z * src_w0_h0_s1.z;
      r_w0_h0_s2.w += w_10.w * src_w0_h0_s1.z;
      r_w0_h0_s2.x += w_11.x * src_w0_h0_s1.w;
      r_w0_h0_s2.y += w_11.y * src_w0_h0_s1.w;
      r_w0_h0_s2.z += w_11.z * src_w0_h0_s1.w;
      r_w0_h0_s2.w += w_11.w * src_w0_h0_s1.w;
      r_w0_h0_s3.x += w_12.x * src_w0_h0_s1.x;
      r_w0_h0_s3.y += w_12.y * src_w0_h0_s1.x;
      r_w0_h0_s3.z += w_12.z * src_w0_h0_s1.x;
      r_w0_h0_s3.w += w_12.w * src_w0_h0_s1.x;
      r_w0_h0_s3.x += w_13.x * src_w0_h0_s1.y;
      r_w0_h0_s3.y += w_13.y * src_w0_h0_s1.y;
      r_w0_h0_s3.z += w_13.z * src_w0_h0_s1.y;
      r_w0_h0_s3.w += w_13.w * src_w0_h0_s1.y;
      r_w0_h0_s3.x += w_14.x * src_w0_h0_s1.z;
      r_w0_h0_s3.y += w_14.y * src_w0_h0_s1.z;
      r_w0_h0_s3.z += w_14.z * src_w0_h0_s1.z;
      r_w0_h0_s3.w += w_14.w * src_w0_h0_s1.z;
      r_w0_h0_s3.x += w_15.x * src_w0_h0_s1.w;
      r_w0_h0_s3.y += w_15.y * src_w0_h0_s1.w;
      r_w0_h0_s3.z += w_15.z * src_w0_h0_s1.w;
      r_w0_h0_s3.w += w_15.w * src_w0_h0_s1.w;
    }
    if (get_sub_group_size() == 32) {
      FLTOP4 w0 = convert_FLTOP4(weights_buffer[filters_offset + get_sub_group_local_id()]);
      filters_offset += 32;

      FLTOP4 w_0 = sub_group_broadcast(w0, 0);
      FLTOP4 w_1 = sub_group_broadcast(w0, 1);
      FLTOP4 w_2 = sub_group_broadcast(w0, 2);
      FLTOP4 w_3 = sub_group_broadcast(w0, 3);
      FLTOP4 w_4 = sub_group_broadcast(w0, 4);
      FLTOP4 w_5 = sub_group_broadcast(w0, 5);
      FLTOP4 w_6 = sub_group_broadcast(w0, 6);
      FLTOP4 w_7 = sub_group_broadcast(w0, 7);
      FLTOP4 w_8 = sub_group_broadcast(w0, 8);
      FLTOP4 w_9 = sub_group_broadcast(w0, 9);
      FLTOP4 w_10 = sub_group_broadcast(w0, 10);
      FLTOP4 w_11 = sub_group_broadcast(w0, 11);
      FLTOP4 w_12 = sub_group_broadcast(w0, 12);
      FLTOP4 w_13 = sub_group_broadcast(w0, 13);
      FLTOP4 w_14 = sub_group_broadcast(w0, 14);
      FLTOP4 w_15 = sub_group_broadcast(w0, 15);

      r_w0_h0_s0.x += w_0.x * src_w0_h0_s0.x;
      r_w0_h0_s0.y += w_0.y * src_w0_h0_s0.x;
      r_w0_h0_s0.z += w_0.z * src_w0_h0_s0.x;
      r_w0_h0_s0.w += w_0.w * src_w0_h0_s0.x;
      r_w0_h0_s0.x += w_1.x * src_w0_h0_s0.y;
      r_w0_h0_s0.y += w_1.y * src_w0_h0_s0.y;
      r_w0_h0_s0.z += w_1.z * src_w0_h0_s0.y;
      r_w0_h0_s0.w += w_1.w * src_w0_h0_s0.y;
      r_w0_h0_s0.x += w_2.x * src_w0_h0_s0.z;
      r_w0_h0_s0.y += w_2.y * src_w0_h0_s0.z;
      r_w0_h0_s0.z += w_2.z * src_w0_h0_s0.z;
      r_w0_h0_s0.w += w_2.w * src_w0_h0_s0.z;
      r_w0_h0_s0.x += w_3.x * src_w0_h0_s0.w;
      r_w0_h0_s0.y += w_3.y * src_w0_h0_s0.w;
      r_w0_h0_s0.z += w_3.z * src_w0_h0_s0.w;
      r_w0_h0_s0.w += w_3.w * src_w0_h0_s0.w;
      r_w0_h0_s1.x += w_4.x * src_w0_h0_s0.x;
      r_w0_h0_s1.y += w_4.y * src_w0_h0_s0.x;
      r_w0_h0_s1.z += w_4.z * src_w0_h0_s0.x;
      r_w0_h0_s1.w += w_4.w * src_w0_h0_s0.x;
      r_w0_h0_s1.x += w_5.x * src_w0_h0_s0.y;
      r_w0_h0_s1.y += w_5.y * src_w0_h0_s0.y;
      r_w0_h0_s1.z += w_5.z * src_w0_h0_s0.y;
      r_w0_h0_s1.w += w_5.w * src_w0_h0_s0.y;
      r_w0_h0_s1.x += w_6.x * src_w0_h0_s0.z;
      r_w0_h0_s1.y += w_6.y * src_w0_h0_s0.z;
      r_w0_h0_s1.z += w_6.z * src_w0_h0_s0.z;
      r_w0_h0_s1.w += w_6.w * src_w0_h0_s0.z;
      r_w0_h0_s1.x += w_7.x * src_w0_h0_s0.w;
      r_w0_h0_s1.y += w_7.y * src_w0_h0_s0.w;
      r_w0_h0_s1.z += w_7.z * src_w0_h0_s0.w;
      r_w0_h0_s1.w += w_7.w * src_w0_h0_s0.w;
      r_w0_h0_s2.x += w_8.x * src_w0_h0_s0.x;
      r_w0_h0_s2.y += w_8.y * src_w0_h0_s0.x;
      r_w0_h0_s2.z += w_8.z * src_w0_h0_s0.x;
      r_w0_h0_s2.w += w_8.w * src_w0_h0_s0.x;
      r_w0_h0_s2.x += w_9.x * src_w0_h0_s0.y;
      r_w0_h0_s2.y += w_9.y * src_w0_h0_s0.y;
      r_w0_h0_s2.z += w_9.z * src_w0_h0_s0.y;
      r_w0_h0_s2.w += w_9.w * src_w0_h0_s0.y;
      r_w0_h0_s2.x += w_10.x * src_w0_h0_s0.z;
      r_w0_h0_s2.y += w_10.y * src_w0_h0_s0.z;
      r_w0_h0_s2.z += w_10.z * src_w0_h0_s0.z;
      r_w0_h0_s2.w += w_10.w * src_w0_h0_s0.z;
      r_w0_h0_s2.x += w_11.x * src_w0_h0_s0.w;
      r_w0_h0_s2.y += w_11.y * src_w0_h0_s0.w;
      r_w0_h0_s2.z += w_11.z * src_w0_h0_s0.w;
      r_w0_h0_s2.w += w_11.w * src_w0_h0_s0.w;
      r_w0_h0_s3.x += w_12.x * src_w0_h0_s0.x;
      r_w0_h0_s3.y += w_12.y * src_w0_h0_s0.x;
      r_w0_h0_s3.z += w_12.z * src_w0_h0_s0.x;
      r_w0_h0_s3.w += w_12.w * src_w0_h0_s0.x;
      r_w0_h0_s3.x += w_13.x * src_w0_h0_s0.y;
      r_w0_h0_s3.y += w_13.y * src_w0_h0_s0.y;
      r_w0_h0_s3.z += w_13.z * src_w0_h0_s0.y;
      r_w0_h0_s3.w += w_13.w * src_w0_h0_s0.y;
      r_w0_h0_s3.x += w_14.x * src_w0_h0_s0.z;
      r_w0_h0_s3.y += w_14.y * src_w0_h0_s0.z;
      r_w0_h0_s3.z += w_14.z * src_w0_h0_s0.z;
      r_w0_h0_s3.w += w_14.w * src_w0_h0_s0.z;
      r_w0_h0_s3.x += w_15.x * src_w0_h0_s0.w;
      r_w0_h0_s3.y += w_15.y * src_w0_h0_s0.w;
      r_w0_h0_s3.z += w_15.z * src_w0_h0_s0.w;
      r_w0_h0_s3.w += w_15.w * src_w0_h0_s0.w;

      w_0 = sub_group_broadcast(w0, 16 + 0);
      w_1 = sub_group_broadcast(w0, 16 + 1);
      w_2 = sub_group_broadcast(w0, 16 + 2);
      w_3 = sub_group_broadcast(w0, 16 + 3);
      w_4 = sub_group_broadcast(w0, 16 + 4);
      w_5 = sub_group_broadcast(w0, 16 + 5);
      w_6 = sub_group_broadcast(w0, 16 + 6);
      w_7 = sub_group_broadcast(w0, 16 + 7);
      w_8 = sub_group_broadcast(w0, 16 + 8);
      w_9 = sub_group_broadcast(w0, 16 + 9);
      w_10 = sub_group_broadcast(w0, 16 + 10);
      w_11 = sub_group_broadcast(w0, 16 + 11);
      w_12 = sub_group_broadcast(w0, 16 + 12);
      w_13 = sub_group_broadcast(w0, 16 + 13);
      w_14 = sub_group_broadcast(w0, 16 + 14);
      w_15 = sub_group_broadcast(w0, 16 + 15);

      r_w0_h0_s0.x += w_0.x * src_w0_h0_s1.x;
      r_w0_h0_s0.y += w_0.y * src_w0_h0_s1.x;
      r_w0_h0_s0.z += w_0.z * src_w0_h0_s1.x;
      r_w0_h0_s0.w += w_0.w * src_w0_h0_s1.x;
      r_w0_h0_s0.x += w_1.x * src_w0_h0_s1.y;
      r_w0_h0_s0.y += w_1.y * src_w0_h0_s1.y;
      r_w0_h0_s0.z += w_1.z * src_w0_h0_s1.y;
      r_w0_h0_s0.w += w_1.w * src_w0_h0_s1.y;
      r_w0_h0_s0.x += w_2.x * src_w0_h0_s1.z;
      r_w0_h0_s0.y += w_2.y * src_w0_h0_s1.z;
      r_w0_h0_s0.z += w_2.z * src_w0_h0_s1.z;
      r_w0_h0_s0.w += w_2.w * src_w0_h0_s1.z;
      r_w0_h0_s0.x += w_3.x * src_w0_h0_s1.w;
      r_w0_h0_s0.y += w_3.y * src_w0_h0_s1.w;
      r_w0_h0_s0.z += w_3.z * src_w0_h0_s1.w;
      r_w0_h0_s0.w += w_3.w * src_w0_h0_s1.w;
      r_w0_h0_s1.x += w_4.x * src_w0_h0_s1.x;
      r_w0_h0_s1.y += w_4.y * src_w0_h0_s1.x;
      r_w0_h0_s1.z += w_4.z * src_w0_h0_s1.x;
      r_w0_h0_s1.w += w_4.w * src_w0_h0_s1.x;
      r_w0_h0_s1.x += w_5.x * src_w0_h0_s1.y;
      r_w0_h0_s1.y += w_5.y * src_w0_h0_s1.y;
      r_w0_h0_s1.z += w_5.z * src_w0_h0_s1.y;
      r_w0_h0_s1.w += w_5.w * src_w0_h0_s1.y;
      r_w0_h0_s1.x += w_6.x * src_w0_h0_s1.z;
      r_w0_h0_s1.y += w_6.y * src_w0_h0_s1.z;
      r_w0_h0_s1.z += w_6.z * src_w0_h0_s1.z;
      r_w0_h0_s1.w += w_6.w * src_w0_h0_s1.z;
      r_w0_h0_s1.x += w_7.x * src_w0_h0_s1.w;
      r_w0_h0_s1.y += w_7.y * src_w0_h0_s1.w;
      r_w0_h0_s1.z += w_7.z * src_w0_h0_s1.w;
      r_w0_h0_s1.w += w_7.w * src_w0_h0_s1.w;
      r_w0_h0_s2.x += w_8.x * src_w0_h0_s1.x;
      r_w0_h0_s2.y += w_8.y * src_w0_h0_s1.x;
      r_w0_h0_s2.z += w_8.z * src_w0_h0_s1.x;
      r_w0_h0_s2.w += w_8.w * src_w0_h0_s1.x;
      r_w0_h0_s2.x += w_9.x * src_w0_h0_s1.y;
      r_w0_h0_s2.y += w_9.y * src_w0_h0_s1.y;
      r_w0_h0_s2.z += w_9.z * src_w0_h0_s1.y;
      r_w0_h0_s2.w += w_9.w * src_w0_h0_s1.y;
      r_w0_h0_s2.x += w_10.x * src_w0_h0_s1.z;
      r_w0_h0_s2.y += w_10.y * src_w0_h0_s1.z;
      r_w0_h0_s2.z += w_10.z * src_w0_h0_s1.z;
      r_w0_h0_s2.w += w_10.w * src_w0_h0_s1.z;
      r_w0_h0_s2.x += w_11.x * src_w0_h0_s1.w;
      r_w0_h0_s2.y += w_11.y * src_w0_h0_s1.w;
      r_w0_h0_s2.z += w_11.z * src_w0_h0_s1.w;
      r_w0_h0_s2.w += w_11.w * src_w0_h0_s1.w;
      r_w0_h0_s3.x += w_12.x * src_w0_h0_s1.x;
      r_w0_h0_s3.y += w_12.y * src_w0_h0_s1.x;
      r_w0_h0_s3.z += w_12.z * src_w0_h0_s1.x;
      r_w0_h0_s3.w += w_12.w * src_w0_h0_s1.x;
      r_w0_h0_s3.x += w_13.x * src_w0_h0_s1.y;
      r_w0_h0_s3.y += w_13.y * src_w0_h0_s1.y;
      r_w0_h0_s3.z += w_13.z * src_w0_h0_s1.y;
      r_w0_h0_s3.w += w_13.w * src_w0_h0_s1.y;
      r_w0_h0_s3.x += w_14.x * src_w0_h0_s1.z;
      r_w0_h0_s3.y += w_14.y * src_w0_h0_s1.z;
      r_w0_h0_s3.z += w_14.z * src_w0_h0_s1.z;
      r_w0_h0_s3.w += w_14.w * src_w0_h0_s1.z;
      r_w0_h0_s3.x += w_15.x * src_w0_h0_s1.w;
      r_w0_h0_s3.y += w_15.y * src_w0_h0_s1.w;
      r_w0_h0_s3.z += w_15.z * src_w0_h0_s1.w;
      r_w0_h0_s3.w += w_15.w * src_w0_h0_s1.w;
    }
    s += 2;
    if (s >= shared_int4_0.z) { break; }
  }
  if (DST_Y >= shared_int4_0.x || DST_S >= shared_int4_0.y) {
    return;
  }
  if (DST_S + 0 >= shared_int4_0.y) { return; }
  {
    FLTOP4 bias_val = convert_FLTOP4(biases_buffer[(DST_S + 0)]);
  {
    FLTOP4 res = r_w0_h0_s0 + bias_val;
    WRITEIMG(dst_tensor_image2d, (int2)((DST_X + 0), ((DST_Y + 0) * shared_int4_0.y + (DST_S + 0))), convert_FLTD4(res));
  }
  }
  if (DST_S + 1 >= shared_int4_0.y) { return; }
  {
    FLTOP4 bias_val = convert_FLTOP4(biases_buffer[(DST_S + 1)]);
  {
    FLTOP4 res = r_w0_h0_s1 + bias_val;
    WRITEIMG(dst_tensor_image2d, (int2)((DST_X + 0), ((DST_Y + 0) * shared_int4_0.y + (DST_S + 1))), convert_FLTD4(res));
  }
  }
  if (DST_S + 2 >= shared_int4_0.y) { return; }
  {
    FLTOP4 bias_val = convert_FLTOP4(biases_buffer[(DST_S + 2)]);
  {
    FLTOP4 res = r_w0_h0_s2 + bias_val;
    WRITEIMG(dst_tensor_image2d, (int2)((DST_X + 0), ((DST_Y + 0) * shared_int4_0.y + (DST_S + 2))), convert_FLTD4(res));
  }
  }
  if (DST_S + 3 >= shared_int4_0.y) { return; }
  {
    FLTOP4 bias_val = convert_FLTOP4(biases_buffer[(DST_S + 3)]);
  {
    FLTOP4 res = r_w0_h0_s3 + bias_val;
    WRITEIMG(dst_tensor_image2d, (int2)((DST_X + 0), ((DST_Y + 0) * shared_int4_0.y + (DST_S + 3))), convert_FLTD4(res));
  }
  }
}
)";

void TestOpenCL() {
    auto platform = cl::Platform::getDefault();

    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
    if (devices.empty() ) {
      std::cout << "no devices" << std::endl;
      return;
    }

    uint32_t deviceIndex = 0;
    std::string selectedVendor;
    for (size_t i = 0; i < devices.size(); ++i) {
      std::string vendor;
      devices[i].getInfo(CL_DEVICE_VENDOR, &vendor);
      if (vendor.find("ntel") != std::string::npos || selectedVendor.empty()) {
        selectedVendor = vendor;
        deviceIndex = i;
      }
    }

    std::cout << "Testing OpenCL on " << selectedVendor << std::endl;

    cl::Context context(devices[deviceIndex]);

    uint32_t wgSize = 64;

    std::string shader = kOpenCLShader;
    if (absl::GetFlag(FLAGS_f16_data) || absl::GetFlag(FLAGS_f16_op)) {
      shader = "#pragma OPENCL EXTENSION cl_khr_fp16 : enable\n" + shader;
    }
    if (absl::GetFlag(FLAGS_f16_data)) {
      shader = "#define FLTD4 half4\n#define READIMG read_imageh\n#define WRITEIMG write_imageh\n" + shader;
      shader = "#define convert_FLTD4 convert_half4\n" + shader;
    } else {
      shader = "#define FLTD4 float4\n#define READIMG read_imagef\n#define WRITEIMG write_imagef\n" + shader;
      shader = "#define convert_FLTD4 convert_float4\n" + shader;
    }
    if (absl::GetFlag(FLAGS_f16_op)) {
      shader = "#define FLTOP4 half4\n" + shader;
      shader = "#define convert_FLTOP4 convert_half4\n" + shader;
    } else {
      shader = "#define FLTOP4 float4\n" + shader;
      shader = "#define convert_FLTOP4 convert_float4\n" + shader;
    }
    cl::Program program(context, shader.c_str(), true);
    {
        cl_int buildErr = CL_SUCCESS;
        auto buildInfo = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(&buildErr);
        for (auto& pair : buildInfo) {
            std::cerr << pair.second << std::endl << std::endl;
        }
        if (buildErr != CL_SUCCESS) {
            return;
        }
    }
    cl::CommandQueue queue(context);

    auto ftype = CL_FLOAT;
    auto fsize = sizeof(float);
    if (absl::GetFlag(FLAGS_f16_data)) {
      ftype = CL_HALF_FLOAT;
      fsize = fsize / 2;
    }

    // Tensor size is divided by 4 in the Y dimension because 4 floats are packed in each texel.
    cl::Image2D dstTensor(context, CL_MEM_WRITE_ONLY, cl::ImageFormat(CL_RGBA, ftype),
                          kSharedDim, kDstDim / 4);
    cl::Image2D srcTensor(context, CL_MEM_READ_ONLY, cl::ImageFormat(CL_RGBA, ftype), kSharedDim,
                          kSrcDim / 4);
    cl::Buffer biasTensor(context, CL_MEM_READ_ONLY, kDstDim * fsize);
    cl::Buffer weightsTensor(context, CL_MEM_READ_ONLY, kDstDim * kSrcDim * fsize);

    auto kernel =
        cl::KernelFunctor<cl::Buffer, cl::Buffer, cl::Image2D, cl::Image2D, cl_int4, cl_int4>(
            program, "main_function");

    double min = std::numeric_limits<double>::infinity();
    double max = -1;
    double avg = 0;
    for (uint32_t i = 0; i < absl::GetFlag(FLAGS_trials); ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        for (uint32_t d = 0; d < absl::GetFlag(FLAGS_dispatches); ++d) {
            queue.enqueueBarrierWithWaitList();
            kernel(cl::EnqueueArgs(queue, cl::NDRange(kSharedDim, kDstDim / 4 / 4, 1),
                                   cl::NDRange(wgSize, 1, 1)),
                   biasTensor, weightsTensor, dstTensor, srcTensor,
                   {1, kDstDim / 4, kSrcDim / 4, kSharedDim}, {1, 0, 0, 0});
        }
        queue.finish();

        double duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                              std::chrono::high_resolution_clock::now() - start)
                              .count() *
                          1.0e-6;
        if (duration < min) {
            min = duration;
        }
        if (duration > max) {
            max = duration;
        }
        avg += duration / absl::GetFlag(FLAGS_trials);
    }
    std::cout << "Min: " << min << " ms " << std::endl;
    std::cout << "Max: " << max << " ms " << std::endl;
    std::cout << "Avg: " << avg << " ms " << std::endl;
}

void TestWebGPU() {
    dawnProcSetProcs(&dawn::native::GetProcs());

    uint32_t wgSize = 32;

    std::unique_ptr<dawn::native::Instance> instance = std::make_unique<dawn::native::Instance>();

    wgpu::RequestAdapterOptions adapterOptions;
    auto adapters = instance->EnumerateAdapters(&adapterOptions);
    if (!adapters.empty()) {
        auto adapter = adapters[0];

        wgpu::AdapterProperties properties;
        adapter.GetProperties(&properties);
        std::cout << "Dawn using " << properties.name << std::endl;

        std::vector<const char*> enabledToggles = {
            "allow_unsafe_apis",
            "disable_workgroup_init",
            "disable_robustness",
            "fxc_optimizations",
            "d3d_disable_ieee_strictness",
        };

        if (absl::GetFlag(FLAGS_dump_shaders)) {
            enabledToggles.push_back("dump_shaders");
        }

        if (absl::GetFlag(FLAGS_timestamp_period) > 0) {
            enabledToggles.push_back("disable_timestamp_query_conversion");
        }

        const char* disabledToggles[] = {
            "lazy_clear_resource_on_first_use",
        };

        wgpu::DawnTogglesDescriptor togglesDesc;
        togglesDesc.enabledToggles = enabledToggles.data();
        togglesDesc.enabledToggleCount = enabledToggles.size();
        togglesDesc.disabledToggles = disabledToggles;
        togglesDesc.disabledToggleCount = sizeof(disabledToggles) / sizeof(disabledToggles[0]);

        wgpu::DeviceDescriptor deviceDesc;
        deviceDesc.nextInChain = &togglesDesc;

        std::vector<wgpu::FeatureName> requiredFeatures = {wgpu::FeatureName::TimestampQuery,
                                                wgpu::FeatureName::ChromiumExperimentalSubgroups};
        if (absl::GetFlag(FLAGS_f16_data) || absl::GetFlag(FLAGS_f16_op)) {
            requiredFeatures.push_back(wgpu::FeatureName::ShaderF16);
        }
        deviceDesc.requiredFeatures = requiredFeatures.data();
        deviceDesc.requiredFeatureCount = requiredFeatures.size();
        deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message,
                                           void* userdata) {
            if (reason == WGPUDeviceLostReason_Destroyed) {
                return;
            }
            if (message) {
                std::cerr << message << std::endl;
            }
        };
        auto device = wgpu::Device::Acquire(adapter.CreateDevice(&deviceDesc));
        if (!device) {
          return;
        }
        device.SetUncapturedErrorCallback(
            [](WGPUErrorType type, char const* message, void* userdata) {
                if (message) {
                    std::cerr << message << std::endl;
                }
            },
            nullptr);
        device.SetLoggingCallback(
            [](WGPULoggingType type, char const* message, void* userdata) {
                if (message) {
                    std::cout << message << std::endl;
                }
            },
            nullptr);

        wgpu::ShaderModuleWGSLDescriptor shaderModuleWGSLDesc;
        std::string shader = kWGSLShader;
        if (absl::GetFlag(FLAGS_f16_data)) {
          shader = "alias fdtype=f16;\nalias storetype=texture_storage_2d<rgba16float, write>;\n" + shader;
        } else {
          shader = "alias fdtype=f32;\nalias storetype=texture_storage_2d<rgba32float, write>;\n" + shader;
        }
        if (absl::GetFlag(FLAGS_f16_op)) {
          shader = "alias foptype=f16;\n" + shader;
          std::regex re("subgroupBroadcast4\\((.+?), (.+?)\\)");
          while (std::regex_search(shader, re)) {
            shader = std::regex_replace(shader, re, R"(bitcast<vec4<f16>>(vec2<u32>(
              subgroupBroadcast(bitcast<vec2<u32>>($1)[0], $2),
              subgroupBroadcast(bitcast<vec2<u32>>($1)[1], $2)
            )))");
          }
        } else {
          shader = "alias foptype=f32;\n" + shader;
          std::regex re("subgroupBroadcast4\\((.+?), (.+?)\\)");
          while (std::regex_search(shader, re)) {
            shader = std::regex_replace(shader, re, R"(vec4<f32>(
              subgroupBroadcast($1[0], $2),
              subgroupBroadcast($1[1], $2),
              subgroupBroadcast($1[2], $2),
              subgroupBroadcast($1[3], $2),
            ))");
          }
        }
        shader = "enable chromium_experimental_subgroups;\n" + shader;
        if (absl::GetFlag(FLAGS_f16_data) || absl::GetFlag(FLAGS_f16_op)) {
          shader = "enable f16;\n" + shader;
        }

        shaderModuleWGSLDesc.code = shader.c_str();

        wgpu::ShaderModuleDescriptor shaderModuleDesc;
        shaderModuleDesc.nextInChain = &shaderModuleWGSLDesc;
        wgpu::ShaderModule shaderModule = device.CreateShaderModule(&shaderModuleDesc);

        wgpu::ComputePipelineDescriptor pipelineDesc;
        pipelineDesc.compute.module = shaderModule;
        pipelineDesc.compute.entryPoint = "main";
        wgpu::ConstantEntry wgSizeEntry;
        wgSizeEntry.key = "wg_size";
        wgSizeEntry.value = wgSize;
        pipelineDesc.compute.constants = &wgSizeEntry;
        pipelineDesc.compute.constantCount = 1u;
        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);

        wgpu::BufferDescriptor uniformBufferDesc;
        uniformBufferDesc.size = sizeof(int32_t) * 2 * 4;
        uniformBufferDesc.usage = wgpu::BufferUsage::Uniform;
        uniformBufferDesc.mappedAtCreation = true;
        wgpu::Buffer uniformBuffer = device.CreateBuffer(&uniformBufferDesc);
        {
            uint32_t* uniformData = static_cast<uint32_t*>(uniformBuffer.GetMappedRange());
            uniformData[0] = 1;
            uniformData[1] = kDstDim / 4;
            uniformData[2] = kSrcDim / 4;
            uniformData[3] = kSharedDim;
            uniformData[4] = 1;
            uniformData[5] = 0;
            uniformData[6] = 0;
            uniformData[7] = 0;
        }

        uniformBuffer.Unmap();

        wgpu::TextureDescriptor textureDesc;
        if (absl::GetFlag(FLAGS_f16_data)) {
            textureDesc.format = wgpu::TextureFormat::RGBA16Float;
        } else {
            textureDesc.format = wgpu::TextureFormat::RGBA32Float;
        }

        // Tensor size is divided by 4 in the Y dimension because 4 floats are packed in each texel.
        textureDesc.size = {kSharedDim, kDstDim / 4};
        textureDesc.usage = wgpu::TextureUsage::StorageBinding;
        textureDesc.label = "dstTensor";
        wgpu::Texture dstTensor = device.CreateTexture(&textureDesc);

        textureDesc.size = {kSharedDim, kSrcDim / 4};
        textureDesc.usage = wgpu::TextureUsage::TextureBinding;
        textureDesc.label = "srcTensor";
        wgpu::Texture srcTensor = device.CreateTexture(&textureDesc);

        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.usage = wgpu::BufferUsage::Storage;

        bufferDesc.size = kDstDim * sizeof(float);
        if (absl::GetFlag(FLAGS_f16_data)) {
          bufferDesc.size /= 2;
        }
        bufferDesc.label = "biasTensor";
        wgpu::Buffer biasTensor = device.CreateBuffer(&bufferDesc);

        bufferDesc.size = kDstDim * kSrcDim * sizeof(float);
        if (absl::GetFlag(FLAGS_f16_data)) {
          bufferDesc.size /= 2;
        }
        bufferDesc.label = "weightsTensor";
        wgpu::Buffer weightsTensor = device.CreateBuffer(&bufferDesc);

        wgpu::BindGroupDescriptor bindGroupDesc;
        wgpu::BindGroupEntry bindGroupEntries[5];
        bindGroupEntries[0].binding = 0;
        bindGroupEntries[0].textureView = dstTensor.CreateView();
        bindGroupEntries[1].binding = 1;
        bindGroupEntries[1].textureView = srcTensor.CreateView();
        bindGroupEntries[2].binding = 2;
        bindGroupEntries[2].buffer = biasTensor;
        bindGroupEntries[3].binding = 3;
        bindGroupEntries[3].buffer = weightsTensor;
        bindGroupEntries[4].binding = 4;
        bindGroupEntries[4].buffer = uniformBuffer;

        bindGroupDesc.layout = pipeline.GetBindGroupLayout(0);
        bindGroupDesc.entries = bindGroupEntries;
        bindGroupDesc.entryCount = sizeof(bindGroupEntries) / sizeof(bindGroupEntries[0]);
        wgpu::BindGroup bindGroup = device.CreateBindGroup(&bindGroupDesc);

        wgpu::QuerySetDescriptor querySetDesc;
        querySetDesc.type = wgpu::QueryType::Timestamp;
        querySetDesc.count = 2 * absl::GetFlag(FLAGS_trials);
        wgpu::QuerySet querySet = device.CreateQuerySet(&querySetDesc);

        bufferDesc.size = sizeof(uint64_t) * 2 * absl::GetFlag(FLAGS_trials);
        bufferDesc.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
        bufferDesc.label = "queryResult";
        wgpu::Buffer querySetResults = device.CreateBuffer(&bufferDesc);

        bufferDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        bufferDesc.label = "queryReadBack";
        wgpu::Buffer querySetReadback = device.CreateBuffer(&bufferDesc);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        for (uint32_t i = 0; i < absl::GetFlag(FLAGS_trials); ++i) {
            wgpu::ComputePassDescriptor computePassDesc;
            wgpu::ComputePassTimestampWrites timestampWrites = {querySet, 2 * i, 2 * i + 1};
            computePassDesc.timestampWrites = &timestampWrites;
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass(&computePassDesc);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            for (uint32_t d = 0; d < absl::GetFlag(FLAGS_dispatches); ++d) {
                pass.DispatchWorkgroups(kSharedDim / wgSize, kDstDim / 4 / 4, 1);
            }
            pass.End();
        }
        encoder.ResolveQuerySet(querySet, 0, 2 * absl::GetFlag(FLAGS_trials), querySetResults, 0);
        encoder.CopyBufferToBuffer(querySetResults, 0, querySetReadback, 0,
                                   sizeof(uint64_t) * 2 * absl::GetFlag(FLAGS_trials));
        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        device.GetQueue().Submit(1, &commandBuffer);

        bool done = false;
        querySetReadback.MapAsync(
            wgpu::MapMode::Read, 0, wgpu::kWholeSize,
            [](WGPUBufferMapAsyncStatus status, void* userdata) {
                if (status != WGPUBufferMapAsyncStatus_Success) {
                    abort();
                }
                *static_cast<bool*>(userdata) = true;
            },
            &done);
        while (!done) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(100ms);
            device.Tick();
        }
        const uint64_t* timestamps =
            static_cast<const uint64_t*>(querySetReadback.GetConstMappedRange());
        double min = std::numeric_limits<double>::infinity();
        double max = -1;
        double avg = 0;
        for (uint32_t i = 0; i < absl::GetFlag(FLAGS_trials); ++i) {
            double duration =
                static_cast<double>(timestamps[2 * i + 1] - timestamps[2 * i]) * 1.0e-6;
            double period = absl::GetFlag(FLAGS_timestamp_period);
            if (period > 0) {
                duration *= period;
            }
            if (duration < min) {
                min = duration;
            }
            if (duration > max) {
                max = duration;
            }
            avg += duration / absl::GetFlag(FLAGS_trials);
        }
        std::cout << "Min: " << min << " ms " << std::endl;
        std::cout << "Max: " << max << " ms " << std::endl;
        std::cout << "Avg: " << avg << " ms " << std::endl;
    }
}

int main(int argc, char** argv) {
    absl::ParseCommandLine(argc, argv);

    TestOpenCL();
    TestWebGPU();
}
