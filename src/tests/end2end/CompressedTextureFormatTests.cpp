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

#include <array>

constexpr dawn::TextureFormat kColorFormat = dawn::TextureFormat::RGBA8Unorm;

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

struct CopyConfig {
    dawn::TextureFormat format;
    uint32_t textureWidth;
    uint32_t textureHeight;
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
    void CopyDataIntoCompressedTexture(dawn::Texture bcCompressedTexture,
                                       const CopyConfig& copyConfig) {
        uint32_t bufferRowPitchInBytes = 0;
        if (copyConfig.rowPitchAlignment != 0) {
            bufferRowPitchInBytes = copyConfig.rowPitchAlignment;
        } else {
            bufferRowPitchInBytes = (copyConfig.textureWidth / kBCBlockWidthInTexels) *
                                    CompressedFormatBlockSizeInBytes(copyConfig.format);
        }

        uint32_t uploadBufferSize =
            bufferRowPitchInBytes * (copyConfig.textureHeight / kBCBlockHeightInTexels);

        const std::vector<uint8_t>& oneBlockCompressedTextureData =
            GetOneBlockBCFormatTextureData(copyConfig.format);
        std::vector<uint8_t> uploadData(uploadBufferSize, 0);
        for (uint32_t h = 0; h < copyConfig.textureHeight / kBCBlockHeightInTexels; ++h) {
            for (uint32_t w = 0; w < copyConfig.textureWidth / kBCBlockWidthInTexels; ++w) {
                for (uint32_t index = 0; index < oneBlockCompressedTextureData.size(); ++index) {
                    uint32_t indexInUploadBuffer = copyConfig.bufferOffset +
                                                   bufferRowPitchInBytes * h +
                                                   oneBlockCompressedTextureData.size() * w + index;
                    uploadData[indexInUploadBuffer] = oneBlockCompressedTextureData[index];
                }
            }
        }

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

    void Init(dawn::Texture bcCompressedTexture,
              dawn::TextureFormat bcFormat,
              uint32_t baseArrayLayer = 0,
              uint32_t baseMipLevel = 0) {
        dawn::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
        samplerDesc.minFilter = dawn::FilterMode::Nearest;
        samplerDesc.magFilter = dawn::FilterMode::Nearest;
        dawn::Sampler sampler = device.CreateSampler(&samplerDesc);

        dawn::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, dawn::ShaderStageBit::Fragment, dawn::BindingType::Sampler},
                     {1, dawn::ShaderStageBit::Fragment, dawn::BindingType::SampledTexture}});

        dawn::TextureViewDescriptor textureViewDescriptor;
        textureViewDescriptor.format = bcFormat;
        textureViewDescriptor.dimension = dawn::TextureViewDimension::e2D;
        textureViewDescriptor.baseMipLevel = baseMipLevel;
        textureViewDescriptor.baseArrayLayer = baseArrayLayer;
        textureViewDescriptor.arrayLayerCount = 1;
        textureViewDescriptor.mipLevelCount = 1;
        dawn::TextureView bcTextureView = bcCompressedTexture.CreateView(&textureViewDescriptor);

        mBindGroup =
            utils::MakeBindGroup(device, bindGroupLayout, {{0, sampler}, {1, bcTextureView}});

        dawn::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &bindGroupLayout);

        utils::ComboRenderPipelineDescriptor renderPipelineDescriptor(device);
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
        renderPipelineDescriptor.cVertexStage.module = vsModule;
        renderPipelineDescriptor.cFragmentStage.module = fsModule;
        renderPipelineDescriptor.layout = pipelineLayout;
        renderPipelineDescriptor.cColorStates[0]->format = kColorFormat;
        mRenderPipeline = device.CreateRenderPipeline(&renderPipelineDescriptor);
    }

    void Verify(uint32_t renderTargetWidth,
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
            pass.SetPipeline(mRenderPipeline);
            pass.SetBindGroup(0, mBindGroup, 0, nullptr);
            pass.Draw(6, 1, 0, 0);
            pass.EndPass();
        }

        dawn::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        for (uint32_t y = expectedOrigin.y; y < expectedOrigin.y + 1; ++y) {
            for (uint32_t x = expectedOrigin.x; x < expectedOrigin.x + expectedExtent.width; ++x) {
                RGBA8 expectedColor = expected[renderTargetWidth * y + x];
                EXPECT_PIXEL_RGBA8_EQ(expectedColor, renderPass.color, x, y);
            }
        }
    }

    void VerifyCopyRegion(const CopyConfig& config) {
        dawn::Texture bcTexture = Create2DSampledTexture(
            device, config.format, config.textureWidth, config.textureHeight,
            config.arrayLayerCount, config.mipmapLevelCount);
        Init(bcTexture, config.format, config.baseArrayLayer, config.baseMipmapLevel);
        CopyDataIntoCompressedTexture(bcTexture, config);

        std::vector<RGBA8> expectedData =
            GetExpectedData(config.format, config.textureWidth, config.textureHeight);
        Verify(config.textureWidth, config.textureHeight, config.copyOrigin3D,
               config.copyExtent3D, expectedData);
    }

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

    static std::vector<RGBA8> FillExpectedDataWithPureRedAndPureGreen(uint32_t textureWidth,
                                                                      uint32_t textureHeight) {
        std::vector<RGBA8> expectedData(textureWidth * textureHeight);

        // The expected data represents 4x4 pixel images with the left side red and the right side
        // green and was encoded with DirectXTex from Microsoft.
        constexpr RGBA8 kRed(255, 0, 0, 255);
        constexpr RGBA8 kGreen(0, 255, 0, 255);
        for (uint32_t y = 0; y < textureHeight; ++y) {
            for (uint32_t x = 0; x < textureWidth; ++x) {
                expectedData[textureWidth * y + x] =
                    (x % kBCBlockWidthInTexels <= kBCBlockWidthInTexels / 2 - 1) ? kRed :
                        kGreen;
            }
        }

        return expectedData;
    }

    const std::array<dawn::TextureFormat, 2> kBCFormats =
        {dawn::TextureFormat::BC5RGSnorm, dawn::TextureFormat::BC5RGUnorm};

    static constexpr uint32_t kBCBlockWidthInTexels = 4;
    static constexpr uint32_t kBCBlockHeightInTexels = 4;
    dawn::BindGroup mBindGroup;
    dawn::RenderPipeline mRenderPipeline;
};

