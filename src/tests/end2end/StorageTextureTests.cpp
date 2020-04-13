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

#include "common/Constants.h"
#include "utils/WGPUHelpers.h"

class StorageTextureTests : public DawnTest {
  public:
    wgpu::Texture CreateTexture(wgpu::TextureFormat format, wgpu::TextureUsage usage) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {kWidth, kHeight, 1};
        descriptor.arrayLayerCount = 1;
        descriptor.sampleCount = 1;
        descriptor.format = format;
        descriptor.mipLevelCount = 1;
        descriptor.usage = usage;
        return device.CreateTexture(&descriptor);
    }

    std::vector<uint32_t> GetExpectedData() {
        constexpr size_t kDataCount = kWidth * kHeight;
        std::vector<uint32_t> outputData(kDataCount);
        for (size_t i = 0; i < kDataCount; ++i) {
            outputData[i] = static_cast<uint32_t>(i + 1u);
        }
        return outputData;
    }

    wgpu::Buffer CreateBufferForTextureCopy(const std::vector<uint32_t>& initialTextureData) {
        constexpr uint32_t kTexelSizeR32Uint = 4u;
        const size_t uploadBufferSize =
            kTextureRowPitchAlignment * (kHeight - 1) + kWidth * kTexelSizeR32Uint;
        std::vector<uint32_t> uploadBufferData(uploadBufferSize / kTexelSizeR32Uint);

        const size_t texelCountPerRowPitch = kTextureRowPitchAlignment / kTexelSizeR32Uint;
        for (size_t y = 0; y < kHeight; ++y) {
            for (size_t x = 0; x < kWidth; ++x) {
                uint32_t data = initialTextureData[kWidth * y + x];

                size_t indexInUploadBuffer = y * texelCountPerRowPitch + x;
                uploadBufferData[indexInUploadBuffer] = data;
            }
        }
        return utils::CreateBufferFromData(device, uploadBufferData.data(), uploadBufferSize,
                                           wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);
    }

    static constexpr size_t kWidth = 4u;
    static constexpr size_t kHeight = 4u;
};

// Test that using read-only storage texture and write-only storage texture in BindGroupLayout is
// valid on all backends. This test is a regression test for chromium:1061156 and passes by not
// asserting or crashing.
TEST_P(StorageTextureTests, BindGroupLayoutWithStorageTextureBindingType) {
    // wgpu::BindingType::ReadonlyStorageTexture is a valid binding type to create a bind group
    // layout.
    {
        wgpu::BindGroupLayoutEntry binding = {0, wgpu::ShaderStage::Compute,
                                              wgpu::BindingType::ReadonlyStorageTexture};
        binding.storageTextureFormat = wgpu::TextureFormat::R32Float;
        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.bindingCount = 1;
        descriptor.bindings = &binding;
        device.CreateBindGroupLayout(&descriptor);
    }

    // wgpu::BindingType::WriteonlyStorageTexture is a valid binding type to create a bind group
    // layout.
    {
        wgpu::BindGroupLayoutEntry binding = {0, wgpu::ShaderStage::Compute,
                                              wgpu::BindingType::WriteonlyStorageTexture};
        binding.storageTextureFormat = wgpu::TextureFormat::R32Float;
        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.bindingCount = 1;
        descriptor.bindings = &binding;
        device.CreateBindGroupLayout(&descriptor);
    }
}

// Test that read-only storage textures are supported in compute shader.
TEST_P(StorageTextureTests, ReadonlyStorageTextureInComputeShader) {
    // TODO(jiawei.shao@intel.com): support read-only storage texture on D3D12, Vulkan and OpenGL.
    DAWN_SKIP_TEST_IF(IsD3D12() || IsVulkan() || IsOpenGL());

    wgpu::Texture readonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopyDst);

    std::vector<uint32_t> initialTextureData = GetExpectedData();
    wgpu::Buffer uploadBuffer = CreateBufferForTextureCopy(initialTextureData);
    wgpu::BufferCopyView bufferCopyView =
        utils::CreateBufferCopyView(uploadBuffer, 0, kTextureRowPitchAlignment, 0);
    wgpu::TextureCopyView textureCopyView;
    textureCopyView.texture = readonlyStorageTexture;
    wgpu::Extent3D copyExtent = {kWidth, kHeight, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copyExtent);

    wgpu::Buffer compareBuffer =
        utils::CreateBufferFromData(device, initialTextureData.data(), initialTextureData.size(),
                                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);

    std::array<uint32_t, kWidth * kHeight> initialBufferData;
    memset(initialBufferData.data(), 0, initialBufferData.size());
    wgpu::Buffer resultBuffer =
        utils::CreateBufferFromData(device, initialBufferData.data(), initialBufferData.size(),
                                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    wgpu::ShaderModule csModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
            #version 450
            layout (set = 0, binding = 0, r32ui) uniform readonly uimage2D srcImage;
            layout (set = 0, binding = 1, std430) readonly buffer SrcBuffer {
                uint inputs[];
            } srcBuffer;
            layout (set = 0, binding = 2, std430) buffer DstBuffer {
                uint results[];
            } dstBuffer;
            void main() {
                const uint kWidth = 4;
                const uint kHeight = 4;
                for (uint y = 0; y < kHeight; ++y) {
                    for (uint x = 0; x < kWidth; ++x) {
                        uint imageValue = imageLoad(srcImage, ivec2(x, y)).x;
                        uint bufferIndex = x + y * kWidth;
                        if (srcBuffer.inputs[bufferIndex] == imageValue) {
                            dstBuffer.results[bufferIndex] = 1;
                        } else {
                            dstBuffer.results[bufferIndex] = 0;
                        }
                    }
                }
            })");
    wgpu::ComputePipelineDescriptor computeDescriptor;
    computeDescriptor.layout = nullptr;
    computeDescriptor.computeStage.module = csModule;
    computeDescriptor.computeStage.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&computeDescriptor);

    wgpu::BindGroup bindGroup = utils::MakeBindGroup(
        device, pipeline.GetBindGroupLayout(0),
        {{0, readonlyStorageTexture.CreateView()}, {1, compareBuffer}, {2, resultBuffer}});

    wgpu::ComputePassEncoder computeEncoder = encoder.BeginComputePass();
    computeEncoder.SetBindGroup(0, bindGroup);
    computeEncoder.SetPipeline(pipeline);
    computeEncoder.Dispatch(1);
    computeEncoder.EndPass();

    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    std::array<uint32_t, kWidth * kHeight> resultData;
    memset(resultData.data(), 1, resultData.size());

    EXPECT_BUFFER_U32_RANGE_EQ(resultData.data(), resultBuffer, 0, resultData.size());
}

DAWN_INSTANTIATE_TEST(StorageTextureTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
