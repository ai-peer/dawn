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

#include <array>
#include <vector>

#include "dawn/common/Numeric.h"
#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {

constexpr unsigned int kNumIterations = 50;

constexpr wgpu::TextureFormat kTextureFormat = wgpu::TextureFormat::RGBA8Unorm;

enum class BindingType {
    UniformBuffer,   // Uniform buffer
    StorageBuffer,   // Storage read_write buffer
    StorageTexture,  // Write-only storage texture
};

enum class DirtyAspectTestType {
    // Switch between 2 pipelines to validate bindings every dispatch
    BusyValidation,
    // Pipeline and bind groups stay unchanged so the binding validation should only happen once
    LazyValidation,
};

struct BindingDescriptor {
    utils::BindingInitializationHelper binding;
    BindingType type;

    wgpu::ShaderStage visibility = wgpu::ShaderStage::Compute;

    // Used by storage buffer binding only
    bool hasDynamicOffset = false;
    uint32_t dynamicOffset = 0;
};

using BindingDescriptorGroups = std::vector<std::vector<BindingDescriptor>>;

struct BindingsValidationParams : AdapterTestParam {
    BindingsValidationParams(const AdapterTestParam& param, DirtyAspectTestType dirtyAspectTestType)
        : AdapterTestParam(param), dirtyAspectTestType(dirtyAspectTestType) {}

