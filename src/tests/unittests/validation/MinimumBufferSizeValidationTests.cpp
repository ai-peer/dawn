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
    // Helper for describing bindings throughout the tests
    struct BindingDescriptor {
        uint32_t set;
        uint32_t binding;
        std::string text;
        uint64_t size;
        wgpu::BindingType type = wgpu::BindingType::StorageBuffer;
        wgpu::ShaderStage visibility = wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment;
    };

    // Runs |func| with a modified version of |originalSizes| as an argument, adding |offset| to
    // each element one at a time This is useful to verify some behavior happens if any element is
    // offset from original
    template <typename F>
    void WithEachSizeOffsetBy(int64_t offset, const std::vector<uint64_t>& originalSizes, F func) {
        std::vector<uint64_t> modifiedSizes = originalSizes;
        for (size_t i = 0; i < originalSizes.size(); ++i) {
            if (offset < 0) {
                ASSERT(originalSizes[i] >= static_cast<uint64_t>(-offset));
            }
            // Run the function with an element offset, and restore element afterwards
            modifiedSizes[i] += offset;
            func(modifiedSizes);
            modifiedSizes[i] -= offset;
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

    // Convert binding type to a glsl string
    std::string BindingTypeToStr(wgpu::BindingType type) {
        switch (type) {
            case wgpu::BindingType::UniformBuffer:
                return "uniform";
            case wgpu::BindingType::StorageBuffer:
                return "buffer";
            case wgpu::BindingType::ReadonlyStorageBuffer:
                return "readonly buffer";
            default:
                UNREACHABLE();
        }
    }

    // Creates a bind group with given bindings for shader text
    std::string GenerateBindingString(const std::string& layout,
                                      const std::vector<BindingDescriptor>& bindings) {
        std::ostringstream ostream;
        size_t ctr = 0;
        for (const BindingDescriptor& b : bindings) {
            ostream << "layout(" << layout << ", set = " << b.set << ", binding = " << b.binding
                    << ") " << BindingTypeToStr(b.type) << " b" << ctr++ << "{\n"
                    << b.text << ";\n};\n";
        }
        return ostream.str();
    }

    // Used for adding custom types available throughout the tests
    static const std::string kStructs = "struct ThreeFloats{float f1; float f2; float f3;};\n";

    // Creates a compute shader with given bindings
    std::string CreateComputeShaderWithBindings(const std::string& layoutType,
                                                const std::vector<BindingDescriptor>& bindings) {
        return R"(
            #version 450
            layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
            )" +
               kStructs + GenerateBindingString(layoutType, bindings) + "void main() {}";
    }

    // Creates a vertex shader with given bindings
    std::string CreateVertexShaderWithBindings(const std::string& layoutType,
                                               const std::vector<BindingDescriptor>& bindings) {
        return "#version 450\n" + kStructs + GenerateBindingString(layoutType, bindings) +
               "void main() {}";
    }

    // Creates a fragment shader with given bindings
    std::string CreateFragmentShaderWithBindings(const std::string& layoutType,
                                                 const std::vector<BindingDescriptor>& bindings) {
        return R"(
            #version 450
            layout(location = 0) out vec4 fragColor;
            )" +
               kStructs + GenerateBindingString(layoutType, bindings) + "void main() {}";
    }
}  // namespace

class MinBufferSizeTestsBase : public ValidationTest {
  public:
    void SetUp() override {
        ValidationTest::SetUp();
    }

    wgpu::Buffer CreateBuffer(uint64_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = usage;

        return device.CreateBuffer(&bufferDescriptor);
    }

    // Creates compute pipeline given a layout and shader
    wgpu::ComputePipeline CreateComputePipeline(const std::vector<wgpu::BindGroupLayout>& layouts,
                                                const std::string& shader) {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, shader.c_str());

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.layout = nullptr;
        if (!layouts.empty()) {
            wgpu::PipelineLayoutDescriptor descriptor;
            descriptor.bindGroupLayoutCount = layouts.size();
            descriptor.bindGroupLayouts = layouts.data();
            csDesc.layout = device.CreatePipelineLayout(&descriptor);
        }
        csDesc.computeStage.module = csModule;
        csDesc.computeStage.entryPoint = "main";

