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

#include "dawn/native/interpreter/QueueInterpreter.h"

#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "dawn/native/CommandBuffer.h"
#include "dawn/native/ErrorData.h"
#include "dawn/native/interpreter/BindGroupInterpreter.h"
#include "dawn/native/interpreter/BufferInterpreter.h"
#include "dawn/native/interpreter/CommandBufferInterpreter.h"
#include "dawn/native/interpreter/DeviceInterpreter.h"
#include "tint/interp/data_race_detector.h"
#include "tint/interp/interactive_debugger.h"
#include "tint/interp/shader_executor.h"
#include "tint/lang/wgsl/ast/module.h"

namespace dawn::native::interpreter {

// static
Ref<Queue> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    return AcquireRef(new Queue(device, descriptor));
}

Queue::Queue(Device* device, const QueueDescriptor* descriptor) : QueueBase(device, descriptor) {}

Queue::~Queue() = default;

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    // Iterate over the command buffers.
    for (uint32_t i = 0; i < commandCount; ++i) {
        DAWN_TRY(ExecuteCommandBuffer(ToBackend(commands[i])));
    }
    return {};
}

MaybeError Queue::ExecuteCommandBuffer(CommandBuffer* commandBuffer) {
    std::unique_ptr<ComputePass> currentPass;

    // Iterate over the commands in the command buffer.
    CommandIterator* itr = commandBuffer->GetCommandIterator();
    Command type;
    while (itr->NextCommandId(&type)) {
        switch (type) {
            case Command::BeginComputePass: {
                itr->NextCommand<BeginComputePassCmd>();
                currentPass = std::make_unique<ComputePass>();
                break;
            }
            case Command::Dispatch: {
                DispatchCmd* dispatch = itr->NextCommand<DispatchCmd>();
                DAWN_TRY(Dispatch(currentPass.get(), dispatch->x, dispatch->y, dispatch->z));
                break;
            }
            case Command::DispatchIndirect: {
                DispatchIndirectCmd* dispatch = itr->NextCommand<DispatchIndirectCmd>();
                tint::interp::Memory& buffer = ToBackend(dispatch->indirectBuffer)->GetMemory();
                uint32_t x, y, z;
                buffer.Load(&x, dispatch->indirectOffset + 0);
                buffer.Load(&y, dispatch->indirectOffset + 4);
                buffer.Load(&z, dispatch->indirectOffset + 8);
                DAWN_TRY(Dispatch(currentPass.get(), x, y, z));
                break;
            }
            case Command::CopyBufferToBuffer: {
                CopyBufferToBufferCmd* copy = itr->NextCommand<CopyBufferToBufferCmd>();
                tint::interp::Memory& src = ToBackend(copy->source)->GetMemory();
                tint::interp::Memory& dst = ToBackend(copy->destination)->GetMemory();
                dst.CopyFrom(copy->destinationOffset, src, copy->sourceOffset, copy->size);
                break;
            }
            case Command::EndComputePass: {
                itr->NextCommand<EndComputePassCmd>();
                currentPass.reset();
                break;
            }
            case Command::SetBindGroup: {
                SetBindGroupCmd* cmd = itr->NextCommand<SetBindGroupCmd>();
                currentPass->bindGroups[cmd->index] = cmd->group;
                if (cmd->dynamicOffsetCount > 0) {
                    uint32_t* dynamicOffsets = itr->NextData<uint32_t>(cmd->dynamicOffsetCount);
                    currentPass->dynamicOffsets[cmd->index].assign(
                        dynamicOffsets, dynamicOffsets + cmd->dynamicOffsetCount);
                } else {
                    // currentPass->dynamicOffsets.erase(cmd->index);
                }
                break;
            }
            case Command::SetComputePipeline: {
                SetComputePipelineCmd* cmd = itr->NextCommand<SetComputePipelineCmd>();
                currentPass->pipeline = cmd->pipeline;
                break;
            }
            default:
                return DAWN_UNIMPLEMENTED_ERROR("unhandled command type: " +
                                                std::to_string(static_cast<int>(type)));
        }
    }

    return {};
}

