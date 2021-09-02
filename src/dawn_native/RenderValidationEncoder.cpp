// Copyright 2021 The Dawn Authors
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

#include "dawn_native/RenderValidationEncoder.h"

#include "common/Constants.h"
#include "common/Math.h"
#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/ComputePassEncoder.h"
#include "dawn_native/ComputePipeline.h"
#include "dawn_native/Device.h"
#include "dawn_native/InternalPipelineStore.h"
#include "dawn_native/Queue.h"

namespace dawn_native {

    namespace {

        constexpr uint64_t kWorkgroupSize = 8;

        // In this shader, each DrawParams corresponds to a single drawIndexedIndirect call that
        // will be made by an immediately subsequent render pass. Each `firstInvalidIndex` in
        // IndexLimitsData is derived from the actual size and format of the index buffer that will
        // be current at the time of the corresponding draw call. This shader's only task is to
        // ensure that for each corresponding entry in these arrays, `firstIndex + indexCount` does
        // not exceed `firstInvalidIndex`. It does this by adjusting `indexCount` if necessary.
        static const char sRenderValidationShaderSource[] = R"(
            struct DrawParams {
                indexCount: u32;
                instanceCount: u32;
                firstIndex: u32;
                baseVertex: u32;
                firstInstance: u32;
            };
            [[block]] struct IndexLimitsData {
                firstInvalidIndex: array<u32>;
            };
            [[block]] struct ParamsData {
                drawCalls: array<DrawParams>;
            };
            [[group(0), binding(0)]] var<storage, read> limits : IndexLimitsData;
            [[group(0), binding(1)]] var<storage, read_write> data : ParamsData;
            [[stage(compute), workgroup_size(8, 1, 1)]]
            fn main([[builtin(global_invocation_id)]] id : vec3<u32>) {
                let firstInvalidIndex = limits.firstInvalidIndex[id.x];
                let draw = &data.drawCalls[id.x];
                if ((*draw).firstIndex >= firstInvalidIndex) {
                    (*draw).indexCount = 0u;
                    return;
                }

                var maxIndexCount: u32 = firstInvalidIndex - (*draw).firstIndex;
                if ((*draw).indexCount > maxIndexCount) {
                    (*draw).indexCount = maxIndexCount;
                }
            }
        )";