        return device.CreateComputePipeline(&csDesc);
    }

    // Creates compute pipeline with default layout
    wgpu::ComputePipeline CreateDefaultComputePipeline(const std::string& shader) {
        return CreateComputePipeline({}, shader);
    }

    // Creates render pipeline give na layout and shaders
    wgpu::RenderPipeline CreateRenderPipeline(const std::vector<wgpu::BindGroupLayout>& layouts,
                                              const std::string& vertexShader,
                                              const std::string& fragShader) {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(
            device, utils::SingleShaderStage::Vertex, vertexShader.c_str());

        wgpu::ShaderModule fsModule = utils::CreateShaderModule(
            device, utils::SingleShaderStage::Fragment, fragShader.c_str());

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        pipelineDescriptor.layout = nullptr;
        if (!layouts.empty()) {
            wgpu::PipelineLayoutDescriptor descriptor;
            descriptor.bindGroupLayoutCount = layouts.size();
            descriptor.bindGroupLayouts = layouts.data();
            pipelineDescriptor.layout = device.CreatePipelineLayout(&descriptor);
        }

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    // Creates render pipeline with default layout
    wgpu::RenderPipeline CreateDefaultRenderPipeline(const std::string& vertexShader,
                                                     const std::string& fragShader) {
        return CreateRenderPipeline({}, vertexShader, fragShader);
    }

    // Creates bind group layout with given minimum sizes for each binding
    wgpu::BindGroupLayout CreateBindGroupLayout(const std::vector<BindingDescriptor>& bindings,
                                                const std::vector<uint64_t>& minimumSizes) {
        ASSERT(bindings.size() == minimumSizes.size());
        std::vector<wgpu::BindGroupLayoutEntry> entries;

        for (size_t i = 0; i < bindings.size(); ++i) {
            const BindingDescriptor& b = bindings[i];
            wgpu::BindGroupLayoutEntry e = {};
            e.binding = b.binding;
            e.type = b.type;
            e.visibility = b.visibility;
            e.minimumBufferSize = minimumSizes[i];
            entries.push_back(e);
        }

        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.entryCount = static_cast<uint32_t>(entries.size());
        descriptor.entries = entries.data();
        return device.CreateBindGroupLayout(&descriptor);
    }

    // Extract the first bind group from a compute shader
    wgpu::BindGroupLayout GetBGLFromComputePass(const std::string& shader) {
        wgpu::ComputePipeline pipeline = CreateDefaultComputePipeline(shader);
        return pipeline.GetBindGroupLayout(0);
    }

    // Extract the first bind group from a render pass
    wgpu::BindGroupLayout GetBGLFromRenderPass(const std::string& vertexShader,
                                               const std::string& fragShader) {
        wgpu::RenderPipeline pipeline = CreateDefaultRenderPipeline(vertexShader, fragShader);
        return pipeline.GetBindGroupLayout(0);
    }

    // Create a bind group with given binding sizes for each entry (backed by the same buffer)
    wgpu::BindGroup CreateBindGroup(wgpu::BindGroupLayout layout,
                                    const std::vector<BindingDescriptor>& bindings,
                                    const std::vector<uint64_t>& bindingSizes) {
        ASSERT(bindings.size() == bindingSizes.size());
        wgpu::Buffer buffer =
            CreateBuffer(1024, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage);

        std::vector<wgpu::BindGroupEntry> entries;
        entries.reserve(bindingSizes.size());

        for (uint32_t i = 0; i < bindingSizes.size(); ++i) {
            wgpu::BindGroupEntry entry = {};
            entry.binding = bindings[i].binding;
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

    // Runs a single dispatch with given pipeline and bind group (to test lazy validation during
    // dispatch)
    void TestDispatch(const wgpu::ComputePipeline& computePipeline,
                      const std::vector<wgpu::BindGroup>& bindGroups,
                      bool expectation) {
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipeline);
        for (size_t i = 0; i < bindGroups.size(); ++i) {
            computePassEncoder.SetBindGroup(i, bindGroups[i]);
        }
        computePassEncoder.Dispatch(1);
        computePassEncoder.EndPass();
        if (!expectation) {
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        } else {
            commandEncoder.Finish();
        }
    }

    // Runs a single draw with given pipeline and bind group (to test lazy validation during draw)
    void TestDraw(const wgpu::RenderPipeline& renderPipeline,
                  const std::vector<wgpu::BindGroup>& bindGroups,
                  bool expectation) {
        DummyRenderPass renderPass(device);

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);
        renderPassEncoder.SetPipeline(renderPipeline);
        for (size_t i = 0; i < bindGroups.size(); ++i) {
            renderPassEncoder.SetBindGroup(i, bindGroups[i]);
        }
        renderPassEncoder.Draw(3);
        renderPassEncoder.EndPass();
        if (!expectation) {
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        } else {
            commandEncoder.Finish();
        }
    }
};

