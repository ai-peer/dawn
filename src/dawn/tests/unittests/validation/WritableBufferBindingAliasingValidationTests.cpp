// Copyright 2022 The Dawn Authors
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

#include <string>
#include <vector>

#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/common/Numeric.h"
#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {
// Helper for describing bindings throughout the tests

// struct BindingDescriptor {
//     uint32_t group;
//     uint32_t binding;
//     std::string decl;
//     std::string ref_type;
//     std::string ref_mem;
//     uint64_t size;
//     wgpu::BufferBindingType type = wgpu::BufferBindingType::Storage;
//     wgpu::ShaderStage visibility = wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment;
// };
struct BindingDescriptor {
    // uint32_t group;
    utils::BindingInitializationHelper binding;  // buffer binding
    // std::string decl;
    wgpu::BufferBindingType type = wgpu::BufferBindingType::Storage;

    bool hasDynamicOffset = false;
    uint32_t dynamicOffset = 0;

    wgpu::ShaderStage visibility = wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment;
};

// // group, binding
// using TestSet = std::vector<std::vector<BindingDescriptor>>;

using BindingDescriptorGroups = std::vector<std::vector<BindingDescriptor>>;
struct TestSet {
    bool valid;
    BindingDescriptorGroups bindingEntries;
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
    // Check invalid with bind group with 4 less (the effective storage / read-only storage buffer
    // size must be a multiple of 4).
    // Check valid with bind group with correct size

    // Make sure (every size - 4) produces an error
    WithEachSizeOffsetBy(-4, correctSizes,
                         [&](const std::vector<uint64_t>& sizes) { func(sizes, false); });

    // Make sure correct sizes work
    func(correctSizes, true);

    // Make sure (every size + 4) works
    WithEachSizeOffsetBy(4, correctSizes,
                         [&](const std::vector<uint64_t>& sizes) { func(sizes, true); });
}

// template <typename F>
// void CheckBindGroupsAlias(F func) {
//     // Make sure correct sizes work
//     func(correctSizes, true);
// }

// Creates a bind group with given bindings for shader text
// std::string GenerateBindingString(const std::vector<BindingDescriptor>& bindings) {
std::string GenerateBindingString(const BindingDescriptorGroups& bindingsGroups) {
    std::ostringstream ostream;
    size_t index = 0;
    size_t groupIndex = 0;
    for (const auto& bindings : bindingsGroups) {
        for (const BindingDescriptor& b : bindings) {
            // ostream << "struct S" << index << " { " << b.decl << "}\n";
            ostream << "struct S" << index << " { "
                    << "buffer : array<f32>"
                    // << "buffer : array<f32, 1024>"
                    // << "buffer : array<f32, " << b.binding.size << ">"
                    // << "@align(16) buffer : array<f32, " << b.binding.size << ">"
                    << "}\n";
            ostream << "@group(" << groupIndex << ") @binding(" << b.binding.binding << ") ";
            switch (b.type) {
                case wgpu::BufferBindingType::Uniform:
                    ostream << "var<uniform> b" << index << " : S" << index << ";\n";
                    break;
                case wgpu::BufferBindingType::Storage:
                    ostream << "var<storage, read_write> b" << index << " : S" << index << ";\n";
                    break;
                case wgpu::BufferBindingType::ReadOnlyStorage:
                    ostream << "var<storage, read> b" << index << " : S" << index << ";\n";
                    break;
                default:
                    UNREACHABLE();
            }
            index++;
        }
        groupIndex++;
    }
    return ostream.str();
}

// std::string GenerateReferenceString(const std::vector<BindingDescriptor>& bindings,
//                                     wgpu::ShaderStage stage) {
std::string GenerateReferenceString(const BindingDescriptorGroups& bindingsGroups,
                                    wgpu::ShaderStage stage) {
    std::ostringstream ostream;
    size_t index = 0;
    for (const auto& bindings : bindingsGroups) {
        for (const BindingDescriptor& b : bindings) {
            if (b.visibility & stage) {
                // if (!b.ref_type.empty() && !b.ref_mem.empty()) {
                //     ostream << "var r" << index << " : " << b.ref_type << " = b" << index << "."
                //             << b.ref_mem << ";\n";
                // }
                ostream << "_ = b" << index
                        << "."
                        // << b.ref_mem << ";\n";
                        << "buffer[0]"
                        << ";\n";
            }
            index++;
        }
    }
    return ostream.str();
}

