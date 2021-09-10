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

        static const char sRenderValidationShaderSource[] = R"(
            let kIndexCountIndex = 0u;
            let kInstanceCountIndex = 1u;
            let kFirstIndexIndex = 2u;
            let kBaseVertexIndex = 3u;
            let kFirstInstanceIndex = 4u;
            let kNumElementsPerDrawCall = 5u;

            struct Uint64 {
                low: u32;
                high: u32;
            };

            [[block]] struct BatchInfo {
                numIndexBufferElements: Uint64;
                numDraws: u32;
                indirectOffsets: array<u32>;
            };

            [[block]] struct IndirectParams {
                data: array<u32>;
            };

            [[group(0), binding(0)]] var<storage, read> batch: BatchInfo;
            [[group(0), binding(1)]] var<storage, read_write> clientParams: IndirectParams;
            [[group(0), binding(2)]] var<storage, write> validatedParams: IndirectParams;

            // TODO(https://crbug.com/dawn/1108): Propagate validation feedback from this shader in
            // various failure modes.
            [[stage(compute), workgroup_size(64, 1, 1)]]
            fn main([[builtin(global_invocation_id)]] id : vec3<u32>) {
                if (id.x >= batch.numDraws) {
                    return;
                }

                let clientBase = batch.indirectOffsets[id.x] / 4u;
                let clientIndexCount = &clientParams.data[clientBase + kIndexCountIndex];
                let clientInstanceCount = &clientParams.data[clientBase + kInstanceCountIndex];
                let clientFirstIndex = &clientParams.data[clientBase + kFirstIndexIndex];
                let clientBaseVertex = &clientParams.data[clientBase + kBaseVertexIndex];
                let clientFirstInstance = &clientParams.data[clientBase + kFirstInstanceIndex];

                let validatedBase = id.x * kNumElementsPerDrawCall;
                let validatedIndexCount = &validatedParams.data[validatedBase + kIndexCountIndex];
                let validatedInstanceCount = &validatedParams.data[validatedBase + kInstanceCountIndex];
                let validatedFirstIndex = &validatedParams.data[validatedBase + kFirstIndexIndex];
                let validatedBaseVertex = &validatedParams.data[validatedBase + kBaseVertexIndex];
                let validatedFirstInstance = &validatedParams.data[validatedBase + kFirstInstanceIndex];
                if (*clientFirstInstance != 0u) {
                    *validatedIndexCount = 0u;
                    *validatedInstanceCount = 0u;
                    *validatedFirstIndex = 0u;
                    *validatedBaseVertex = 0u;
                    *validatedFirstInstance = 0u;
                    return;
                }

                let numElementsHigh = batch.numIndexBufferElements.high;
                let numElementsLow = batch.numIndexBufferElements.low;
                if (numElementsHigh >= 2u) {
                    // We can't possibly overflow an index buffer with this many elements, so no
                    // more validation is necessary.
                    *validatedIndexCount = *clientIndexCount;
                    *validatedInstanceCount = *clientInstanceCount;
                    *validatedFirstIndex = *clientFirstIndex;
                    *validatedBaseVertex = *clientBaseVertex;
                    *validatedFirstInstance = *clientFirstInstance;
                    return;
                }

                if (numElementsLow < *clientFirstIndex && numElementsHigh == 0u) {
                    *validatedIndexCount = 0u;
                    *validatedInstanceCount = 0u;
                    *validatedFirstIndex = 0u;
                    *validatedBaseVertex = 0u;
                    *validatedFirstInstance = 0u;
                    return;
                }

                // Note that at this point, this difference can only underflow if `numElementsHigh`
                // is 1u; in that case the the underflowed value is still the correct index count
                // limit.
                var maxIndexCount: u32 = numElementsLow - *clientFirstIndex;
                if (*clientIndexCount > maxIndexCount) {
                    *validatedIndexCount = 0u;
                    *validatedInstanceCount = 0u;
                    *validatedFirstIndex = 0u;
                    *validatedBaseVertex = 0u;
                    *validatedFirstInstance = 0u;
                    return;
                }

                *validatedIndexCount = *clientIndexCount;
                *validatedInstanceCount = *clientInstanceCount;
                *validatedFirstIndex = *clientFirstIndex;
                *validatedBaseVertex = *clientBaseVertex;
                *validatedFirstInstance = *clientFirstInstance;
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

        MaybeError EncodeRenderValidationCommandsImpl(DeviceBase* device,
                                                      CommandEncoder* commandEncoder,
                                                      RenderPassResourceUsageTracker* usageTracker,
                                                      RenderValidationMetadata* validationMetadata,
                                                      uint64_t* indirectScratchBufferBaseOffset) {
            for (auto& entry : *validationMetadata->GetBundleMetadata()) {
                DAWN_TRY(EncodeRenderValidationCommandsImpl(device, commandEncoder, usageTracker,
                                                            &entry.second,
                                                            indirectScratchBufferBaseOffset));
            }

            auto* const store = device->GetInternalPipelineStore();
            ScratchBuffer& paramsBuffer = store->scratchIndirectStorage;
            ScratchBuffer& batchBuffer = store->scratchStorage;

            struct Pass {
                BufferBase* clientIndirectBuffer;
                uint64_t clientIndirectOffset;
                uint64_t clientIndirectSize;
                Ref<DeferredBufferRef> indirectBufferRef;
                std::vector<uint32_t> batchInfo;
            };

            std::vector<Pass> passes;
            RenderValidationMetadata::IndexedIndirectBufferValidationInfoMap& bufferInfoMap =
                *validationMetadata->GetIndexedIndirectBufferValidationInfo();
            for (auto& entry : bufferInfoMap) {
                const RenderValidationMetadata::IndexedIndirectConfig& config = entry.first;
                const uint64_t numIndexBufferElements = config.second;
                for (const RenderValidationMetadata::IndexedIndirectValidationPass& pass :
                     *entry.second.GetPasses()) {
                    const uint64_t minOffsetFromAlignedBoundary =
                        pass.minOffset % kMinStorageBufferOffsetAlignment;
                    const uint64_t minOffsetAlignedDown =
                        pass.minOffset - minOffsetFromAlignedBoundary;
                    Pass newPass;
                    newPass.clientIndirectBuffer = config.first;
                    newPass.clientIndirectOffset = minOffsetAlignedDown;
                    newPass.clientIndirectSize =
                        pass.maxOffset - minOffsetAlignedDown + kDrawIndexedIndirectSize;
                    newPass.indirectBufferRef = pass.bufferRef;

                    // Populate the BatchInfo data to be uploaded for this validation pass. See
                    // the BatchInfo struct definition within the shader: a u64, a u32, and then an
                    // additional u32 for every draw call.
                    newPass.batchInfo.resize(pass.offsets.size() + 3);
                    *reinterpret_cast<uint64_t*>(newPass.batchInfo.data()) = numIndexBufferElements;
                    ASSERT(pass.offsets.size() <= std::numeric_limits<uint32_t>::max());
                    newPass.batchInfo[2] = static_cast<uint32_t>(pass.offsets.size());
                    for (size_t i = 0; i < pass.offsets.size(); ++i) {
                        newPass.batchInfo[i + 3] =
                            static_cast<uint32_t>(pass.offsets[i] - minOffsetAlignedDown);
                    }
                    passes.push_back(std::move(newPass));
                }
            }

            uint64_t& indirectBaseOffset = *indirectScratchBufferBaseOffset;
            for (const Pass& pass : passes) {
                const uint64_t alignedSize =
                    Align(pass.clientIndirectSize, kMinStorageBufferOffsetAlignment);
                if (indirectBaseOffset + alignedSize > kMaxStorageBufferBindingSize) {
                    paramsBuffer.Reset();
                    indirectBaseOffset = 0;
                }

                DAWN_TRY(paramsBuffer.EnsureCapacity(indirectBaseOffset + alignedSize));
                pass.indirectBufferRef->SetBuffer(paramsBuffer.GetBuffer(), indirectBaseOffset);

                DAWN_TRY(batchBuffer.EnsureCapacity(pass.batchInfo.size() * sizeof(uint32_t)));
                commandEncoder->APIWriteBufferInternal(
                    batchBuffer.GetBuffer(), 0,
                    reinterpret_cast<const uint8_t*>(pass.batchInfo.data()),
                    pass.batchInfo.size() * sizeof(uint32_t));

                ComputePipelineBase* pipeline;
                DAWN_TRY_ASSIGN(pipeline, GetOrCreateRenderValidationPipeline(device));

                Ref<BindGroupLayoutBase> layout;
                DAWN_TRY_ASSIGN(layout, pipeline->GetBindGroupLayout(0));

                BindGroupEntry bindings[3];
                bindings[0].binding = 0;
                bindings[0].buffer = batchBuffer.GetBuffer();
                bindings[0].size = batchBuffer.GetBuffer()->GetSize();
                bindings[1].binding = 1;
                bindings[1].buffer = pass.clientIndirectBuffer;
                bindings[1].offset = pass.clientIndirectOffset;
                bindings[1].size = pass.clientIndirectSize;
                bindings[2].binding = 2;
                bindings[2].buffer = paramsBuffer.GetBuffer();
                bindings[2].size = paramsBuffer.GetBuffer()->GetSize() - indirectBaseOffset;
                bindings[2].offset = indirectBaseOffset;

                indirectBaseOffset += alignedSize;

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
            }

            if (!passes.empty()) {
                usageTracker->BufferUsedAs(batchBuffer.GetBuffer(), wgpu::BufferUsage::Storage);
                usageTracker->BufferUsedAs(paramsBuffer.GetBuffer(), wgpu::BufferUsage::Indirect);
            }
            return {};
        }

    }  // namespace

    MaybeError EncodeRenderValidationCommands(DeviceBase* device,
                                              CommandEncoder* commandEncoder,
                                              RenderPassResourceUsageTracker* usageTracker,
                                              RenderValidationMetadata* validationMetadata) {
        uint64_t indirectScratchBufferBaseOffset = 0;
        return EncodeRenderValidationCommandsImpl(device, commandEncoder, usageTracker,
                                                  validationMetadata,
                                                  &indirectScratchBufferBaseOffset);
    }

}  // namespace dawn_native
