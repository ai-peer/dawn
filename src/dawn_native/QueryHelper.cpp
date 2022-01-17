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
#include "dawn_native/utils/WGPUHelpers.h"

namespace dawn::native {

    namespace {

        // Assert the offsets in dawn::native::TimestampParams are same with the ones in the shader
        static_assert(offsetof(dawn::native::TimestampParams, first) == 0, "");
        static_assert(offsetof(dawn::native::TimestampParams, count) == 4, "");
        static_assert(offsetof(dawn::native::TimestampParams, offset) == 8, "");
        static_assert(offsetof(dawn::native::TimestampParams, period) == 12, "");
        static_assert(offsetof(dawn::native::TimestampParams, factor) == 16, "");

        static const char sConvertTimestampsToNanoseconds[] = R"(
            struct Timestamp {
                low  : u32;
                high : u32;
            };

            struct TimestampArr {
                t : array<Timestamp>;
            };

            struct AvailabilityArr {
                v : array<u32>;
            };

            struct TimestampParams {
                first  : u32;
                count  : u32;
                offset : u32;
                period : u32;
                factor : u32;
            };

            struct Result {
                value : u32;
                carry : u32;
            };

            [[group(0), binding(0)]]
                var<storage, read_write> timestamps : TimestampArr;
            [[group(0), binding(1)]]
                var<storage, read> availability : AvailabilityArr;
            [[group(0), binding(2)]] var<uniform> params : TimestampParams;

            // The carry value comes from the Result.carry of mul(Timestamp.low, period, 0u),
            // 0 means no bits are carried from the multiplication of the low bits.
            fn mulOp(timestamp: u32, period: u32, carry: u32) -> Result {
                // If the product of timestamp and period does not exceed the maximum of u32,
                // directly do the multiplication, otherwise, use two u32 to represent the high
                // 16-bits and low 16-bits of the timestamp, then multiply them by the period
                // separately.
                var result: Result;
                if (timestamp <= u32(0xFFFFFFFFu / period)) {
                    result.value = timestamp * period + carry;
                    result.carry = 0u;
                } else {
                    var timestamp_low = timestamp & 0xFFFFu;
                    var timestamp_high = timestamp >> 16u;

                    var result_low = timestamp_low * period + carry;
                    var result_high = timestamp_high * period + (result_low >> 16u);
                    result.carry = result_high >> 16u;

                    result.value = result_high << 16u;
                    result.value = result.value | (result_low & 0xFFFFu);
                }

                return result;
            }

            let sizeofTimestamp : u32 = 8u;

            [[stage(compute), workgroup_size(8, 1, 1)]]
            fn main([[builtin(global_invocation_id)]] GlobalInvocationID : vec3<u32>) {
                if (GlobalInvocationID.x >= params.count) { return; }

                var index = GlobalInvocationID.x + params.offset / sizeofTimestamp;

                var timestamp = timestamps.t[index];

                // Return 0 for the unavailable value.
                if (availability.v[GlobalInvocationID.x + params.first] == 0u) {
                    timestamps.t[index].low = 0u;
                    timestamps.t[index].high = 0u;
                    return;
                }

                // Multiply the values in timestamps buffer by the period.
                var period = params.period;
                var result_low: Result = mulOp(timestamp.low, period, 0u);
                var result_high: Result = mulOp(timestamp.high, period, result_low.carry);

                // The period above is u32, it is obtained by multiplying the origin period
                // (which is float) by the params.factor and converting it to unsigned
                // 32-bit integer. So the product of the timestamp and period needs to be divided
                // by the factor to get the desired result.
                var factor = params.factor;
                var n = u32(log2(f32(factor)));
                timestamps.t[index].high = result_high.value >> n;

                var high_bits_to_low = result_high.value & (factor - 1u);
                result_low.value = result_low.value >> n;
                result_low.value = high_bits_to_low << (32u - n) | result_low.value;
                timestamps.t[index].low = result_low.value;
            }
        )";

        ResultOrError<ComputePipelineBase*> GetOrCreateTimestampComputePipeline(
            DeviceBase* device) {
            InternalPipelineStore* store = device->GetInternalPipelineStore();

            if (store->timestampComputePipeline == nullptr) {
                // Create compute shader module if not cached before.
                if (store->timestampCS == nullptr) {
                    DAWN_TRY_ASSIGN(
                        store->timestampCS,
                        utils::CreateShaderModule(device, sConvertTimestampsToNanoseconds));
                }

                // Create binding group layout
                Ref<BindGroupLayoutBase> bgl;
                DAWN_TRY_ASSIGN(
                    bgl, utils::MakeBindGroupLayout(
                             device,
                             {
                                 {0, wgpu::ShaderStage::Compute, kInternalStorageBufferBinding},
                                 {1, wgpu::ShaderStage::Compute,
                                  wgpu::BufferBindingType::ReadOnlyStorage},
                                 {2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},
                             },
                             /* allowInternalBinding */ true));

                // Create pipeline layout
                Ref<PipelineLayoutBase> layout;
                DAWN_TRY_ASSIGN(layout, utils::MakeBasicPipelineLayout(device, bgl));

                // Create ComputePipeline.
                ComputePipelineDescriptor computePipelineDesc = {};
                // Generate the layout based on shader module.
                computePipelineDesc.layout = layout.Get();
                computePipelineDesc.compute.module = store->timestampCS.Get();
                computePipelineDesc.compute.entryPoint = "main";

                DAWN_TRY_ASSIGN(store->timestampComputePipeline,
                                device->CreateComputePipeline(&computePipelineDesc));
            }

            return store->timestampComputePipeline.Get();
        }

    }  // anonymous namespace

    MaybeError EncodeConvertTimestampsToNanoseconds(CommandEncoder* encoder,
                                                    BufferBase* timestamps,
                                                    BufferBase* availability,
                                                    BufferBase* params) {
        DeviceBase* device = encoder->GetDevice();

        ComputePipelineBase* pipeline;
        DAWN_TRY_ASSIGN(pipeline, GetOrCreateTimestampComputePipeline(device));

        // Prepare bind group layout.
        Ref<BindGroupLayoutBase> layout;
        DAWN_TRY_ASSIGN(layout, pipeline->GetBindGroupLayout(0));

        // Create bind group after all binding entries are set.
        Ref<BindGroupBase> bindGroup;
        DAWN_TRY_ASSIGN(bindGroup,
                        utils::MakeBindGroup(device, layout,
                                             {{0, timestamps}, {1, availability}, {2, params}}));

        // Create compute encoder and issue dispatch.
        ComputePassDescriptor passDesc = {};
        // TODO(dawn:723): change to not use AcquireRef for reentrant object creation.
        Ref<ComputePassEncoder> pass = AcquireRef(encoder->APIBeginComputePass(&passDesc));
        pass->APISetPipeline(pipeline);
        pass->APISetBindGroup(0, bindGroup.Get());
        pass->APIDispatch(
            static_cast<uint32_t>((timestamps->GetSize() / sizeof(uint64_t) + 7) / 8));
        pass->APIEndPass();

        return {};
    }

}  // namespace dawn::native
