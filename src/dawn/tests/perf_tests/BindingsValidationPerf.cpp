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

#include <vector>

#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {

constexpr unsigned int kNumIterations = 50;

enum class StorageBindingType {
    Buffer,   // Storage read_write buffer
    Texture,  // write-only storage texture
};

enum class BindingBindGroupType {
    SameBindGroup,
    SeparateBindGroup,
};

struct BindingDescriptor {
    utils::BindingInitializationHelper binding;
    StorageBindingType type;

    wgpu::ShaderStage visibility = wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment;

    // For storage buffer only
    bool hasDynamicOffset = false;
    uint32_t dynamicOffset = 0;
};

using BindingDescriptorGroups = std::vector<std::vector<BindingDescriptor>>;

// TODO: remove
enum class UploadMethod {
    WriteBuffer,
    MappedAtCreation,
};

// Perf delta exists between ranges [0, 1MB] vs [1MB, MAX_SIZE).
// These are sample buffer sizes within each range.
enum class UploadSize {
    BufferSize_1KB = 1 * 1024,
    BufferSize_64KB = 64 * 1024,
    BufferSize_1MB = 1 * 1024 * 1024,

    BufferSize_4MB = 4 * 1024 * 1024,
    BufferSize_16MB = 16 * 1024 * 1024,
};

// TOdO: test storage as well

// constexpr char kComputeShader[] = R"(
// struct S0 { buffer : array<f32> }
// @group(0) @binding(0) var<storage, read_write> b0 : S0;
// struct S1 { buffer : array<f32> }
// @group(1) @binding(0) var<storage, read_write> b1 : S1;
// @compute @workgroup_size(1,1,1) fn main() {
// _ = b0.buffer[0];
// _ = b1.buffer[0];
// })";

// constexpr char kVertexShader[] = R"(
// struct S0 { buffer : array<f32>}
// @group(0) @binding(0) var<storage, read_write> b0 : S0;
// struct S1 { buffer : array<f32>}
// @group(1) @binding(0) var<storage, read_write> b1 : S1;
// @vertex fn main() -> @builtin(position) vec4<f32> {
//     return vec4<f32>();
// })";

// constexpr char kFragmentShader[] = R"(
// struct S0 { buffer : array<f32>}
// @group(0) @binding(0) var<storage, read_write> b0 : S0;
// struct S1 { buffer : array<f32>}
// @group(1) @binding(0) var<storage, read_write> b1 : S1;
// @fragment fn main() {
// _ = b0.buffer[0];
// _ = b1.buffer[0];
// })";

struct BindingsValidationParams : AdapterTestParam {
    BindingsValidationParams(const AdapterTestParam& param,
                             BindingBindGroupType bindingBindGroupType)
        : AdapterTestParam(param), bindingBindGroupType(bindingBindGroupType) {}

    BindingBindGroupType bindingBindGroupType;
};

std::ostream& operator<<(std::ostream& ostream, const BindingsValidationParams& param) {
    ostream << static_cast<const AdapterTestParam&>(param);

    switch (param.bindingBindGroupType) {
        case BindingBindGroupType::SameBindGroup:
            ostream << "_SameBindGroup";
            break;
        case BindingBindGroupType::SeparateBindGroup:
            ostream << "_SeparateBindGroup";
            break;
        default:
            UNREACHABLE();
    }

    return ostream;
}

// Creates a bind group with given bindings for shader text
std::string GenerateBindingString(const BindingDescriptorGroups& bindingsGroups) {
    std::ostringstream ostream;
    size_t index = 0;
    size_t groupIndex = 0;
    for (const auto& bindings : bindingsGroups) {
        for (const BindingDescriptor& b : bindings) {
            ostream << "struct S" << index << " { "
                    << "buffer : array<f32>"
                    << "}\n";
            ostream << "@group(" << groupIndex << ") @binding(" << b.binding.binding << ") ";
            switch (b.type) {
                // case wgpu::BufferBindingType::Storage:
                case StorageBindingType::Buffer:
                    ostream << "var<storage, read_write> b" << index << " : S" << index << ";\n";
                    break;
                // case StorageBindingType::Texture:
                //     ostream << "var<storage, read_write> b" << index << " : S" << index << ";\n";
                //     break;
                default:
                    UNREACHABLE();
            }
            index++;
        }
        groupIndex++;
    }
    return ostream.str();
}

std::string GenerateReferenceString(const BindingDescriptorGroups& bindingsGroups,
                                    wgpu::ShaderStage stage) {
    std::ostringstream ostream;
    size_t index = 0;
    for (const auto& bindings : bindingsGroups) {
        for (const BindingDescriptor& b : bindings) {
            if (b.visibility & stage) {
                ostream << "_ = b" << index << "."
                        << "buffer[0]"
                        << ";\n";
            }
            index++;
        }
    }
    return ostream.str();
}

std::string CreateComputeShaderWithBindings(const BindingDescriptorGroups& bindingsGroups) {
    return GenerateBindingString(bindingsGroups) + "@compute @workgroup_size(1,1,1) fn main() {\n" +
           GenerateReferenceString(bindingsGroups, wgpu::ShaderStage::Compute) + "}";
}

}  // namespace

// Test
class BindingsValidationPerf : public DawnPerfTestWithParams<BindingsValidationParams> {
  public:
    BindingsValidationPerf() : DawnPerfTestWithParams(kNumIterations, 1) {}
    ~BindingsValidationPerf() override = default;