// The check between BGL and pipeline at pipeline creation time
class MinBufferSizePipelineCreationTests : public MinBufferSizeTestsBase {};

// Pipeline can be created if minimum buffer size in layout is specified as 0
TEST_F(MinBufferSizePipelineCreationTests, ZeroMinBufferSize) {
    std::vector<BindingDescriptor> bindings = {{0, 0, "float a; float b", 8}, {0, 1, "float c", 4}};

    std::string computeShader = CreateComputeShaderWithBindings("std140", bindings);
    std::string vertexShader = CreateVertexShaderWithBindings("std140", {});
    std::string fragShader = CreateFragmentShaderWithBindings("std140", bindings);

    wgpu::BindGroupLayout layout = CreateBindGroupLayout(bindings, {0, 0});
    CreateRenderPipeline({layout}, vertexShader, fragShader);
    CreateComputePipeline({layout}, computeShader);
}

// Fail if layout given has non-zero minimum sizes smaller than shader requirements
TEST_F(MinBufferSizePipelineCreationTests, LayoutSizesTooSmall) {
    std::vector<BindingDescriptor> bindings = {{0, 0, "float a; float b", 8}, {0, 1, "float c", 4}};

    std::string computeShader = CreateComputeShaderWithBindings("std140", bindings);
    std::string vertexShader = CreateVertexShaderWithBindings("std140", {});
    std::string fragShader = CreateFragmentShaderWithBindings("std140", bindings);

    auto ValidateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
        wgpu::BindGroupLayout layout = CreateBindGroupLayout(bindings, sizes);
        if (expectation) {
            CreateRenderPipeline({layout}, vertexShader, fragShader);
            CreateComputePipeline({layout}, computeShader);
        } else {
            ASSERT_DEVICE_ERROR(CreateRenderPipeline({layout}, vertexShader, fragShader));
            ASSERT_DEVICE_ERROR(CreateComputePipeline({layout}, computeShader));
        }
    };

    CheckSizeBounds({8, 4}, ValidateSizes);
}

// The check between the BGL and the bindings at bindgroup creation time
class MinBufferSizeBindGroupCreationTests : public MinBufferSizeTestsBase {};

// Fail if a binding is smaller than minimum buffer size
TEST_F(MinBufferSizeBindGroupCreationTests, BindingTooSmall) {
    std::vector<BindingDescriptor> bindings = {{0, 0, "float a; float b", 8}, {0, 1, "float c", 4}};
    wgpu::BindGroupLayout layout = CreateBindGroupLayout(bindings, {8, 4});

    auto ValidateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
        if (expectation) {
            CreateBindGroup(layout, bindings, sizes);
        } else {
            ASSERT_DEVICE_ERROR(CreateBindGroup(layout, bindings, sizes));
        }
    };

    CheckSizeBounds({8, 4}, ValidateSizes);
}

// Check two layouts with different minimum size are unequal
TEST_F(MinBufferSizeBindGroupCreationTests, LayoutEquality) {
    auto MakeLayout = [&](uint64_t size) {
        return utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer, false, false,
                      wgpu::TextureViewDimension::Undefined, wgpu::TextureComponentType::Float,
                      wgpu::TextureFormat::Undefined, size}});
    };

    EXPECT_EQ(MakeLayout(0).Get(), MakeLayout(0).Get());
    EXPECT_NE(MakeLayout(0).Get(), MakeLayout(4).Get());
}

// The check between the bindgroup binding sizes and the required pipeline sizes at draw time
class MinBufferSizeDrawTimeValidationTests : public MinBufferSizeTestsBase {};

