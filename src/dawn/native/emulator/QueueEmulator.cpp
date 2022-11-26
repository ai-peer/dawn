// Copyright 2023 The Dawn Authors
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

#include "dawn/native/emulator/QueueEmulator.h"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "dawn/native/CommandBuffer.h"
#include "dawn/native/ErrorData.h"
#include "dawn/native/emulator/BindGroupEmulator.h"
#include "dawn/native/emulator/BufferEmulator.h"
#include "dawn/native/emulator/DeviceEmulator.h"
#include "tint/ast/module.h"
#include "tint/interp/data_race_detector.h"
#include "tint/interp/interactive_debugger.h"
#include "tint/interp/shader_executor.h"

namespace dawn::native::emulator {

// static
Ref<Queue> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    Ref<Queue> queue = AcquireRef(new Queue(device, descriptor));
    return queue;
}

Queue::Queue(Device* device, const QueueDescriptor* descriptor) : QueueBase(device, descriptor) {}

Queue::~Queue() {}

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    // Track the current pipeline and bind groups.
    Ref<PipelineBase> currentPipeline = nullptr;
    std::unordered_map<uint32_t, Ref<BindGroupBase>> currentBindGroups;
    std::unordered_map<uint32_t, std::vector<uint32_t>> currentDynamicOffsets;

    // Iterate over the command buffers.
    for (uint32_t i = 0; i < commandCount; ++i) {
        // Iterate over the commands in the command buffer.
        CommandIterator* itr = (commands[i])->GetCommandIteratorForTesting();
        Command type;
        while (itr->NextCommandId(&type)) {
            switch (type) {
                case Command::BeginComputePass: {
                    itr->NextCommand<BeginComputePassCmd>();
                    break;
                }
                case Command::Dispatch: {
                    DispatchCmd* dispatch = itr->NextCommand<DispatchCmd>();
                    DAWN_TRY(Dispatch(currentPipeline, currentBindGroups, currentDynamicOffsets,
                                      dispatch->x, dispatch->y, dispatch->z));
                    break;
                }
                case Command::DispatchIndirect: {
                    DispatchIndirectCmd* dispatch = itr->NextCommand<DispatchIndirectCmd>();
                    tint::interp::Memory& buffer = ToBackend(dispatch->indirectBuffer)->Get();
                    uint32_t x, y, z;
                    buffer.Load(&x, dispatch->indirectOffset + 0, 4);
                    buffer.Load(&y, dispatch->indirectOffset + 4, 4);
                    buffer.Load(&z, dispatch->indirectOffset + 8, 4);
                    DAWN_TRY(Dispatch(currentPipeline, currentBindGroups, currentDynamicOffsets, x,
                                      y, z));
                    break;
                }
                case Command::CopyBufferToBuffer: {
                    CopyBufferToBufferCmd* copy = itr->NextCommand<CopyBufferToBufferCmd>();
                    tint::interp::Memory& src = ToBackend(copy->source)->Get();
                    tint::interp::Memory& dst = ToBackend(copy->destination)->Get();
                    dst.CopyFrom(copy->destinationOffset, src, copy->sourceOffset, copy->size);
                    break;
                }
                case Command::EndComputePass: {
                    itr->NextCommand<EndComputePassCmd>();
                    break;
                }
                case Command::SetBindGroup: {
                    SetBindGroupCmd* cmd = itr->NextCommand<SetBindGroupCmd>();
                    uint32_t index = static_cast<uint32_t>(cmd->index);
                    currentBindGroups[index] = cmd->group;
                    if (cmd->dynamicOffsetCount > 0) {
                        uint32_t* dynamicOffsets = itr->NextData<uint32_t>(cmd->dynamicOffsetCount);
                        currentDynamicOffsets[index].assign(
                            dynamicOffsets, dynamicOffsets + cmd->dynamicOffsetCount);
                    } else {
                        currentDynamicOffsets.erase(index);
                    }
                    break;
                }
                case Command::SetComputePipeline: {
                    SetComputePipelineCmd* cmd = itr->NextCommand<SetComputePipelineCmd>();
                    currentPipeline = cmd->pipeline;
                    break;
                }
                default:
                    return DAWN_UNIMPLEMENTED_ERROR("unhandled command type: " +
                                                    std::to_string(static_cast<int>(type)));
            }
        }
    }

    return {};
}

MaybeError Queue::Dispatch(Ref<PipelineBase> pipeline,
                           std::unordered_map<uint32_t, Ref<BindGroupBase>>& bindGroups,
                           std::unordered_map<uint32_t, std::vector<uint32_t>>& dynamicOffsets,
                           uint32_t groupsX,
                           uint32_t groupsY,
                           uint32_t groupsZ) {
    const ProgrammableStage& stage = pipeline->GetStage(SingleShaderStage::Compute);
    Ref<ShaderModuleBase> module = stage.module;
    const tint::Program* program = module->GetTintProgram();

    // Map bindings to their corresponding Tint interpreter resources.
    tint::interp::BindingList bindings;
    for (auto& itr : bindGroups) {
        Ref<BindGroupBase> group = itr.second;
        BindGroupLayoutBase* layout = group->GetLayout();
        uint32_t dynamicOffsetIndex = 0;
        for (BindingIndex index{0}; index < layout->GetBindingCount(); index++) {
            const BindingInfo& info = layout->GetBindingInfo(index);
            switch (info.bindingType) {
                case BindingInfoType::Buffer: {
                    BufferBinding buffer = group->GetBindingAsBufferBinding(index);
                    tint::interp::Memory& memory = ToBackend(buffer.buffer)->Get();

                    uint64_t offset = buffer.offset;
                    if (info.buffer.hasDynamicOffset) {
                        offset += dynamicOffsets[itr.first][dynamicOffsetIndex];
                        dynamicOffsetIndex++;
                    }

                    bindings[{itr.first, static_cast<uint32_t>(info.binding)}] =
                        tint::interp::Binding::MakeBufferBinding(&memory, offset, buffer.size);
                    break;
                }
                default:
                    return DAWN_UNIMPLEMENTED_ERROR("unhandled binding type");
            }
        }
    }

    // Get the values of pipeline-overridable constants.
    tint::interp::NamedOverrideList overrides;
    for (auto& entry : stage.constants) {
        overrides.insert({entry});
    }

    // Create the shader executor.
    std::unique_ptr<tint::interp::ShaderExecutor> shader_executor;
    {
        auto result =
            tint::interp::ShaderExecutor::Create(*program, stage.entryPoint, std::move(overrides));
        if (!result) {
            return DAWN_INTERNAL_ERROR("Create failed: " + result.Failure());
        } else {
            shader_executor = result.Move();
        }
    }

    // Enable data race detection if requested.
    std::unique_ptr<tint::interp::DataRaceDetector> data_race_detector;
    if (GetDevice()->IsToggleEnabled(Toggle::EnableDRD)) {
        data_race_detector = std::make_unique<tint::interp::DataRaceDetector>(*shader_executor);
    }

    // Set up the interactive debugger if requested.
    std::unique_ptr<tint::interp::InteractiveDebugger> debugger;
    if (GetDevice()->IsToggleEnabled(Toggle::Interactive)) {
        debugger = std::make_unique<tint::interp::InteractiveDebugger>(*shader_executor);
    }

    // Run the shader.
    auto result = shader_executor->Run({groupsX, groupsY, groupsZ}, std::move(bindings));
    if (!result) {
        return DAWN_INTERNAL_ERROR("Run failed: " + result.Failure());
    }

    return {};
}

}  // namespace dawn::native::emulator
