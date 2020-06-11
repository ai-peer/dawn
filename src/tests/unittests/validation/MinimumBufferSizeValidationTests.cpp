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

#include "tests/unittests/validation/ValidationTest.h"

#include "common/Assert.h"
#include "common/Constants.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class MinimumBufferSizeValidationTest : public ValidationTest {
  public:
    void SetUp() override {
        mBindGroupLayout = CreateBasicLayout({0, 0, 0});
    }

    wgpu::BindGroupLayout CreateBasicLayout(const std::vector<uint64_t>& minimumSizes) {
        return utils::MakeBindGroupLayout(
            device,
            {{0,
              wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Vertex,
              wgpu::BindingType::UniformBuffer, false, false, wgpu::TextureViewDimension::Undefined,
              wgpu::TextureComponentType::Float, wgpu::TextureFormat::Undefined, minimumSizes[0]},
             {1, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
              wgpu::BindingType::StorageBuffer, false, false, wgpu::TextureViewDimension::Undefined,
              wgpu::TextureComponentType::Float, wgpu::TextureFormat::Undefined, minimumSizes[1]},
             {2, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
              wgpu::BindingType::ReadonlyStorageBuffer, false, false,
              wgpu::TextureViewDimension::Undefined, wgpu::TextureComponentType::Float,
              wgpu::TextureFormat::Undefined, minimumSizes[2]}});
    }

    wgpu::Buffer CreateBuffer(uint64_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = usage;

        return device.CreateBuffer(&bufferDescriptor);
    }

    wgpu::BindGroupLayout mBindGroupLayout;

    wgpu::RenderPipeline CreateRenderPipeline(const wgpu::BindGroupLayout& bindGroupLayout) {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                layout(std140, set = 0, binding = 0) uniform uBuffer {
                    float value0;
                    float value1;
                };
                void main() {
                })");

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
                #version 450
                layout(std140, set = 0, binding = 0) uniform uBuffer {
                    float value0;
                };
                layout(std140, set = 0, binding = 1) buffer sBuffer {
                    float value1;
                } sBuffer2;
                layout(std140, set = 0, binding = 2) readonly buffer rBuffer {
                    readonly float value3;
                } rBuffer2;
                layout(location = 0) out vec4 fragColor;
                void main() {
                })");

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        wgpu::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &bindGroupLayout);
        pipelineDescriptor.layout = pipelineLayout;
        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    wgpu::ComputePipeline CreateComputePipeline(const wgpu::BindGroupLayout& bindGroupLayout) {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
                #version 450
                const uint kTileSize = 4;
                const uint kInstances = 11;

                layout(local_size_x = kTileSize, local_size_y = kTileSize, local_size_z = 1) in;
                layout(std140, set = 0, binding = 0) uniform uBuffer {
                    float value0;
                    float value1;
                };
                layout(std140, set = 0, binding = 1) buffer sBuffer {
                    float value1;
                } sBuffer2;
                layout(std140, set = 0, binding = 2) readonly buffer rBuffer {
                    readonly float value3;
                } rBuffer2;
                void main() {
                })");

        wgpu::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &bindGroupLayout);

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.layout = pipelineLayout;
        csDesc.computeStage.module = csModule;
        csDesc.computeStage.entryPoint = "main";

        return device.CreateComputePipeline(&csDesc);
    }

    void TestRenderPassBindGroup(wgpu::BindGroup bindGroup, bool expectation) {
        wgpu::RenderPipeline renderPipeline = CreateRenderPipeline(mBindGroupLayout);
        DummyRenderPass renderPass(device);

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);
        renderPassEncoder.SetPipeline(renderPipeline);
        renderPassEncoder.SetBindGroup(0, bindGroup);
        renderPassEncoder.Draw(3);
        renderPassEncoder.EndPass();
        if (!expectation) {
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        } else {
            commandEncoder.Finish();
        }
    }

    void TestComputePassBindGroup(wgpu::BindGroup bindGroup, bool expectation) {
        wgpu::ComputePipeline computePipeline = CreateComputePipeline(mBindGroupLayout);

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipeline);
        computePassEncoder.SetBindGroup(0, bindGroup);
        computePassEncoder.Dispatch(1);
        computePassEncoder.EndPass();
        if (!expectation) {
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        } else {
            commandEncoder.Finish();
        }
    }

    void ForEachSmallerSize(const std::vector<uint64_t>& originalSizes,
                            const std::function<void(const std::vector<uint64_t>&)>& func) {
        for (size_t i = 0; i < originalSizes.size(); ++i) {
            std::vector<uint64_t> modifiedSizes = originalSizes;
            ASSERT(originalSizes[i] > 0);
            modifiedSizes[i] -= 1;
            func(modifiedSizes);
        }
    }
};