        ResultOrError<ComputePipelineBase*> GetOrCreateRenderValidationPipeline(
            DeviceBase* device) {
            InternalPipelineStore* store = device->GetInternalPipelineStore();

            if (store->renderValidationPipeline == nullptr) {
                // Create compute shader module if not cached before.
                if (store->renderValidationShader == nullptr) {
                    ShaderModuleDescriptor descriptor;
                    ShaderModuleWGSLDescriptor wgslDesc;
                    wgslDesc.source = sRenderValidationShaderSource;
                    descriptor.nextInChain = reinterpret_cast<ChainedStruct*>(&wgslDesc);
                    DAWN_TRY_ASSIGN(store->renderValidationShader,
                                    device->CreateShaderModule(&descriptor));
                }

                BindGroupLayoutEntry entries[2];
                entries[0].binding = 0;
                entries[0].visibility = wgpu::ShaderStage::Compute;
                entries[0].buffer.type = wgpu::BufferBindingType::Storage;
                entries[1].binding = 1;
                entries[1].visibility = wgpu::ShaderStage::Compute;
                entries[1].buffer.type = wgpu::BufferBindingType::Storage;

                BindGroupLayoutDescriptor bindGroupLayoutDescriptor;
                bindGroupLayoutDescriptor.entryCount = 2;
                bindGroupLayoutDescriptor.entries = entries;
                Ref<BindGroupLayoutBase> bindGroupLayout;
                DAWN_TRY_ASSIGN(bindGroupLayout,
                                device->CreateBindGroupLayout(&bindGroupLayoutDescriptor, true));

                PipelineLayoutDescriptor pipelineDescriptor;
                pipelineDescriptor.bindGroupLayoutCount = 1;
                pipelineDescriptor.bindGroupLayouts = &bindGroupLayout.Get();
                Ref<PipelineLayoutBase> pipelineLayout;
                DAWN_TRY_ASSIGN(pipelineLayout, device->CreatePipelineLayout(&pipelineDescriptor));

                ComputePipelineDescriptor computePipelineDescriptor = {};
                computePipelineDescriptor.layout = pipelineLayout.Get();
                computePipelineDescriptor.compute.module = store->renderValidationShader.Get();
                computePipelineDescriptor.compute.entryPoint = "main";

                DAWN_TRY_ASSIGN(store->renderValidationPipeline,
                                device->CreateComputePipeline(&computePipelineDescriptor));
            }

            return store->renderValidationPipeline.Get();
        }

    }  // namespace

    MaybeError EncodeRenderValidationCommands(DeviceBase* device,
                                              CommandEncoder* commandEncoder,
                                              RenderPassResourceUsageTracker* usageTracker,
                                              const RenderValidationMetadata& validationMetadata) {
        const auto& draws = validationMetadata.GetIndexedIndirectDraws();
        if (draws.empty()) {
            return {};
        }

        const size_t numEntries = RoundUp(draws.size(), kWorkgroupSize);
        auto* const store = device->GetInternalPipelineStore();
        ScratchBuffer& paramsBuffer = store->scratchIndirectStorage;
        DAWN_TRY(paramsBuffer.EnsureCapacity(numEntries * kDrawIndexedIndirectSize));

        uint64_t nextParamsOffset = 0;
        std::vector<uint8_t> limits(numEntries * sizeof(uint32_t));
        uint32_t* nextLimitPtr = reinterpret_cast<uint32_t*>(limits.data());
        for (const auto& draw : draws) {
            commandEncoder->APICopyBufferToBufferInternal(
                draw.clientIndirectBuffer.Get(), draw.clientIndirectOffset,
                paramsBuffer.GetBuffer(), nextParamsOffset, kDrawIndexedIndirectSize);
            draw.cmd->indirectBuffer = paramsBuffer.GetBuffer();
            draw.cmd->indirectOffset = nextParamsOffset;
            nextParamsOffset += kDrawIndexedIndirectSize;

            *nextLimitPtr = draw.firstInvalidIndex;
            ++nextLimitPtr;
        }

        ScratchBuffer& limitsBuffer = store->scratchStorage;
        DAWN_TRY(limitsBuffer.EnsureCapacity(limits.size()));
        commandEncoder->EncodeWriteBuffer(limitsBuffer.GetBuffer(), 0, std::move(limits));

        ComputePipelineBase* pipeline;
        DAWN_TRY_ASSIGN(pipeline, GetOrCreateRenderValidationPipeline(device));

        Ref<BindGroupLayoutBase> layout;
        DAWN_TRY_ASSIGN(layout, pipeline->GetBindGroupLayout(0));

        BindGroupEntry bindings[2];
        bindings[0].binding = 0;
        bindings[0].buffer = limitsBuffer.GetBuffer();
        bindings[0].size = limitsBuffer.GetBuffer()->GetSize();
        bindings[1].binding = 1;
        bindings[1].buffer = paramsBuffer.GetBuffer();
        bindings[1].size = paramsBuffer.GetBuffer()->GetSize();

        BindGroupDescriptor bindGroupDescriptor = {};
        bindGroupDescriptor.layout = layout.Get();
        bindGroupDescriptor.entryCount = 2;
        bindGroupDescriptor.entries = bindings;

        Ref<BindGroupBase> bindGroup;
        DAWN_TRY_ASSIGN(bindGroup, device->CreateBindGroup(&bindGroupDescriptor));

        ComputePassDescriptor descriptor = {};
        // TODO(dawn:723): change to not use AcquireRef for reentrant object creation.
        Ref<ComputePassEncoder> pass = AcquireRef(commandEncoder->APIBeginComputePass(&descriptor));
        pass->APISetPipeline(pipeline);
        pass->APISetBindGroup(0, bindGroup.Get());
        pass->APIDispatch(numEntries / kWorkgroupSize);
        pass->APIEndPass();

        usageTracker->BufferUsedAs(limitsBuffer.GetBuffer(), wgpu::BufferUsage::Storage);
        usageTracker->BufferUsedAs(paramsBuffer.GetBuffer(), wgpu::BufferUsage::Indirect);

        return {};
    }

}  // namespace dawn_native
