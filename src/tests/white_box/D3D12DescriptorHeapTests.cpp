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

#include "tests/DawnTest.h"

#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/ShaderVisibleDescriptorAllocatorD3D12.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr uint32_t kRTSize = 4;

// Pooling tests are required to advance the GPU completed serial to reuse heaps.
// This requires Tick() to be called at-least |kFrameDepth| times. This constant
// should be updated if the internals of Tick() change.
constexpr uint32_t kFrameDepth = 2;

using namespace dawn_native::d3d12;

class D3D12DescriptorHeapTests : public DawnTest {
  protected:
    void TestSetUp() override {
        DAWN_SKIP_TEST_IF(UsesWire());
        mD3DDevice = reinterpret_cast<Device*>(device.Get());
    }

    uint32_t GetShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE heapType) const {
        return mD3DDevice->GetShaderVisibleDescriptorAllocator()
            ->GetShaderVisibleHeapSizeForTesting(heapType);
    }

    wgpu::PipelineLayout MakeBasicPipelineLayout(
        wgpu::Device device,
        std::vector<wgpu::BindGroupLayout> bindingInitializer) const {
        wgpu::PipelineLayoutDescriptor descriptor;

        descriptor.bindGroupLayoutCount = bindingInitializer.size();
        descriptor.bindGroupLayouts = bindingInitializer.data();

        return device.CreatePipelineLayout(&descriptor);
    }

    wgpu::ShaderModule MakeSimpleVSModule() const {
        return utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
        #version 450
        void main() {
            const vec2 pos[3] = vec2[3](vec2(-1.f, 1.f), vec2(1.f, 1.f), vec2(-1.f, -1.f));
            gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
        })");
    }

    wgpu::ShaderModule MakeFSModule(std::vector<wgpu::BindingType> bindingTypes) const {
        ASSERT(bindingTypes.size() <= kMaxBindGroups);

        std::ostringstream fs;
        fs << R"(
        #version 450
        layout(location = 0) out vec4 fragColor;
        )";

        for (size_t i = 0; i < bindingTypes.size(); ++i) {
            switch (bindingTypes[i]) {
                case wgpu::BindingType::UniformBuffer:
                    fs << "layout (std140, set = " << i << ", binding = 0) uniform UniformBuffer"
                       << i << R"( {
                        vec4 color;
                    } buffer)"
                       << i << ";\n";
                    break;
                case wgpu::BindingType::StorageBuffer:
                    fs << "layout (std430, set = " << i << ", binding = 0) buffer StorageBuffer"
                       << i << R"( {
                        vec4 color;
                    } buffer)"
                       << i << ";\n";
                    break;
                default:
                    UNREACHABLE();
            }
        }

        fs << R"(
        void main() {
            fragColor = vec4(0.0);
        )";
        for (size_t i = 0; i < bindingTypes.size(); ++i) {
            fs << "fragColor += buffer" << i << ".color;\n";
        }
        fs << "}\n";

        return utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment,
                                         fs.str().c_str());
    }

    wgpu::RenderPipeline MakeTestPipeline(const utils::BasicRenderPass& renderPass,
                                          std::vector<wgpu::BindingType> bindingTypes,
                                          std::vector<wgpu::BindGroupLayout> bindGroupLayouts) {
        wgpu::ShaderModule vsModule = MakeSimpleVSModule();
        wgpu::ShaderModule fsModule = MakeFSModule(bindingTypes);

        wgpu::PipelineLayout pipelineLayout = MakeBasicPipelineLayout(device, bindGroupLayouts);

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.layout = pipelineLayout;
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        pipelineDescriptor.cColorStates[0].format = renderPass.colorFormat;
        pipelineDescriptor.cColorStates[0].colorBlend.operation = wgpu::BlendOperation::Add;
        pipelineDescriptor.cColorStates[0].colorBlend.srcFactor = wgpu::BlendFactor::One;
        pipelineDescriptor.cColorStates[0].colorBlend.dstFactor = wgpu::BlendFactor::One;
        pipelineDescriptor.cColorStates[0].alphaBlend.operation = wgpu::BlendOperation::Add;
        pipelineDescriptor.cColorStates[0].alphaBlend.srcFactor = wgpu::BlendFactor::One;
        pipelineDescriptor.cColorStates[0].alphaBlend.dstFactor = wgpu::BlendFactor::One;

        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    Device* mD3DDevice = nullptr;
};

