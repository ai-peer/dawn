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

// #include <array>
#include <vector>

#include "dawn/common/Numeric.h"
#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {

// constexpr unsigned int kNumIterations = 50;
constexpr unsigned int kNumIterations = 1;

enum class StorageBindingType {
    Buffer,   // Storage read_write buffer
    Texture,  // write-only storage texture
};

// enum class BindingBindGroupType {
//     SameBindGroup,
//     SeparateBindGroup,
// };

enum class DirtyAspectTestType {
    // Switch between 2 pipeline to validate bindings every dispatch
    BusyValidation,
    // Pipeline and bind groups stay unchanged so the binding validation should only happen once
    LazyValidation,
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
    BindingsValidationParams(const AdapterTestParam& param, DirtyAspectTestType dirtyAspectTestType)
        : AdapterTestParam(param), dirtyAspectTestType(dirtyAspectTestType) {}

    DirtyAspectTestType dirtyAspectTestType;

    // TODO: move BindingGroupsDescriptor here?
};

std::ostream& operator<<(std::ostream& ostream, const BindingsValidationParams& param) {
    ostream << static_cast<const AdapterTestParam&>(param);

    switch (param.dirtyAspectTestType) {
        case DirtyAspectTestType::BusyValidation:
            ostream << "_BusyValidation";
            break;
        case DirtyAspectTestType::LazyValidation:
            ostream << "_LazyValidation";
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
            // ostream << "struct S" << index << " { "
            //         << "buffer : array<f32>"
            //         << "}\n";
            ostream << "@group(" << groupIndex << ") @binding(" << b.binding.binding << ") ";
            switch (b.type) {
                // case wgpu::BufferBindingType::Storage:
                case StorageBindingType::Buffer:
                    ostream << "var<storage, read_write> b" << index << " : array<f32>;\n";
                    break;
                case StorageBindingType::Texture:
                    ostream << "var b" << index << " : texture_storage_2d<rgba8unorm, write>;\n";
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

std::string GenerateReferenceString(const BindingDescriptorGroups& bindingsGroups,
                                    wgpu::ShaderStage stage) {
    std::ostringstream ostream;
    size_t index = 0;
    for (const auto& bindings : bindingsGroups) {
        for (const BindingDescriptor& b : bindings) {
            if (b.visibility & stage) {
                switch (b.type) {
                    case StorageBindingType::Buffer:
                        ostream << "_ = b" << index << "[0];\n";
                        break;
                    case StorageBindingType::Texture:
                        // ostream << "textureStore(b" << index << "[0]";
                        ostream << "_ = textureDimensions(b" << index << ");\n";
                        break;
                    default:
                        UNREACHABLE();
                }
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
                    e.storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
                    e.storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
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

        return bindGroups;
    }

  private:
    void Step() override;

    // wgpu::Buffer dst;
    // std::vector<uint8_t> data;

    // TODO: move into params
    BindingDescriptorGroups bindingGroupsDescriptors;

    // std::vector<wgpu::Buffer> storageBuffers;
    // std::vector<wgpu::Texture> storageTextures;

    // wgpu::ComputePipeline computePipeline;

    std::vector<wgpu::ComputePipeline> computePipelines;

    std::vector<wgpu::BindGroup> bindGroups;
};

void BindingsValidationPerf::SetUp() {
    DawnPerfTestWithParams<BindingsValidationParams>::SetUp();

    wgpu::SupportedLimits supportedLimits = {};
    device.GetLimits(&supportedLimits);
    wgpu::Limits limits = supportedLimits.limits;

    // // Create 2 storage buffers
    // // ?? don't seems that we need 2
    // for (uint32_t i = 0; i < 2; i++) {
    //     wgpu::Buffer buffer = CreateBuffer(limits.maxStorageBuffersPerShaderStage * 256 + 16,
    //                                        wgpu::BufferUsage::Uniform |
    //                                        wgpu::BufferUsage::Storage);
    //     storageBuffers.push_back(std::move(buffer));
    // }
    wgpu::Buffer buffer = CreateBuffer(limits.maxStorageBuffersPerShaderStage * 256 + 16,
                                       wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage);

    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size = {16, 16, 1};
    descriptor.sampleCount = 1;
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.mipLevelCount = 1;
    descriptor.usage = wgpu::TextureUsage::StorageBinding;
    // wgpu::Texture texture = device.CreateTexture(&descriptor);
    wgpu::TextureView textureView = device.CreateTexture(&descriptor).CreateView();

    // Setup bindings with 2 bindGroups
    bindingGroupsDescriptors = {{}, {}};

    // Fill bindings with maximum number of storage buffer bindings and storage texture bindings
    // to test worst case validation scenario.

    // TODO: dynamic buffer offsets
    for (uint32_t i = 0; i < limits.maxStorageBuffersPerShaderStage; i++) {
        bindingGroupsDescriptors[0].push_back(BindingDescriptor{
            // Making sure no range overlap exists.
            {i, buffer, 256 * i, 16},
            StorageBindingType::Buffer});
        bindingGroupsDescriptors[1].push_back(BindingDescriptor{
            // Making sure no range overlap exists.
            {i, buffer, 256 * i, 16},
            StorageBindingType::Buffer});
    }

    // // TODO: fix validation, subresource
    // for (uint32_t i = 0; i < limits.maxStorageTexturesPerShaderStage; i++) {
    //     bindingGroupsDescriptors[0].push_back(BindingDescriptor{
    //         // Making sure no range overlap exists.
    //         {i, textureView},
    //         StorageBindingType::Texture});
    //     bindingGroupsDescriptors[1].push_back(BindingDescriptor{
    //         // Making sure no range overlap exists.
    //         {i, textureView},
    //         StorageBindingType::Texture});
    // }

    //
    std::vector<wgpu::BindGroupLayout> layouts;
    for (const std::vector<BindingDescriptor>& bindings : bindingGroupsDescriptors) {
        layouts.push_back(CreateBindGroupLayout(bindings));
    }

    // std::string computeShader = CreateComputeShaderWithBindings(bindingGroupsDescriptors);

    // 2 pipeline shares the same shader.
    std::string computeShader = CreateComputeShaderWithBindings({bindingGroupsDescriptors[0]});

    // computePipeline = CreateComputePipeline(layouts, computeShader);
    computePipelines.push_back(CreateComputePipeline({layouts[0]}, computeShader));
    computePipelines.push_back(CreateComputePipeline({layouts[1]}, computeShader));

    bindGroups = CreateBindGroups(layouts, bindingGroupsDescriptors);

    // ASSERT(maxStorageBuffersPerShaderStage >= 8)
    // ASSERT(maxStorageTexturesPerShaderStage >= 4)
}

void BindingsValidationPerf::Step() {
    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();

    // ASSERT(bindGroups.size() == bindingGroupsDescriptors.size());
    // ASSERT(bindGroups.size() > 0);

    // Storage buffer pass

    // computePassEncoder.SetPipeline(computePipeline);

    // Switch between 2 pipeline / bindgroups to avoid lazy aspect not being checked
    {
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipelines[0]);
        computePassEncoder.SetBindGroup(0, bindGroups[0]);
        computePassEncoder.DispatchWorkgroups(1);
        computePassEncoder.End();
    }

    if (GetParam().dirtyAspectTestType == DirtyAspectTestType::BusyValidation) {
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipelines[1]);
        computePassEncoder.SetBindGroup(0, bindGroups[1]);
        computePassEncoder.DispatchWorkgroups(1);
        computePassEncoder.End();
    }

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

    // for (size_t i = 0; i < bindGroups.size(); ++i) {
    //     computePassEncoder.SetBindGroup(i, bindGroups[i]);
    // }

    // Storage texture pass

    commandEncoder.Finish();
}

TEST_P(BindingsValidationPerf, Run) {
    RunTest();
}

// TODO: replace params
DAWN_INSTANTIATE_TEST_P(BindingsValidationPerf,
                        {NullBackend()},
                        {DirtyAspectTestType::BusyValidation, DirtyAspectTestType::LazyValidation});
