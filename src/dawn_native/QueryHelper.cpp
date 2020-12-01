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

#include "dawn_native/QueryHelper.h"

#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/ComputePassEncoder.h"
#include "dawn_native/ComputePipeline.h"
#include "dawn_native/Device.h"
#include "dawn_native/InternalPipelineStore.h"

namespace dawn_native {

    namespace {

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

            [[set(0), binding(0)]] var<storage_buffer> input : [[access(read)]] Timestamps;
            [[set(0), binding(1)]] var<storage_buffer> output : [[access(read_write)]] Timestamps;
            [[set(0), binding(2)]] var<uniform> params : TsParams;

            [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

            [[stage(compute), workgroup_size(8, 1, 1)]]
            fn main() -> void {
                var index : u32 = GlobalInvocationID.x;

                if (index >= params.maxCount) { return; }

                var currentIn: vec2<u32> = input.t[index];

                if (currentIn.x == 0u && currentIn.y == 0u) { return; }

                # Check whether there is a reset timestamp (less than the last timestamp),
                # if found, set it and the following to zero.
                var previous : vec2<u32> = vec2<u32>(0u, 0u);
                var baseline : vec2<u32> = vec2<u32>(0u, 0u);
                for (var i : u32 = 0u; i <= index; i = i + 1) {
                    var  timestamp: vec2<u32> = input.t[i];

                    # Skip the zero value which is not a timestamp.
                    if (timestamp.x == 0u && timestamp.y == 0u) { continue; }

                    # Return if a reset timestamp is found
                    if (timestamp.y < previous.y) { return; }
                    if (timestamp.y == previous.y && timestamp.x < previous.x) { return; }

                    # Set this timestamp as previous. For example,
                    # if the src data is [0, t0, 0, t1], the previous timestamp should be t0.
                    previous.x = timestamp.x;
                    previous.y = timestamp.y;

                    # Record the first non-zero timestamp as a baseline.
                    if (baseline.x == 0u &&  baseline.y == 0u) {
                        baseline.x = timestamp.x;
                        baseline.y = timestamp.y;
                    }
                }

                # Subtract each value from the baseline
                var currentOut : vec2<u32> = vec2<u32>(0u, 0u);
                currentOut.x = currentIn.x - baseline.x;
                currentOut.y = currentIn.y - baseline.y;
                if (currentIn.x < baseline.x) {
                    currentOut.y = currentOut.y - 1u;
                }

                # Multiply output values by the period
                var w: u32 = 0u;
                var period : f32 = params.period;

                # If the product of low 32-bits and the period does not exceed the maximum of u32,
                # directly do the multiplication, otherwise, use two u32 to represent the high
                # 16-bits and low 16-bits of this u32, then multiply them by the period separately.
                if (currentOut.x <= u32(f32(0xFFFFFFFFu) / period)) {
                    output.t[index].x = u32(round(f32(currentOut.x) * period));
                } else {
                    var lo : u32 = currentOut.x & 0xFFFF;
                    var hi : u32 = currentOut.x >> 16;

                    var t0 : u32 = u32(round(f32(lo) * period));
                    var t1 : u32 = u32(round(f32(hi) * period)) + (t0 >> 16);
                    w = t1 >> 16;

                    var result : u32 = t1 << 16;
                    result = result | (t0 & 0xFFFF);
                    output.t[index].x = result;
                }

                # Get the nearest integer to the float result. For high 32-bits, the round
                # function will greatly help reduce the accuracy loss of the final result.
                output.t[index].y = u32(round(f32(currentOut.y) * period)) + w;
            }
        )";

        ComputePipelineBase* GetOrCreateTimestampComputePipeline(DeviceBase* device) {
            InternalPipelineStore* store = device->GetInternalPipelineStore();

            if (store->timestampComputePipeline == nullptr) {
                // Create compute shader module if not cached before.
                if (store->timestampCS == nullptr) {
                    ShaderModuleDescriptor descriptor;
                    ShaderModuleWGSLDescriptor wgslDesc;
                    wgslDesc.source = sTimestampCompute;
                    descriptor.nextInChain = reinterpret_cast<ChainedStruct*>(&wgslDesc);

                    store->timestampCS = AcquireRef(device->CreateShaderModule(&descriptor));
                }

                // Create ComputePipeline.
                ComputePipelineDescriptor computePipelineDesc = {};
                // Generate the layout based on shader module.
                computePipelineDesc.layout = nullptr;
                computePipelineDesc.computeStage.module = store->timestampCS.Get();
                computePipelineDesc.computeStage.entryPoint = "main";

                store->timestampComputePipeline =
                    AcquireRef(device->CreateComputePipeline(&computePipelineDesc));
            }

            return store->timestampComputePipeline.Get();
        }

    }  // anonymous namespace

    void DoTimestampCompute(CommandEncoder* encoder,
                            BufferBase* input,
                            BufferBase* output,
                            BufferBase* params) {
        DeviceBase* device = encoder->GetDevice();

        ComputePipelineBase* pipeline = GetOrCreateTimestampComputePipeline(device);

        // Prepare bind group layout.
        Ref<BindGroupLayoutBase> layout = AcquireRef(pipeline->GetBindGroupLayout(0));

        // Prepare bind group descriptor
        BindGroupEntry bindGroupEntries[3] = {};
        BindGroupDescriptor bgDesc = {};
        bgDesc.layout = layout.Get();
        bgDesc.entryCount = 3;
        bgDesc.entries = bindGroupEntries;

        // Set bind group entries.
        bindGroupEntries[0].binding = 0;
        bindGroupEntries[0].buffer = input;
        bindGroupEntries[0].size = input->GetSize();
        bindGroupEntries[1].binding = 1;
        bindGroupEntries[1].buffer = output;
        bindGroupEntries[1].size = input->GetSize();
        bindGroupEntries[2].binding = 2;
        bindGroupEntries[2].buffer = params;
        bindGroupEntries[2].size = params->GetSize();

        // Create bind group after all binding entries are set.
        Ref<BindGroupBase> bindGroup = AcquireRef(device->CreateBindGroup(&bgDesc));

        // Create compute encoder and issue dispatch.
        ComputePassDescriptor passDesc = {};
        Ref<ComputePassEncoder> pass = AcquireRef(encoder->BeginComputePass(&passDesc));
        pass->SetPipeline(pipeline);
        pass->SetBindGroup(0, bindGroup.Get());
        pass->Dispatch(8, 1, 1);
        pass->EndPass();
    }

}  // namespace dawn_native
