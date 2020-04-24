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
    // TODO(jiawei.shao@intel.com): support all formats that can be used in storage textures.
    static std::vector<uint32_t> GetExpectedData() {
        constexpr size_t kDataCount = kWidth * kHeight;
        std::vector<uint32_t> outputData(kDataCount);
        for (size_t i = 0; i < kDataCount; ++i) {
            outputData[i] = static_cast<uint32_t>(i + 1u);
        }
        return outputData;
    }

    wgpu::Texture CreateTexture(wgpu::TextureFormat format,
                                wgpu::TextureUsage usage,
                                uint32_t width = kWidth,
                                uint32_t height = kHeight) {
        wgpu::TextureDescriptor descriptor;
        descriptor.size = {width, height, 1};
        descriptor.format = format;
        descriptor.usage = usage;
        return device.CreateTexture(&descriptor);
    }

    wgpu::Buffer CreateEmptyBufferForTextureCopy(uint32_t texelSize) {
        ASSERT(kWidth * texelSize <= kTextureRowPitchAlignment);
        const size_t uploadBufferSize =
            kTextureRowPitchAlignment * (kHeight - 1) + kWidth * texelSize;
        wgpu::BufferDescriptor descriptor;
        descriptor.size = uploadBufferSize;
        descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        return device.CreateBuffer(&descriptor);
    }

    // TODO(jiawei.shao@intel.com): support all formats that can be used in storage textures.
    wgpu::Texture CreateTextureWithTestData(const std::vector<uint32_t>& initialTextureData,
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
        wgpu::Buffer uploadBuffer =
            utils::CreateBufferFromData(device, uploadBufferData.data(), uploadBufferSize,
                                        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

        wgpu::Texture outputTexture =
            CreateTexture(wgpu::TextureFormat::R32Uint,
                          wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopyDst);

        wgpu::BufferCopyView bufferCopyView =
            utils::CreateBufferCopyView(uploadBuffer, 0, kTextureRowPitchAlignment, 0);
        wgpu::TextureCopyView textureCopyView;
        textureCopyView.texture = outputTexture;
        wgpu::Extent3D copyExtent = {kWidth, kHeight, 1};

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copyExtent);
        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        queue.Submit(1, &commandBuffer);

        return outputTexture;
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
        desc.primitiveTopology = wgpu::PrimitiveTopology::PointList;
        return device.CreateRenderPipeline(&desc);
    }

    void CheckDrawsGreen(const char* vertexShader,
                         const char* fragmentShader,
                         wgpu::Texture readonlyStorageTexture) {
        wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(
            device, pipeline.GetBindGroupLayout(0), {{0, readonlyStorageTexture.CreateView()}});
        wgpu::Texture outputTexture =
            CreateTexture(kOutputAttachmentFormat,
                          wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc, 1, 1);
        utils::ComboRenderPassDescriptor renderPassDescriptor({outputTexture.CreateView()});
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = encoder.BeginRenderPass(&renderPassDescriptor);
        renderPassEncoder.SetBindGroup(0, bindGroup);
        renderPassEncoder.SetPipeline(pipeline);
        renderPassEncoder.Draw(1);
        renderPassEncoder.EndPass();

        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        queue.Submit(1, &commandBuffer);

        // Check if the contents in the output texture are all as expected (green).
        std::array<RGBA8, kWidth * kHeight> expected;
        expected.fill(RGBA8(0, 255, 0, 255));
        EXPECT_TEXTURE_RGBA8_EQ(expected.data(), outputTexture, 0, 0, 1, 1, 0, 0);
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

    // Prepare the read-only storage texture and fill it with the expected data.
    // TODO(jiawei.shao@intel.com): test more texture formats.
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    std::vector<uint32_t> kInitialTextureData = GetExpectedData();
    wgpu::Texture readonlyStorageTexture =
        CreateTextureWithTestData(kInitialTextureData, kTexelSizeR32Uint);

    // Create a compute shader that reads the pixels from both the read-only storage texture and if
    // they are equal to the expected values then write '1' to the result buffer.
    const char* computeShader = R"(
            #version 450
            layout (set = 0, binding = 0, r32ui) uniform readonly uimage2D srcImage;
            layout (set = 0, binding = 1, std430) buffer DstBuffer {
                uint results[];
            } dstBuffer;
            uvec4[16] GetExpectedData() {
                uvec4 expected[16];
                for (uint i = 0; i < 16; ++i) {
                    expected[i] = uvec4(i + 1, 0, 0, 1u);
                }
                return expected;
            }
            void main() {
                uvec4 expected[16] = GetExpectedData();
                for (uint y = 0; y < 4; ++y) {
                    for (uint x = 0; x < 4; ++x) {
                        uvec4 pixel = imageLoad(srcImage, ivec2(x, y));
                        uint bufferIndex = x + y * 4;
                        if (expected[bufferIndex] == pixel) {
                            dstBuffer.results[bufferIndex] = 1;
                        } else {
                            dstBuffer.results[bufferIndex] = 0;
                        }
                    }
                }
            })";

    wgpu::ComputePipeline pipeline = CreateComputePipeline(computeShader);

    // Clear the content of the result buffer into 0.
    std::array<uint32_t, kWidth * kHeight> initialBufferData;
    initialBufferData.fill(0);
    wgpu::Buffer resultBuffer =
        utils::CreateBufferFromData(device, initialBufferData.data(), sizeof(initialBufferData),
                                    wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
    wgpu::BindGroup bindGroup =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                             {{0, readonlyStorageTexture.CreateView()}, {1, resultBuffer}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder computeEncoder = encoder.BeginComputePass();
    computeEncoder.SetBindGroup(0, bindGroup);
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

    // Prepare the read-only storage texture and fill it with the expected data.
    // TODO(jiawei.shao@intel.com): test more texture formats
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    const std::vector<uint32_t> kInitialTextureData = GetExpectedData();
    wgpu::Texture readonlyStorageTexture =
        CreateTextureWithTestData(kInitialTextureData, kTexelSizeR32Uint);

    // Create a rendering pipeline that reads the pixels from both the read-only storage texture and
    // if they are equal to the expected values then use green as the output color, otherwise use
    // red as the output color.
    const char* vertexShader = R"(
            #version 450
            layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D srcImage;
            layout(location = 0) out vec4 o_color;
            uvec4[16] GetExpectedData() {
                uvec4 expected[16];
                for (uint i = 0; i < 16; ++i) {
                    expected[i] = uvec4(i + 1, 0, 0, 1);
                }
                return expected;
            }
            void main() {
                gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
                uvec4 expected[16] = GetExpectedData();
                for (uint y = 0; y < 4; ++y) {
                    for (uint x = 0; x < 4; ++x) {
                        uvec4 pixel = imageLoad(srcImage, ivec2(x, y));
                        uint bufferIndex = x + y * 4;
                        if (expected[bufferIndex] != pixel) {
                            o_color = vec4(1.f, 0.f, 0.f, 1.f);
                            return;
                        }
                    }
                }
                o_color = vec4(0.f, 1.f, 0.f, 1.f);
            })";
    const char* fragmentShader = R"(
            #version 450
            layout(location = 0) in vec4 o_color;
            layout(location = 0) out vec4 fragColor;
            void main() {
                fragColor = o_color;
            })";
    CheckDrawsGreen(vertexShader, fragmentShader, readonlyStorageTexture);
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

    // Prepare the read-only storage texture and fill it with the expected data.
    // TODO(jiawei.shao@intel.com): test more texture formats
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    const std::vector<uint32_t> kInitialTextureData = GetExpectedData();
    wgpu::Texture readonlyStorageTexture =
        CreateTextureWithTestData(kInitialTextureData, kTexelSizeR32Uint);

    // Create a rendering pipeline that reads the pixels from both the read-only storage texture and
    // if they are equal to the expected values then use green as the output color, otherwise use
    // red as the output color.
    const char* vertexShader = R"(
            #version 450
            void main() {
                gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
            })";
    const char* fragmentShader = R"(
            #version 450
            layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D srcImage;
            layout(location = 0) out vec4 o_color;
            uvec4[16] GetExpectedData() {
                uvec4 expected[16];
                for (uint i = 0; i < 16; ++i) {
                    expected[i] = uvec4(i + 1, 0, 0, 1);
                }
                return expected;
            }
            void main() {
                uvec4 expected[16] = GetExpectedData();
                for (uint y = 0; y < 4; ++y) {
                    for (uint x = 0; x < 4; ++x) {
                        uvec4 pixel = imageLoad(srcImage, ivec2(x, y));
                        uint bufferIndex = x + y * 4;
                        if (expected[bufferIndex] != pixel) {
                            o_color = vec4(1.f, 0.f, 0.f, 1.f);
                            return;
                        }
                    }
                }
                o_color = vec4(0.f, 1.f, 0.f, 1.f);
            })";
    CheckDrawsGreen(vertexShader, fragmentShader, readonlyStorageTexture);
}