// Verify the shader visible heaps switch over within a single submit.
TEST_P(D3D12DescriptorHeapTests, SwitchOverHeaps) {
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);

    // Fill in a sampler heap with "sampler only" bindgroups (1x sampler per group) by creating a
    // sampler bindgroup each draw. After HEAP_SIZE + 1 draws, the heaps must switch over.
    renderPipelineDescriptor.vertexStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            void main() {
                gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
            })");

    renderPipelineDescriptor.cFragmentStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(#version 450
            layout(set = 0, binding = 0) uniform sampler sampler0;
            layout(location = 0) out vec4 fragColor;
            void main() {
               fragColor = vec4(0.0, 0.0, 0.0, 0.0);
            })");

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    Device* d3dDevice = reinterpret_cast<Device*>(device.Get());
    ShaderVisibleDescriptorAllocator* allocator = d3dDevice->GetShaderVisibleDescriptorAllocator();
    const uint64_t samplerHeapSize =
        allocator->GetShaderVisibleHeapSizeForTesting(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    const Serial heapSerial = allocator->GetShaderVisibleHeapsSerial();

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);

        pass.SetPipeline(renderPipeline);

        for (uint32_t i = 0; i < samplerHeapSize + 1; ++i) {
            pass.SetBindGroup(0, utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                      {{0, sampler}}));
            pass.Draw(3, 1, 0, 0);
        }

        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_EQ(allocator->GetShaderVisibleHeapsSerial(), heapSerial + 1);
}

// Verify shader-visible heaps can be recycled for multiple submits.
TEST_P(D3D12DescriptorHeapTests, PoolHeapsInMultipleSubmits) {
    constexpr D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

    ShaderVisibleDescriptorAllocator* allocator = mD3DDevice->GetShaderVisibleDescriptorAllocator();

    std::list<ComPtr<ID3D12DescriptorHeap>> heaps = {
        allocator->GetShaderVisibleHeapForTesting(heapType)};

    EXPECT_EQ(allocator->GetShaderVisiblePoolSizeForTesting(heapType), 0u);

    // Allocate + Tick() up to |kFrameDepth| and ensure heaps are always unique.
    for (uint32_t i = 0; i < kFrameDepth; i++) {
        EXPECT_TRUE(allocator->AllocateAndSwitchShaderVisibleHeaps().IsSuccess());
        ComPtr<ID3D12DescriptorHeap> heap = allocator->GetShaderVisibleHeapForTesting(heapType);
        EXPECT_TRUE(std::find(heaps.begin(), heaps.end(), heap) == heaps.end());
        heaps.push_back(heap);
        mD3DDevice->Tick();
    }

    // Repeat up to |kFrameDepth| again but ensure heaps are the same in the expected order
    // (oldest heaps are recycled first). The "+ 1" is so we also include the very first heap in the
    // check.
    for (uint32_t i = 0; i < kFrameDepth + 1; i++) {
        EXPECT_TRUE(allocator->AllocateAndSwitchShaderVisibleHeaps().IsSuccess());
        ComPtr<ID3D12DescriptorHeap> heap = allocator->GetShaderVisibleHeapForTesting(heapType);
        EXPECT_TRUE(heaps.front() == heap);
        heaps.pop_front();
        mD3DDevice->Tick();
    }

    EXPECT_TRUE(heaps.empty());
    EXPECT_EQ(allocator->GetShaderVisiblePoolSizeForTesting(heapType), kFrameDepth);
}

