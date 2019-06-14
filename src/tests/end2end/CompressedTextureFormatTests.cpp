// Copyright 2018 The Dawn Authors
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

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

constexpr dawn::TextureFormat kColorFormat = dawn::TextureFormat::R8G8B8A8Unorm;

dawn::Texture Create2DTexture(dawn::Device device,
                              dawn::TextureFormat format,
                              uint32_t width,
                              uint32_t height,
                              dawn::TextureUsageBit usage) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = width;
    descriptor.size.height = height;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.mipLevelCount = 1;
    descriptor.format = format;
    descriptor.usage = usage;
    return device.CreateTexture(&descriptor);
}

class BCCompressedTextureFormatsTest : public DawnTest {
  protected:
    // Initialize the compressed texture by filling in 4 compressed texture data blocks (2 blocks
    // in one row)
    void fillDataIntoCompressedTexture(const std::vector<uint8_t>& compressedTextureData,
                                       dawn::Texture bcCompressedTexture) {
        uint32_t uploadBufferSize = kTextureRowPitchAlignment * (kTextureHeight / kBCBlockHeight);
        std::vector<uint8_t> uploadData(uploadBufferSize);
        for (uint32_t h = 0; h < kTextureHeight / kBCBlockHeight; ++h) {
            for (uint32_t w = 0; w < kTextureWidth / kBCBlockWidth; ++w) {
                for (uint32_t index = 0; index < compressedTextureData.size(); ++index) {
                    uint32_t indexInUploadBuffer =
                        kTextureRowPitchAlignment * h + compressedTextureData.size() * w + index;
                    uploadData[indexInUploadBuffer] = compressedTextureData[index];
                }
            }
        }

        dawn::Buffer stagingBuffer = utils::CreateBufferFromData(
            device, uploadData.data(), uploadBufferSize, dawn::BufferUsageBit::TransferSrc);
        dawn::BufferCopyView bufferCopyView =
            utils::CreateBufferCopyView(stagingBuffer, 0, kTextureRowPitchAlignment, 0);
        dawn::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(bcCompressedTexture, 0, 0, {0, 0, 0});
        dawn::Extent3D copySize = {kTextureWidth, kTextureHeight, 1};

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);

        dawn::CommandBuffer copy = encoder.Finish();
        queue.Submit(1, &copy);
    }

    void init(dawn::TextureFormat bcFormat, const std::vector<uint8_t>& compressedTextureData) {
        dawn::Texture bcCompressedTexture =
            Create2DTexture(device, bcFormat, kTextureWidth, kTextureHeight,
                            dawn::TextureUsageBit::Sampled | dawn::TextureUsageBit::TransferDst);
        fillDataIntoCompressedTexture(compressedTextureData, bcCompressedTexture);

        dawn::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
        samplerDesc.minFilter = dawn::FilterMode::Nearest;
        samplerDesc.magFilter = dawn::FilterMode::Nearest;
        dawn::Sampler sampler = device.CreateSampler(&samplerDesc);

        dawn::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, dawn::ShaderStageBit::Fragment, dawn::BindingType::Sampler},
                     {1, dawn::ShaderStageBit::Fragment, dawn::BindingType::SampledTexture}});

        bindGroup = utils::MakeBindGroup(
            device, bindGroupLayout, {{0, sampler}, {1, bcCompressedTexture.CreateDefaultView()}});

        dawn::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &bindGroupLayout);

        utils::ComboRenderPipelineDescriptor descriptor(device);
        const char* vertexShader = R"(
            #version 450
            layout (location = 0) out vec2 o_texCoord;
            void main() {
                const vec2 pos[6] = vec2[6](vec2(-1.f, -1.f),
                                            vec2(-1.f,  1.f),
                                            vec2( 1.f, -1.f),
                                            vec2(-1.f,  1.f),
                                            vec2( 1.f, -1.f),
                                            vec2( 1.f,  1.f));
                const vec2 texCoord[6] = vec2[6](vec2(0.f, 0.f),
                                                 vec2(0.f, 1.f),
                                                 vec2(1.f, 0.f),
                                                 vec2(0.f, 1.f),
                                                 vec2(1.f, 0.f),
                                                 vec2(1.f, 1.f));
                gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
                o_texCoord = texCoord[gl_VertexIndex];
            })";

        const char* fragmentShader = R"(
            #version 450
            layout(set = 0, binding = 0) uniform sampler sampler0;
            layout(set = 0, binding = 1) uniform texture2D texture0;
            layout(location = 0) in vec2 texCoord;
            layout(location = 0) out vec4 fragColor;

            void main() {
                fragColor = texture(sampler2D(texture0, sampler0), texCoord);
            })";

        dawn::ShaderModule vsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, vertexShader);
        dawn::ShaderModule fsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, fragmentShader);
        descriptor.cVertexStage.module = vsModule;
        descriptor.cFragmentStage.module = fsModule;
        descriptor.layout = pipelineLayout;
        descriptor.cColorStates[0]->format = kColorFormat;
        pipeline = device.CreateRenderPipeline(&descriptor);
    }

    // TODO(jiawei.shao@intel.com): add checks on non-RGB BC formats (sRGB, BC2, BC3, BC4).
    void RunTestForRGBFormats(dawn::TextureFormat bcFormat,
                              const std::vector<uint8_t>& compressedData) {
        init(bcFormat, compressedData);
        utils::BasicRenderPass renderPass =
            utils::CreateBasicRenderPass(device, kTextureWidth, kTextureHeight);

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup, 0, nullptr);
            pass.Draw(6, 1, 0, 0);
            pass.EndPass();
        }

        dawn::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // The data in compressedData represents 4x4 pixel images with the left side red and the
        // right side green and was encoded with DirectXTex from Microsoft. The compressed texture
        // in this test is 8x8 that consists of 4 blocks of compressedData (2 blocks per row).
        constexpr RGBA8 kRed(255, 0, 0, 255);
        constexpr RGBA8 kGreen(0, 255, 0, 255);
        for (uint32_t y = 0; y < kTextureHeight; ++y) {
            for (uint32_t x = 0; x < kTextureWidth; ++x) {
                RGBA8 ref = (x % kBCBlockWidth <= kBCBlockWidth / 2 - 1) ? kRed : kGreen;
                EXPECT_PIXEL_RGBA8_EQ(ref, renderPass.color, x, y);
            }
        }
    }

    static constexpr uint32_t kBCBlockWidth = 4;
    static constexpr uint32_t kBCBlockHeight = 4;
    static constexpr uint32_t kTextureWidth = 8;
    static constexpr uint32_t kTextureHeight = 8;

    dawn::BindGroup bindGroup;
    dawn::RenderPipeline pipeline;
};