    void SetUp() override;

  private:
    wgpu::Buffer CreateBuffer(uint64_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = usage;

        return device.CreateBuffer(&bufferDescriptor);
    }

    wgpu::BindGroupLayout CreateBindGroupLayout(const std::vector<BindingDescriptor>& bindings) {
        std::vector<wgpu::BindGroupLayoutEntry> entries;

        for (size_t i = 0; i < bindings.size(); ++i) {
            const BindingDescriptor& b = bindings[i];
            wgpu::BindGroupLayoutEntry e = {};

            e.binding = b.binding.binding;
            e.visibility = b.visibility;

            switch (b.type) {
                case StorageBindingType::Buffer:
                    e.buffer.type = wgpu::BufferBindingType::Storage;
                    // e.buffer.minBindingSize = 0;
                    e.buffer.hasDynamicOffset = b.hasDynamicOffset;
                    break;
                case StorageBindingType::Texture:

                    break;
                default:
                    break;
            }
            entries.push_back(e);
        }

        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.entryCount = static_cast<uint32_t>(entries.size());
        descriptor.entries = entries.data();
        return device.CreateBindGroupLayout(&descriptor);
    }

    wgpu::ComputePipeline CreateComputePipeline(const std::vector<wgpu::BindGroupLayout>& layouts,
                                                const std::string& shader) {
        wgpu::ShaderModule csModule = utils::CreateShaderModule(device, shader.c_str());

        wgpu::ComputePipelineDescriptor csDesc;
        wgpu::PipelineLayoutDescriptor descriptor;
        descriptor.bindGroupLayoutCount = layouts.size();
        descriptor.bindGroupLayouts = layouts.data();
        csDesc.layout = device.CreatePipelineLayout(&descriptor);
        csDesc.compute.module = csModule;
        csDesc.compute.entryPoint = "main";

        return device.CreateComputePipeline(&csDesc);
    }

  private:
    void Step() override;

    // wgpu::Buffer dst;
    // std::vector<uint8_t> data;

    std::vector<wgpu::Buffer> storageBuffers;
    // std::vector<wgpu::Texture> storageTextures;

    wgpu::ComputePipeline computePipeline;
};

void BindingsValidationPerf::SetUp() {
    DawnPerfTestWithParams<BindingsValidationParams>::SetUp();

    // create a series buffer
    wgpu::Buffer buffer =
        CreateBuffer(1024, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage);
    storageBuffers.push_back(std::move(buffer));

    // create a series of storage textures

    // Fill bindings with maximum number of storage buffer bindings and storage texture bindings
    BindingDescriptorGroups bindingGroups;
    switch (GetParam().bindingBindGroupType) {
        case BindingBindGroupType::SameBindGroup:
            // TODO: fill to maximum binding
            bindingGroups = {{
                {{0, storageBuffers[0], 256, 16}, StorageBindingType::Buffer},
                {{1, storageBuffers[0], 0, 8}, StorageBindingType::Buffer},
            }};
            break;
        case BindingBindGroupType::SeparateBindGroup:
            bindingGroups = {{
                                 {{0, storageBuffers[0], 256, 16}, StorageBindingType::Buffer},
                             },
                             {
                                 {{1, storageBuffers[0], 0, 8}, StorageBindingType::Buffer},
                             }};
            break;
        default:
            UNREACHABLE();
    }

    //
    std::vector<wgpu::BindGroupLayout> layouts;
    for (const std::vector<BindingDescriptor>& bindings : bindingGroups) {
        layouts.push_back(CreateBindGroupLayout(bindings));
    }

    std::string computeShader = CreateComputeShaderWithBindings(bindingGroups);
    computePipeline = CreateComputePipeline(layouts, computeShader);

    // ASSERT(maxStorageBuffersPerShaderStage >= 8)
    // ASSERT(maxStorageTexturesPerShaderStage >= 4)
}

void BindingsValidationPerf::Step() {
    // wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    // wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
    // computePassEncoder.SetPipeline(computePipeline);

    // ASSERT(bindGroups.size() == test.bindingEntries.size());
    // ASSERT(bindGroups.size() > 0);
    // for (size_t i = 0; i < bindGroups.size(); ++i) {
    //     // Assuming that in our test we
    //     // (1) only have buffer bindings and
    //     // (2) only have buffer bindings with same hasDynamicOffset across one bindGroup,
    //     // the dynamic buffer binding is always compact.
    //     if (test.bindingEntries[i][0].hasDynamicOffset) {
    //         // build the dynamicOffset vector
    //         const auto& b = test.bindingEntries[i];
    //         std::vector<uint32_t> dynamicOffsets(b.size());
    //         for (size_t j = 0; j < b.size(); ++j) {
    //             dynamicOffsets[j] = b[j].dynamicOffset;
    //         }

    //         computePassEncoder.SetBindGroup(i, bindGroups[i], dynamicOffsets.size(),
    //                                         dynamicOffsets.data());
    //     } else {
    //         computePassEncoder.SetBindGroup(i, bindGroups[i]);
    //     }
    // }

    // computePassEncoder.DispatchWorkgroups(1);
    // computePassEncoder.End();
    // commandEncoder.Finish();
}

TEST_P(BindingsValidationPerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_TEST_P(BindingsValidationPerf,
                        {NullBackend()},
                        {BindingBindGroupType::SameBindGroup,
                         BindingBindGroupType::SeparateBindGroup});
