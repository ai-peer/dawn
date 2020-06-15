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

namespace {
    struct BindingSizeExpectation {
        std::string text;
        uint64_t size;
    };

    // Runs |func| with a modified version of |originalSizes| as an argument, adding |offset| to
    // each element one at a time This is useful to verify some behavior happens if any element is
    // offset from original
    template <typename F>
    void WithEachSizeOffsetBy(int64_t offset, const std::vector<uint64_t>& originalSizes, F func) {
        for (size_t i = 0; i < originalSizes.size(); ++i) {
            std::vector<uint64_t> modifiedSizes = originalSizes;
            if (offset < 0) {
                ASSERT(originalSizes[i] >= (uint64_t)-offset);
            }
            modifiedSizes[i] += offset;
            func(modifiedSizes);
        }
    }

    // Runs |func| with |correctSizes|, and an expectation of success and failure
    template <typename F>
    void CheckSizeBounds(const std::vector<uint64_t>& correctSizes, F func) {
        // To validate size:
        // Check invalid with bind group with one less
        // Check valid with bind group with correct size

        // Make sure (every size - 1) produces an error
        WithEachSizeOffsetBy(-1, correctSizes,
                             [&](const std::vector<uint64_t>& sizes) { func(sizes, false); });

        // Make sure correct sizes work
        func(correctSizes, true);

        // Make sure (every size + 1) works
        WithEachSizeOffsetBy(1, correctSizes,
                             [&](const std::vector<uint64_t>& sizes) { func(sizes, true); });
    }

    // Creates a bind group with given bindings for shader text
    std::string GenerateBindingString(const std::string& layout,
                                      const std::vector<BindingSizeExpectation>& expectations) {
        std::ostringstream ostream;
        for (size_t i = 0; i < expectations.size(); ++i) {
            ostream << "layout(" << layout << ", set = 0, binding = " << i << ") buffer b" << i
                    << "{\n"
                    << expectations[i].text << ";\n};\n";
        }
        return ostream.str();
    }
}  // namespace

class MinimumBufferSizeValidationTest : public ValidationTest {
  public:
    void SetUp() override {
        ValidationTest::SetUp();
        mBindGroupLayout = CreateBasicLayout({0, 0, 0});
    }

    wgpu::BindGroupLayout CreateBasicLayout(const std::vector<uint64_t>& minimumSizes) {
        wgpu::BindGroupLayoutEntry b0 = {};
        b0.binding = 0;
        b0.visibility =
            wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Vertex;
        b0.type = wgpu::BindingType::UniformBuffer;
        b0.minimumBufferSize = minimumSizes[0];

        wgpu::BindGroupLayoutEntry b1 = {};
        b1.binding = 1;
        b1.visibility = wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment;
        b1.type = wgpu::BindingType::StorageBuffer;
        b1.minimumBufferSize = minimumSizes[1];

        wgpu::BindGroupLayoutEntry b2 = {};
        b2.binding = 2;
        b2.visibility = wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment;
        b2.type = wgpu::BindingType::ReadonlyStorageBuffer;
        b2.minimumBufferSize = minimumSizes[2];

        return utils::MakeBindGroupLayout(device, {b0, b1, b2});
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

                layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
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

    // Extract the first bind group from a compute shader
    wgpu::BindGroupLayout GetBGLFromComputePipeline(const std::string& shader) {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, shader.c_str());

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.layout = nullptr;
        csDesc.computeStage.module = csModule;
        csDesc.computeStage.entryPoint = "main";

        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

        return pipeline.GetBindGroupLayout(0);
    }

    // Extract the first bind group from a render pass
    wgpu::BindGroupLayout GetBGLFromRenderPipeline(const std::string& vertexShader,
                                                   const std::string& fragShader) {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(
            device, utils::SingleShaderStage::Vertex, vertexShader.c_str());

        wgpu::ShaderModule fsModule = utils::CreateShaderModule(
            device, utils::SingleShaderStage::Fragment, fragShader.c_str());

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        pipelineDescriptor.layout = nullptr;

        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDescriptor);