    DirtyAspectTestType dirtyAspectTestType;
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

// Creates a bind group with given bindings for shader text.
std::string GenerateBindingString(const BindingDescriptorGroups& bindingsGroups) {
    std::ostringstream ostream;
    size_t index = 0;
    size_t groupIndex = 0;
    for (const auto& bindings : bindingsGroups) {
        for (const BindingDescriptor& b : bindings) {
            ostream << "@group(" << groupIndex << ") @binding(" << b.binding.binding << ") ";
            switch (b.type) {
                case BindingType::UniformBuffer:
                    ostream << "var<uniform> b" << index << " : array<vec4<f32>, 4>;\n";
                    break;
                case BindingType::StorageBuffer:
                    ostream << "var<storage, read_write> b" << index << " : array<f32>;\n";
                    break;
                case BindingType::StorageTexture:
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

// Creates reference shader text to make sure variables don't get optimized out.
std::string GenerateReferenceString(const BindingDescriptorGroups& bindingsGroups,
                                    wgpu::ShaderStage stage) {
    std::ostringstream ostream;
    size_t index = 0;
    for (const auto& bindings : bindingsGroups) {
        for (const BindingDescriptor& b : bindings) {
            if (b.visibility & stage) {
                switch (b.type) {
                    case BindingType::UniformBuffer:
                        ostream << "_ = b" << index << "[0].x;\n";
                        break;
                    case BindingType::StorageBuffer:
                        ostream << "_ = b" << index << "[0];\n";
                        break;
                    case BindingType::StorageTexture:
                        ostream << "textureStore(b" << index
                                << ", vec2<i32>(0, 0), vec4<f32>(1.0, 1.0, 1.0, 1.0));\n";
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

// BindingsValidationPerf tests per compute pass dispatch validation performance.
// It creates 2 compute pipelines with similiar bind group layouts.
// bindGroup[0] contains 12 uniform buffer bindings, 4 storage buffer bindings and 4 storage texture bindings.
// bindGroup[1] contains 4 storage buffer bindings with dynamic offsets.
// In Step() it creates a compute pass and dispatch, which will validate the bindings.
// If DirtyAspectTestType == BusyValidation, it creates another compute pass and dispatch to avoid
// only validating the bind group as a lazy aspect once.
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
                case BindingType::UniformBuffer:
                    e.buffer.type = wgpu::BufferBindingType::Uniform;
                    break;
                case BindingType::StorageBuffer:
                    e.buffer.type = wgpu::BufferBindingType::Storage;
                    e.buffer.hasDynamicOffset = b.hasDynamicOffset;
                    break;
                case BindingType::StorageTexture:
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

    // bindingGroupsDescriptors[0] - for pipeline[0]
    // bindingGroupsDescriptors[1] - for pipeline[1]
    std::array<BindingDescriptorGroups, 2> bindingGroupsDescriptors;
    std::array<std::vector<wgpu::BindGroup>, 2> bindGroups;
    std::array<wgpu::ComputePipeline, 2> computePipelines;
};

void BindingsValidationPerf::SetUp() {
    DawnPerfTestWithParams<BindingsValidationParams>::SetUp();

    wgpu::SupportedLimits supportedLimits = {};
    device.GetLimits(&supportedLimits);
    wgpu::Limits limits = supportedLimits.limits;

    // Fill bindings with default maximum number of storage buffer bindings and storage texture
    // bindings to test worst case validation scenario.
    const uint32_t numUniformBufferBindings = std::min(12u, limits.maxUniformBuffersPerShaderStage);
    const uint32_t numStorageBufferBindings = std::min(8u, limits.maxStorageBuffersPerShaderStage);
    const uint32_t numStorageTextureBindings =
        std::min(4u, limits.maxStorageTexturesPerShaderStage);

    ASSERT(limits.maxBindingsPerBindGroup >= numUniformBufferBindings + numStorageBufferBindings + numStorageTextureBindings);

    const uint32_t numStorageBufferBindingsWithDynamicOffset = numStorageBufferBindings / 2;
    const uint32_t numStorageBufferBindingsWithStaticOffset =
        numStorageBufferBindings - numStorageBufferBindingsWithDynamicOffset;

    wgpu::Buffer storageBuffer = CreateBuffer(numStorageBufferBindings * 256 + 16,
                                       wgpu::BufferUsage::Storage);
    wgpu::Buffer uniformBuffer = CreateBuffer(1024,
                                       wgpu::BufferUsage::Uniform);

    // bindingGroupsDescriptors[0/1][0] - static offset storage buffers & storage textures
    // bindingGroupsDescriptors[0/1][1] - dynamic offset storage buffers
    // Make bindings for pipelines[0] and [1] a bit different to avoid potential caching.
    bindingGroupsDescriptors[0] = {{}, {}};
    bindingGroupsDescriptors[1] = {{}, {}};

    // bindGroup[0]
    // Create uniform buffer bindings
    for (uint32_t i = 0; i < numUniformBufferBindings; i++) {
        bindingGroupsDescriptors[0][0].push_back(BindingDescriptor{
            {i, uniformBuffer, 0, 64},
            BindingType::UniformBuffer});
        bindingGroupsDescriptors[1][0].push_back(BindingDescriptor{
            {i, uniformBuffer, 0, 1024},
            BindingType::UniformBuffer});
    }

    // Create storage buffer bindings (with static offsets)
    for (uint32_t i = 0; i < numStorageBufferBindingsWithStaticOffset; i++) {
        bindingGroupsDescriptors[0][0].push_back(BindingDescriptor{
            // Making sure no buffer-binding-aliasing exists.
            {numUniformBufferBindings + i, storageBuffer, 256 * i, 16},
            BindingType::StorageBuffer});
        bindingGroupsDescriptors[1][0].push_back(BindingDescriptor{
            // Making sure no buffer-binding-aliasing exists.
            {numUniformBufferBindings + i, storageBuffer, 256 * i, 8},
            BindingType::StorageBuffer});
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

        bindingGroupsDescriptors[0][0].push_back(
            BindingDescriptor{{numUniformBufferBindings + numStorageBufferBindingsWithStaticOffset + i, texture.CreateView(&viewDesc)},
                              BindingType::StorageTexture});

        viewDesc.baseArrayLayer = numStorageTextureBindings - 1 - i;
        bindingGroupsDescriptors[1][0].push_back(
            BindingDescriptor{{numUniformBufferBindings + numStorageBufferBindingsWithStaticOffset + i, texture.CreateView(&viewDesc)},
                              BindingType::StorageTexture});
    }

    // bindGroup[1]
    // Create storage buffer bindigns (with dynamic offsets)
    for (uint32_t i = 0; i < numStorageBufferBindingsWithDynamicOffset; i++) {
        bindingGroupsDescriptors[0][1].push_back(BindingDescriptor{
            // Making sure no buffer-binding-aliasing exists.
            {i, storageBuffer, 0, 16},
            BindingType::StorageBuffer,
            wgpu::ShaderStage::Compute,
            true,
            256 * (i + numStorageBufferBindingsWithStaticOffset)});
        bindingGroupsDescriptors[1][1].push_back(BindingDescriptor{
            // Making sure no buffer-binding-aliasing exists.
            {i, storageBuffer, 0, 8},
            BindingType::StorageBuffer,
            wgpu::ShaderStage::Compute,
            true,
            256 * (i + numStorageBufferBindingsWithStaticOffset)});
    }

    std::array<std::vector<wgpu::BindGroupLayout>, 2> layouts;
    for (const std::vector<BindingDescriptor>& bindings : bindingGroupsDescriptors[0]) {
        layouts[0].push_back(CreateBindGroupLayout(bindings));
    }
    for (const std::vector<BindingDescriptor>& bindings : bindingGroupsDescriptors[1]) {
        layouts[1].push_back(CreateBindGroupLayout(bindings));
    }

    // Pipelines share the same shader.
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
        // Switch to another pipeline and bind group,
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

DAWN_INSTANTIATE_TEST_P(BindingsValidationPerf,
                        {NullBackend()},
                        {DirtyAspectTestType::BusyValidation, DirtyAspectTestType::LazyValidation});