// Verify shader-visible heaps do not recycle in a pending submit.
TEST_P(D3D12DescriptorHeapTests, PoolHeapsInPendingSubmit) {
    constexpr D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    constexpr uint32_t kNumOfSwitches = 5;

    ShaderVisibleDescriptorAllocator* allocator = mD3DDevice->GetShaderVisibleDescriptorAllocator();

    const Serial heapSerial = allocator->GetShaderVisibleHeapsSerial();

    std::set<ComPtr<ID3D12DescriptorHeap>> heaps = {
        allocator->GetShaderVisibleHeapForTesting(heapType)};

    EXPECT_EQ(allocator->GetShaderVisiblePoolSizeForTesting(heapType), 0u);

    // Switch-over |kNumOfSwitches| and ensure heaps are always unique.
    for (uint32_t i = 0; i < kNumOfSwitches; i++) {
        EXPECT_TRUE(allocator->AllocateAndSwitchShaderVisibleHeaps().IsSuccess());
        ComPtr<ID3D12DescriptorHeap> heap = allocator->GetShaderVisibleHeapForTesting(heapType);
        EXPECT_TRUE(std::find(heaps.begin(), heaps.end(), heap) == heaps.end());
        heaps.insert(heap);
    }

    // After |kNumOfSwitches|, no heaps are recycled.
    EXPECT_EQ(allocator->GetShaderVisibleHeapsSerial(), heapSerial + kNumOfSwitches);
    EXPECT_EQ(allocator->GetShaderVisiblePoolSizeForTesting(heapType), kNumOfSwitches);
}

// Verify switching shader-visible heaps do not recycle in a pending submit but do so
// once no longer pending.
TEST_P(D3D12DescriptorHeapTests, PoolHeapsInPendingAndMultipleSubmits) {
    constexpr D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    constexpr uint32_t kNumOfSwitches = 5;

    ShaderVisibleDescriptorAllocator* allocator = mD3DDevice->GetShaderVisibleDescriptorAllocator();
    const Serial heapSerial = allocator->GetShaderVisibleHeapsSerial();

    std::set<ComPtr<ID3D12DescriptorHeap>> heaps = {
        allocator->GetShaderVisibleHeapForTesting(heapType)};

    EXPECT_EQ(allocator->GetShaderVisiblePoolSizeForTesting(heapType), 0u);

    // Switch-over |kNumOfSwitches| to create a pool of unique heaps.
    for (uint32_t i = 0; i < kNumOfSwitches; i++) {
        EXPECT_TRUE(allocator->AllocateAndSwitchShaderVisibleHeaps().IsSuccess());
        ComPtr<ID3D12DescriptorHeap> heap = allocator->GetShaderVisibleHeapForTesting(heapType);
        EXPECT_TRUE(std::find(heaps.begin(), heaps.end(), heap) == heaps.end());
        heaps.insert(heap);
    }

    EXPECT_EQ(allocator->GetShaderVisibleHeapsSerial(), heapSerial + kNumOfSwitches);
    EXPECT_EQ(allocator->GetShaderVisiblePoolSizeForTesting(heapType), kNumOfSwitches);

    // Ensure switched-over heaps can be recycled by advancing the GPU by at-least |kFrameDepth|.
    for (uint32_t i = 0; i < kFrameDepth; i++) {
        mD3DDevice->Tick();
    }

    // Switch-over |kNumOfSwitches| again reusing the same heaps.
    for (uint32_t i = 0; i < kNumOfSwitches; i++) {
        EXPECT_TRUE(allocator->AllocateAndSwitchShaderVisibleHeaps().IsSuccess());
        ComPtr<ID3D12DescriptorHeap> heap = allocator->GetShaderVisibleHeapForTesting(heapType);
        EXPECT_TRUE(std::find(heaps.begin(), heaps.end(), heap) != heaps.end());
        heaps.erase(heap);
    }

    // After switching-over |kNumOfSwitches| x 2, ensure no additional heaps exist.
    EXPECT_EQ(allocator->GetShaderVisibleHeapsSerial(), heapSerial + kNumOfSwitches * 2);
    EXPECT_EQ(allocator->GetShaderVisiblePoolSizeForTesting(heapType), kNumOfSwitches);
}

