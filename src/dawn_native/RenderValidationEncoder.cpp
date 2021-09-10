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

#include <limits>

namespace dawn_native {

    namespace {
        // NOTE: This must match the workgroup_size attribute on the compute entry point below.
        constexpr uint64_t kWorkgroupSize = 64;

        // TODO(https://crbug.com/dawn/1108): Propagate validation feedback from this shader in
        // various failure modes.
        static const char sRenderValidationShaderSource[] = R"(
            let kNumIndirectParamsPerDrawCall = 5u;

            [[block]] struct BatchInfo {
                numIndexBufferElementsLow: u32;
                numIndexBufferElementsHigh: u32;
                numDraws: u32;
                indirectOffsets: array<u32>;
            };

            [[block]] struct IndirectParams {
                data: array<u32>;
            };

            [[group(0), binding(0)]] var<storage, read> batch: BatchInfo;
            [[group(0), binding(1)]] var<storage, read_write> clientParams: IndirectParams;
            [[group(0), binding(2)]] var<storage, write> validatedParams: IndirectParams;

            fn fail(drawIndex: u32) {
                let index = drawIndex * kNumIndirectParamsPerDrawCall;
                validatedParams.data[index] = 0u;
                validatedParams.data[index + 1u] = 0u;
                validatedParams.data[index + 2u] = 0u;
                validatedParams.data[index + 3u] = 0u;
                validatedParams.data[index + 4u] = 0u;
            }

            fn pass(drawIndex: u32) {
                let vIndex = drawIndex * kNumIndirectParamsPerDrawCall;
                let cIndex = batch.indirectOffsets[drawIndex];
                validatedParams.data[vIndex] = clientParams.data[cIndex];
                validatedParams.data[vIndex + 1u] = clientParams.data[cIndex + 1u];
                validatedParams.data[vIndex + 2u] = clientParams.data[cIndex + 2u];
                validatedParams.data[vIndex + 3u] = clientParams.data[cIndex + 3u];
                validatedParams.data[vIndex + 4u] = clientParams.data[cIndex + 4u];
            }