// Fail if binding sizes are too small at draw time
TEST_F(MinBufferSizeDrawTimeValidationTests, ZeroMinSizeAndTooSmallBinding) {
    std::vector<BindingDescriptor> bindings = {{0, 0, "float a; float b", 8}, {0, 1, "float c", 4}};

    std::string computeShader = CreateComputeShaderWithBindings("std140", bindings);
    std::string vertexShader = CreateVertexShaderWithBindings("std140", {});
    std::string fragShader = CreateFragmentShaderWithBindings("std140", bindings);

    wgpu::BindGroupLayout layout = CreateBindGroupLayout(bindings, {0, 0});

    wgpu::ComputePipeline computePipeline = CreateComputePipeline({layout}, computeShader);
    wgpu::RenderPipeline renderPipeline = CreateRenderPipeline({layout}, vertexShader, fragShader);

    auto ValidateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
        wgpu::BindGroup bindGroup = CreateBindGroup(layout, bindings, sizes);
        TestDispatch(computePipeline, {bindGroup}, expectation);
        TestDraw(renderPipeline, {bindGroup}, expectation);
    };

    CheckSizeBounds({8, 4}, ValidateSizes);
}

// Draw time validation works for non-contiguous bindings
TEST_F(MinBufferSizeDrawTimeValidationTests, UnorderedBindings) {
    std::vector<BindingDescriptor> bindings = {{0, 2, "float a; float b", 8},
                                               {0, 0, "float c", 4},
                                               {0, 4, "float d; float e; float f", 12}};

    std::string computeShader = CreateComputeShaderWithBindings("std140", bindings);
    std::string vertexShader = CreateVertexShaderWithBindings("std140", {});
    std::string fragShader = CreateFragmentShaderWithBindings("std140", bindings);

    wgpu::BindGroupLayout layout = CreateBindGroupLayout(bindings, {0, 0, 0});

    wgpu::ComputePipeline computePipeline = CreateComputePipeline({layout}, computeShader);
    wgpu::RenderPipeline renderPipeline = CreateRenderPipeline({layout}, vertexShader, fragShader);

    auto ValidateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
        wgpu::BindGroup bindGroup = CreateBindGroup(layout, bindings, sizes);
        TestDispatch(computePipeline, {bindGroup}, expectation);
        TestDraw(renderPipeline, {bindGroup}, expectation);
    };

    CheckSizeBounds({8, 4, 12}, ValidateSizes);
}

// Draw time validation works for multiple bind groups
TEST_F(MinBufferSizeDrawTimeValidationTests, MultipleGroups) {
    std::vector<BindingDescriptor> bg0Bindings = {{0, 0, "float a; float b", 8},
                                                  {0, 1, "float c", 4}};
    std::vector<BindingDescriptor> bg1Bindings = {{1, 0, "float d; float e; float f", 12},
                                                  {1, 1, "mat2 g", 32}};
    std::vector<BindingDescriptor> bindings;
    for (const BindingDescriptor& b : bg0Bindings) {
        bindings.push_back(b);
    }
    for (const BindingDescriptor& b : bg1Bindings) {
        bindings.push_back(b);
    }

    std::string computeShader = CreateComputeShaderWithBindings("std140", bindings);
    std::string vertexShader = CreateVertexShaderWithBindings("std140", {});
    std::string fragShader = CreateFragmentShaderWithBindings("std140", bindings);

    wgpu::BindGroupLayout layout0 = CreateBindGroupLayout(bg0Bindings, {0, 0});
    wgpu::BindGroupLayout layout1 = CreateBindGroupLayout(bg1Bindings, {0, 0});

    wgpu::ComputePipeline computePipeline =
        CreateComputePipeline({layout0, layout1}, computeShader);
    wgpu::RenderPipeline renderPipeline =
        CreateRenderPipeline({layout0, layout1}, vertexShader, fragShader);

    auto ValidateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
        wgpu::BindGroup bindGroup0 = CreateBindGroup(layout0, bg0Bindings, {sizes[0], sizes[1]});
        wgpu::BindGroup bindGroup1 = CreateBindGroup(layout0, bg0Bindings, {sizes[2], sizes[3]});
        TestDispatch(computePipeline, {bindGroup0, bindGroup1}, expectation);
        TestDraw(renderPipeline, {bindGroup0, bindGroup1}, expectation);
    };

    CheckSizeBounds({8, 4, 12, 32}, ValidateSizes);
}