// Verify that encoding many bindgroups then what could fit in a single heap works.
// Test is successful if it does not fail.
TEST_P(D3D12DescriptorHeapTests, EncodeManyBindGroups) {
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);

    renderPipelineDescriptor.vertexStage.module = MakeSimpleVSModule();

    renderPipelineDescriptor.cFragmentStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(#version 450
            layout(set = 0, binding = 0) uniform sampler sampler0;
            layout(location = 0) out vec4 fragColor;
            void main() {
               fragColor = vec4(0.0, 0.0, 0.0, 0.0);
            })");

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    constexpr uint32_t bindingsPerGroup = 1;

    const uint32_t heapSize = GetShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    const uint32_t numOfBindGroupsPerHeap = heapSize / bindingsPerGroup;
    const uint32_t numOfBindGroups = numOfBindGroupsPerHeap;

    std::vector<wgpu::BindGroup> bindGroups;
    for (uint32_t i = 0; i < numOfBindGroups; i++) {
        bindGroups.push_back(
            utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0), {{0, sampler}}));
    }

    // Encode a heap worth of descriptors |numOfHeaps| times.
    constexpr uint32_t numOfHeaps = 2;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);

        pass.SetPipeline(renderPipeline);

        for (uint32_t i = 0; i < numOfHeaps * numOfBindGroupsPerHeap; ++i) {
            pass.SetBindGroup(0, bindGroups[i % numOfBindGroups]);
            pass.Draw(3, 1, 0, 0);
        }

        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
}

// Verify that encoding a few bindgroups but using them many times to fill several heaps works.
// Test is successful if it does not fail.
TEST_P(D3D12DescriptorHeapTests, EncodeFewBindGroupsManyTimes) {
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);

    renderPipelineDescriptor.vertexStage.module = MakeSimpleVSModule();

    renderPipelineDescriptor.cFragmentStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(#version 450
            layout(set = 0, binding = 0) uniform sampler sampler0;
            layout(set = 0, binding = 1) uniform sampler sampler1;
            layout(set = 0, binding = 2) uniform sampler sampler2;
            layout(location = 0) out vec4 fragColor;
            void main() {
               fragColor = vec4(0.0, 0.0, 0.0, 0.0);
            })");

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    constexpr uint32_t numOfBindGroups = 3;

    std::vector<wgpu::BindGroup> bindGroups;
    for (uint32_t i = 0; i < numOfBindGroups; i++) {
        bindGroups.push_back(utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0),
                                                  {
                                                      {0, sampler},
                                                      {1, sampler},
                                                      {2, sampler},
                                                  }));
    }

    // Encode a heap worth of descriptors |numOfHeaps| times.
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);

        pass.SetPipeline(renderPipeline);

        constexpr uint32_t bindingsPerGroup = 3;
        constexpr uint32_t numOfHeaps = 5;

        const uint32_t heapSize = GetShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        const uint32_t bindGroupsPerHeap = heapSize / bindingsPerGroup;

        for (uint32_t i = 0; i < numOfHeaps * bindGroupsPerHeap; ++i) {
            pass.SetBindGroup(0, bindGroups[i % numOfBindGroups]);
            pass.Draw(3, 1, 0, 0);
        }

        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
}