// Test that write-only storage textures are supported in compute shader.
TEST_P(StorageTextureTests, WriteonlyStorageTextureInComputeShader) {
    // TODO(jiawei.shao@intel.com): support read-only storage texture on D3D12, Vulkan and OpenGL.
    DAWN_SKIP_TEST_IF(IsD3D12() || IsVulkan() || IsOpenGL());

    // TODO(jiawei.shao@intel.com): test more texture formats.
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    const std::vector<uint32_t> kInitialTextureData = GetExpectedData();

    // Prepare the write-only storage texture.
    wgpu::Texture writeonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopySrc);

    // Create a compute shader that writes the expected pixel values into the storage texture.
    const char* computeShader = R"(
            #version 450
            layout (set = 0, binding = 0, r32ui) uniform writeonly uimage2D dstImage;
            uvec4[16] GetExpectedData() {
                uvec4 expected[16];
                for (uint i = 0; i < 16; ++i) {
                    expected[i] = uvec4(i + 1, 0, 0, 1);
                }
                return expected;
            }
            void main() {
                uvec4 expected[16] = GetExpectedData();
                for (uint y = 0; y < 4; ++y) {
                    for (uint x = 0; x < 4; ++x) {
                        uint bufferIndex = x + y * 4;
                        uvec4 pixel = expected[bufferIndex];
                        imageStore(dstImage, ivec2(x, y), pixel);
                    }
                }
            })";

    wgpu::ComputePipeline pipeline = CreateComputePipeline(computeShader);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {{0, writeonlyStorageTexture.CreateView()}});

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
        EXPECT_BUFFER_U32_RANGE_EQ(kInitialTextureData.data() + kWidth * y, resultBuffer,
                                   resultBufferOffset, kWidth);
    }
}