// The correctness of minimum buffer size for the defaulted layout for a pipeline
class MinBufferSizeDefaultLayoutTests : public MinBufferSizeTestsBase {
  public:
    // Checks BGL |layout| has minimum buffer sizes equal to sizes in |bindings|
    void CheckLayoutBindingSizeValidation(const wgpu::BindGroupLayout& layout,
                                          const std::vector<BindingDescriptor>& bindings) {
        auto ValidateSizes = [&](const std::vector<uint64_t>& sizes, bool expectation) {
            if (expectation) {
                CreateBindGroup(layout, bindings, sizes);
            } else {
                ASSERT_DEVICE_ERROR(CreateBindGroup(layout, bindings, sizes));
            }
        };

        std::vector<uint64_t> correctSizes;
        correctSizes.reserve(bindings.size());
        for (const BindingDescriptor& b : bindings) {
            correctSizes.push_back(b.size);
        }

        CheckSizeBounds(correctSizes, ValidateSizes);
    }

    // Constructs shaders with given layout type and bindings, checking defaulted sizes match sizes
    // in |bindings|
    void CheckShaderBindingSizeReflection(const std::string& layoutType,
                                          const std::vector<BindingDescriptor>& bindings) {
        std::string computeShader = CreateComputeShaderWithBindings(layoutType, bindings);
        std::string vertexShader = CreateVertexShaderWithBindings(layoutType, {});
        std::string fragShader = CreateFragmentShaderWithBindings(layoutType, bindings);

        wgpu::BindGroupLayout computeLayout = GetBGLFromComputePass(computeShader);
        wgpu::BindGroupLayout renderLayout = GetBGLFromRenderPass(vertexShader, fragShader);

        CheckLayoutBindingSizeValidation(computeLayout, bindings);
        CheckLayoutBindingSizeValidation(renderLayout, bindings);
    }
};

// Various bindings in std140 have correct minimum size reflection
TEST_F(MinBufferSizeDefaultLayoutTests, std140Inferred) {
    CheckShaderBindingSizeReflection("std140", {{0, 0, "float a", 4},
                                                {0, 1, "float b[]", 16},
                                                {0, 2, "mat2 c", 32},
                                                {0, 3, "int d; float e[]", 32},
                                                {0, 4, "ThreeFloats f", 12},
                                                {0, 5, "ThreeFloats g[]", 16}});
}

// Various bindings in std430 have correct minimum size reflection
TEST_F(MinBufferSizeDefaultLayoutTests, std430Inferred) {
    CheckShaderBindingSizeReflection("std430", {{0, 0, "float a", 4},
                                                {0, 1, "float b[]", 4},
                                                {0, 2, "mat2 c", 16},
                                                {0, 3, "int d; float e[]", 8},
                                                {0, 4, "ThreeFloats f", 12},
                                                {0, 5, "ThreeFloats g[]", 12}});
}

// Sizes are inferred for all binding types with std140 layout
TEST_F(MinBufferSizeDefaultLayoutTests, std140BindingTypes) {
    CheckShaderBindingSizeReflection(
        "std140", {{0, 0, "int d; float e[]", 32, wgpu::BindingType::UniformBuffer},
                   {0, 1, "ThreeFloats f", 12, wgpu::BindingType::StorageBuffer},
                   {0, 2, "ThreeFloats g[]", 16, wgpu::BindingType::ReadonlyStorageBuffer}});
}

// Sizes are inferred for all binding types with std430 layout
TEST_F(MinBufferSizeDefaultLayoutTests, std430BindingTypes) {
    CheckShaderBindingSizeReflection(
        "std430", {{0, 0, "float a", 4, wgpu::BindingType::StorageBuffer},
                   {0, 1, "ThreeFloats b[]", 12, wgpu::BindingType::ReadonlyStorageBuffer}});
}

// Minimum size should be the max requirement of both vertex and fragment stages
TEST_F(MinBufferSizeDefaultLayoutTests, RenderPassConsidersBothStages) {
    std::string vertexShader = CreateVertexShaderWithBindings(
        "std140", {{0, 0, "float a", 4, wgpu::BindingType::UniformBuffer},
                   {0, 1, "float b[]", 16, wgpu::BindingType::UniformBuffer}});
    std::string fragShader = CreateFragmentShaderWithBindings(
        "std140", {{0, 0, "float a; float b", 8, wgpu::BindingType::UniformBuffer},
                   {0, 1, "float c; float d", 8, wgpu::BindingType::UniformBuffer}});

    wgpu::BindGroupLayout renderLayout = GetBGLFromRenderPass(vertexShader, fragShader);

    CheckLayoutBindingSizeValidation(renderLayout, {{0, 0, "", 8}, {0, 1, "", 16}});
}