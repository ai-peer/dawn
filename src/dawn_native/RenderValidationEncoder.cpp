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
        constexpr uint64_t kIndexBufferInfoSize = 4;
        constexpr uint64_t kIndexedIndirectValidationEntrySize =
            kIndexBufferInfoSize + kDrawIndexedIndirectSize;

        // In this shader, each Entry corresponds to a single drawIndexedIndirect call that will be
        // made by an immediately subsequent render pass. `firstInvalidIndex` is derived from the
        // actual size and format of the index buffer that will be current at the time of the
        // corresponding draw call. The remaining fields of each Entry are used directly by the
        // draw call as its indirect parameters, so all this shader needs to do is ensure that
        // `indexCount` is safe given the values of `firstIndex` and `firstInvalidIndex`.
        static const char sRenderValidationShaderSource[] = R"(
            struct Entry {
                firstInvalidIndex: u32;
                indexCount: u32;
                instanceCount: u32;
                firstIndex: u32;
                baseVertex: u32;
                firstInstance: u32;
            };

            [[block]] struct IndexedIndirectData {
                entries : array<Entry>;
            };

            [[group(0), binding(0)]] var<storage, read_write> data : IndexedIndirectData;
            [[stage(compute), workgroup_size(8, 1, 1)]]
            fn main([[builtin(global_invocation_id)]] id : vec3<u32>) {
                let entry = &data.entries[id.x];
                if ((*entry).firstIndex >= (*entry).firstInvalidIndex) {
                    (*entry).indexCount = 0u;
                    return;
                }

                var maxIndexCount: u32 = (*entry).firstInvalidIndex - (*entry).firstIndex;
                if ((*entry).indexCount > maxIndexCount) {
                    (*entry).indexCount = maxIndexCount;
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

                BindGroupLayoutEntry entry;
                entry.binding = 0;
                entry.visibility = wgpu::ShaderStage::Compute;
                entry.buffer.type = wgpu::BufferBindingType::Storage;

                BindGroupLayoutDescriptor bindGroupLayoutDescriptor;
                bindGroupLayoutDescriptor.entryCount = 1;
                bindGroupLayoutDescriptor.entries = &entry;
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

    RenderValidationEncoder::RenderValidationEncoder() = default;

    RenderValidationEncoder::~RenderValidationEncoder() = default;

    RenderValidationEncoder::RenderValidationEncoder(RenderValidationEncoder&&) = default;

    RenderValidationEncoder& RenderValidationEncoder::operator=(RenderValidationEncoder&&) =
        default;

    void RenderValidationEncoder::EnqueueIndexedIndirectDraw(
        CommandBufferStateTracker* commandBufferState,
        BufferBase* indirectBuffer,
        uint64_t indirectOffset,
        DrawIndexedIndirectCmd* cmd) {
        mIndexedIndirectDraws.emplace_back();
        IndexedIndirectDraw& draw = mIndexedIndirectDraws.back();
        const size_t indexBufferSize = commandBufferState->GetIndexBufferSize();
        const size_t indexNumBytes =
            commandBufferState->GetIndexFormat() == wgpu::IndexFormat::Uint32 ? 4 : 2;
        draw.firstInvalidIndex = indexBufferSize / indexNumBytes;
        draw.clientIndirectBuffer = indirectBuffer;
        draw.clientIndirectOffset = indirectOffset;
        draw.cmd = cmd;
    }

    void RenderValidationEncoder::EnqueueBundle(RenderValidationEncoder* validationEncoder) {
        mBundleValidationEncoders.push_back(validationEncoder);
    }

    MaybeError RenderValidationEncoder::EncodeValidationCommands(
        DeviceBase* device,
        CommandEncoder* commandEncoder,
        RenderPassResourceUsageTracker* usageTracker) {
        const size_t numDraws = GetNumIndexedIndirectDraws();
        if (numDraws == 0) {
            return {};
        }
        uint64_t numEntries = RoundUp(numDraws, kWorkgroupSize);
        ValidationScratchBuffer* scratchBuffer = device->GetValidationScratchBuffer();
        DAWN_TRY(scratchBuffer->Reset(kIndexedIndirectValidationEntrySize * numEntries));
        return EncodeValidationCommandsImpl(device, commandEncoder, usageTracker);
    }

    MaybeError RenderValidationEncoder::EncodeValidationCommandsImpl(
        DeviceBase* device,
        CommandEncoder* commandEncoder,
        RenderPassResourceUsageTracker* usageTracker) {
        ValidationScratchBuffer* scratchBuffer = device->GetValidationScratchBuffer();
        BufferBase* const buffer = scratchBuffer->GetBuffer();
        for (auto* encoder : mBundleValidationEncoders) {
            DAWN_TRY(encoder->EncodeValidationCommandsImpl(device, commandEncoder, usageTracker));
        }
        for (const auto& draw : mIndexedIndirectDraws) {
            uint64_t offset = scratchBuffer->Claim(kIndexedIndirectValidationEntrySize);
            uint64_t indirectOffset = offset + kIndexBufferInfoSize;
            DAWN_TRY(device->GetQueue()->WriteBuffer(buffer, offset, &draw.firstInvalidIndex,
                                                     kIndexBufferInfoSize));
            commandEncoder->APICopyBufferToBuffer(draw.clientIndirectBuffer.Get(),
                                                  draw.clientIndirectOffset, buffer, indirectOffset,
                                                  kDrawIndexedIndirectSize);
            draw.cmd->indirectBuffer = buffer;
            draw.cmd->indirectOffset = indirectOffset;
        }

        ComputePipelineBase* pipeline;
        DAWN_TRY_ASSIGN(pipeline, GetOrCreateRenderValidationPipeline(device));

        Ref<BindGroupLayoutBase> layout;
        DAWN_TRY_ASSIGN(layout, pipeline->GetBindGroupLayout(0));

        BindGroupEntry entry;
        entry.binding = 0;
        entry.buffer = buffer;
        entry.size = buffer->GetSize();

        BindGroupDescriptor bindGroupDescriptor = {};
        bindGroupDescriptor.layout = layout.Get();
        bindGroupDescriptor.entryCount = 1;
        bindGroupDescriptor.entries = &entry;

        Ref<BindGroupBase> bindGroup;
        DAWN_TRY_ASSIGN(bindGroup, device->CreateBindGroup(&bindGroupDescriptor));

        ComputePassDescriptor descriptor = {};
        // TODO(dawn:723): change to not use AcquireRef for reentrant object creation.
        Ref<ComputePassEncoder> pass = AcquireRef(commandEncoder->APIBeginComputePass(&descriptor));
        pass->APISetPipeline(pipeline);
        pass->APISetBindGroup(0, bindGroup.Get());
        pass->APIDispatch(buffer->GetSize() / kIndexedIndirectValidationEntrySize / kWorkgroupSize);
        pass->APIEndPass();

        usageTracker->BufferUsedAs(buffer, wgpu::BufferUsage::Indirect);

        return {};
    }

    size_t RenderValidationEncoder::GetNumIndexedIndirectDraws() const {
        size_t count = mIndexedIndirectDraws.size();
        for (const auto* encoder : mBundleValidationEncoders) {
            count += encoder->GetNumIndexedIndirectDraws();
        }
        return count;
    }

}  // namespace dawn_native