// Normal binding should work
TEST_F(MinimumBufferSizeValidationTest, Basic) {
    // First buffer is 8 because vertex stage requires 8 and fragment stage requires 4
    wgpu::Buffer uniformBuffer = CreateBuffer(8, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(4, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(4, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(
        device, mBindGroupLayout,
        {{0, uniformBuffer, 0, 8}, {1, storageBuffer, 0, 4}, {2, readonlyStorageBuffer, 0, 4}});

    TestRenderPassBindGroup(bindGroup, true);
    TestComputePassBindGroup(bindGroup, true);
}

// Render pass min size = max(fragment, vertex) requirements
TEST_F(MinimumBufferSizeValidationTest, RenderPassConsidersBothStages) {
    wgpu::Buffer uniformBuffer = CreateBuffer(8, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(4, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(4, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(
        device, mBindGroupLayout,
        {{0, uniformBuffer, 0, 7}, {1, storageBuffer, 0, 4}, {2, readonlyStorageBuffer, 0, 4}});

    // Infer pass requires 8 bytes for |uniformBuffer|, since 7 fails and 8 passes in the basic test
    TestRenderPassBindGroup(bindGroup, false);
    TestComputePassBindGroup(bindGroup, false);
}

// Buffer too small compared to layout requirements
TEST_F(MinimumBufferSizeValidationTest, BufferTooSmall) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer, false, false,
                  wgpu::TextureViewDimension::Undefined, wgpu::TextureComponentType::Float,
                  wgpu::TextureFormat::Undefined, 8}});
    wgpu::Buffer uniformBuffer = CreateBuffer(4, wgpu::BufferUsage::Uniform);

    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, mBindGroupLayout, {{0, uniformBuffer, 0, 4}}));
}

// Check two layouts with different minimum size are unequal
TEST_F(MinimumBufferSizeValidationTest, LayoutEquality) {
    auto makeLayout = [&](uint64_t size) {
        return utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer, false, false,
                      wgpu::TextureViewDimension::Undefined, wgpu::TextureComponentType::Float,
                      wgpu::TextureFormat::Undefined, size}});
    };

    EXPECT_EQ(makeLayout(0).Get(), makeLayout(0).Get());
    EXPECT_NE(makeLayout(0).Get(), makeLayout(4).Get());
}

