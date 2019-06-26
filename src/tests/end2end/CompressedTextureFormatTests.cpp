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

#include "tests/DawnTest.h"

#include "common/Assert.h"
#include "common/Constants.h"
#include "common/Math.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

// Create a 2D texture for sampling in the tests.
dawn::Texture Create2DSampledTexture(dawn::Device device,
                                     dawn::TextureFormat format,
                                     uint32_t width,
                                     uint32_t height,
                                     uint32_t arrayLayerCount = 1,
                                     uint32_t mipLevelCount = 1) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.format = format;
    descriptor.size.width = width;
    descriptor.size.height = height;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = arrayLayerCount;
    descriptor.sampleCount = 1;
    descriptor.mipLevelCount = mipLevelCount;
    descriptor.usage = dawn::TextureUsageBit::Sampled | dawn::TextureUsageBit::TransferDst;
    return device.CreateTexture(&descriptor);
}

// The helper struct to configure the copies between buffers and textures.
struct CopyConfig {
    dawn::TextureFormat format;
    uint32_t textureWidthLevel0;
    uint32_t textureHeightLevel0;
    dawn::Extent3D copyExtent3D;
    dawn::Origin3D copyOrigin3D = {0, 0, 0};
    uint32_t arrayLayerCount = 1;
    uint32_t mipmapLevelCount = 1;
    uint32_t baseMipmapLevel = 0;
    uint32_t baseArrayLayer = 0;
    uint32_t bufferOffset = 0;
    uint32_t rowPitchAlignment = kTextureRowPitchAlignment;
};

class CompressedTextureBCFormatTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        mBindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, dawn::ShaderStageBit::Fragment, dawn::BindingType::Sampler},
                     {1, dawn::ShaderStageBit::Fragment, dawn::BindingType::SampledTexture}});
    }

    // Copy the compressed texture data into the destination texture as is specified in copyConfig.
    void CopyDataIntoCompressedTexture(dawn::Texture bcCompressedTexture,
                                       const CopyConfig& copyConfig) {
        // Compute the upload buffer size with rowPitchAlignment and the copy region.
        uint32_t actualWidthAtLevel = copyConfig.textureWidthLevel0 >> copyConfig.baseMipmapLevel;
        uint32_t actualHeightAtLevel = copyConfig.textureHeightLevel0 >> copyConfig.baseMipmapLevel;
        uint32_t copyWidthInBlockAtLevel =
            (actualWidthAtLevel + kBCBlockWidthInTexels - 1) / kBCBlockWidthInTexels;
        uint32_t copyHeightInBlockAtLevel =
            (actualHeightAtLevel + kBCBlockHeightInTexels - 1) / kBCBlockHeightInTexels;
        uint32_t bufferRowPitchInBytes = 0;
        if (copyConfig.rowPitchAlignment != 0) {
            bufferRowPitchInBytes = copyConfig.rowPitchAlignment;
        } else {
            bufferRowPitchInBytes =
                copyWidthInBlockAtLevel * CompressedFormatBlockSizeInBytes(copyConfig.format);
        }
        uint32_t uploadBufferSize = bufferRowPitchInBytes * copyHeightInBlockAtLevel;

        // Fill uploadData with the pre-prepared one-block compressed texture data.
        std::vector<uint8_t> uploadData(uploadBufferSize, 0);
        std::vector<uint8_t> oneBlockCompressedTextureData =
            GetOneBlockBCFormatTextureData(copyConfig.format);
        for (uint32_t h = 0; h < copyHeightInBlockAtLevel; ++h) {
            for (uint32_t w = 0; w < copyWidthInBlockAtLevel; ++w) {
                uint32_t uploadBufferOffset = copyConfig.bufferOffset + bufferRowPitchInBytes * h +
                                              oneBlockCompressedTextureData.size() * w;
                std::memcpy(&uploadData[uploadBufferOffset], oneBlockCompressedTextureData.data(),
                            oneBlockCompressedTextureData.size() * sizeof(uint8_t));
            }
        }

        // Copy texture data from a staging buffer to the destination texture.
        dawn::Buffer stagingBuffer = utils::CreateBufferFromData(
            device, uploadData.data(), uploadBufferSize, dawn::BufferUsageBit::TransferSrc);
        dawn::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(
            stagingBuffer, copyConfig.bufferOffset, copyConfig.rowPitchAlignment, 0);
        dawn::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(bcCompressedTexture, copyConfig.baseMipmapLevel,
                                         copyConfig.baseArrayLayer, copyConfig.copyOrigin3D);

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copyConfig.copyExtent3D);
        dawn::CommandBuffer copy = encoder.Finish();
        queue.Submit(1, &copy);
    }

    // Create the bind group that includes a BC texture and a sampler.
    dawn::BindGroup CreateBindGroupForTest(dawn::Texture bcCompressedTexture,
                                           dawn::TextureFormat bcFormat,
                                           uint32_t baseArrayLayer = 0,
                                           uint32_t baseMipLevel = 0) {
        dawn::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
        samplerDesc.minFilter = dawn::FilterMode::Nearest;
        samplerDesc.magFilter = dawn::FilterMode::Nearest;
        dawn::Sampler sampler = device.CreateSampler(&samplerDesc);

        dawn::TextureViewDescriptor textureViewDescriptor;
        textureViewDescriptor.format = bcFormat;
        textureViewDescriptor.dimension = dawn::TextureViewDimension::e2D;
        textureViewDescriptor.baseMipLevel = baseMipLevel;
        textureViewDescriptor.baseArrayLayer = baseArrayLayer;
        textureViewDescriptor.arrayLayerCount = 1;
        textureViewDescriptor.mipLevelCount = 1;
        dawn::TextureView bcTextureView = bcCompressedTexture.CreateView(&textureViewDescriptor);

        return utils::MakeBindGroup(device, mBindGroupLayout, {{0, sampler}, {1, bcTextureView}});
    }

    // Create a render pipeline for sampling from a BC texture and rendering into the render target.
    dawn::RenderPipeline CreateRenderPipelineForTest() {
        dawn::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &mBindGroupLayout);

        utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
        dawn::ShaderModule vsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
            #version 450
            layout(location=0) out vec2 texCoord;
            void main() {
                const vec2 pos[3] = vec2[3](
                    vec2(-3.0f, -1.0f),
                    vec2( 3.0f, -1.0f),
                    vec2( 0.0f,  2.0f)
                );
                gl_Position = vec4(pos[gl_VertexIndex], 0.0f, 1.0f);
                texCoord = gl_Position.xy / 2.0f + vec2(0.5f);
            })");
        dawn::ShaderModule fsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
            #version 450
            layout(set = 0, binding = 0) uniform sampler sampler0;
            layout(set = 0, binding = 1) uniform texture2D texture0;
            layout(location = 0) in vec2 texCoord;
            layout(location = 0) out vec4 fragColor;

            void main() {
                fragColor = texture(sampler2D(texture0, sampler0), texCoord);
            })");
        renderPipelineDescriptor.cVertexStage.module = vsModule;
        renderPipelineDescriptor.cFragmentStage.module = fsModule;
        renderPipelineDescriptor.layout = pipelineLayout;
        renderPipelineDescriptor.cColorStates[0]->format =
            utils::BasicRenderPass::kDefaultColorFormat;
        return device.CreateRenderPipeline(&renderPipelineDescriptor);
    }

    // Run the given render pipeline and bind group and verify the pixels in the render target.
    void VerifyCompressedTexturePixelValues(dawn::RenderPipeline renderPipeline,
                                            dawn::BindGroup bindGroup,
                                            uint32_t renderTargetWidth,
                                            uint32_t renderTargetHeight,
                                            const dawn::Origin3D& expectedOrigin,
                                            const dawn::Extent3D& expectedExtent,
                                            const std::vector<RGBA8>& expected) {
        ASSERT(expected.size() == renderTargetWidth * renderTargetHeight);
        utils::BasicRenderPass renderPass =
            utils::CreateBasicRenderPass(device, renderTargetWidth, renderTargetHeight);

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
            pass.SetPipeline(renderPipeline);
            pass.SetBindGroup(0, bindGroup, 0, nullptr);
            pass.Draw(6, 1, 0, 0);
            pass.EndPass();
        }

        dawn::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        EXPECT_TEXTURE_RGBA8_EQ(expected.data(), renderPass.color, expectedOrigin.x,
                                expectedOrigin.y, expectedExtent.width, expectedExtent.height, 0,
                                0);
    }

    // Run the tests that copies pre-prepared BC format data into a BC texture and verifies if we
    // can render correctly with the pixel values sampled from the BC texture.
    void TestCopyRegionIntoBCFormatTextures(const CopyConfig& config) {
        dawn::Texture bcTexture = Create2DSampledTexture(
            device, config.format, config.textureWidthLevel0, config.textureHeightLevel0,
            config.arrayLayerCount, config.mipmapLevelCount);
        CopyDataIntoCompressedTexture(bcTexture, config);

        dawn::BindGroup bindGroup = CreateBindGroupForTest(
            bcTexture, config.format, config.baseArrayLayer, config.baseMipmapLevel);
        dawn::RenderPipeline renderPipeline = CreateRenderPipelineForTest();

        uint32_t noPaddingWidthAtLevel = config.textureWidthLevel0 >> config.baseMipmapLevel;
        uint32_t noPaddingHeightAtLevel = config.textureHeightLevel0 >> config.baseMipmapLevel;

        // The copy region may exceed the subresource size because of the required paddings for BC
        // blocks, so we should limit the size of the expectedData to make it match the real size
        // of the render target.
        dawn::Extent3D noPaddingExtent3D = config.copyExtent3D;
        if (config.copyOrigin3D.x + config.copyExtent3D.width > noPaddingWidthAtLevel) {
            noPaddingExtent3D.width = noPaddingWidthAtLevel - config.copyOrigin3D.x;
        }
        if (config.copyOrigin3D.y + config.copyExtent3D.height > noPaddingHeightAtLevel) {
            noPaddingExtent3D.height = noPaddingHeightAtLevel - config.copyOrigin3D.y;
        }

        std::vector<RGBA8> expectedData =
            GetExpectedData(config.format, noPaddingWidthAtLevel, noPaddingHeightAtLevel);
        VerifyCompressedTexturePixelValues(renderPipeline, bindGroup, noPaddingWidthAtLevel,
                                           noPaddingHeightAtLevel, config.copyOrigin3D,
                                           noPaddingExtent3D, expectedData);
    }

    // Return the BC block size in bytes.
    // TODO(jiawei.shao@intel.com): support all BC formats.
    static uint32_t CompressedFormatBlockSizeInBytes(dawn::TextureFormat format) {
        switch (format) {
            case dawn::TextureFormat::BC5RGSnorm:
            case dawn::TextureFormat::BC5RGUnorm:
                return 16;
            default:
                UNREACHABLE();
                return 0;
        }
    }

    // Return the pre-prepared one-block BC texture data.
    // TODO(jiawei.shao@intel.com): prepare texture data for all BC formats.
    static std::vector<uint8_t> GetOneBlockBCFormatTextureData(dawn::TextureFormat bcFormat) {
        switch (bcFormat) {
            // The expected data represents 4x4 pixel images with the left side red and the right
            // side green and was encoded with DirectXTex from Microsoft.
            case dawn::TextureFormat::BC5RGSnorm:
                return {0x7f, 0x81, 0x40, 0x2,  0x24, 0x40, 0x2,  0x24,
                        0x7f, 0x81, 0x9,  0x90, 0x0,  0x9,  0x90, 0x0};
            case dawn::TextureFormat::BC5RGUnorm:
                return {0xff, 0x0, 0x40, 0x2,  0x24, 0x40, 0x2,  0x24,
                        0xff, 0x0, 0x9,  0x90, 0x0,  0x9,  0x90, 0x0};
            default:
                UNREACHABLE();
                return {};
        }
    }

    // Return the texture data that is decoded from the result of GetOneBlockBCFormatTextureData in
    // RGBA8 formats.
    // TODO(jiawei.shao@intel.com): prepare texture data for all BC formats.
    static std::vector<RGBA8> GetExpectedData(dawn::TextureFormat bcFormat,
                                              uint32_t textureWidth,
                                              uint32_t textureHeight) {
        switch (bcFormat) {
            case dawn::TextureFormat::BC5RGSnorm:
            case dawn::TextureFormat::BC5RGUnorm:
                return FillExpectedDataWithPureRedAndPureGreen(textureWidth, textureHeight);
            default:
                UNREACHABLE();
                return {};
        }
    }

    // Get one kind of expected data that is filled with pure green and pure red.
    static std::vector<RGBA8> FillExpectedDataWithPureRedAndPureGreen(uint32_t textureWidth,
                                                                      uint32_t textureHeight) {
        constexpr RGBA8 kRed(255, 0, 0, 255);
        constexpr RGBA8 kGreen(0, 255, 0, 255);

        std::vector<RGBA8> expectedData(textureWidth * textureHeight, kRed);
        for (uint32_t y = 0; y < textureHeight; ++y) {
            for (uint32_t x = 0; x < textureWidth; ++x) {
                if (x % kBCBlockWidthInTexels >= kBCBlockWidthInTexels / 2) {
                    expectedData[textureWidth * y + x] = kGreen;
                }
            }
        }
        return expectedData;
    }

    // TODO(jiawei.shao@intel.com): support all BC formats.
    const std::array<dawn::TextureFormat, 2> kBCFormats = {dawn::TextureFormat::BC5RGSnorm,
                                                           dawn::TextureFormat::BC5RGUnorm};
    // Tthe block width and height in texels are 4 for all BC formats.
    static constexpr uint32_t kBCBlockWidthInTexels = 4;
    static constexpr uint32_t kBCBlockHeightInTexels = 4;

    dawn::BindGroupLayout mBindGroupLayout;
};

