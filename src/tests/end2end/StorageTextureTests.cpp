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
#include "utils/ComboRenderPipelineDescriptor.h"
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

    wgpu::Buffer CreateBufferForTextureCopy(const std::vector<uint32_t>& initialTextureData,
                                            uint32_t texelSize) {
        const size_t uploadBufferSize =
            kTextureRowPitchAlignment * (kHeight - 1) + kWidth * texelSize;
        std::vector<uint32_t> uploadBufferData(uploadBufferSize / texelSize);

        const size_t texelCountPerRowPitch = kTextureRowPitchAlignment / texelSize;
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

    // TODO(jiawei.shao@intel.com): test more texture formats
    constexpr uint32_t kTexelSizeR32Uint = 4u;

    // Prepare the read-only storage texture and fill it with the expected data.
    wgpu::Texture readonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopyDst);
    std::vector<uint32_t> initialTextureData = GetExpectedData();
    wgpu::Buffer uploadBuffer = CreateBufferForTextureCopy(initialTextureData, kTexelSizeR32Uint);
    wgpu::BufferCopyView bufferCopyView =
        utils::CreateBufferCopyView(uploadBuffer, 0, kTextureRowPitchAlignment, 0);
    wgpu::TextureCopyView textureCopyView;
    textureCopyView.texture = readonlyStorageTexture;
    wgpu::Extent3D copyExtent = {kWidth, kHeight, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copyExtent);

    // Prepare the reference storage buffer and fill it with the expected data.
    wgpu::Buffer compareBuffer = utils::CreateBufferFromData(
        device, initialTextureData.data(), initialTextureData.size() * sizeof(uint32_t),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);

    // Clear the content of the result buffer into 0.
    std::array<uint32_t, kWidth * kHeight> initialBufferData;
    initialBufferData.fill(0);
    wgpu::Buffer resultBuffer =
        utils::CreateBufferFromData(device, initialBufferData.data(), sizeof(initialBufferData),
                                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    // Create a compute shader that reads the value from both the read-only storage texture and the
    // read-only storage buffer and if they are equal then write '1' to the result buffer.
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

    // Check if the contents in the result buffer are all as expected.
    std::array<uint32_t, kWidth * kHeight> resultData;
    resultData.fill(1);
    EXPECT_BUFFER_U32_RANGE_EQ(resultData.data(), resultBuffer, 0, resultData.size());
}

TEST_P(StorageTextureTests, ReadonlyStorageTextureInFragmentShader) {
    // TODO(jiawei.shao@intel.com): support read-only storage texture on D3D12, Vulkan and OpenGL.
    DAWN_SKIP_TEST_IF(IsD3D12() || IsVulkan() || IsOpenGL());

    // TODO(jiawei.shao@intel.com): test more texture formats
    constexpr uint32_t kTexelSizeR32Uint = 4u;

    // Prepare the read-only storage texture and fill it with the expected data.
    wgpu::Texture readonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopyDst);
    std::vector<uint32_t> initialTextureData = GetExpectedData();
    wgpu::Buffer uploadBuffer = CreateBufferForTextureCopy(initialTextureData, kTexelSizeR32Uint);
    wgpu::BufferCopyView bufferCopyView =
        utils::CreateBufferCopyView(uploadBuffer, 0, kTextureRowPitchAlignment, 0);
    wgpu::TextureCopyView storageTextureCopyView;
    storageTextureCopyView.texture = readonlyStorageTexture;
    wgpu::Extent3D copyExtent = {kWidth, kHeight, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &storageTextureCopyView, &copyExtent);

    // Prepare the sampled texture and fill it with the expected data.
    wgpu::Texture sampledTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Sampled | wgpu::TextureUsage::CopyDst);
    wgpu::TextureCopyView sampledTextureCopyView;
    sampledTextureCopyView.texture = sampledTexture;
    encoder.CopyBufferToTexture(&bufferCopyView, &sampledTextureCopyView, &copyExtent);

    // Create a rendering pipeline that reads the value from both the read-only storage texture and
    // the sampled texture and if they are equal then use green as the output color, otherwise use
    // red as the output color.
    wgpu::ShaderModule vsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            void main() {
                const vec2 pos[6] = vec2[6](vec2(-2.f, -2.f),
                                            vec2(-2.f,  2.f),
                                            vec2( 2.f, -2.f),
                                            vec2(-2.f,  2.f),
                                            vec2( 2.f, -2.f),
                                            vec2( 2.f,  2.f));
                gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
            }
        )");
    wgpu::ShaderModule fsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
            #version 450
            layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D srcImage;
            layout(set = 0, binding = 1) uniform utexture2D srcSampledTexture;
            layout(set = 0, binding = 2) uniform sampler mySampler;
            layout(location = 0) out vec4 fragColor;

            void main() {
                uvec4 valueFromStorageTexture = imageLoad(srcImage, ivec2(gl_FragCoord));
                uvec4 valueFromSampledTexture = texelFetch(usampler2D(srcSampledTexture, mySampler), ivec2(gl_FragCoord), 0);
                if (valueFromStorageTexture == valueFromSampledTexture) {
                    fragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
                } else {
                    fragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
                }
            }
        )");

    constexpr wgpu::TextureFormat kOutputFormat = wgpu::TextureFormat::RGBA8Unorm;
    utils::ComboRenderPipelineDescriptor desc(device);
    desc.vertexStage.module = vsModule;
    desc.cFragmentStage.module = fsModule;
    desc.cColorStates[0].format = kOutputFormat;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);

    wgpu::Sampler sampler;
    wgpu::SamplerDescriptor descriptor;
    sampler = device.CreateSampler(&descriptor);

    wgpu::BindGroup bindGroup = utils::MakeBindGroup(
        device, pipeline.GetBindGroupLayout(0),
        {{0, readonlyStorageTexture.CreateView()}, {1, sampledTexture.CreateView()}, {2, sampler}});

    wgpu::Texture outputTexture = CreateTexture(
        kOutputFormat, wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc);
    utils::ComboRenderPassDescriptor renderPassDescriptor({outputTexture.CreateView()});
    wgpu::RenderPassEncoder renderPassEncoder = encoder.BeginRenderPass(&renderPassDescriptor);
    renderPassEncoder.SetBindGroup(0, bindGroup);
    renderPassEncoder.SetPipeline(pipeline);
    renderPassEncoder.Draw(6);
    renderPassEncoder.EndPass();

    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    // Check if the contents in the output texture are all as expected (green).
    std::array<RGBA8, kWidth * kHeight> expected;
    expected.fill(RGBA8(0, 255, 0, 255));
    EXPECT_TEXTURE_RGBA8_EQ(expected.data(), outputTexture, 0, 0, kWidth, kHeight, 0, 0);
}

DAWN_INSTANTIATE_TEST(StorageTextureTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