// Verify submitting one bindgroup then submitting a heaps worth of bindgroups still works.
TEST_P(D3D12DescriptorHeapTests, EncodeSingleAndManyBindGroups) {
    utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);

    renderPipelineDescriptor.vertexStage.module = MakeSimpleVSModule();

    renderPipelineDescriptor.cFragmentStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(#version 450
            layout(set = 0, binding = 0) uniform sampler sampler0;
            layout(location = 0) out vec4 fragColor;
            void main() {
               fragColor = vec4(0.0, 0.0, 0.0, 0.0);
            })");

    wgpu::RenderPipeline renderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);
    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0), {{0, sampler}});

    // Encode a single descriptor and submit.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);

            pass.SetPipeline(renderPipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.Draw(3, 1, 0, 0);
            pass.EndPass();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    // Encode a heap worth of descriptors |numOfHeaps| times.
    {
        constexpr uint32_t bindingsPerGroup = 1;

        const uint32_t heapSize = GetShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        const uint32_t numOfBindGroupsPerHeap = heapSize / bindingsPerGroup;

        std::vector<wgpu::BindGroup> bindGroups;
        for (uint32_t i = 0; i < numOfBindGroupsPerHeap; i++) {
            bindGroups.push_back(
                utils::MakeBindGroup(device, renderPipeline.GetBindGroupLayout(0), {{0, sampler}}));
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);

            pass.SetPipeline(renderPipeline);

            constexpr uint32_t numOfHeaps = 2;
            for (uint32_t i = 0; i < numOfHeaps * numOfBindGroupsPerHeap; ++i) {
                pass.SetBindGroup(0, bindGroups[i % numOfBindGroupsPerHeap]);
                pass.Draw(3, 1, 0, 0);
            }

            pass.EndPass();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }
}

// Verify encoding bindgroups with multiple submits works.
TEST_P(D3D12DescriptorHeapTests, EncodeBindGroupOverMultipleSubmits) {
    constexpr uint32_t bindingsPerGroup = 1;

    const uint32_t heapSize = GetShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const uint32_t numOfBindGroups = heapSize / bindingsPerGroup;

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    // Create a bind group layout which uses a single uniform buffer.
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

    // Create a pipeline that uses the uniform bind group layout.
    wgpu::RenderPipeline pipeline =
        MakeTestPipeline(renderPass, {wgpu::BindingType::UniformBuffer}, {layout});

    // Encode a heap worth of descriptors plus one more.
    {
        std::array<float, 4> blackColor = {0, 0, 0, 1};
        wgpu::Buffer uniformBuffer = utils::CreateBufferFromData(
            device, &blackColor, sizeof(blackColor), wgpu::BufferUsage::Uniform);

        std::vector<wgpu::BindGroup> bindGroups;
        for (uint32_t i = 0; i < numOfBindGroups; i++) {
            bindGroups.push_back(
                utils::MakeBindGroup(device, layout, {{0, uniformBuffer, 0, sizeof(blackColor)}}));
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);

            pass.SetPipeline(pipeline);

            for (uint32_t i = 0; i < numOfBindGroups + 1; ++i) {
                pass.SetBindGroup(0, bindGroups[i % numOfBindGroups]);
                pass.Draw(3, 1, 0, 0);
            }

            pass.EndPass();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    // Encode a bindgroup again to overwrite the first descriptor.
    {
        std::array<float, 4> redColor = {1, 0, 0, 1};
        wgpu::Buffer uniformBuffer = utils::CreateBufferFromData(
            device, &redColor, sizeof(redColor), wgpu::BufferUsage::Uniform);

        wgpu::BindGroup firstBindGroup =
            utils::MakeBindGroup(device, layout, {{0, uniformBuffer, 0, sizeof(redColor)}});

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);

            pass.SetPipeline(pipeline);

            pass.SetBindGroup(0, firstBindGroup);
            pass.Draw(3, 1, 0, 0);

            pass.EndPass();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    // Make sure |firstBindGroup| was encoded correctly.
    EXPECT_PIXEL_RGBA8_EQ(RGBA8::kRed, renderPass.color, 0, 0);
}

DAWN_INSTANTIATE_TEST(D3D12DescriptorHeapTests,
                      D3D12Backend({"use_d3d12_small_shader_visible_heap"}));