// Buffers checked at draw/dispatch, and one is too small
TEST_F(MinimumBufferSizeValidationTest, ZeroMinSizeAndTooSmallBuffer) {
    wgpu::Buffer uniformBuffer = CreateBuffer(8, wgpu::BufferUsage::Uniform);
    // 2 instead of 4 bytes
    wgpu::Buffer storageBuffer = CreateBuffer(2, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(4, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(
        device, mBindGroupLayout,
        {{0, uniformBuffer, 0, 8}, {1, storageBuffer, 0, 2}, {2, readonlyStorageBuffer, 0, 4}});

    TestRenderPassBindGroup(bindGroup, false);
    TestComputePassBindGroup(bindGroup, false);
}

// If pipeline and bind group layouts don't match, fail
TEST_F(MinimumBufferSizeValidationTest, MismatchedPipelineAndGroupLayout) {
    // The difference in layouts will be the minimum buffer size at the end (4 instead of 0)
    wgpu::BindGroupLayout differentLayout = CreateBasicLayout({0, 0, 4});

    wgpu::Buffer uniformBuffer = CreateBuffer(8, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(4, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(4, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(
        device, differentLayout,
        {{0, uniformBuffer, 0, 8}, {1, storageBuffer, 0, 4}, {2, readonlyStorageBuffer, 0, 4}});

    TestRenderPassBindGroup(bindGroup, false);
    TestComputePassBindGroup(bindGroup, false);
}

// Can't create pipeline with smaller buffers than shader requirements
TEST_F(MinimumBufferSizeValidationTest, PipelineSizesTooSmall) {
    // Evaluates a given size and expectation
    auto validateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
        wgpu::BindGroupLayout layout = CreateBasicLayout(sizes);
        if (expectation) {
            CreateRenderPipeline(layout);
            CreateComputePipeline(layout);
        } else {
            ASSERT_DEVICE_ERROR(CreateRenderPipeline(layout));
            ASSERT_DEVICE_ERROR(CreateComputePipeline(layout));
        }
    };

    // Validate original sizes work
    std::vector<uint64_t> originalSizes = {8, 4, 4};
    validateSizes(originalSizes, true);

    // Make sure (every size - 1) produces an error
    ForEachSmallerSize(originalSizes,
                       [&](const std::vector<uint64_t>& sizes) { validateSizes(sizes, false); });
}

// Minimum sizes are extracted from shader for harder cases (such as runtime arrays)
TEST_F(MinimumBufferSizeValidationTest, MinimumSizesInferred) {
    wgpu::ShaderModule csModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
            #version 450
            const uint kTileSize = 4;
            const uint kInstances = 11;

            layout(local_size_x = kTileSize, local_size_y = kTileSize, local_size_z = 1) in;
            layout(std140, set = 0, binding = 0) uniform uBuffer {
                float value0;
            };
            layout(std140, set = 0, binding = 1) uniform uBuffer2 {
                float value1[];
            };
            layout(std140, set = 0, binding = 2) buffer sBuffer {
                float value2;
            } sBufferB;
            layout(std140, set = 0, binding = 3) buffer sBuffer2 {
                float value3[];
            } sBuffer2B;
            void main() {
            })");

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.layout = nullptr;
    csDesc.computeStage.module = csModule;
    csDesc.computeStage.entryPoint = "main";

    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);
    wgpu::BindGroupLayout layout = pipeline.GetBindGroupLayout(0);

    // To validate size:
    // Check invalid with bind group with one less
    // Check valid with bind group with correct size
    wgpu::Buffer uniformBuffer = CreateBuffer(16, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(16, wgpu::BufferUsage::Storage);

    // Helper to construct a bind group with given binding sizes
    auto makeGroup = [&](const std::vector<uint64_t>& sizes) {
        return utils::MakeBindGroup(device, layout,
                                    {{0, uniformBuffer, 0, sizes[0]},
                                     {1, uniformBuffer, 0, sizes[1]},
                                     {2, storageBuffer, 0, sizes[2]},
                                     {3, storageBuffer, 0, sizes[3]}});
    };

    auto validateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
        if (expectation) {
            makeGroup(sizes);
        } else {
            ASSERT_DEVICE_ERROR(makeGroup(sizes));
        }
    };

    // Validate original sizes work
    // Runtime arrays require minimum size of the stride, in this case 16
    std::vector<uint64_t> originalSizes = {4, 16, 4, 16};
    validateSizes(originalSizes, true);

    // Make sure (every size - 1) produces an error
    ForEachSmallerSize(originalSizes,
                       [&](const std::vector<uint64_t>& sizes) { validateSizes(sizes, false); });
}
