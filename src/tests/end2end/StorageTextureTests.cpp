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

#include "common/Assert.h"
#include "common/Constants.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class StorageTextureTests : public DawnTest {
  public:
    wgpu::Texture CreateTexture(wgpu::TextureFormat format, wgpu::TextureUsage usage) {
        wgpu::TextureDescriptor descriptor;
        descriptor.size = {kWidth, kHeight, 1};
        descriptor.format = format;
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

    wgpu::Buffer CreateEmptyBufferForTextureCopy(uint32_t texelSize) {
        const size_t uploadBufferSize =
            kTextureRowPitchAlignment * (kHeight - 1) + kWidth * texelSize;
        wgpu::BufferDescriptor descriptor;
        descriptor.size = uploadBufferSize;
        descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        return device.CreateBuffer(&descriptor);
    }

    wgpu::Buffer CreateBufferForTextureCopy(const std::vector<uint32_t>& initialTextureData,
                                            uint32_t texelSize) {
        ASSERT(kWidth * texelSize <= kTextureRowPitchAlignment);
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

    void EncodeUploadingDataIntoTexture(wgpu::CommandEncoder encoder,
                                        wgpu::Texture texture,
                                        const std::vector<uint32_t>& initialTextureData,
                                        uint32_t texelSize) {
        wgpu::Buffer uploadBuffer = CreateBufferForTextureCopy(initialTextureData, texelSize);
        wgpu::BufferCopyView bufferCopyView =
            utils::CreateBufferCopyView(uploadBuffer, 0, kTextureRowPitchAlignment, 0);
        wgpu::TextureCopyView textureCopyView;
        textureCopyView.texture = texture;
        wgpu::Extent3D copyExtent = {kWidth, kHeight, 1};
        encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copyExtent);
    }

    wgpu::ComputePipeline CreateComputePipeline(const char* computeShader) {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, computeShader);
        wgpu::ComputePipelineDescriptor computeDescriptor;
        computeDescriptor.layout = nullptr;
        computeDescriptor.computeStage.module = csModule;
        computeDescriptor.computeStage.entryPoint = "main";
        return device.CreateComputePipeline(&computeDescriptor);
    }

    wgpu::RenderPipeline CreateRenderPipeline(const char* vertexShader,
                                              const char* fragmentShader) {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, vertexShader);
        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, fragmentShader);

        utils::ComboRenderPipelineDescriptor desc(device);
        desc.vertexStage.module = vsModule;
        desc.cFragmentStage.module = fsModule;
        desc.cColorStates[0].format = kOutputAttachmentFormat;
        return device.CreateRenderPipeline(&desc);
    }

    void ValidateOutputAttachment(wgpu::CommandEncoder encoder,
                                  wgpu::RenderPipeline pipeline,
                                  wgpu::BindGroup bindGroup) {
        wgpu::Texture outputTexture =
            CreateTexture(kOutputAttachmentFormat,
                          wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc);
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

    static constexpr size_t kWidth = 4u;
    static constexpr size_t kHeight = 4u;
    static constexpr wgpu::TextureFormat kOutputAttachmentFormat = wgpu::TextureFormat::RGBA8Unorm;
};

// Test that using read-only storage texture and write-only storage texture in BindGroupLayout is
// valid on all backends. This test is a regression test for chromium:1061156 and passes by not
// asserting or crashing.
TEST_P(StorageTextureTests, BindGroupLayoutWithStorageTextureBindingType) {
    // wgpu::BindingType::ReadonlyStorageTexture is a valid binding type to create a bind group
    // layout.
    {
        wgpu::BindGroupLayoutEntry entry = {0, wgpu::ShaderStage::Compute,
                                            wgpu::BindingType::ReadonlyStorageTexture};
        entry.storageTextureFormat = wgpu::TextureFormat::R32Float;
        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.entryCount = 1;
        descriptor.entries = &entry;
        device.CreateBindGroupLayout(&descriptor);
    }

    // wgpu::BindingType::WriteonlyStorageTexture is a valid binding type to create a bind group
    // layout.
    {
        wgpu::BindGroupLayoutEntry entry = {0, wgpu::ShaderStage::Compute,
                                            wgpu::BindingType::WriteonlyStorageTexture};
        entry.storageTextureFormat = wgpu::TextureFormat::R32Float;
        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.entryCount = 1;
        descriptor.entries = &entry;
        device.CreateBindGroupLayout(&descriptor);
    }
}

