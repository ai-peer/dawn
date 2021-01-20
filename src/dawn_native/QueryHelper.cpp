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

        // Assert the offsets in dawn_native::TimestampParams are same with the ones in the shader
        static_assert(offsetof(dawn_native::TimestampParams, count) == 0, "");
        static_assert(offsetof(dawn_native::TimestampParams, offset) == 4, "");
        static_assert(offsetof(dawn_native::TimestampParams, period) == 8, "");

        static const char sConvertTimestampsToNanoseconds[] = R"(
            struct Timestamp {
                [[offset(0)]] low  : u32;
                [[offset(4)]] high : u32;
            };

            [[block]] struct TimestampArr {
                [[offset(0)]] t : [[stride(8)]] array<Timestamp>;
            };

            [[block]] struct AvailabilityArr {
                [[offset(0)]] v : [[stride(4)]] array<u32>;
            };

            [[block]] struct TimestampParams {
                [[offset(0)]]  count  : u32;
                [[offset(4)]]  offset : u32;
                [[offset(8)]]  period : f32;
            };

            [[set(0), binding(0)]]
                var<storage_buffer> timestamps : [[access(read_write)]] TimestampArr;
            [[set(0), binding(1)]]
                var<storage_buffer> availability : [[access(read)]] AvailabilityArr;
            [[set(0), binding(2)]] var<uniform> params : TimestampParams;

            [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

            const sizeofTimestamp : u32 = 8u;

            [[stage(compute), workgroup_size(8, 1, 1)]]
            fn main() -> void {
                if (GlobalInvocationID.x >= params.count) { return; }

                var index : u32 = GlobalInvocationID.x + params.offset / sizeofTimestamp;

                var timestamp : Timestamp = timestamps.t[index];

                # Return 0 for the unavailable value.
                if (availability.v[index] == 0u) {
                    timestamps.t[index].low = 0u;
                    timestamps.t[index].high = 0u;
                    return;
                }

                # Multiply the values in timestamps buffer by the period.
                var period : f32 = params.period;
                var w : u32 = 0u;

                # If the product of low 32-bits and the period does not exceed the maximum of u32,
                # directly do the multiplication, otherwise, use two u32 to represent the high
                # 16-bits and low 16-bits of this u32, then multiply them by the period separately.
                if (timestamp.low <= u32(f32(0xFFFFFFFFu) / period)) {
                    timestamps.t[index].low = u32(round(f32(timestamp.low) * period));
                } else {
                    var lo : u32 = timestamp.low & 0xFFFF;
                    var hi : u32 = timestamp.low >> 16;

                    var t0 : u32 = u32(round(f32(lo) * period));
                    var t1 : u32 = u32(round(f32(hi) * period)) + (t0 >> 16);
                    w = t1 >> 16;

                    var result : u32 = t1 << 16;
                    result = result | (t0 & 0xFFFF);
                    timestamps.t[index].low = result;
                }

                # Get the nearest integer to the float result. For high 32-bits, the round
                # function will greatly help reduce the accuracy loss of the final result.
                timestamps.t[index].high = u32(round(f32(timestamp.high) * period)) + w;
            }
        )";

        // Assert the offsets in dawn_native::OcclusionParams are same with the ones in the shader
        static_assert(offsetof(dawn_native::OcclusionParams, count) == 0, "");
        static_assert(offsetof(dawn_native::OcclusionParams, offset) == 4, "");

        static const char sConvertOcclusionToBinary[] = R"(
            struct Occlusion {
                [[offset(0)]] low  : u32;
                [[offset(4)]] high : u32;
            };

            [[block]] struct OcclusionArr {
                [[offset(0)]] t : [[stride(8)]] array<Occlusion>;
            };

            [[block]] struct AvailabilityArr {
                [[offset(0)]] v : [[stride(4)]] array<u32>;
            };

            [[block]] struct OcclusionParams {
                [[offset(0)]]  count  : u32;
                [[offset(4)]]  offset : u32;
            };

            [[set(0), binding(0)]]
                var<storage_buffer> occlusions : [[access(read_write)]] OcclusionArr;
            [[set(0), binding(1)]]
                var<storage_buffer> availability : [[access(read)]] AvailabilityArr;
            [[set(0), binding(2)]] var<uniform> params : OcclusionParams;

            [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;

            const sizeofOcclusion : u32 = 8u;

            [[stage(compute), workgroup_size(8, 1, 1)]]
            fn main() -> void {
                if (GlobalInvocationID.x >= params.count) { return; }

                var index : u32 = GlobalInvocationID.x + params.offset / sizeofOcclusion;

                # Return 0 for the unavailable value.
                if (availability.v[index] == 0u) {
                    occlusions.t[index].low = 0u;
                    occlusions.t[index].high = 0u;
                    return;
                }

                var occlusion : Occlusion = occlusions.t[index];

                # Normalize the occlusion value greater than 1 to 1 as binary result
                if (occlusion.low > 0 || occlusion.high > 0) {
                    occlusions.t[index].low = 1u;
                    occlusions.t[index].high = 0u;
                }
            }
        )";

        ComputePipelineBase* GetOrCreateTimestampComputePipeline(DeviceBase* device) {
            InternalPipelineStore* store = device->GetInternalPipelineStore();

            if (store->timestampComputePipeline == nullptr) {
                // Create compute shader module if not cached before.
                if (store->timestampCS == nullptr) {
                    ShaderModuleDescriptor descriptor;
                    ShaderModuleWGSLDescriptor wgslDesc;
                    wgslDesc.source = sConvertTimestampsToNanoseconds;
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

        ComputePipelineBase* GetOrCreateOcclusionComputePipeline(DeviceBase* device) {
            InternalPipelineStore* store = device->GetInternalPipelineStore();

            if (store->occlusionComputePipeline == nullptr) {
                // Create compute shader module if not cached before.
                if (store->occlusionCS == nullptr) {
                    ShaderModuleDescriptor descriptor;
                    ShaderModuleWGSLDescriptor wgslDesc;
                    wgslDesc.source = sConvertOcclusionToBinary;
                    descriptor.nextInChain = reinterpret_cast<ChainedStruct*>(&wgslDesc);

                    store->occlusionCS = AcquireRef(device->CreateShaderModule(&descriptor));
                }

                // Create ComputePipeline.
                ComputePipelineDescriptor computePipelineDesc = {};
                // Generate the layout based on shader module.
                computePipelineDesc.layout = nullptr;
                computePipelineDesc.computeStage.module = store->occlusionCS.Get();
                computePipelineDesc.computeStage.entryPoint = "main";

                store->occlusionComputePipeline =
                    AcquireRef(device->CreateComputePipeline(&computePipelineDesc));
            }

            return store->occlusionComputePipeline.Get();
        }

        ComputePipelineBase* GetOrCreateQueryComputePipeline(DeviceBase* device,
                                                             wgpu::QueryType type) {
            switch (type) {
                case wgpu::QueryType::Timestamp:
                    return GetOrCreateTimestampComputePipeline(device);
                case wgpu::QueryType::Occlusion:
                    return GetOrCreateOcclusionComputePipeline(device);
                default:
                    return nullptr;
            }
        }

    }  // anonymous namespace

    void EncodeQueryResultConversion(CommandEncoder* encoder,
                                     wgpu::QueryType type,
                                     BufferBase* queries,
                                     BufferBase* availability,
                                     BufferBase* params) {
        DeviceBase* device = encoder->GetDevice();

        ComputePipelineBase* pipeline = GetOrCreateQueryComputePipeline(device, type);

        if (pipeline != nullptr) {
            // Prepare bind group layout.
            Ref<BindGroupLayoutBase> layout = AcquireRef(pipeline->GetBindGroupLayout(0));

            // Prepare bind group descriptor
            std::array<BindGroupEntry, 3> bindGroupEntries = {};
            BindGroupDescriptor bgDesc = {};
            bgDesc.layout = layout.Get();
            bgDesc.entryCount = 3;
            bgDesc.entries = bindGroupEntries.data();

            // Set bind group entries.
            bindGroupEntries[0].binding = 0;
            bindGroupEntries[0].buffer = queries;
            bindGroupEntries[0].size = queries->GetSize();
            bindGroupEntries[1].binding = 1;
            bindGroupEntries[1].buffer = availability;
            bindGroupEntries[1].size = availability->GetSize();
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
            pass->Dispatch(static_cast<uint32_t>((queries->GetSize() / sizeof(uint64_t) + 7) / 8));
            pass->EndPass();
        }
    }

}  // namespace dawn_native