        return pipeline.GetBindGroupLayout(0);
    }

    // Create a bind group with given sizes for each entry
    wgpu::BindGroup GenerateBindGroup(wgpu::BindGroupLayout layout,
                                      const std::vector<uint64_t>& bindingSizes) {
        wgpu::Buffer buffer =
            CreateBuffer(1024, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage);

        std::vector<wgpu::BindGroupEntry> entries;
        entries.reserve(bindingSizes.size());

        for (uint32_t i = 0; i < bindingSizes.size(); ++i) {
            wgpu::BindGroupEntry entry = {};
            entry.binding = i;
            entry.buffer = buffer;
            ASSERT(bindingSizes[i] < 1024);
            entry.size = bindingSizes[i];
            entries.push_back(entry);
        }

        wgpu::BindGroupDescriptor descriptor;
        descriptor.layout = layout;
        descriptor.entryCount = entries.size();
        descriptor.entries = entries.data();

        return device.CreateBindGroup(&descriptor);
    }

    // Checks |layout| contains minimum buffer sizes in |expectations|
    void ValidateBindingSizesWithLayout(wgpu::BindGroupLayout layout,
                                        const std::vector<BindingSizeExpectation>& expectations) {
        auto ValidateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
            if (expectation) {
                GenerateBindGroup(layout, sizes);
            } else {
                ASSERT_DEVICE_ERROR(GenerateBindGroup(layout, sizes));
            }
        };

        // Validate correct sizes work
        std::vector<uint64_t> correctSizes;
        for (const BindingSizeExpectation& e : expectations) {
            correctSizes.push_back(e.size);
        }

        CheckSizeBounds(correctSizes, ValidateSizes);
    }

    // Checks we infer sizes in |expectations| using |layoutType| for packing rules
    void ValidateBindingSizes(const std::string& layoutType,
                              const std::vector<BindingSizeExpectation>& expectations) {
        std::string structs = "struct ThreeFloats{float f1; float f2; float f3;};";

        std::string computeShader = R"(
            #version 450
            layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
            )" + structs + GenerateBindingString(layoutType, expectations) +
                                    "void main() {}";

        std::string vertexShader = "#version 450\nvoid main() {}";
        std::string fragShader = R"(
            #version 450
            layout(location = 0) out vec4 fragColor;
            )" + structs + GenerateBindingString(layoutType, expectations) +
                                 "void main() {}";

        wgpu::BindGroupLayout layoutCompute = GetBGLFromComputePipeline(computeShader);
        wgpu::BindGroupLayout layoutRender = GetBGLFromRenderPipeline(vertexShader, fragShader);

        ValidateBindingSizesWithLayout(layoutCompute, expectations);
        ValidateBindingSizesWithLayout(layoutRender, expectations);
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
    auto MakeLayout = [&](uint64_t size) {
        return utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer, false, false,
                      wgpu::TextureViewDimension::Undefined, wgpu::TextureComponentType::Float,
                      wgpu::TextureFormat::Undefined, size}});
    };

    EXPECT_EQ(MakeLayout(0).Get(), MakeLayout(0).Get());
    EXPECT_NE(MakeLayout(0).Get(), MakeLayout(4).Get());
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
    auto ValidateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
        wgpu::BindGroupLayout layout = CreateBasicLayout(sizes);
        if (expectation) {
            CreateRenderPipeline(layout);
            CreateComputePipeline(layout);
        } else {
            ASSERT_DEVICE_ERROR(CreateRenderPipeline(layout));
            ASSERT_DEVICE_ERROR(CreateComputePipeline(layout));
        }
    };

    CheckSizeBounds({8, 4, 4}, ValidateSizes);
}

TEST_F(MinimumBufferSizeValidationTest, std140Inferred) {
    ValidateBindingSizes("std140", {{"float a", 4},
                                    {"float b[]", 16},
                                    {"mat2 c", 32},
                                    {"int d; float e[]", 32},
                                    {"ThreeFloats f", 12},
                                    {"ThreeFloats g[]", 16}});
}

TEST_F(MinimumBufferSizeValidationTest, std430Inferred) {
    ValidateBindingSizes("std430", {{"float a", 4},
                                    {"float b[]", 4},
                                    {"mat2 c", 16},
                                    {"int d; float e[]", 8},
                                    {"ThreeFloats f", 12},
                                    {"ThreeFloats g[]", 12}});
}