MaybeError Queue::Dispatch(const ComputePass* pass,
                           uint32_t groupsX,
                           uint32_t groupsY,
                           uint32_t groupsZ) {
    const ProgrammableStage& stage = pass->pipeline->GetStage(SingleShaderStage::Compute);
    Ref<ShaderModuleBase> module = stage.module;
    auto scoped_program = module->UseTintProgram();
    auto program = scoped_program->GetTintProgram();

    // Map bindings to their corresponding Tint interpreter resources.
    tint::interp::BindingList bindings;
    for (BindGroupIndex idx{0}; idx < pass->bindGroups.size(); idx++) {
        Ref<BindGroupBase> group = pass->bindGroups[idx];
        if (group == nullptr) {
            continue;
        }

        BindGroupLayoutInternalBase* layout = group->GetLayout();
        uint32_t dynamicOffsetIndex = 0;
        for (BindingIndex index{0}; index < layout->GetBindingCount(); index++) {
            const BindingInfo& info = layout->GetBindingInfo(index);
            if (!std::holds_alternative<BufferBindingLayout>(info.bindingLayout)) {
                return DAWN_UNIMPLEMENTED_ERROR("unhandled binding type");
            }
            auto bufferLayout = std::get<BufferBindingLayout>(info.bindingLayout);
            BufferBinding buffer = group->GetBindingAsBufferBinding(index);
            tint::interp::Memory& memory = ToBackend(buffer.buffer)->GetMemory();

            uint64_t offset = buffer.offset;
            if (bufferLayout.hasDynamicOffset) {
                offset += pass->dynamicOffsets.at(idx)[dynamicOffsetIndex];
                dynamicOffsetIndex++;
            }

            bindings[{static_cast<uint32_t>(idx), static_cast<uint32_t>(info.binding)}] =
                tint::interp::Binding::MakeBufferBinding(&memory, offset, buffer.size);
        }
    }

    // Get the values of pipeline-overridable constants.
    tint::interp::NamedOverrideList overrides;
    for (auto& entry : stage.constants) {
        overrides.insert({entry});
    }

    // Create the shader executor.
    std::unique_ptr<tint::interp::ShaderExecutor> shaderExecutor;
    {
        auto result = tint::interp::ShaderExecutor::Create(program->program, stage.entryPoint,
                                                           std::move(overrides));
        if (result != tint::Success) {
            return DAWN_INTERNAL_ERROR("Create failed: " + result.Failure());
        }
        shaderExecutor = result.Move();
    }

    // Enable data race detection if requested.
    std::unique_ptr<tint::interp::DataRaceDetector> dataRaceDetector;
    if (GetDevice()->IsToggleEnabled(Toggle::WgslInterpreterEnableDRD)) {
        dataRaceDetector = std::make_unique<tint::interp::DataRaceDetector>(*shaderExecutor);
    }

    // Set up the interactive debugger if requested.
    std::unique_ptr<tint::interp::InteractiveDebugger> debugger;
    if (GetDevice()->IsToggleEnabled(Toggle::WgslInterpreterInteractive)) {
        debugger = std::make_unique<tint::interp::InteractiveDebugger>(*shaderExecutor, std::cin);
    }

    // Run the shader.
    auto result = shaderExecutor->Run({groupsX, groupsY, groupsZ}, std::move(bindings));
    if (result != tint::Success) {
        return DAWN_INTERNAL_ERROR("Run failed: " + result.Failure());
    }

    return {};
}

ResultOrError<ExecutionSerial> Queue::CheckAndUpdateCompletedSerials() {
    return DAWN_UNIMPLEMENTED_ERROR("interpreter::Queue::CheckAndUpdateCompletedSerials");
}

MaybeError Queue::WaitForIdleForDestruction() {
    return {};
}

bool Queue::HasPendingCommands() const {
    return false;
}

MaybeError Queue::SubmitPendingCommands() {
    return DAWN_UNIMPLEMENTED_ERROR("interpreter::Queue::SubmitPendingCommands");
}

void Queue::ForceEventualFlushOfCommands() {}

ResultOrError<bool> Queue::WaitForQueueSerial(ExecutionSerial serial, Nanoseconds timeout) {
    return DAWN_UNIMPLEMENTED_ERROR("interpreter::Queue::WaitForQueueSerial");
}

}  // namespace dawn::native::interpreter