// Used for adding custom types available throughout the tests
// NOLINTNEXTLINE(runtime/string)
static const std::string kStructs = "struct ThreeFloats {f1 : f32, f2 : f32, f3 : f32,}\n";

// Creates a compute shader with given bindings
// std::string CreateComputeShaderWithBindings(const std::vector<BindingDescriptor>& bindings) {
std::string CreateComputeShaderWithBindings(const BindingDescriptorGroups& bindingsGroups) {
    return kStructs + GenerateBindingString(bindingsGroups) +
           "@compute @workgroup_size(1,1,1) fn main() {\n" +
           GenerateReferenceString(bindingsGroups, wgpu::ShaderStage::Compute) + "}";
}

// Creates a vertex shader with given bindings
std::string CreateVertexShaderWithBindings(const BindingDescriptorGroups& bindingsGroups) {
    return kStructs + GenerateBindingString(bindingsGroups) +
           "@vertex fn main() -> @builtin(position) vec4<f32> {\n" +
           GenerateReferenceString(bindingsGroups, wgpu::ShaderStage::Vertex) +
           "\n   return vec4<f32>(); " + "}";
}

// Creates a fragment shader with given bindings
std::string CreateFragmentShaderWithBindings(const BindingDescriptorGroups& bindingsGroups) {
    return kStructs + GenerateBindingString(bindingsGroups) + "@fragment fn main() {\n" +
           GenerateReferenceString(bindingsGroups, wgpu::ShaderStage::Fragment) + "}";
}

// // Concatenates vectors containing BindingDescriptor
// std::vector<BindingDescriptor> CombineBindings(
//     std::initializer_list<std::vector<BindingDescriptor>> bindings) {
//     std::vector<BindingDescriptor> result;
//     for (const std::vector<BindingDescriptor>& b : bindings) {
//         result.insert(result.end(), b.begin(), b.end());
//     }
//     return result;
// }

// bool IsBufferBindingAliasing(const utils::BindingInitializationHelper& x,
//                              const utils::BindingInitializationHelper& y) {
//     // bool IsBufferBindingAliasing(
//     //     const wgpu::BindGroupEntry& x,
//     //     const wgpu::BindGroupEntry& y) {

//     // if (x.buffer == y.buffer) {
//     if (x.size > 0 && y.size > 0) {
//         // assume no overflow cases (already validate at create bind group)?
//         if (x.offset <= y.offset + y.size - 1 && y.offset <= x.offset + x.size - 1) {
//             // overlap
//             return true;
//         }
//     }
//     // }
//     return false;
// }

// // bool FindAnyAliasingExist(const std::vector<BindingDescriptor>& bindings) {
// bool FindAnyAliasingExist(const BindingDescriptorGroups& bindingsGroups) {
//     for (size_t g0 = 0; g0 < bindingsGroups.size(); ++g0) {
//         const auto& bindings0 = bindingsGroups[g0];
//         for (size_t i = 0; i < bindings0.size(); ++i) {
//             for (size_t g1 = 0; g1 < bindingsGroups.size(); ++g1) {
//                 const auto& bindings1 = bindingsGroups[g1];
//                 size_t startBindingIdx = g0 == g1 ? i + 1 : 0;
//                 for (size_t j = startBindingIdx; j < bindings1.size(); ++j) {
//                     if (IsBufferBindingAliasing(bindings0[i].binding, bindings1[j].binding)) {
//                         return true;
//                     }
//                 }
//             }
//         }
//     }

//     // // Naive O(N^2)
//     // for (size_t i = 0; i < bindings.size(); ++i) {
//     //     for (size_t j = i + 1; j < bindings.size(); ++j) {
//     //         if (IsBufferBindingAliasing(bindings[i].binding, bindings[j].binding)) {
//     //             return true;
//     //         }
//     //     }
//     // }
//     return false;
// }

}  // namespace

class WritableBufferBindingAliasingValidationTests : public ValidationTest {
  public:
    void SetUp() override { ValidationTest::SetUp(); }