// Test that read-only storage textures are supported in compute shader.
TEST_P(StorageTextureTests, ReadonlyStorageTextureInComputeShader) {
    // TODO(jiawei.shao@intel.com): support read-only storage texture on D3D12, Vulkan and OpenGL.
    DAWN_SKIP_TEST_IF(IsD3D12() || IsVulkan() || IsOpenGL());

    // TODO(jiawei.shao@intel.com): test more texture formats
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    std::vector<uint32_t> kInitialBufferData = GetExpectedData();

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    // Prepare the read-only storage texture and fill it with the expected data.
    wgpu::Texture readonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopyDst);
    EncodeUploadingDataIntoTexture(encoder, readonlyStorageTexture, kInitialBufferData,
                                   kTexelSizeR32Uint);

    // Prepare the reference storage buffer and fill it with the expected data.
    wgpu::Buffer compareBuffer = utils::CreateBufferFromData(
        device, kInitialBufferData.data(), kInitialBufferData.size() * sizeof(uint32_t),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);

    // Create a compute shader that reads the value from both the read-only storage texture and the
    // read-only storage buffer and if they are equal then write '1' to the result buffer.
    const char* computeShader = R"(
            #version 450
            layout (set = 0, binding = 0, r32ui) uniform readonly uimage2D srcImage;
            layout (set = 0, binding = 1, std430) readonly buffer SrcBuffer {
                uint inputs[];
            } srcBuffer;
            layout (set = 1, binding = 0, std430) buffer DstBuffer {
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
            })";

    wgpu::ComputePipeline pipeline = CreateComputePipeline(computeShader);
    wgpu::BindGroup inputBindGroup =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                             {{0, readonlyStorageTexture.CreateView()}, {1, compareBuffer}});

    // Clear the content of the result buffer into 0.
    std::array<uint32_t, kWidth * kHeight> initialBufferData;
    initialBufferData.fill(0);
    wgpu::Buffer resultBuffer =
        utils::CreateBufferFromData(device, initialBufferData.data(), sizeof(initialBufferData),
                                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);

    wgpu::BindGroup outputBindGroup =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(1), {{0, resultBuffer}});

    wgpu::ComputePassEncoder computeEncoder = encoder.BeginComputePass();
    computeEncoder.SetBindGroup(0, inputBindGroup);
    computeEncoder.SetBindGroup(1, outputBindGroup);
    computeEncoder.SetPipeline(pipeline);
    computeEncoder.Dispatch(1);
    computeEncoder.EndPass();

    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    // Check if the contents in the result buffer are what we expect.
    std::array<uint32_t, kWidth * kHeight> resultData;
    resultData.fill(1);
    EXPECT_BUFFER_U32_RANGE_EQ(resultData.data(), resultBuffer, 0, resultData.size());
}

