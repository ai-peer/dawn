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

#include <array>
#include <vector>

#include "dawn/common/Numeric.h"
#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {

// constexpr unsigned int kNumIterations = 50;
constexpr unsigned int kNumIterations = 1;

constexpr wgpu::TextureFormat kTextureFormat = wgpu::TextureFormat::RGBA8Unorm;

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

    wgpu::ShaderStage visibility = wgpu::ShaderStage::Compute;

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
                        ostream << "textureStore(b" << index
                                << ", vec2<i32>(0, 0), vec4<f32>(1.0, 1.0, 1.0, 1.0));\n";
                        // ostream << "_ = textureDimensions(b" << index << ");\n";
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
                    e.storageTexture.format = kTextureFormat;
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

    // // TODO: move into params
    // BindingDescriptorGroups bindingGroupsDescriptors;

    // bindingGroupsDescriptors[0] - for pipeline[0]
    // bindingGroupsDescriptors[1] - for pipeline[1]
    std::array<BindingDescriptorGroups, 2> bindingGroupsDescriptors;

    // std::vector<wgpu::Buffer> storageBuffers;
    // std::vector<wgpu::Texture> storageTextures;

    // wgpu::ComputePipeline computePipeline;

    // std::vector<wgpu::ComputePipeline> computePipelines;
    std::array<wgpu::ComputePipeline, 2> computePipelines;

    // std::vector<wgpu::BindGroup> bindGroups;
    std::array<std::vector<wgpu::BindGroup>, 2> bindGroups;
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

    // Fill bindings with default maximum number of storage buffer bindings and storage texture
    // bindings to test worst case validation scenario.
    const uint32_t numStorageBufferBindings = std::min(8u, limits.maxStorageBuffersPerShaderStage);
    const uint32_t numStorageTextureBindings =
        std::min(4u, limits.maxStorageTexturesPerShaderStage);

    ASSERT(limits.maxBindingsPerBindGroup >= numStorageBufferBindings + numStorageTextureBindings);

    const uint32_t numStorageBufferBindingsWithDynamicOffset = numStorageBufferBindings / 2;
    const uint32_t numStorageBufferBindingsWithStaticOffset =
        numStorageBufferBindings - numStorageBufferBindingsWithDynamicOffset;

    wgpu::Buffer buffer = CreateBuffer(numStorageBufferBindings * 256 + 16,
                                       wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage);

    // Setup bindings with 2 bindGroups (0 - , 1 - dynamic buffers)
    // TODO: std::array< ,2>
    // bindingGroupsDescriptors = {{}, {}};

    // bindingGroupsDescriptors[0/1][0] - static offset storage buffers & storage textures
    // bindingGroupsDescriptors[0/1][1] - dynamic offset storage buffers
    bindingGroupsDescriptors[0] = {{}, {}};
    bindingGroupsDescriptors[1] = {{}, {}};

    // Create storage buffer bindings (with static offsets)
    for (uint32_t i = 0; i < numStorageBufferBindingsWithStaticOffset; i++) {
        bindingGroupsDescriptors[0][0].push_back(BindingDescriptor{
            // Making sure no range overlap exists.
            {i, buffer, 256 * i, 16},
            StorageBindingType::Buffer});
        bindingGroupsDescriptors[1][0].push_back(BindingDescriptor{
            // Making sure no range overlap exists.
            {i, buffer, 256 * i, 8},
            StorageBindingType::Buffer});
    }

    // Create storage buffer bindigns (with dynamic offsets)
    for (uint32_t i = 0; i < numStorageBufferBindingsWithDynamicOffset; i++) {
        bindingGroupsDescriptors[0][1].push_back(BindingDescriptor{
            // Making sure no range overlap exists.
            {i, buffer, 0, 16},
            StorageBindingType::Buffer,
            wgpu::ShaderStage::Compute,
            true,
            256 * (i + numStorageBufferBindingsWithStaticOffset)});
        bindingGroupsDescriptors[1][1].push_back(BindingDescriptor{
            // Making sure no range overlap exists.
            {i, buffer, 0, 8},
            StorageBindingType::Buffer,
            wgpu::ShaderStage::Compute,
            true,
            256 * (i + numStorageBufferBindingsWithStaticOffset)});
    }

    // Create storage texture bindings
    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size = {16, 16, numStorageTextureBindings};
    descriptor.sampleCount = 1;
    descriptor.format = kTextureFormat;
    descriptor.mipLevelCount = 1;
    descriptor.usage = wgpu::TextureUsage::StorageBinding;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    ASSERT(limits.maxTextureArrayLayers >= numStorageTextureBindings);

    for (uint32_t i = 0; i < numStorageTextureBindings; i++) {
        // Making sure no texture-view-aliasing exists.
        wgpu::TextureViewDescriptor viewDesc = {};
        viewDesc.format = kTextureFormat;
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        viewDesc.aspect = wgpu::TextureAspect::All;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipLevelCount = 1;
        viewDesc.baseArrayLayer = i;
        viewDesc.arrayLayerCount = 1;

        // wgpu::TextureView textureView = texture.CreateView(&viewDesc);
        bindingGroupsDescriptors[0][0].push_back(
            BindingDescriptor{{numStorageBufferBindings + i, texture.CreateView(&viewDesc)},
                              StorageBindingType::Texture});

        viewDesc.baseArrayLayer = numStorageTextureBindings - 1 - i;
        bindingGroupsDescriptors[1][0].push_back(
            BindingDescriptor{{numStorageBufferBindings + i, texture.CreateView(&viewDesc)},
                              StorageBindingType::Texture});
    }

    // std::vector<wgpu::BindGroupLayout> layouts;
    // for (const std::vector<BindingDescriptor>& bindings : bindingGroupsDescriptors) {
    //     layouts.push_back(CreateBindGroupLayout(bindings));
    // }
    std::array<std::vector<wgpu::BindGroupLayout>, 2> layouts;
    for (const std::vector<BindingDescriptor>& bindings : bindingGroupsDescriptors[0]) {
        layouts[0].push_back(CreateBindGroupLayout(bindings));
    }
    for (const std::vector<BindingDescriptor>& bindings : bindingGroupsDescriptors[1]) {
        layouts[1].push_back(CreateBindGroupLayout(bindings));
    }

    // std::string computeShader = CreateComputeShaderWithBindings(bindingGroupsDescriptors);

    // 2 pipeline shares the same shader.
    std::string computeShader = CreateComputeShaderWithBindings(bindingGroupsDescriptors[0]);

    computePipelines[0] = CreateComputePipeline(layouts[0], computeShader);
    computePipelines[1] = CreateComputePipeline(layouts[1], computeShader);

    bindGroups[0] = CreateBindGroups(layouts[0], bindingGroupsDescriptors[0]);
    bindGroups[1] = CreateBindGroups(layouts[1], bindingGroupsDescriptors[1]);
}

