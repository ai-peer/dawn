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
#include <string>
#include <vector>

#include "dawn/common/Numeric.h"
#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {

constexpr unsigned int kNumIterations = 50;
constexpr unsigned int kDispatchTimes = 1000;

constexpr wgpu::TextureFormat kTextureFormat = wgpu::TextureFormat::RGBA8Unorm;

enum class BindingType {
    UniformBuffer,   // Uniform buffer
    StorageBuffer,   // Storage read_write buffer
    StorageTexture,  // Write-only storage texture
};

enum class DirtyAspectTestType {
    // Switch between 2 bindGroup instances to validate bindings every dispatch
    BusyValidation,
    // bindGroup stays unchanged so the binding validation should only happen once
    LazyValidation,
};

struct BindGroupLayoutEntryDescriptor {
    BindingType type;

    // Used by storage buffer binding only
    bool hasDynamicOffset = false;

    wgpu::ShaderStage visibility = wgpu::ShaderStage::Compute;
};

using BindGroupLayoutEntryDescriptorSequence =
    std::vector<std::vector<BindGroupLayoutEntryDescriptor>>;

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
std::string GenerateBindingString(const BindGroupLayoutEntryDescriptorSequence& descriptors) {
    std::ostringstream ostream;
    size_t index = 0;
    uint32_t groupIndex = 0;
    for (const auto& entries : descriptors) {
        // Assume bindingIndex is compact.
        uint32_t bindingIndex = 0;
        for (const BindGroupLayoutEntryDescriptor& b : entries) {
            ostream << "@group(" << groupIndex << ") @binding(" << bindingIndex << ") ";
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
            bindingIndex++;
            index++;
        }
        groupIndex++;
    }
    return ostream.str();
}