// Test that read-only storage textures are supported in vertex shader.
TEST_P(StorageTextureTests, ReadonlyStorageTextureInVertexShader) {
    // TODO(jiawei.shao@intel.com): support read-only storage texture on D3D12, Vulkan and OpenGL.
    DAWN_SKIP_TEST_IF(IsD3D12() || IsVulkan() || IsOpenGL());

    // When we run dawn_end2end_tests with "--use-spvc-parser", extracting the binding type of a
    // read-only image will always return shaderc_spvc_binding_type_writeonly_storage_texture.
    // TODO(jiawei.shao@intel.com): enable this test when we specify "--use-spvc-parser" after the
    // bug in spvc parser is fixed.
    DAWN_SKIP_TEST_IF(IsSpvcParserBeingUsed());

    // TODO(jiawei.shao@intel.com): test more texture formats
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    const std::vector<uint32_t> kInitialTextureData = GetExpectedData();

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    // Prepare the read-only storage texture and fill it with the expected data.
    wgpu::Texture readonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopyDst);
    EncodeUploadingDataIntoTexture(encoder, readonlyStorageTexture, kInitialTextureData,
                                   kTexelSizeR32Uint);

    // Prepare the sampled texture and fill it with the expected data.
    wgpu::Texture sampledTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Sampled | wgpu::TextureUsage::CopyDst);
    EncodeUploadingDataIntoTexture(encoder, sampledTexture, kInitialTextureData, kTexelSizeR32Uint);

    // Create a rendering pipeline that reads the value from both the read-only storage texture and
    // the sampled texture and if they are equal then use green as the output color, otherwise use
    // red as the output color.
    const char* vertexShader = R"(
            #version 450
            layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D srcImage;
            layout(set = 0, binding = 1) uniform utexture2D srcSampledTexture;
            layout(set = 0, binding = 2) uniform sampler mySampler;
            layout(location = 0) out vec4 o_color;
            void main() {
                const vec2 pos[6] = vec2[6](vec2(-2.f, -2.f),
                                            vec2(-2.f,  2.f),
                                            vec2( 2.f, -2.f),
                                            vec2(-2.f,  2.f),
                                            vec2( 2.f, -2.f),
                                            vec2( 2.f,  2.f));
                gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);

                if (gl_VertexIndex < 4u) {
                    for (uint i = 0u; i < 4u; ++i) {
                       ivec2 texcoord = ivec2(gl_VertexIndex, i);
                       uvec4 valueFromStorageTexture = imageLoad(srcImage, texcoord);
                       uvec4 valueFromSampledTexture =
                           texelFetch(usampler2D(srcSampledTexture, mySampler), texcoord, 0);
                       if (valueFromStorageTexture != valueFromSampledTexture) {
                           o_color = vec4(1.f, 0.f, 0.f, 1.f);
                           return;
                       }
                    }
                    o_color = vec4(0.f, 1.f, 0.f, 1.f);
                } else {
                    o_color = vec4(0.f, 1.f, 0.f, 1.f);
                }
            })";
    const char* fragmentShader = R"(
            #version 450
            layout(location = 0) in vec4 o_color;
            layout(location = 0) out vec4 fragColor;
            void main() {
                fragColor = o_color;
            })";

    wgpu::SamplerDescriptor descriptor;
    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {{0, readonlyStorageTexture.CreateView()},
                                                      {1, sampledTexture.CreateView()},
                                                      {2, device.CreateSampler(&descriptor)}});
    ValidateOutputAttachment(encoder, pipeline, bindGroup);
}

// Test that read-only storage textures are supported in fragment shader.
TEST_P(StorageTextureTests, ReadonlyStorageTextureInFragmentShader) {
    // TODO(jiawei.shao@intel.com): support read-only storage texture on D3D12, Vulkan and OpenGL.
    DAWN_SKIP_TEST_IF(IsD3D12() || IsVulkan() || IsOpenGL());

    // When we run dawn_end2end_tests with "--use-spvc-parser", extracting the binding type of a
    // read-only image will always return shaderc_spvc_binding_type_writeonly_storage_texture.
    // TODO(jiawei.shao@intel.com): enable this test when we specify "--use-spvc-parser" after the
    // bug in spvc parser is fixed.
    DAWN_SKIP_TEST_IF(IsSpvcParserBeingUsed());

    // TODO(jiawei.shao@intel.com): test more texture formats
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    const std::vector<uint32_t> kInitialTextureData = GetExpectedData();

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    // Prepare the read-only storage texture and fill it with the expected data.
    wgpu::Texture readonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopyDst);
    EncodeUploadingDataIntoTexture(encoder, readonlyStorageTexture, kInitialTextureData,
                                   kTexelSizeR32Uint);

    // Prepare the sampled texture and fill it with the expected data.
    wgpu::Texture sampledTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Sampled | wgpu::TextureUsage::CopyDst);
    EncodeUploadingDataIntoTexture(encoder, sampledTexture, kInitialTextureData, kTexelSizeR32Uint);

    // Create a rendering pipeline that reads the value from both the read-only storage texture and
    // the sampled texture and if they are equal then use green as the output color, otherwise use
    // red as the output color.
    const char* vertexShader = R"(
            #version 450
            void main() {
                const vec2 pos[6] = vec2[6](vec2(-2.f, -2.f),
                                            vec2(-2.f,  2.f),
                                            vec2( 2.f, -2.f),
                                            vec2(-2.f,  2.f),
                                            vec2( 2.f, -2.f),
                                            vec2( 2.f,  2.f));
                gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
            })";
    const char* fragmentShader = R"(
            #version 450
            layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D srcImage;
            layout(set = 0, binding = 1) uniform utexture2D srcSampledTexture;
            layout(set = 0, binding = 2) uniform sampler mySampler;
            layout(location = 0) out vec4 fragColor;

            void main() {
                uvec4 valueFromStorageTexture = imageLoad(srcImage, ivec2(gl_FragCoord));
                uvec4 valueFromSampledTexture =
                    texelFetch(usampler2D(srcSampledTexture, mySampler), ivec2(gl_FragCoord), 0);
                if (valueFromStorageTexture == valueFromSampledTexture) {
                    fragColor = vec4(0.f, 1.f, 0.f, 1.f);
                } else {
                    fragColor = vec4(1.f, 0.f, 0.f, 1.f);
                }
            })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    wgpu::SamplerDescriptor descriptor;
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {{0, readonlyStorageTexture.CreateView()},
                                                      {1, sampledTexture.CreateView()},
                                                      {2, device.CreateSampler(&descriptor)}});
    ValidateOutputAttachment(encoder, pipeline, bindGroup);
}

