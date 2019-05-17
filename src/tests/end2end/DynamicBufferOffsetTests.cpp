// Copyright 2019 The Dawn Authors
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

#include "tests/DawnTest.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

constexpr uint32_t kRTSize = 400;
constexpr uint32_t kBufferElementsCount = kMinDynamicBufferOffsetAlignment / sizeof(uint32_t) + 2;
constexpr uint32_t kBufferSize = kBufferElementsCount * sizeof(uint32_t);
constexpr dawn::TextureFormat kColorFormat = dawn::TextureFormat::R8G8B8A8Uint;

class DynamicBufferOffsetTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        std::array<uint32_t, kBufferElementsCount> uniformData = {0};

        uniformData[0] = 1;
        uniformData[1] = 2;
        uniformData[uniformData.size() - 2] = 5;
        uniformData[uniformData.size() - 1] = 6;

        mUniformBuffer = utils::CreateBufferFromData(device, uniformData.data(), kBufferSize,
                                                     dawn::BufferUsageBit::Uniform);

        dawn::BufferDescriptor storageBufferDescriptor;
        storageBufferDescriptor.size = kBufferSize;
        storageBufferDescriptor.usage = dawn::BufferUsageBit::Storage |
                                        dawn::BufferUsageBit::TransferDst |
                                        dawn::BufferUsageBit::TransferSrc;

        mStorageBuffer = device.CreateBuffer(&storageBufferDescriptor);

        dawn::BufferDescriptor mapReadBufferDescriptor;
        mapReadBufferDescriptor.size = kBufferSize;
        mapReadBufferDescriptor.usage =
            dawn::BufferUsageBit::MapRead | dawn::BufferUsageBit::TransferDst;

        mMapReadBuffer = device.CreateBuffer(&mapReadBufferDescriptor);

        mBindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, dawn::ShaderStageBit::Compute | dawn::ShaderStageBit::Fragment,
                      dawn::BindingType::DynamicUniformBuffer},
                     {1, dawn::ShaderStageBit::Compute | dawn::ShaderStageBit::Fragment,
                      dawn::BindingType::DynamicStorageBuffer}});

        mBindGroup = utils::MakeBindGroup(
            device, mBindGroupLayout,
            {{0, mUniformBuffer, 0, kBufferSize}, {1, mStorageBuffer, 0, kBufferSize}});

        dawn::TextureDescriptor textureDescriptor;
        textureDescriptor.dimension = dawn::TextureDimension::e2D;
        textureDescriptor.size.width = kRTSize;
        textureDescriptor.size.height = kRTSize;
        textureDescriptor.size.depth = 1;
        textureDescriptor.arrayLayerCount = 1;
        textureDescriptor.sampleCount = 1;
        textureDescriptor.format = kColorFormat;
        textureDescriptor.mipLevelCount = 1;
        textureDescriptor.usage =
            dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
        mColorAttachment = device.CreateTexture(&textureDescriptor);
    }
    // Create objects to use as resources inside test bind groups.

    const void* mappedData = nullptr;
    dawn::BindGroup mBindGroup;
    dawn::BindGroupLayout mBindGroupLayout;
    dawn::Buffer mUniformBuffer;
    dawn::Buffer mStorageBuffer;
    dawn::Buffer mMapReadBuffer;
    dawn::Texture mColorAttachment;

    static void MapReadCallback(DawnBufferMapAsyncStatus status,
                                const void* data,
                                uint64_t,
                                DawnCallbackUserdata userdata) {
        ASSERT_EQ(DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS, status);
        ASSERT_NE(nullptr, data);

        auto test = reinterpret_cast<DynamicBufferOffsetTests*>(static_cast<uintptr_t>(userdata));
        test->mappedData = data;
    }

    const void* MapReadAsyncAndWait() {
        mMapReadBuffer.MapReadAsync(MapReadCallback, static_cast<dawn::CallbackUserdata>(
                                                         reinterpret_cast<uintptr_t>(this)));

        while (mappedData == nullptr) {
            WaitABit();
        }

        return mappedData;
    }

    dawn::RenderPipeline CreateRenderPipeline() {
        dawn::ShaderModule vsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
                #version 450
                void main() {
                    const vec2 pos[3] = vec2[3](vec2(-1.0f, 0.0f), vec2(-1.0f, -1.0f), vec2(0.0f, -1.0f));
                    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
                })");

        dawn::ShaderModule fsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
                #version 450
                layout(std140, set = 0, binding = 0) uniform uBuffer {
                     uvec2 value;
                };
                layout(std140, set = 0, binding = 1) buffer SBuffer {
                     uvec2 result;
                } sBuffer;
                layout(location = 0) out uvec4 fragColor;
                void main() {
                    sBuffer.result.xy = value.xy;
                    fragColor = uvec4(value.x, value.y, 255, 255);
                })");

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.cVertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        pipelineDescriptor.cColorStates[0]->format = kColorFormat;
        dawn::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &mBindGroupLayout);
        pipelineDescriptor.layout = pipelineLayout;

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    dawn::ComputePipeline CreateComputePipeline() {
        dawn::ShaderModule csModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Compute, R"(
                #version 450
                const uint kTileSize = 4;
                const uint kInstances = 11;

                layout(local_size_x = kTileSize, local_size_y = kTileSize, local_size_z = 1) in;
                layout(std140, set = 0, binding = 0) uniform UniformBuffer {
                    uvec2 value;
                };
                layout(std140, set = 0, binding = 1) buffer SBuffer {
                    uvec2 result;
                } sBuffer;

        void main() {
            sBuffer.result.xy = value.xy;
        })");

        dawn::ComputePipelineDescriptor csDesc;
        dawn::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &mBindGroupLayout);
        csDesc.layout = pipelineLayout;

        dawn::PipelineStageDescriptor computeStage;
        computeStage.module = csModule;
        computeStage.entryPoint = "main";
        csDesc.computeStage = &computeStage;

        return device.CreateComputePipeline(&csDesc);
    }
};

