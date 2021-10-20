// Copyright 2018 The Dawn Authors
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

#include "dawn_native/ComputePassEncoder.h"

#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/ComputePipeline.h"
#include "dawn_native/Device.h"
#include "dawn_native/InternalPipelineStore.h"
#include "dawn_native/ObjectType_autogen.h"
#include "dawn_native/PassResourceUsageTracker.h"
#include "dawn_native/QuerySet.h"

namespace dawn_native {

    namespace {

        MaybeError ValidatePerDimensionDispatchSizeLimit(const DeviceBase* device, uint32_t size) {
            if (size > device->GetLimits().v1.maxComputeWorkgroupsPerDimension) {
                return DAWN_VALIDATION_ERROR("Dispatch size exceeds defined limits");
            }

            return {};
        }

        ResultOrError<ComputePipelineBase*> GetOrCreateIndirectDispatchValidationPipeline(
            DeviceBase* device) {
            InternalPipelineStore* store = device->GetInternalPipelineStore();

            if (store->dispatchIndirectValidationPipeline == nullptr) {
                ShaderModuleDescriptor descriptor;
                ShaderModuleWGSLDescriptor wgslDesc;

                std::string shaderSource =
                    "let maxComputeWorkgroupsPerDimension : u32 = " +
                    std::to_string(device->GetLimits().v1.maxComputeWorkgroupsPerDimension) +
                    "u;\n";

                // TODO(https://crbug.com/dawn/1108): Propagate validation feedback from this
                // shader in various failure modes.
                shaderSource += R"(
                    [[block]] struct IndirectParams {
                        data: array<u32>;
                    };

                    [[block]] struct ValidatedParams {
                        data: array<u32, 3>;
                    };

                    [[group(0), binding(0)]] var<storage, read_write> clientParams: IndirectParams;
                    [[group(0), binding(1)]] var<storage, write> validatedParams: ValidatedParams;

                    [[stage(compute), workgroup_size(1, 1, 1)]]
                    fn main() {
                        // The client indirect buffer must be aligned to |minStorageBufferOffsetAlignment|.
                        // which is larger than the indirect buffer offset.
                        // To avoid passing an additional offset into the shader, we make the binding size
                        // exactly large enough to fit the indirect client data. This means the start of
                        // of the indirect data is a fixed distance of 3 from the end.
                        let clientOffset = arrayLength(&clientParams.data) - 3u;

                        for (var i = 0u; i < 3u; i = i + 1u) {
                            var numWorkgroups = clientParams.data[clientOffset + i];
                            if (numWorkgroups > maxComputeWorkgroupsPerDimension) {
                                numWorkgroups = 0u;
                            }
                            validatedParams.data[i] = numWorkgroups;
                        }
                    }
                )";
                wgslDesc.source = shaderSource.c_str();
                descriptor.nextInChain = reinterpret_cast<ChainedStruct*>(&wgslDesc);

                Ref<ShaderModuleBase> shaderModule;
                DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&descriptor));

                BindGroupLayoutEntry entries[2];
                entries[0].binding = 0;
                entries[0].visibility = wgpu::ShaderStage::Compute;
                entries[0].buffer.type = kInternalStorageBufferBinding;
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
                computePipelineDescriptor.compute.module = shaderModule.Get();
                computePipelineDescriptor.compute.entryPoint = "main";