// Test copying into the whole BC texture with 2x2 blocks and sampling from it.
TEST_P(CompressedTextureBCFormatTest, Basic) {
    CopyConfig config;
    config.textureWidthLevel0 = 8;
    config.textureHeightLevel0 = 8;
    config.copyExtent3D = {config.textureWidthLevel0, config.textureHeightLevel0, 1};

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        TestCopyRegionIntoBCFormatTextures(config);
    }
}

// Test copying into a sub-region of a texture with BC formats works correctly.
TEST_P(CompressedTextureBCFormatTest, CopyIntoSubRegion) {
    CopyConfig config;
    config.textureHeightLevel0 = 8;
    config.textureWidthLevel0 = 8;
    config.rowPitchAlignment = kTextureRowPitchAlignment;

    const dawn::Origin3D kOrigin = {4, 4, 0};
    const dawn::Extent3D kExtent3D = {4, 4, 1};
    config.copyOrigin3D = kOrigin;
    config.copyExtent3D = kExtent3D;

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        TestCopyRegionIntoBCFormatTextures(config);
    }
}

// Test using rowPitch == 0 in the copies with BC formats works correctly.
TEST_P(CompressedTextureBCFormatTest, CopyWithZeroRowPitch) {
    CopyConfig config;
    config.textureHeightLevel0 = 8;

    config.rowPitchAlignment = 0;

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        config.textureWidthLevel0 = kTextureRowPitchAlignment /
                                    CompressedFormatBlockSizeInBytes(config.format) *
                                    kBCBlockWidthInTexels;
        config.copyExtent3D = {config.textureWidthLevel0, config.textureHeightLevel0, 1};
        TestCopyRegionIntoBCFormatTextures(config);
    }
}