// Test the basic usage of BC formats.
TEST_P(CompressedTextureBCFormatTest, Basic) {
    CopyConfig config;
    config.textureWidth = 8;
    config.textureHeight = 8;
    config.copyExtent3D = {config.textureWidth, config.textureHeight, 1};

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        VerifyCopyRegion(config);
    }
}

// Test copying into a sub-region of a texture with BC formats works correctly.
TEST_P(CompressedTextureBCFormatTest, CopyIntoSubRegion) {
    CopyConfig config;
    config.textureHeight = 8;
    config.textureWidth = 8;
    config.rowPitchAlignment = kTextureRowPitchAlignment;

    const dawn::Origin3D kOrigin = {4, 4, 0};
    const dawn::Extent3D kExtent3D = {4, 4, 1};
    config.copyOrigin3D = kOrigin;
    config.copyExtent3D = kExtent3D;

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        VerifyCopyRegion(config);
    }
}

// Test using rowPitch == 0 in the copies with BC formats works correctly.
TEST_P(CompressedTextureBCFormatTest, CopyWithZeroRowPitch) {
    CopyConfig config;
    config.textureHeight = 8;

    config.rowPitchAlignment = 0;

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        config.textureWidth = kTextureRowPitchAlignment /
                              CompressedFormatBlockSizeInBytes(config.format) *
                              kBCBlockWidthInTexels;
        config.copyExtent3D = {config.textureWidth, config.textureHeight, 1};

        VerifyCopyRegion(config);
    }
}

// Test copying into the non-zero layer of a 2D array texture with BC formats works correctly.
TEST_P(CompressedTextureBCFormatTest, CopyIntoNonZeroArrayLayer) {
    CopyConfig config;
    config.textureHeight = 8;
    config.textureWidth = 8;
    config.copyExtent3D = {config.textureWidth, config.textureHeight, 1};
    config.rowPitchAlignment = kTextureRowPitchAlignment;

    constexpr uint32_t kArrayLayerCount = 3;
    config.arrayLayerCount = kArrayLayerCount;
    config.baseArrayLayer = kArrayLayerCount - 1;

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        VerifyCopyRegion(config);
    }
}

// Test copying into a non-zero mipmap level of a texture with BC texture formats.
TEST_P(CompressedTextureBCFormatTest, CopyIntoNonZeroMipmapLevel) {
    CopyConfig config;
    config.textureHeight = 60;
    config.textureWidth = 60;
    config.rowPitchAlignment = kTextureRowPitchAlignment;

    constexpr uint32_t kMipmapLevelCount = 3;
    config.mipmapLevelCount = kMipmapLevelCount;
    config.baseMipmapLevel = kMipmapLevelCount - 1;

    const uint32_t kActualWidthAtLevel = config.textureWidth >> config.baseMipmapLevel;
    const uint32_t kActualHeightAtLevel = config.textureHeight >> config.baseMipmapLevel;
    ASSERT(kActualWidthAtLevel % kBCBlockWidthInTexels != 0);
    ASSERT(kActualHeightAtLevel % kBCBlockHeightInTexels != 0);

    const uint32_t kCopyWidthAtLevel = (kActualWidthAtLevel + kBCBlockWidthInTexels - 1) /
                                       kBCBlockWidthInTexels * kBCBlockWidthInTexels;
    const uint32_t kCopyHeightAtLevel = (kActualHeightAtLevel + kBCBlockHeightInTexels - 1) /
                                        kBCBlockHeightInTexels * kBCBlockHeightInTexels;

    config.copyExtent3D = {kCopyWidthAtLevel, kCopyHeightAtLevel, 1};

    for (dawn::TextureFormat format : kBCFormats) {
        config.format = format;
        dawn::Texture bcTexture = Create2DSampledTexture(
            device, config.format, config.textureWidth, config.textureHeight, 1, kMipmapLevelCount);

        Init(bcTexture, config.format, config.baseArrayLayer, config.baseMipmapLevel);
        CopyDataIntoCompressedTexture(bcTexture, config);

        std::vector<RGBA8> expectedData =
            GetExpectedData(config.format, kActualWidthAtLevel, kActualHeightAtLevel);
        Verify(kActualWidthAtLevel, kActualHeightAtLevel, {0, 0, 0},
               {kActualWidthAtLevel, kActualHeightAtLevel, 1}, expectedData);
    }
}

// TODO(jiawei.shao@intel.com): support BC formats on D3D12, Metal and OpenGL backend
DAWN_INSTANTIATE_TEST(CompressedTextureBCFormatTest, VulkanBackend);