// Dynamic offsets are all zero and no effect to result.
TEST_P(DynamicBufferOffsetTests, BasicRenderPipeline) {
    dawn::RenderPipeline pipeline = CreateRenderPipeline();
    utils::BasicRenderPass renderPass =
        utils::BasicRenderPass(kRTSize, kRTSize, mColorAttachment, kColorFormat);

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    std::array<uint64_t, 2> offsets = {0, 0};
    dawn::RenderPassEncoder renderPassEncoder =
        commandEncoder.BeginRenderPass(&renderPass.renderPassInfo);
    renderPassEncoder.SetPipeline(pipeline);
    renderPassEncoder.SetBindGroup(0, mBindGroup, 2, offsets.data());
    renderPassEncoder.Draw(3, 1, 0, 0);
    renderPassEncoder.EndPass();
    commandEncoder.CopyBufferToBuffer(mStorageBuffer, 0, mMapReadBuffer, 0, kBufferSize);
    dawn::CommandBuffer commands = commandEncoder.Finish();
    queue.Submit(1, &commands);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(1, 2, 255, 255), renderPass.color, 0, 0);

    const uint32_t* mappedData = static_cast<const uint32_t*>(MapReadAsyncAndWait());
    ASSERT_EQ(static_cast<uint32_t>(1), mappedData[0]);
    ASSERT_EQ(static_cast<uint32_t>(2), mappedData[1]);
}

// Have non-zeor dynamic offsets.
TEST_P(DynamicBufferOffsetTests, SetDynamicOffestsRenderPipeline) {
    dawn::RenderPipeline pipeline = CreateRenderPipeline();
    utils::BasicRenderPass renderPass =
        utils::BasicRenderPass(kRTSize, kRTSize, mColorAttachment, kColorFormat);

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    std::array<uint64_t, 2> offsets = {kMinDynamicBufferOffsetAlignment,
                                       kMinDynamicBufferOffsetAlignment};
    dawn::RenderPassEncoder renderPassEncoder =
        commandEncoder.BeginRenderPass(&renderPass.renderPassInfo);
    renderPassEncoder.SetPipeline(pipeline);
    renderPassEncoder.SetBindGroup(0, mBindGroup, 2, offsets.data());
    renderPassEncoder.Draw(3, 1, 0, 0);
    renderPassEncoder.EndPass();
    commandEncoder.CopyBufferToBuffer(mStorageBuffer, 0, mMapReadBuffer, 0, kBufferSize);
    dawn::CommandBuffer commands = commandEncoder.Finish();
    queue.Submit(1, &commands);
    EXPECT_PIXEL_RGBA8_EQ(RGBA8(5, 6, 255, 255), renderPass.color, 0, 0);

    const uint32_t* mappedData = static_cast<const uint32_t*>(MapReadAsyncAndWait());
    ASSERT_EQ(static_cast<uint32_t>(5), mappedData[kBufferElementsCount - 2]);
    ASSERT_EQ(static_cast<uint32_t>(6), mappedData[kBufferElementsCount - 1]);
}

// Dynamic offsets are all zero and no effect to result.
TEST_P(DynamicBufferOffsetTests, BasicComputePipeline) {
    dawn::ComputePipeline pipeline = CreateComputePipeline();

    std::array<uint64_t, 2> offsets = {0, 0};

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
    computePassEncoder.SetPipeline(pipeline);
    computePassEncoder.SetBindGroup(0, mBindGroup, 2, offsets.data());
    computePassEncoder.Dispatch(1, 1, 1);
    computePassEncoder.EndPass();
    commandEncoder.CopyBufferToBuffer(mStorageBuffer, 0, mMapReadBuffer, 0, kBufferSize);
    dawn::CommandBuffer commands = commandEncoder.Finish();
    queue.Submit(1, &commands);

    const uint32_t* mappedData = static_cast<const uint32_t*>(MapReadAsyncAndWait());
    ASSERT_EQ(static_cast<uint32_t>(1), mappedData[0]);
    ASSERT_EQ(static_cast<uint32_t>(2), mappedData[1]);
}

// Have non-zeor dynamic offsets.
TEST_P(DynamicBufferOffsetTests, SetDynamicOffestsComputePipeline) {
    dawn::ComputePipeline pipeline = CreateComputePipeline();

    std::array<uint64_t, 2> offsets = {kMinDynamicBufferOffsetAlignment,
                                       kMinDynamicBufferOffsetAlignment};

    dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    dawn::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
    computePassEncoder.SetPipeline(pipeline);
    computePassEncoder.SetBindGroup(0, mBindGroup, 2, offsets.data());
    computePassEncoder.Dispatch(1, 1, 1);
    computePassEncoder.EndPass();
    commandEncoder.CopyBufferToBuffer(mStorageBuffer, 0, mMapReadBuffer, 0, kBufferSize);
    dawn::CommandBuffer commands = commandEncoder.Finish();
    queue.Submit(1, &commands);

    const uint32_t* mappedData = static_cast<const uint32_t*>(MapReadAsyncAndWait());
    ASSERT_EQ(static_cast<uint32_t>(5), mappedData[kBufferElementsCount - 2]);
    ASSERT_EQ(static_cast<uint32_t>(6), mappedData[kBufferElementsCount - 1]);
}

DAWN_INSTANTIATE_TEST(DynamicBufferOffsetTests, MetalBackend);