// Test copying into the non-zero layer of a 2D array texture with BC formats works correctly.
TEST_P(CompressedTextureBCFormatTest, CopyIntoNonZeroArrayLayer) {
    CopyConfig config;
    config.textureHeightLevel0 = 8;
    config.textureWidthLevel0 = 8;
    config.copyExtent3D = {config.textureWidthLevel0, config.textureHeightLevel0, 1};
    config.rowPitchAlignment = kTextureRowPitchAlignment;

    constexpr uint32_t kArrayLayerCount = 3;
    config.arrayLayerCount = kArrayLayerCount;
    config.baseArrayLayer = kArrayLayerCount - 1;

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        TestCopyRegionIntoBCFormatTextures(config);
    }
}

// Test copying into a non-zero mipmap level of a texture with BC texture formats.
TEST_P(CompressedTextureBCFormatTest, CopyIntoNonZeroMipmapLevel) {
    CopyConfig config;
    config.textureHeightLevel0 = 60;
    config.textureWidthLevel0 = 60;
    config.rowPitchAlignment = kTextureRowPitchAlignment;

    constexpr uint32_t kMipmapLevelCount = 3;
    config.mipmapLevelCount = kMipmapLevelCount;
    config.baseMipmapLevel = kMipmapLevelCount - 1;

    // The actual size of the texture at mipmap level == 2 is not a multiple of 4, paddings are
    // required in the copies.
    const uint32_t kActualWidthAtLevel = config.textureWidthLevel0 >> config.baseMipmapLevel;
    const uint32_t kActualHeightAtLevel = config.textureHeightLevel0 >> config.baseMipmapLevel;
    ASSERT(kActualWidthAtLevel % kBCBlockWidthInTexels != 0);
    ASSERT(kActualHeightAtLevel % kBCBlockHeightInTexels != 0);

    const uint32_t kCopyWidthAtLevel = (kActualWidthAtLevel + kBCBlockWidthInTexels - 1) /
                                       kBCBlockWidthInTexels * kBCBlockWidthInTexels;
    const uint32_t kCopyHeightAtLevel = (kActualHeightAtLevel + kBCBlockHeightInTexels - 1) /
                                        kBCBlockHeightInTexels * kBCBlockHeightInTexels;

    config.copyExtent3D = {kCopyWidthAtLevel, kCopyHeightAtLevel, 1};

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        TestCopyRegionIntoBCFormatTextures(config);
    }
}

// TODO(jiawei.shao@intel.com): support BC formats on D3D12, Metal and OpenGL backend
DAWN_INSTANTIATE_TEST(CompressedTextureBCFormatTest, VulkanBackend);