// Test that write-only storage textures are supported in compute shader.
TEST_P(StorageTextureTests, WriteonlyStorageTextureInComputeShader) {
    // TODO(jiawei.shao@intel.com): support read-only storage texture on D3D12, Vulkan and OpenGL.
    DAWN_SKIP_TEST_IF(IsD3D12() || IsVulkan() || IsOpenGL());

    // TODO(jiawei.shao@intel.com): test more texture formats
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    const std::vector<uint32_t> kInitialBufferData = GetExpectedData();

    // Prepare the read-only storage buffer and fill it with the expected data.
    wgpu::Buffer readonlyStorageBuffer = utils::CreateBufferFromData(
        device, kInitialBufferData.data(), kInitialBufferData.size() * sizeof(uint32_t),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);

    // Prepare the write-only storage texture.
    wgpu::Texture writeonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopySrc);

    // Create a compute shader that reads the value from the read-only storage buffer and write it
    // into the storage texture.
    const char* computeShader = R"(
            #version 450
            layout (set = 0, binding = 0, std430) readonly buffer SrcBuffer {
                uint inputs[];
            } srcBuffer;
            layout (set = 0, binding = 1, r32ui) uniform writeonly uimage2D dstImage;
            void main() {
                const uint kWidth = 4;
                const uint kHeight = 4;
                for (uint y = 0; y < kHeight; ++y) {
                    for (uint x = 0; x < kWidth; ++x) {
                        uint bufferIndex = x + y * kWidth;
                        uint value = srcBuffer.inputs[bufferIndex];
                        imageStore(dstImage, ivec2(x, y), uvec4(value));
                    }
                }
            })";

    wgpu::ComputePipeline pipeline = CreateComputePipeline(computeShader);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(
        device, pipeline.GetBindGroupLayout(0),
        {{0, readonlyStorageBuffer}, {1, writeonlyStorageTexture.CreateView()}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder computePassEncoder = encoder.BeginComputePass();
    computePassEncoder.SetBindGroup(0, bindGroup);
    computePassEncoder.SetPipeline(pipeline);
    computePassEncoder.Dispatch(1);
    computePassEncoder.EndPass();

    // Copy the content from the write-only storage texture to the result buffer.
    wgpu::Buffer resultBuffer = CreateEmptyBufferForTextureCopy(kTexelSizeR32Uint);
    wgpu::BufferCopyView bufferCopyView =
        utils::CreateBufferCopyView(resultBuffer, 0, kTextureRowPitchAlignment, 0);
    wgpu::TextureCopyView textureCopyView;
    textureCopyView.texture = writeonlyStorageTexture;
    wgpu::Extent3D copyExtent = {kWidth, kHeight, 1};
    encoder.CopyTextureToBuffer(&textureCopyView, &bufferCopyView, &copyExtent);

    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    queue.Submit(1, &commandBuffer);

    // Check if the contents in the result buffer are what we expect.
    for (size_t y = 0; y < kHeight; ++y) {
        const size_t resultBufferOffset = kTextureRowPitchAlignment * y;
        EXPECT_BUFFER_U32_RANGE_EQ(kInitialBufferData.data() + kWidth * y, resultBuffer,
                                   resultBufferOffset, kWidth);
    }
}

DAWN_INSTANTIATE_TEST(StorageTextureTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