// Test that write-only storage textures are supported in fragment shader.
TEST_P(StorageTextureTests, WriteonlyStorageTextureInFragmentShader) {
    // TODO(jiawei.shao@intel.com): support read-only storage texture on D3D12, Vulkan and OpenGL.
    DAWN_SKIP_TEST_IF(IsD3D12() || IsVulkan() || IsOpenGL());

    // TODO(jiawei.shao@intel.com): test more texture formats.
    constexpr uint32_t kTexelSizeR32Uint = 4u;
    const std::vector<uint32_t> kInitialTextureData = GetExpectedData();

    // Prepare the write-only storage texture.
    wgpu::Texture writeonlyStorageTexture = CreateTexture(
        wgpu::TextureFormat::R32Uint, wgpu::TextureUsage::Storage | wgpu::TextureUsage::CopySrc);

    // Create a render pipeline that writes the expected pixel values into the storage texture
    // without fragment shader outputs.
    const char* vertexShader = R"(
            #version 450
            void main() {
                gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
            })";
    const char* fragmentShader = R"(
            #version 450
            layout(set = 0, binding = 0, r32ui) uniform writeonly uimage2D dstImage;
            uvec4[16] GetExpectedData() {
                uvec4 expected[16];
                for (uint i = 0; i < 16; ++i) {
                    expected[i] = uvec4(i + 1, 0, 0, 1);
                }
                return expected;
            }
            void main() {
                uvec4 expected[16] = GetExpectedData();
                for (uint y = 0; y < 4; ++y) {
                    for (uint x = 0; x < 4; ++x) {
                        uint bufferIndex = x + y * 4;
                        uvec4 pixel = expected[bufferIndex];
                        imageStore(dstImage, ivec2(x, y), pixel);
                    }
                }
            })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {{0, writeonlyStorageTexture.CreateView()}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    // TODO(jiawei.shao@intel.com): remove the output attachment when Dawn supports beginning a
    // render pass with no attachments.
    wgpu::Texture dummyOutputTexture =
        CreateTexture(kOutputAttachmentFormat,
                      wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc, 1, 1);
    utils::ComboRenderPassDescriptor renderPassDescriptor({dummyOutputTexture.CreateView()});
    wgpu::RenderPassEncoder renderPassEncoder = encoder.BeginRenderPass(&renderPassDescriptor);
    renderPassEncoder.SetBindGroup(0, bindGroup);
    renderPassEncoder.SetPipeline(pipeline);
    renderPassEncoder.Draw(1);
    renderPassEncoder.EndPass();

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
        EXPECT_BUFFER_U32_RANGE_EQ(kInitialTextureData.data() + kWidth * y, resultBuffer,
                                   resultBufferOffset, kWidth);
    }
}

DAWN_INSTANTIATE_TEST(StorageTextureTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