                DAWN_TRY_ASSIGN(store->dispatchIndirectValidationPipeline,
                                device->CreateComputePipeline(&computePipelineDescriptor));
            }

            return store->dispatchIndirectValidationPipeline.Get();
        }

        MaybeError CreateIndirectDispatchValidationResources(
            DeviceBase* device,
            BufferBase* indirectBuffer,
            uint64_t indirectOffset,
            Ref<ComputePipelineBase>* validationPipeline,
            Ref<BufferBase>* validatedIndirectBuffer,
            Ref<BindGroupBase>* bindGroup) {
            auto* const store = device->GetInternalPipelineStore();

            ScratchBuffer& scratchBuffer = store->scratchIndirectStorage;
            DAWN_TRY(scratchBuffer.EnsureCapacity(kDispatchIndirectSize));
            *validatedIndirectBuffer = scratchBuffer.GetBuffer();

            DAWN_TRY_ASSIGN(*validationPipeline,
                            GetOrCreateIndirectDispatchValidationPipeline(device));

            Ref<BindGroupLayoutBase> layout;
            DAWN_TRY_ASSIGN(layout, (*validationPipeline)->GetBindGroupLayout(0));

            uint32_t storageBufferOffsetAlignment =
                device->GetLimits().v1.minStorageBufferOffsetAlignment;

            BindGroupEntry bindings[2];
            BindGroupEntry& clientIndirectBinding = bindings[0];
            clientIndirectBinding.binding = 0;
            clientIndirectBinding.buffer = indirectBuffer;

            const uint64_t offsetFromAlignedBoundary =
                indirectOffset % storageBufferOffsetAlignment;
            const uint64_t offsetAlignedDown = indirectOffset - offsetFromAlignedBoundary;

            clientIndirectBinding.offset = offsetAlignedDown;
            clientIndirectBinding.size = kDispatchIndirectSize + offsetFromAlignedBoundary;

            BindGroupEntry& validatedParamsBinding = bindings[1];
            validatedParamsBinding.binding = 1;
            validatedParamsBinding.buffer = validatedIndirectBuffer->Get();
            validatedParamsBinding.offset = 0;
            validatedParamsBinding.size = kDispatchIndirectSize;

            BindGroupDescriptor bindGroupDescriptor = {};
            bindGroupDescriptor.layout = layout.Get();
            bindGroupDescriptor.entryCount = 2;
            bindGroupDescriptor.entries = bindings;

            DAWN_TRY_ASSIGN(*bindGroup, device->CreateBindGroup(&bindGroupDescriptor));

            return {};
        }

    }  // namespace

    ComputePassEncoder::ComputePassEncoder(DeviceBase* device,
                                           CommandEncoder* commandEncoder,
                                           EncodingContext* encodingContext)
        : ProgrammablePassEncoder(device, encodingContext), mCommandEncoder(commandEncoder) {
    }

    ComputePassEncoder::ComputePassEncoder(DeviceBase* device,
                                           CommandEncoder* commandEncoder,
                                           EncodingContext* encodingContext,
                                           ErrorTag errorTag)
        : ProgrammablePassEncoder(device, encodingContext, errorTag),
          mCommandEncoder(commandEncoder) {
    }

    ComputePassEncoder* ComputePassEncoder::MakeError(DeviceBase* device,
                                                      CommandEncoder* commandEncoder,
                                                      EncodingContext* encodingContext) {
        return new ComputePassEncoder(device, commandEncoder, encodingContext, ObjectBase::kError);
    }

    ObjectType ComputePassEncoder::GetType() const {
        return ObjectType::ComputePassEncoder;
    }

    void ComputePassEncoder::APIEndPass() {
        if (mEncodingContext->TryEncode(
                this,
                [&](CommandAllocator* allocator) -> MaybeError {
                    if (IsValidationEnabled()) {
                        DAWN_TRY(ValidateProgrammableEncoderEnd());
                    }

                    allocator->Allocate<EndComputePassCmd>(Command::EndComputePass);

                    return {};
                },
                "encoding EndPass()")) {
            mEncodingContext->ExitComputePass(this, mUsageTracker.AcquireResourceUsage());
        }
    }

    void ComputePassEncoder::APIDispatch(uint32_t x, uint32_t y, uint32_t z) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(mCommandBufferState.ValidateCanDispatch());
                    DAWN_TRY(ValidatePerDimensionDispatchSizeLimit(GetDevice(), x));
                    DAWN_TRY(ValidatePerDimensionDispatchSizeLimit(GetDevice(), y));
                    DAWN_TRY(ValidatePerDimensionDispatchSizeLimit(GetDevice(), z));
                }

                // Record the synchronization scope for Dispatch, which is just the current
                // bindgroups.
                AddDispatchSyncScope();

                DispatchCmd* dispatch = allocator->Allocate<DispatchCmd>(Command::Dispatch);
                dispatch->x = x;
                dispatch->y = y;
                dispatch->z = z;

                return {};
            },
            "encoding Dispatch (x: %u, y: %u, z: %u)", x, y, z);
    }

    void ComputePassEncoder::APIDispatchIndirect(BufferBase* indirectBuffer,
                                                 uint64_t indirectOffset) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(indirectBuffer));
                    DAWN_TRY(ValidateCanUseAs(indirectBuffer, wgpu::BufferUsage::Indirect));
                    DAWN_TRY(mCommandBufferState.ValidateCanDispatch());

                    // Indexed dispatches need a compute-shader based validation to check that the
                    // dispatch sizes aren't too big. Disallow them as unsafe until the validation
                    // is implemented.
                    if (GetDevice()->IsToggleEnabled(Toggle::DisallowUnsafeAPIs)) {
                        return DAWN_VALIDATION_ERROR(
                            "DispatchIndirect is disallowed because it doesn't validate that the "
                            "dispatch "
                            "size is valid yet.");
                    }

                    if (indirectOffset % 4 != 0) {
                        return DAWN_VALIDATION_ERROR("Indirect offset must be a multiple of 4");
                    }

                    if (indirectOffset >= indirectBuffer->GetSize() ||
                        indirectOffset + kDispatchIndirectSize > indirectBuffer->GetSize()) {
                        return DAWN_VALIDATION_ERROR("Indirect offset out of bounds");
                    }
                }

                // Record the synchronization scope for Dispatch, both the bindgroups and the
                // indirect buffer.
                SyncScopeUsageTracker scope;
                scope.BufferUsedAs(indirectBuffer, wgpu::BufferUsage::Indirect);
                mUsageTracker.AddReferencedBuffer(indirectBuffer);
                AddDispatchSyncScope(std::move(scope));

                if (IsValidationEnabled()) {
                    // Validate each indirect dispatch with a single dispatch to copy the indirect
                    // buffer params into a scratch buffer if they're valid, and otherwise zero them
                    // out. We could consider moving the validation earlier in the pass after the
                    // last point the indirect buffer was used with writable usage, as well as batch
                    // validation for multiple dispatches into one, but inserting commands at
                    // arbitrary points in the past is not possible right now.
                    Ref<ComputePipelineBase> validationPipeline;
                    Ref<BufferBase> validatedIndirectBuffer;
                    Ref<BindGroupBase> validationBindGroup;
                    DAWN_TRY(CreateIndirectDispatchValidationResources(
                        GetDevice(), indirectBuffer, indirectOffset, &validationPipeline,
                        &validatedIndirectBuffer, &validationBindGroup));

                    ComputePipelineBase* previousPipeline =
                        static_cast<ComputePipelineBase*>(mCommandBufferState.GetPipeline());
                    BindGroupBase* previousBindGroup =
                        mCommandBufferState.GetBindGroup(BindGroupIndex(0));

                    // Issue commands to validate the indirect buffer.
                    APISetPipeline(validationPipeline.Get());
                    APISetBindGroup(0, validationBindGroup.Get());
                    APIDispatch(1);

                    // Reset the previously-bound pipeline.
                    APISetPipeline(previousPipeline);
                    if (previousBindGroup) {
                        // Reset the previously-bound bind group.
                        APISetBindGroup(0, previousBindGroup);
                    } else {
                        // Clear out the state if there was no prevous bind group.
                        mCommandBufferState.SetBindGroup(BindGroupIndex(0), nullptr);
                    }

                    // Validated indirect dispatch is written into a new buffer.
                    // Point the dispatch indirect command to it.
                    DispatchIndirectCmd* dispatch =
                        allocator->Allocate<DispatchIndirectCmd>(Command::DispatchIndirect);
                    dispatch->indirectBuffer = std::move(validatedIndirectBuffer);
                    dispatch->indirectOffset = 0;
                } else {
                    DispatchIndirectCmd* dispatch =
                        allocator->Allocate<DispatchIndirectCmd>(Command::DispatchIndirect);
                    dispatch->indirectBuffer = indirectBuffer;
                    dispatch->indirectOffset = indirectOffset;
                }

                return {};
            },
            "encoding DispatchIndirect with %s", indirectBuffer);
    }

    void ComputePassEncoder::APISetPipeline(ComputePipelineBase* pipeline) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(pipeline));
                }

                mCommandBufferState.SetComputePipeline(pipeline);

                SetComputePipelineCmd* cmd =
                    allocator->Allocate<SetComputePipelineCmd>(Command::SetComputePipeline);
                cmd->pipeline = pipeline;

                return {};
            },
            "encoding SetPipeline with %s", pipeline);
    }

    void ComputePassEncoder::APISetBindGroup(uint32_t groupIndexIn,
                                             BindGroupBase* group,
                                             uint32_t dynamicOffsetCount,
                                             const uint32_t* dynamicOffsets) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                BindGroupIndex groupIndex(groupIndexIn);

                if (IsValidationEnabled()) {
                    DAWN_TRY(ValidateSetBindGroup(groupIndex, group, dynamicOffsetCount,
                                                  dynamicOffsets));
                }

                mUsageTracker.AddResourcesReferencedByBindGroup(group);

                RecordSetBindGroup(allocator, groupIndex, group, dynamicOffsetCount,
                                   dynamicOffsets);
                mCommandBufferState.SetBindGroup(groupIndex, group);

                return {};
            },
            "encoding SetBindGroup with %s at index %u", group, groupIndexIn);
    }

    void ComputePassEncoder::APIWriteTimestamp(QuerySetBase* querySet, uint32_t queryIndex) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(querySet));
                    DAWN_TRY(ValidateTimestampQuery(querySet, queryIndex));
                }

                mCommandEncoder->TrackQueryAvailability(querySet, queryIndex);

                WriteTimestampCmd* cmd =
                    allocator->Allocate<WriteTimestampCmd>(Command::WriteTimestamp);
                cmd->querySet = querySet;
                cmd->queryIndex = queryIndex;

                return {};
            },
            "encoding WriteTimestamp to %s.", querySet);
    }

    void ComputePassEncoder::AddDispatchSyncScope(SyncScopeUsageTracker scope) {
        PipelineLayoutBase* layout = mCommandBufferState.GetPipelineLayout();
        for (BindGroupIndex i : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
            scope.AddBindGroup(mCommandBufferState.GetBindGroup(i));
        }
        mUsageTracker.AddDispatch(scope.AcquireSyncScopeUsage());
    }

}  // namespace dawn_native
