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

const char g_passthrough_2d_4_channel_frag[] =
    " \
    [[binding 1, set 0]] var<uniform> my_sampler: sampler; \
    [[binding 2, set 0]] var<uniform> my_texture: texture_sampled_2d<f32>; \
    [[location 0]] var<in> fragUV : vec2<f32>; \
    [[location 0]] var<out> rgbaColor : vec4<f32>; \
    fn fragment_main() -> void { \
        rgbaColor = texture_sample(my_texture, my_sampler, fragUV); \
        return; \
    } \
    entry_point fragment as \"main\" = fragment_main; \
";