            [[stage(compute), workgroup_size(64, 1, 1)]]
            fn main([[builtin(global_invocation_id)]] id : vec3<u32>) {
                if (id.x >= batch.numDraws) {
                    return;
                }

                let clientIndex = batch.indirectOffsets[id.x];
                let firstInstance = clientParams.data[clientIndex + 4u];
                if (firstInstance != 0u) {
                    fail(id.x);
                    return;
                }

                if (batch.numIndexBufferElementsHigh >= 2u) {
                    // firstIndex and indexCount are both u32. The maximum possible sum of these
                    // values is 0x1fffffffe, which is less than 0x200000000. Nothing to validate.
                    pass(id.x);
                    return;
                }

                let firstIndex = clientParams.data[clientIndex + 2u];
                if (batch.numIndexBufferElementsLow < firstIndex &&
                    batch.numIndexBufferElementsHigh == 0u) {
                    fail(id.x);
                    return;
                }

                // Note that this subtraction may underflow, but only when
                // numIndexBufferElementsHigh is 1u. The result is still correct in that case.
                let maxIndexCount = batch.numIndexBufferElementsLow - firstIndex;
                let indexCount = clientParams.data[clientIndex];
                if (indexCount > maxIndexCount) {
                    fail(id.x);
                    return;
                }
                pass(id.x);
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

                BindGroupLayoutEntry entries[3];
                entries[0].binding = 0;
                entries[0].visibility = wgpu::ShaderStage::Compute;
                entries[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
                entries[1].binding = 1;
                entries[1].visibility = wgpu::ShaderStage::Compute;
                entries[1].buffer.type = kInternalStorageBufferBinding;
                entries[2].binding = 2;
                entries[2].visibility = wgpu::ShaderStage::Compute;
                entries[2].buffer.type = wgpu::BufferBindingType::Storage;

                BindGroupLayoutDescriptor bindGroupLayoutDescriptor;
                bindGroupLayoutDescriptor.entryCount = 3;
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
                                              RenderValidationMetadata* validationMetadata) {
        auto* const store = device->GetInternalPipelineStore();
        ScratchBuffer& paramsBuffer = store->scratchIndirectStorage;
        ScratchBuffer& batchBuffer = store->scratchStorage;

        struct Pass {
            BufferBase* clientIndirectBuffer;
            uint64_t clientIndirectOffset;
            uint64_t clientIndirectSize;
            BufferBase* validatedParamsBuffer;
            uint64_t validatedParamsOffset;
            uint64_t validatedParamsSize;
            BufferBase* batchBuffer;
            std::vector<uint32_t> batchInfo;
            std::vector<DeferredBufferLocationUpdate> deferredBufferLocationUpdates;
        };

        uint64_t validatedParamsBaseOffset = 0;
        std::vector<Pass> passes;
        RenderValidationMetadata::IndexedIndirectBufferValidationInfoMap& bufferInfoMap =
            *validationMetadata->GetIndexedIndirectBufferValidationInfo();
        for (auto& entry : bufferInfoMap) {
            const RenderValidationMetadata::IndexedIndirectConfig& config = entry.first;
            const uint64_t numIndexBufferElements = config.second;
            for (const RenderValidationMetadata::IndexedIndirectValidationPass& pass :
                 entry.second.GetPasses()) {
                const uint64_t minOffsetFromAlignedBoundary =
                    pass.minOffset % kMinStorageBufferOffsetAlignment;
                const uint64_t minOffsetAlignedDown = pass.minOffset - minOffsetFromAlignedBoundary;
                Pass newPass;
                newPass.clientIndirectBuffer = config.first;
                newPass.clientIndirectOffset = minOffsetAlignedDown;
                newPass.clientIndirectSize =
                    pass.maxOffset - minOffsetAlignedDown + kDrawIndexedIndirectSize;

                const uint64_t alignedSize =
                    Align(newPass.clientIndirectSize, kMinStorageBufferOffsetAlignment);
                if (validatedParamsBaseOffset + alignedSize > kMaxStorageBufferBindingSize) {
                    paramsBuffer.Reset();
                    validatedParamsBaseOffset = 0;
                }
                DAWN_TRY(paramsBuffer.EnsureCapacity(validatedParamsBaseOffset + alignedSize));
                newPass.validatedParamsBuffer = paramsBuffer.GetBuffer();
                newPass.validatedParamsOffset = validatedParamsBaseOffset;
                newPass.validatedParamsSize =
                    newPass.validatedParamsBuffer->GetSize() - validatedParamsBaseOffset;
                validatedParamsBaseOffset += alignedSize;

                newPass.batchInfo.resize(pass.draws.size() + 3);
                *reinterpret_cast<uint64_t*>(newPass.batchInfo.data()) = numIndexBufferElements;
                ASSERT(pass.draws.size() <= std::numeric_limits<uint32_t>::max());
                newPass.batchInfo[2] = static_cast<uint32_t>(pass.draws.size());
                newPass.deferredBufferLocationUpdates.reserve(pass.draws.size());
                for (size_t i = 0; i < pass.draws.size(); ++i) {
                    newPass.batchInfo[i + 3] =
                        static_cast<uint32_t>(pass.draws[i].clientBufferOffset -
                                              minOffsetAlignedDown) >>
                        2;

                    DeferredBufferLocationUpdate deferredUpdate;
                    deferredUpdate.location = pass.draws[i].bufferLocation;
                    deferredUpdate.buffer = newPass.validatedParamsBuffer;
                    deferredUpdate.offset =
                        newPass.validatedParamsOffset + i * kDrawIndexedIndirectSize;
                    newPass.deferredBufferLocationUpdates.push_back(std::move(deferredUpdate));
                }

                DAWN_TRY(batchBuffer.EnsureCapacity(newPass.batchInfo.size() * sizeof(uint32_t)));
                newPass.batchBuffer = batchBuffer.GetBuffer();

                passes.push_back(std::move(newPass));
            }
        }

        for (Pass& pass : passes) {
            commandEncoder->APIWriteBufferInternal(
                pass.batchBuffer, 0, reinterpret_cast<const uint8_t*>(pass.batchInfo.data()),
                pass.batchInfo.size() * sizeof(uint32_t));

            ComputePipelineBase* pipeline;
            DAWN_TRY_ASSIGN(pipeline, GetOrCreateRenderValidationPipeline(device));

            Ref<BindGroupLayoutBase> layout;
            DAWN_TRY_ASSIGN(layout, pipeline->GetBindGroupLayout(0));

            BindGroupEntry bindings[3];
            bindings[0].binding = 0;
            bindings[0].buffer = pass.batchBuffer;
            bindings[0].size = pass.batchBuffer->GetSize();
            bindings[1].binding = 1;
            bindings[1].buffer = pass.clientIndirectBuffer;
            bindings[1].offset = pass.clientIndirectOffset;
            bindings[1].size = pass.clientIndirectSize;
            bindings[2].binding = 2;
            bindings[2].buffer = pass.validatedParamsBuffer;
            bindings[2].offset = pass.validatedParamsOffset;
            bindings[2].size = pass.validatedParamsSize;

            BindGroupDescriptor bindGroupDescriptor = {};
            bindGroupDescriptor.layout = layout.Get();
            bindGroupDescriptor.entryCount = 3;
            bindGroupDescriptor.entries = bindings;

            Ref<BindGroupBase> bindGroup;
            DAWN_TRY_ASSIGN(bindGroup, device->CreateBindGroup(&bindGroupDescriptor));

            const size_t numDraws = pass.batchInfo.size() - 3;
            ComputePassDescriptor descriptor = {};
            // TODO(dawn:723): change to not use AcquireRef for reentrant object creation.
            Ref<ComputePassEncoder> passEncoder =
                AcquireRef(commandEncoder->APIBeginComputePass(&descriptor));
            passEncoder->APISetPipeline(pipeline);
            passEncoder->APISetBindGroup(0, bindGroup.Get());
            passEncoder->APIDispatch((numDraws + kWorkgroupSize - 1) / kWorkgroupSize);
            passEncoder->APIEndPass();

            commandEncoder->EncodeSetValidatedBufferLocationsInternal(
                std::move(pass.deferredBufferLocationUpdates));
        }

        if (!passes.empty()) {
            usageTracker->BufferUsedAs(batchBuffer.GetBuffer(), wgpu::BufferUsage::Storage);
            usageTracker->BufferUsedAs(paramsBuffer.GetBuffer(), wgpu::BufferUsage::Indirect);
        }
        return {};
    }

}  // namespace dawn_native
