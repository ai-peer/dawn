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

#include "tests/DawnNativeTest.h"

#include "dawn_native/CommandBuffer.h"
#include "dawn_native/Commands.h"
#include "dawn_native/ComputePassEncoder.h"
#include "utils/WGPUHelpers.h"

class CommandBufferEncodingTests : public DawnNativeTest {
  protected:
    void ExpectCommands(dawn_native::CommandIterator* commands,
                        std::vector<std::pair<dawn_native::Command,
                                              std::function<void(dawn_native::CommandIterator*)>>>
                            expectedCommands) {
        dawn_native::Command commandId;
        uint32_t commandIndex = 0;
        while (commands->NextCommandId(&commandId)) {
            EXPECT_LT(commandIndex, expectedCommands.size()) << "Unexpected command";
            if (commandIndex < expectedCommands.size()) {
                EXPECT_EQ(commandId, expectedCommands[commandIndex].first)
                    << "at command " << commandIndex;
                if (commandId == expectedCommands[commandIndex].first) {
                    expectedCommands[commandIndex].second(commands);
                    ++commandIndex;
                    continue;
                }
            }
            ++commandIndex;
            SkipCommand(commands, commandId);
        }
    }
};

// Indirect dispatch validation changes the bind groups in the middle
// of a pass. Test that bindings are restored after the validation runs.
TEST_F(CommandBufferEncodingTests, ComputePassEncoderIndirectDispatchStateRestoration) {
    using namespace dawn_native;

    wgpu::BindGroupLayout staticLayout =
        utils::MakeBindGroupLayout(device, {{
                                               0,
                                               wgpu::ShaderStage::Compute,
                                               wgpu::BufferBindingType::Uniform,
                                           }});

    wgpu::BindGroupLayout dynamicLayout =
        utils::MakeBindGroupLayout(device, {{
                                               0,
                                               wgpu::ShaderStage::Compute,
                                               wgpu::BufferBindingType::Uniform,
                                               true,
                                           }});

    // Create a simple pipeline
    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = utils::CreateShaderModule(device, R"(
        [[stage(compute), workgroup_size(1, 1, 1)]]
        fn main() {
        })");
    csDesc.compute.entryPoint = "main";

    wgpu::PipelineLayout pl0 = utils::MakePipelineLayout(device, {staticLayout, dynamicLayout});
    csDesc.layout = pl0;
    wgpu::ComputePipeline pipeline0 = device.CreateComputePipeline(&csDesc);

    wgpu::PipelineLayout pl1 = utils::MakePipelineLayout(device, {dynamicLayout, staticLayout});
    csDesc.layout = pl1;
    wgpu::ComputePipeline pipeline1 = device.CreateComputePipeline(&csDesc);

    // Create a simple buffer to use for both the indirect buffer and the bind groups.
    wgpu::Buffer indirectBuffer =
        utils::CreateBufferFromData<uint32_t>(device, wgpu::BufferUsage::Indirect, {1, 2, 3});

    wgpu::BufferDescriptor uniformBufferDesc = {};
    uniformBufferDesc.size = 512;
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform;
    wgpu::Buffer uniformBuffer = device.CreateBuffer(&uniformBufferDesc);

    wgpu::BindGroup staticBG = utils::MakeBindGroup(device, staticLayout, {{0, uniformBuffer}});

    wgpu::BindGroup dynamicBG =
        utils::MakeBindGroup(device, dynamicLayout, {{0, uniformBuffer, 0, 256}});

    uint32_t dynamicOffset = 256;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();

    CommandBufferStateTracker* stateTracker =
        FromAPI(pass.Get())->GetCommandBufferStateTrackerForTesting();

    pass.SetPipeline(pipeline0);
    pass.SetBindGroup(0, staticBG);
    pass.SetBindGroup(1, dynamicBG, 1, &dynamicOffset);
    EXPECT_EQ(ToAPI(stateTracker->GetComputePipeline()), pipeline0.Get());

    pass.DispatchIndirect(indirectBuffer, 0);

    // Expect restored state.
    EXPECT_EQ(ToAPI(stateTracker->GetComputePipeline()), pipeline0.Get());
    EXPECT_EQ(ToAPI(stateTracker->GetPipelineLayout()), pl0.Get());

    pass.DispatchIndirect(indirectBuffer, 0);

    // Expect restored pipeline.
    EXPECT_EQ(ToAPI(stateTracker->GetComputePipeline()), pipeline0.Get());
    EXPECT_EQ(ToAPI(stateTracker->GetPipelineLayout()), pl0.Get());

    // Change the pipeline
    pass.SetPipeline(pipeline1);
    pass.SetBindGroup(0, dynamicBG, 1, &dynamicOffset);
    pass.SetBindGroup(1, staticBG);
    EXPECT_EQ(ToAPI(stateTracker->GetComputePipeline()), pipeline1.Get());
    EXPECT_EQ(ToAPI(stateTracker->GetPipelineLayout()), pl1.Get());

    pass.DispatchIndirect(indirectBuffer, 0);

    // Expect restored pipeline.
    EXPECT_EQ(ToAPI(stateTracker->GetComputePipeline()), pipeline1.Get());
    EXPECT_EQ(ToAPI(stateTracker->GetPipelineLayout()), pl1.Get());

    pass.EndPass();

    wgpu::CommandBuffer commandBuffer = encoder.Finish();

    auto ExpectSetPipeline = [](wgpu::ComputePipeline pipeline) {
        return [pipeline](CommandIterator* commands) {
            auto* cmd = commands->NextCommand<SetComputePipelineCmd>();
            EXPECT_EQ(ToAPI(cmd->pipeline.Get()), pipeline.Get());
        };
    };

    auto ExpectSetBindGroup = [](uint32_t index, wgpu::BindGroup bg,
                                 std::vector<uint32_t> offsets = {}) {
        return [index, bg, offsets](CommandIterator* commands) {
            auto* cmd = commands->NextCommand<SetBindGroupCmd>();
            uint32_t* dynamicOffsets = nullptr;
            if (cmd->dynamicOffsetCount > 0) {
                dynamicOffsets = commands->NextData<uint32_t>(cmd->dynamicOffsetCount);
            }

            ASSERT_EQ(cmd->index, BindGroupIndex(index));
            ASSERT_EQ(ToAPI(cmd->group.Get()), bg.Get());
            ASSERT_EQ(cmd->dynamicOffsetCount, offsets.size());
            for (uint32_t i = 0; i < cmd->dynamicOffsetCount; ++i) {
                ASSERT_EQ(dynamicOffsets[i], offsets[i]);
            }
        };
    };

    auto ExpectSetValidationPipeline = [&](CommandIterator* commands) {
        auto* cmd = commands->NextCommand<SetComputePipelineCmd>();
        EXPECT_NE(cmd->pipeline.Get(), nullptr);
    };

    auto ExpectSetValidationBindGroup = [&](CommandIterator* commands) {
        auto* cmd = commands->NextCommand<SetBindGroupCmd>();
        ASSERT_EQ(cmd->index, BindGroupIndex(0));
        ASSERT_NE(cmd->group.Get(), nullptr);
        ASSERT_EQ(cmd->dynamicOffsetCount, 0u);
    };

    auto ExpectSetValidationDispatch = [&](CommandIterator* commands) {
        auto* cmd = commands->NextCommand<DispatchCmd>();
        ASSERT_EQ(cmd->x, 1u);
        ASSERT_EQ(cmd->y, 1u);
        ASSERT_EQ(cmd->z, 1u);
    };

    ExpectCommands(
        FromAPI(commandBuffer.Get())->GetCommandIteratorForTesting(),
        {
            {Command::BeginComputePass,
             [&](CommandIterator* commands) { SkipCommand(commands, Command::BeginComputePass); }},
            // Expect the state to be set.
            {Command::SetComputePipeline, ExpectSetPipeline(pipeline0)},
            {Command::SetBindGroup, ExpectSetBindGroup(0, staticBG)},
            {Command::SetBindGroup, ExpectSetBindGroup(1, dynamicBG, {dynamicOffset})},

            // Expect the validation.
            {Command::SetComputePipeline, ExpectSetValidationPipeline},
            {Command::SetBindGroup, ExpectSetValidationBindGroup},
            {Command::Dispatch, ExpectSetValidationDispatch},

            // Expect the state to be restored.
            {Command::SetComputePipeline, ExpectSetPipeline(pipeline0)},
            {Command::SetBindGroup, ExpectSetBindGroup(0, staticBG)},
            {Command::SetBindGroup, ExpectSetBindGroup(1, dynamicBG, {dynamicOffset})},

            // Expect the dispatchIndirect.
            {Command::DispatchIndirect,
             [&](CommandIterator* commands) { commands->NextCommand<DispatchIndirectCmd>(); }},

            // Expect the validation.
            {Command::SetComputePipeline, ExpectSetValidationPipeline},
            {Command::SetBindGroup, ExpectSetValidationBindGroup},
            {Command::Dispatch, ExpectSetValidationDispatch},

            // Expect the state to be restored.
            {Command::SetComputePipeline, ExpectSetPipeline(pipeline0)},
            {Command::SetBindGroup, ExpectSetBindGroup(0, staticBG)},
            {Command::SetBindGroup, ExpectSetBindGroup(1, dynamicBG, {dynamicOffset})},

            // Expect the dispatchIndirect.
            {Command::DispatchIndirect,
             [&](CommandIterator* commands) { commands->NextCommand<DispatchIndirectCmd>(); }},

            // Expect the state to be set (new pipeline).
            {Command::SetComputePipeline, ExpectSetPipeline(pipeline1)},
            {Command::SetBindGroup, ExpectSetBindGroup(0, dynamicBG, {dynamicOffset})},
            {Command::SetBindGroup, ExpectSetBindGroup(1, staticBG)},

            // Expect the validation.
            {Command::SetComputePipeline, ExpectSetValidationPipeline},
            {Command::SetBindGroup, ExpectSetValidationBindGroup},
            {Command::Dispatch, ExpectSetValidationDispatch},

            // Expect the state to be restored.
            {Command::SetComputePipeline, ExpectSetPipeline(pipeline1)},
            {Command::SetBindGroup, ExpectSetBindGroup(0, dynamicBG, {dynamicOffset})},
            {Command::SetBindGroup, ExpectSetBindGroup(1, staticBG)},

            // Expect the dispatchIndirect.
            {Command::DispatchIndirect,
             [&](CommandIterator* commands) { commands->NextCommand<DispatchIndirectCmd>(); }},

            {Command::EndComputePass,
             [&](CommandIterator* commands) { commands->NextCommand<EndComputePassCmd>(); }},
        });
}
