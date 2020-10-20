// Copyright 2020 The Dawn Authors
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

const char g_blit_texture_vertex[] =
    " \
    type Uniforms = [[block]] struct { \
        [[offset(0)]] rotationMatrix : mat4x4<f32>; \
    }; \
    [[location(0)]] var<in> pos : vec3<f32>; \
    [[location(1)]] var<in> uv : vec2<f32>; \
    [[location(0)]] var<out> fragUV: vec2<f32>; \
    [[builtin(position)]] var<out> Position : vec4<f32>; \
    [[binding(0), set(0)]] var <uniform> uniforms : Uniforms; \
    [[stage(vertex)]] \
    fn vertex_main() -> void { \
      Position = uniforms.rotationMatrix * vec4<f32>(pos, 1.0); \
      fragUV = uv; \
      return; \
    } \
";