// Creates reference shader text to make sure variables don't get optimized out.
std::string GenerateReferenceString(const BindGroupLayoutEntryDescriptorSequence& descriptors,
                                    wgpu::ShaderStage stage) {
    std::ostringstream ostream;
    size_t index = 0;
    for (const auto& entries : descriptors) {
        for (const BindGroupLayoutEntryDescriptor& b : entries) {
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

std::string CreateComputeShaderWithBindings(
    const BindGroupLayoutEntryDescriptorSequence& descriptors) {
    return GenerateBindingString(descriptors) + "@compute @workgroup_size(1,1,1) fn main() {\n" +
           GenerateReferenceString(descriptors, wgpu::ShaderStage::Compute) + "}";
}

}  // namespace

// BindingsValidationPerf tests per compute pass dispatch validation performance.
// It creates a compute pipeline, with a pipeline layout of 2 bind group layout entries.
// bindGroupLayoutEntry[0] contains 12 uniform buffer bindings, 4 storage buffer bindings and 4
// storage texture bindings. bindGroupLayoutEntry[1] contains 4 storage buffer bindings with dynamic
// offsets. In Step() it creates a compute pass and dispatch, which will validate the bindings. If
// DirtyAspectTestType == BusyValidation, it switches between 2 different bindGroup instances and
// dispatch to avoid only validating the bind group as a lazy aspect once.
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

    wgpu::BindGroupLayout CreateBindGroupLayout(
        const std::vector<BindGroupLayoutEntryDescriptor>& descriptors) {
        std::vector<wgpu::BindGroupLayoutEntry> entries;

        for (size_t i = 0; i < descriptors.size(); ++i) {
            const BindGroupLayoutEntryDescriptor& b = descriptors[i];
            wgpu::BindGroupLayoutEntry e = {};

            // Assume binding index is compact.
            e.binding = i;
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

    std::vector<wgpu::BindGroup> CreateBindGroups(
        const std::vector<wgpu::BindGroupLayout>& layouts,
        const std::vector<std::vector<utils::BindingInitializationHelper>>& bindingsGroups) {
        std::vector<wgpu::BindGroup> bindGroups;

        ASSERT(layouts.size() == bindingsGroups.size());
        for (size_t groupIdx = 0; groupIdx < layouts.size(); groupIdx++) {
            const auto& bindings = bindingsGroups[groupIdx];

            std::vector<wgpu::BindGroupEntry> entries;
            entries.reserve(bindings.size());
            for (const auto& binding : bindings) {
                entries.push_back(binding.GetAsBinding());
            }

            wgpu::BindGroupDescriptor descriptor;
            descriptor.layout = layouts[groupIdx];
            descriptor.entryCount = checked_cast<uint32_t>(entries.size());
            descriptor.entries = entries.data();

            bindGroups.push_back(device.CreateBindGroup(&descriptor));
        }

        return bindGroups;
    }

    void Step() override;

    // Swtich between 2 bindGroups instances
    std::array<std::vector<wgpu::BindGroup>, 2> bindGroups;
    // Switch between 2 set of dynamic offsets
    std::array<std::vector<uint32_t>, 2> dynamicOffsets;

    wgpu::ComputePipeline computePipeline;
};

void BindingsValidationPerf::SetUp() {
    DawnPerfTestWithParams<BindingsValidationParams>::SetUp();

    wgpu::SupportedLimits supportedLimits = {};
    device.GetLimits(&supportedLimits);
    wgpu::Limits limits = supportedLimits.limits;

    // Fill bindings with default maximum number of storage buffer bindings and storage texture
    // bindings to test worst case validation scenario.
    const uint32_t numUniformBufferBindings = limits.maxUniformBuffersPerShaderStage;
    const uint32_t numStorageBufferBindings = limits.maxStorageBuffersPerShaderStage;
    const uint32_t numStorageTextureBindings = limits.maxStorageTexturesPerShaderStage;

    ASSERT(limits.maxBindingsPerBindGroup >=
           numUniformBufferBindings + numStorageBufferBindings + numStorageTextureBindings);

    const uint32_t numStorageBufferBindingsWithDynamicOffset = numStorageBufferBindings / 2;
    const uint32_t numStorageBufferBindingsWithStaticOffset =
        numStorageBufferBindings - numStorageBufferBindingsWithDynamicOffset;

    wgpu::Buffer storageBuffer =
        CreateBuffer(numStorageBufferBindings * 256 + 16, wgpu::BufferUsage::Storage);
    wgpu::Buffer uniformBuffer = CreateBuffer(1024, wgpu::BufferUsage::Uniform);

    // BindGroupLayout has 2 entries
    // bindGroupLayoutEntry[0] - static offset storage buffers & storage textures
    // bindGroupLayoutEntry[1] - dynamic offset storage buffers
    BindGroupLayoutEntryDescriptorSequence bindingLayoutEntryDescriptors = {{}, {}};

    // 2 bindGroup instances to switch between per dispatch.
    std::array<std::vector<std::vector<utils::BindingInitializationHelper>>, 2>
        bindGroupDescriptors;
    bindGroupDescriptors[0] = {{}, {}};
    bindGroupDescriptors[1] = {{}, {}};

    // bindGroupLayoutEntry[0]
    // Create uniform buffer bindings
    for (uint32_t i = 0; i < numUniformBufferBindings; i++) {
        bindingLayoutEntryDescriptors[0].push_back(
            BindGroupLayoutEntryDescriptor{BindingType::UniformBuffer});
        bindGroupDescriptors[0][0].push_back(
            utils::BindingInitializationHelper{i, uniformBuffer, 0, 64});
        bindGroupDescriptors[1][0].push_back(
            utils::BindingInitializationHelper{i, uniformBuffer, 0, 1024});
    }

    // Create storage buffer bindings (with static offsets)
    for (uint32_t i = 0; i < numStorageBufferBindingsWithStaticOffset; i++) {
        bindingLayoutEntryDescriptors[0].push_back(
            BindGroupLayoutEntryDescriptor{BindingType::StorageBuffer});
        // Making sure no buffer-binding-aliasing exists.
        bindGroupDescriptors[0][0].push_back(utils::BindingInitializationHelper{
            numUniformBufferBindings + i, storageBuffer, 256 * i, 16});
        bindGroupDescriptors[1][0].push_back(utils::BindingInitializationHelper{
            numUniformBufferBindings + i, storageBuffer, 256 * i, 8});
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

        bindingLayoutEntryDescriptors[0].push_back(
            BindGroupLayoutEntryDescriptor{BindingType::StorageTexture});

        bindGroupDescriptors[0][0].push_back(utils::BindingInitializationHelper{
            numUniformBufferBindings + numStorageBufferBindingsWithStaticOffset + i,
            texture.CreateView(&viewDesc)});

        viewDesc.baseArrayLayer = numStorageTextureBindings - 1 - i;
        bindGroupDescriptors[1][0].push_back(utils::BindingInitializationHelper{
            numUniformBufferBindings + numStorageBufferBindingsWithStaticOffset + i,
            texture.CreateView(&viewDesc)});
    }

    // bindGroupLayoutEntry[1]
    // Create storage buffer bindings (with dynamic offsets)
    for (uint32_t i = 0; i < numStorageBufferBindingsWithDynamicOffset; i++) {
        bindingLayoutEntryDescriptors[1].push_back(
            BindGroupLayoutEntryDescriptor{BindingType::StorageBuffer, true});
        bindGroupDescriptors[0][1].push_back(
            utils::BindingInitializationHelper{i, storageBuffer, 0, 16});
        bindGroupDescriptors[1][1].push_back(
            utils::BindingInitializationHelper{i, storageBuffer, 0, 8});
    }

    dynamicOffsets[0].resize(bindGroupDescriptors[0][1].size());
    dynamicOffsets[1].resize(bindGroupDescriptors[1][1].size());
    for (size_t i = 0; i < numStorageBufferBindingsWithDynamicOffset; ++i) {
        dynamicOffsets[0][i] = 256 * (i + numStorageBufferBindingsWithStaticOffset);
        dynamicOffsets[1][i] = 256 * (numStorageBufferBindingsWithDynamicOffset - 1 - i +
                                      numStorageBufferBindingsWithStaticOffset);
    }

    std::vector<wgpu::BindGroupLayout> layouts;
    for (const std::vector<BindGroupLayoutEntryDescriptor>& entries :
         bindingLayoutEntryDescriptors) {
        layouts.push_back(CreateBindGroupLayout(entries));
    }

    std::string computeShader = CreateComputeShaderWithBindings(bindingLayoutEntryDescriptors);
    computePipeline = CreateComputePipeline(layouts, computeShader);

    bindGroups[0] = CreateBindGroups(layouts, bindGroupDescriptors[0]);
    bindGroups[1] = CreateBindGroups(layouts, bindGroupDescriptors[1]);
}

void BindingsValidationPerf::Step() {
    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();

    computePassEncoder.SetPipeline(computePipeline);

    if (GetParam().dirtyAspectTestType == DirtyAspectTestType::BusyValidation) {
        for (size_t i = 0; i < kDispatchTimes; i++) {
            // Switch between 2 bindGroup instances and dispatch.
            // The validation should happen every dispatch.
            const size_t b = i % 2;
            computePassEncoder.SetBindGroup(0, bindGroups[b][0]);
            // bindGroupLayoutEntry[1] has dynamic offsets
            computePassEncoder.SetBindGroup(1, bindGroups[b][1], dynamicOffsets[b].size(),
                                            dynamicOffsets[b].data());
            computePassEncoder.DispatchWorkgroups(1);
        }
    } else {
        // The bindGroup is set once and then dispatch a number of times.
        // The validation should happen once.
        computePassEncoder.SetBindGroup(0, bindGroups[0][0]);
        // bindGroupLayoutEntry[1] has dynamic offsets
        computePassEncoder.SetBindGroup(1, bindGroups[0][1], dynamicOffsets[0].size(),
                                        dynamicOffsets[0].data());
        for (size_t i = 0; i < kDispatchTimes; i++) {
            computePassEncoder.DispatchWorkgroups(1);
        }
    }

    computePassEncoder.End();
    commandEncoder.Finish();
}

TEST_P(BindingsValidationPerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_TEST_P(BindingsValidationPerf,
                        {NullBackend()},
                        {DirtyAspectTestType::BusyValidation, DirtyAspectTestType::LazyValidation});
