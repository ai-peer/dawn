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

#ifndef DAWNNATIVE_QUERYHELPER_H_
#define DAWNNATIVE_QUERYHELPER_H_

namespace dawn_native {

    // TODO (hao.x.li@intel.com): Move to cpp file when adding the help functions.
    static const char sTimestampCompute[] = R"(
            [[block]] struct Timestamps {
                # Low 32-bits : t[0].x
                # High 32-bits: t[0].y
                [[offset(0)]] t : [[stride(8)]] array<vec2<u32>>;
            };

            [[block]] struct TsParams {
                [[offset(0)]] maxCount : u32;
                [[offset(4)]] period : f32;
            };

            [[set(0), binding(0)]] var<storage_buffer> input : Timestamps;
            [[set(0), binding(1)]] var<storage_buffer> output : Timestamps;
            [[set(0), binding(2)]] var<uniform> params : TsParams;

            [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

            [[stage(compute), workgroup_size(1, 1, 1)]]
            fn main() -> void {
                var index : u32 = GlobalInvocationID.x;

                if (index >= params.maxCount) { return; }
                if (input.t[index].x == 0u && input.t[index].y == 0u) { return; }

                # Find the first non-zero value as a timestamp baseline
                var baseIndex : u32 = 0u;
                for (var i : u32 = 0u; i <= index; i = i + 1) {
                    if (input.t[i].x != 0 || input.t[i].y != 0) {
                        baseIndex = i;
                        break;
                    }
                }

                # If the timestamp is less than the last timestamp, it means this is a
                # timestamp which has been reset, set all the following to zero.
                var prev : vec2<u32> = vec2<u32>(0u, 0u);
                for (var i : u32 = 1u; i <= index; i = i + 1) {
                    # Record the previos timestamp. For example,
                    # if the src data is [0, t0, 0, t1], the previos timestamp should be t0.
                    if (prev.x == 0 && prev.y == 0 && input.t[i].x != 0 && input.t[i].y != 0 ) {
                        prev.x = input.t[i].x;
                        prev.y = input.t[i].y;
                    }

                    # Skip the zero value checking which is not a timestamp.
                    if (input.t[i].x == 0 && input.t[i].y == 0) { continue; }

                    if (input.t[i].y < prev.y) {
                        return;
                    }
                    if (input.t[i].y == prev.y && input.t[i].x < prev.x) {
                        return;
                    }

                    # Record the current timestamp as the previos.
                    prev.x = input.t[i].x;
                    prev.y = input.t[i].y;
                }

                # Subtract each value from the baseline
                output.t[index].y = input.t[index].y - input.t[baseIndex].y;
                if (input.t[index].x < input.t[baseIndex].x) {
                output.t[index].y = output.t[index].y - 1u;
                }
                output.t[index].x = input.t[index].x - input.t[baseIndex].x;

                # Multiply output values with params.period
                var w: u32 = 0u;
                # Check if the product of low 32-bits and the period exceeds the maximum of u32.
                # Otherwise, just do the multiplication
                if (output.t[index].x > u32(f32(0xFFFFFFFFu) / params.period)) {
                    # Use two u32 to represent the high 16-bits and low 16-bits of this u32
                    var lo : u32 = output.t[index].x & 0xFFFF;
                    var hi : u32 = output.t[index].x >> 16;

                    var t0 : u32 = u32(round(f32(lo) * params.period));
                    var t1 : u32 = u32(round(f32(hi) * params.period)) + (t0 >> 16);
                    w = t1 >> 16;

                    var result_lo : u32 = t1 << 16;
                    result_lo = result_lo | (t0 & 0xFFFF);
                    output.t[index].x = result_lo;
                } else {
                    output.t[index].x = u32(round(f32(output.t[index].x) * params.period));
                }

                # Get the nearest integer to the float result. For high 32bits, the round function
                # will greatly help reduce the accuracy loss of the final result.
                output.t[index].y = u32(round(f32(output.t[index].y) * params.period)) + w;

                return;
            }
        )";

}  // namespace dawn_native

#endif  // DAWNNATIVE_QUERYHELPER_H_