void BindingsValidationPerf::Step() {
    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();

    {
        const size_t p = 0;  // pipeline id
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipelines[p]);
        computePassEncoder.SetBindGroup(0, bindGroups[p][0]);
        {
            // bindGroup[1] has dynamic offsets
            std::vector<uint32_t> dynamicOffsets(bindingGroupsDescriptors[p][1].size());
            for (size_t i = 0; i < dynamicOffsets.size(); ++i) {
                dynamicOffsets[i] = bindingGroupsDescriptors[p][1][i].dynamicOffset;
            }
            computePassEncoder.SetBindGroup(1, bindGroups[p][1], dynamicOffsets.size(),
                                            dynamicOffsets.data());
        }
        computePassEncoder.DispatchWorkgroups(1);
        computePassEncoder.End();
    }

    if (GetParam().dirtyAspectTestType == DirtyAspectTestType::BusyValidation) {
        // Switch between to another pipeline and bind group,
        // So that bind group validating as a lazy aspect needs to be checked on every dispatch
        const size_t p = 1;  // pipeline id
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipelines[1]);
        computePassEncoder.SetBindGroup(0, bindGroups[p][0]);
        {
            // bindGroup[1] has dynamic offsets
            std::vector<uint32_t> dynamicOffsets(bindingGroupsDescriptors[p][1].size());
            for (size_t i = 0; i < dynamicOffsets.size(); ++i) {
                dynamicOffsets[i] = bindingGroupsDescriptors[p][1][i].dynamicOffset;
            }
            computePassEncoder.SetBindGroup(1, bindGroups[p][1], dynamicOffsets.size(),
                                            dynamicOffsets.data());
        }
        computePassEncoder.DispatchWorkgroups(1);
        computePassEncoder.End();
    }

    commandEncoder.Finish();
}

TEST_P(BindingsValidationPerf, Run) {
    RunTest();
}

// TODO: replace params
DAWN_INSTANTIATE_TEST_P(BindingsValidationPerf,
                        {NullBackend()},
                        {DirtyAspectTestType::BusyValidation, DirtyAspectTestType::LazyValidation});