TEST_P(BCCompressedTextureFormatsTest, BC1RGBAUnorm) {
    const std::vector<uint8_t> kBC1UnormData = {0x0, 0xf8, 0xe0, 0x7, 0x50, 0x50, 0x50, 0x50};
    RunTestForRGBFormats(dawn::TextureFormat::BC1RGBAUnorm, kBC1UnormData);
}

TEST_P(BCCompressedTextureFormatsTest, BC5RGSnorm) {
    const std::vector<uint8_t> kBC5SNormData = {0x7f, 0x81, 0x40, 0x2,  0x24, 0x40, 0x2,  0x24,
                                                0x7f, 0x81, 0x9,  0x90, 0x0,  0x9,  0x90, 0x0};
    RunTestForRGBFormats(dawn::TextureFormat::BC5RGSnorm, kBC5SNormData);
}

TEST_P(BCCompressedTextureFormatsTest, BC5RGUnorm) {
    const std::vector<uint8_t> kBC5UnormData = {0xff, 0x0, 0x40, 0x2,  0x24, 0x40, 0x2,  0x24,
                                                0xff, 0x0, 0x9,  0x90, 0x0,  0x9,  0x90, 0x0};
    RunTestForRGBFormats(dawn::TextureFormat::BC5RGUnorm, kBC5UnormData);
}

TEST_P(BCCompressedTextureFormatsTest, BC6HSFloat) {
    // The 2nd and 6th byte in the result of DirectXTex encoding is 0x1e, which is incorrect.
     const std::vector<uint8_t> kBC6HSFloatData = {0xe3, 0x1f, 0x0, 0x0,  0x0, 0xe0, 0x1f, 0x0,
                                                  0x0,  0xff, 0x0, 0xff, 0x0, 0xff, 0x0,  0xff};
    RunTestForRGBFormats(dawn::TextureFormat::BC6HRGBSfloat, kBC6HSFloatData);
}

TEST_P(BCCompressedTextureFormatsTest, BC6HUFloat) {
    const std::vector<uint8_t> kBC6HUFloatData = {0xe3, 0x3d, 0x0, 0x0,  0x0, 0xe0, 0x3d, 0x0,
                                                  0x0,  0xff, 0x0, 0xff, 0x0, 0xff, 0x0,  0xff};
    RunTestForRGBFormats(dawn::TextureFormat::BC6HRGBUfloat, kBC6HUFloatData);
}

TEST_P(BCCompressedTextureFormatsTest, BC7Unorm) {
    const std::vector<uint8_t> kBC7UnormData = {0x50, 0x1f, 0xfc, 0xf, 0x0,  0xf0, 0xe3, 0xe1,
                                                0xe1, 0xe1, 0xc1, 0xf, 0xfc, 0xc0, 0xf,  0xfc};
    RunTestForRGBFormats(dawn::TextureFormat::BC7RGBAUnorm, kBC7UnormData);
}

// TODO(jiawei.shao@intel.com): support BC formats on OpenGL backend
DAWN_INSTANTIATE_TEST(BCCompressedTextureFormatsTest, D3D12Backend, MetalBackend, VulkanBackend);