    wgpu::Buffer CreateBuffer(uint64_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = usage;

        return device.CreateBuffer(&bufferDescriptor);
    }

    // Creates compute pipeline given a layout and shader
    wgpu::ComputePipeline CreateComputePipeline(const std::vector<wgpu::BindGroupLayout>& layouts,
                                                const std::string& shader) {
        wgpu::ShaderModule csModule = utils::CreateShaderModule(device, shader.c_str());

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.layout = nullptr;
        if (!layouts.empty()) {
            wgpu::PipelineLayoutDescriptor descriptor;
            descriptor.bindGroupLayoutCount = layouts.size();
            descriptor.bindGroupLayouts = layouts.data();
            csDesc.layout = device.CreatePipelineLayout(&descriptor);
        }
        csDesc.compute.module = csModule;
        csDesc.compute.entryPoint = "main";

        return device.CreateComputePipeline(&csDesc);
    }

    // Creates compute pipeline with default layout
    wgpu::ComputePipeline CreateComputePipelineWithDefaultLayout(const std::string& shader) {
        return CreateComputePipeline({}, shader);
    }

    // Creates render pipeline give na layout and shaders
    wgpu::RenderPipeline CreateRenderPipeline(const std::vector<wgpu::BindGroupLayout>& layouts,
                                              const std::string& vertexShader,
                                              const std::string& fragShader) {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, vertexShader.c_str());

        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, fragShader.c_str());

        utils::ComboRenderPipelineDescriptor pipelineDescriptor;
        pipelineDescriptor.vertex.module = vsModule;
        pipelineDescriptor.cFragment.module = fsModule;
        pipelineDescriptor.cTargets[0].writeMask = wgpu::ColorWriteMask::None;
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
    wgpu::RenderPipeline CreateRenderPipelineWithDefaultLayout(const std::string& vertexShader,
                                                               const std::string& fragShader) {
        return CreateRenderPipeline({}, vertexShader, fragShader);
    }

    // Creates bind group layout with given minimum sizes for each binding
    wgpu::BindGroupLayout CreateBindGroupLayout(const std::vector<BindingDescriptor>& bindings) {
        std::vector<wgpu::BindGroupLayoutEntry> entries;

        for (size_t i = 0; i < bindings.size(); ++i) {
            const BindingDescriptor& b = bindings[i];
            wgpu::BindGroupLayoutEntry e = {};
            // e.binding = b.binding;
            e.binding = b.binding.binding;
            e.visibility = b.visibility;
            e.buffer.type = b.type;
            // e.buffer.minBindingSize = minimumSizes[i];
            e.buffer.minBindingSize = 0;
            e.buffer.hasDynamicOffset = b.hasDynamicOffset;

            entries.push_back(e);
        }

        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.entryCount = static_cast<uint32_t>(entries.size());
        descriptor.entries = entries.data();
        return device.CreateBindGroupLayout(&descriptor);
    }

    // Extract the first bind group from a compute shader
    wgpu::BindGroupLayout GetBGLFromComputeShader(const std::string& shader, uint32_t index) {
        wgpu::ComputePipeline pipeline = CreateComputePipelineWithDefaultLayout(shader);
        return pipeline.GetBindGroupLayout(index);
    }

    // Extract the first bind group from a render pass
    wgpu::BindGroupLayout GetBGLFromRenderShaders(const std::string& vertexShader,
                                                  const std::string& fragShader,
                                                  uint32_t index) {
        wgpu::RenderPipeline pipeline =
            CreateRenderPipelineWithDefaultLayout(vertexShader, fragShader);
        return pipeline.GetBindGroupLayout(index);
    }

    // wgpu::BindGroup CreateBindGroup(wgpu::BindGroupLayout layout,
    //                                 const std::vector<BindingDescriptor>& bindings) {
    std::vector<wgpu::BindGroup> CreateBindGroups(const std::vector<wgpu::BindGroupLayout>& layouts,
                                                  const BindingDescriptorGroups& bindingsGroups) {
        std::vector<wgpu::BindGroup> bindGroups;

        ASSERT(layouts.size() == bindingsGroups.size());
        for (size_t groupIdx = 0; groupIdx < layouts.size(); groupIdx++) {
            const auto& bindings = bindingsGroups[groupIdx];

            std::vector<wgpu::BindGroupEntry> entries;
            entries.reserve(bindings.size());
            for (const auto& binding : bindings) {
                entries.push_back(binding.binding.GetAsBinding());
            }

            wgpu::BindGroupDescriptor descriptor;
            descriptor.layout = layouts[groupIdx];
            descriptor.entryCount = checked_cast<uint32_t>(entries.size());
            descriptor.entries = entries.data();

            bindGroups.push_back(device.CreateBindGroup(&descriptor));
        }

        // return device.CreateBindGroup(&descriptor);
        return bindGroups;
    }

    // Runs a single dispatch with given pipeline and bind group (to test lazy validation during
    // dispatch)
    void TestDispatch(const wgpu::ComputePipeline& computePipeline,
                      const std::vector<wgpu::BindGroup>& bindGroups,
                      const TestSet& test) {
        //   bool expectation) {

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipeline);

        ASSERT(bindGroups.size() == test.bindingEntries.size());
        ASSERT(bindGroups.size() > 0);
        for (size_t i = 0; i < bindGroups.size(); ++i) {
            // assume in our test we only have buffer binding with same hasDynamicOffset across one
            // bindGroup and that we only have buffer bindings. So the dynamic buffer binding is
            // always compact
            if (test.bindingEntries[i][0].hasDynamicOffset) {
                // build the dynamicOffset vector
                const auto& b = test.bindingEntries[i];
                std::vector<uint32_t> dynamicOffsets(b.size());
                for (size_t j = 0; j < b.size(); ++j) {
                    dynamicOffsets[j] = b[j].dynamicOffset;
                }

                computePassEncoder.SetBindGroup(i, bindGroups[i], dynamicOffsets.size(),
                                                dynamicOffsets.data());
            } else {
                computePassEncoder.SetBindGroup(i, bindGroups[i]);
            }
        }

        computePassEncoder.DispatchWorkgroups(1);
        computePassEncoder.End();
        if (!test.valid) {
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        } else {
            commandEncoder.Finish();
        }
    }

    // Runs a single draw with given pipeline and bind group (to test lazy validation during draw)
    void TestDraw(const wgpu::RenderPipeline& renderPipeline,
                  const std::vector<wgpu::BindGroup>& bindGroups,
                  const TestSet& test) {
        //   bool expectation) {
        PlaceholderRenderPass renderPass(device);

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);
        renderPassEncoder.SetPipeline(renderPipeline);
        // for (size_t i = 0; i < bindGroups.size(); ++i) {
        //     renderPassEncoder.SetBindGroup(i, bindGroups[i]);
        // }

        ASSERT(bindGroups.size() == test.bindingEntries.size());
        ASSERT(bindGroups.size() > 0);
        for (size_t i = 0; i < bindGroups.size(); ++i) {
            // assume in our test we only have buffer binding with same hasDynamicOffset across one
            // bindGroup and that we only have buffer bindings. So the dynamic buffer binding is
            // always compact
            if (test.bindingEntries[i][0].hasDynamicOffset) {
                // build the dynamicOffset vector
                const auto& b = test.bindingEntries[i];
                std::vector<uint32_t> dynamicOffsets(b.size());
                for (size_t j = 0; j < b.size(); ++j) {
                    dynamicOffsets[j] = b[j].dynamicOffset;
                }

                renderPassEncoder.SetBindGroup(i, bindGroups[i], dynamicOffsets.size(),
                                               dynamicOffsets.data());
            } else {
                renderPassEncoder.SetBindGroup(i, bindGroups[i]);
            }
        }

        renderPassEncoder.Draw(3);
        renderPassEncoder.End();
        if (!test.valid) {
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        } else {
            commandEncoder.Finish();
        }
    }

    // void TestBindings(const wgpu::ComputePipeline& computePipeline, wgpu::BindGroupLayout layout,
    // const std::vector<BindingDescriptor>& bindings) {
    void TestBindings(const wgpu::ComputePipeline& computePipeline,
                      const wgpu::RenderPipeline& renderPipeline,
                      const std::vector<wgpu::BindGroupLayout>& layouts,
                      const TestSet& test) {
        std::vector<wgpu::BindGroup> bindGroups = CreateBindGroups(layouts, test.bindingEntries);

        // bool valid = !FindAnyAliasingExist(test);

        TestDispatch(computePipeline, bindGroups, test);
        TestDraw(renderPipeline, bindGroups, test);
    }
};

// class WritableBufferBindingAliasingValidationTests : public MinBufferSizeTestsBase {};

TEST_F(WritableBufferBindingAliasingValidationTests, BasicTest) {
    // std::vector<BindingDescriptor> bindings = {{0, 0, "buffer : array<f32>", "f32", "buffer[0]",
    // 12},
    //                                            {0, 1, "buffer : array<f32>", "f32", "buffer[0]",
    //                                            8}};

    wgpu::Buffer bufferStorage =
        CreateBuffer(1024, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage);
    wgpu::Buffer bufferStorage2 =
        CreateBuffer(1024, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage);
    // wgpu::Buffer bufferStorage =
    //     CreateBuffer(1024, wgpu::BufferUsage::Storage);
    wgpu::Buffer bufferNoStorage = CreateBuffer(1024, wgpu::BufferUsage::Uniform);

    std::vector<TestSet> testSet = {
        // same buffer, ranges don't overlap
        {true,
         {{
             {{0, bufferStorage, 256, 16}, wgpu::BufferBindingType::Storage},
             {{1, bufferStorage, 0, 8}, wgpu::BufferBindingType::Storage},
         }}},
        // same buffer, ranges overlap, in same bind group
        {false,
         {{
             {{0, bufferStorage, 0, 16}, wgpu::BufferBindingType::Storage},
             {{1, bufferStorage, 0, 8}, wgpu::BufferBindingType::Storage},
         }}},
        // same buffer, ranges don't overlap, in different bind group
        {true,
         {{
              {{0, bufferStorage, 256, 16}, wgpu::BufferBindingType::Storage},
          },
          {
              {{0, bufferStorage, 0, 8}, wgpu::BufferBindingType::Storage},
          }}},
        // same buffer, ranges overlap, in different bind group
        {false,
         {{
              {{0, bufferStorage, 0, 16}, wgpu::BufferBindingType::Storage},
          },
          {
              {{0, bufferStorage, 0, 8}, wgpu::BufferBindingType::Storage},
          }}},

        // same buffer, ranges overlap, but with read-only storage buffer type
        {true,
         {{
             {{0, bufferStorage, 0, 16}, wgpu::BufferBindingType::ReadOnlyStorage},
             {{1, bufferStorage, 0, 8}, wgpu::BufferBindingType::ReadOnlyStorage},
         }}},

        // different buffer, ranges overlap, valid
        {true,
         {{
             {{0, bufferStorage, 0, 16}, wgpu::BufferBindingType::Storage},
             {{1, bufferStorage2, 0, 8}, wgpu::BufferBindingType::Storage},
         }}},

        // same buffer, ranges don't overlap, but dynamic offset creates overlap.
        {false,
         {{
             {{0, bufferStorage, 256, 16}, wgpu::BufferBindingType::Storage, true, 0},
             {{1, bufferStorage, 0, 8}, wgpu::BufferBindingType::Storage, true, 256},
         }}},
    };

    for (const auto& test : testSet) {
        std::vector<wgpu::BindGroupLayout> layouts;
        for (const std::vector<BindingDescriptor>& bindings : test.bindingEntries) {
            // for each bind group bindings
            layouts.push_back(CreateBindGroupLayout(bindings));
        }

        std::string computeShader = CreateComputeShaderWithBindings(test.bindingEntries);
        wgpu::ComputePipeline computePipeline = CreateComputePipeline(layouts, computeShader);

        std::string vertexShader = CreateVertexShaderWithBindings(test.bindingEntries);
        std::string fragmentShader = CreateFragmentShaderWithBindings(test.bindingEntries);
        wgpu::RenderPipeline renderPipeline =
            CreateRenderPipeline(layouts, vertexShader, fragmentShader);

        TestBindings(computePipeline, renderPipeline, layouts, test);
    }
}

// set pipeline, dispatch, reset pipeline, dispatch (test lazy)
