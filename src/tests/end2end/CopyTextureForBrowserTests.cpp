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
#include "common/Math.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/TestUtils.h"
#include "utils/TextureFormatUtils.h"
#include "utils/WGPUHelpers.h"

class CopyTextureForBrowserTests : public DawnTest {
  protected:
    static constexpr wgpu::TextureFormat kTextureFormat = wgpu::TextureFormat::RGBA8Unorm;
    static constexpr uint64_t kDstTextureConfigBufferSize = 8;
    static constexpr uint64_t kTransformBufferSize = 16;
    static constexpr uint64_t kDefaultTextureWidth = 900;
    static constexpr uint64_t kDefaultTextureHeight = 1000;

    struct TextureSpec {
        wgpu::Origin3D copyOrigin = {};
        wgpu::Extent3D textureSize = {kDefaultTextureWidth, kDefaultTextureHeight};
        uint32_t level = 0;
        wgpu::TextureFormat format = kTextureFormat;
    };

    static std::vector<RGBA8> GetSourceTextureData(const utils::TextureDataCopyLayout& layout) {
        std::vector<RGBA8> textureData(layout.texelBlockCount);
        for (uint32_t layer = 0; layer < layout.mipSize.depth; ++layer) {
            const uint32_t sliceOffset = layout.texelBlocksPerImage * layer;
            for (uint32_t y = 0; y < layout.mipSize.height; ++y) {
                const uint32_t rowOffset = layout.texelBlocksPerRow * y;
                for (uint32_t x = 0; x < layout.mipSize.width; ++x) {
                    textureData[sliceOffset + rowOffset + x] =
                        RGBA8(static_cast<uint8_t>((x + layer * x) % 256),
                              static_cast<uint8_t>((y + layer * y) % 256),
                              static_cast<uint8_t>(x % 256), static_cast<uint8_t>(y % 256));
                }
            }
        }

        return textureData;
    }

    static uint32_t ChannelNumberOfTextureFormat(wgpu::TextureFormat format) {
        switch (format) {
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::RGB10A2Unorm:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGBA32Float:
                return 4;
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG16Float:
                return 2;
            default:
                UNREACHABLE();
        }
    }

    void SetUp() override {
        DawnTest::SetUp();

        testPipeline = MakeTestPipeline();

        wgpu::BufferDescriptor transformBufferDesc = {};
        transformBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        transformBufferDesc.size = kTransformBufferSize;
        transformBuffer = device.CreateBuffer(&transformBufferDesc);

        wgpu::BufferDescriptor dstTextureConfigBufferDesc = {};
        dstTextureConfigBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        dstTextureConfigBufferDesc.size = kDstTextureConfigBufferSize;
        dstTextureConfigBuffer = device.CreateBuffer(&dstTextureConfigBufferDesc);
    }

    wgpu::RenderPipeline MakeTestPipeline() {
        wgpu::ShaderModule vsModule = utils::CreateShaderModuleFromWGSL(device, R"(
            [[block]] struct Uniforms {
                [[offset(0)]] u_scale : vec2<f32>;
                [[offset(8)]] u_offset : vec2<f32>;
            };
            const texcoord : array<vec2<f32>, 3> = array<vec2<f32>, 3>(
                vec2<f32>(-0.5, 0.0),
                vec2<f32>( 1.5, 0.0),
                vec2<f32>( 0.5, 2.0));
            [[location(0)]] var<out> v_srcTexcoord : vec2<f32>;
            [[location(1)]] var<out> v_dstTexcoord : vec2<f32>;
            [[builtin(position)]] var<out> Position : vec4<f32>;
            [[builtin(vertex_index)]] var<in> VertexIndex : u32;
            [[binding(0), group(0)]] var<uniform> uniforms : Uniforms;
            [[stage(vertex)]] fn main() -> void {
                Position = vec4<f32>((texcoord[VertexIndex] * 2.0 - vec2<f32>(1.0, 1.0)), 0.0, 1.0);

                // Texture coordinate takes top-left as origin point. We need to map the
                // texture to triangle carefully.
                v_srcTexcoord = (texcoord[VertexIndex] * vec2<f32>(1.0, -1.0) + vec2<f32>(0.0, 1.0)) *
                    uniforms.u_scale + uniforms.u_offset;
                
                // For the dst texture, it doesn't need to do flipY-scale ops.
                v_dstTexcoord = texcoord[VertexIndex] * vec2<f32>(1.0, -1.0) + vec2<f32>(0.0, 1.0);
            }
        )");

        wgpu::ShaderModule fsModule = utils::CreateShaderModuleFromWGSL(device, R"(
            [[block]] struct DstFormatConfig {
                [[offset(0)]] dstFormatSwizzled : u32;
                [[offset(4)]] dstFormatChannelNumber: u32;
            };
            [[binding(1), group(0)]] var srcSampler : sampler;
            [[binding(2), group(0)]] var srcTexture : texture_2d<f32>;
            [[binding(3), group(0)]] var dstSampler : sampler;
            [[binding(4), group(0)]] var dstTexture : texture_2d<f32>;
            [[binding(5), group(0)]] var<uniform> dstFormatConfig : DstFormatConfig;
            [[location(0)]] var<in> v_srcTexcoord : vec2<f32>;
            [[location(1)]] var<in> v_dstTexcoord : vec2<f32>;
            [[location(0)]] var<out> outputColor : vec4<f32>;
            [[stage(fragment)]] fn main() -> void {
                var success : bool = true;
                var clampedSrcTexcoord : vec2<f32> =
                    clamp(v_srcTexcoord, vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0));
                var clampedDstTexcoord : vec2<f32> =
                    clamp(v_dstTexcoord, vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0));
                if (all(clampedSrcTexcoord == v_srcTexcoord) &&
                    all(clampedDstTexcoord == v_dstTexcoord)) {
                    var tempColor : vec4<f32> = textureSample(srcTexture, srcSampler, v_srcTexcoord);
                    var dstColor : vec4<f32> = textureSample(dstTexture, dstSampler, v_dstTexcoord);
                    if (dstFormatConfig.dstFormatSwizzled > 0) {
                        // In webgpu, swizzle is specialized to rg<ba> <-> <b>gr<a>;
                        var temp : f32 = tempColor[0];
                        tempColor[0] = tempColor[2];
                        tempColor[2] = temp;
                    }
                    var srcColor : vec4<f32> = tempColor;

                    // The dst color format supported are either 4 channels or 2 channels.
                    // And 2 channles color format are all RG channels.
                    var srcColorBitCast : vec4<u32> = bitcast<vec4<u32>>(srcColor);
                    var dstColorBitCast : vec4<u32> = bitcast<vec4<u32>>(dstColor);
                    if (dstFormatConfig.dstFormatChannelNumber == 2) {
                        success = success &&
                                  (srcColorBitCast[0] == dstColorBitCast[0]) &&
                                  (srcColorBitCast[1] == dstColorBitCast[1]);
                    } else {
                        success = success &&
                                  all(srcColorBitCast == dstColorBitCast);
                    }

                    if (success) {
                        outputColor = vec4<f32>(0.0, 1.0, 0.0, 1.0);
                    } else {
                        outputColor = vec4<f32>(1.0, 0.0, 0.0, 1.0);
                    }
                }
            }
        )");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.vertexStage.module = vsModule;
        descriptor.cFragmentStage.module = fsModule;

        return device.CreateRenderPipeline(&descriptor);
    }

    void DoTest(const TextureSpec& srcSpec,
                const TextureSpec& dstSpec,
                const wgpu::Extent3D& copySize = {kDefaultTextureWidth, kDefaultTextureHeight},
                const wgpu::CopyTextureForBrowserOptions options = {}) {
        wgpu::TextureDescriptor srcDescriptor;
        srcDescriptor.size = srcSpec.textureSize;
        srcDescriptor.format = srcSpec.format;
        srcDescriptor.mipLevelCount = srcSpec.level + 1;
        srcDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment;
        wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

        wgpu::Texture dstTexture;
        wgpu::TextureDescriptor dstDescriptor;
        dstDescriptor.size = dstSpec.textureSize;
        dstDescriptor.format = dstSpec.format;
        dstDescriptor.mipLevelCount = dstSpec.level + 1;
        dstDescriptor.usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                              wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment;
        dstTexture = device.CreateTexture(&dstDescriptor);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        const utils::TextureDataCopyLayout copyLayout =
            utils::GetTextureDataCopyLayoutForTexture2DAtLevel(
                kTextureFormat,
                {srcSpec.textureSize.width, srcSpec.textureSize.height, copySize.depth},
                srcSpec.level);

        const std::vector<RGBA8> textureArrayCopyData = GetSourceTextureData(copyLayout);
        wgpu::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(srcTexture, srcSpec.level, {0, 0, srcSpec.copyOrigin.z});

        wgpu::TextureDataLayout textureDataLayout;
        textureDataLayout.offset = 0;
        textureDataLayout.bytesPerRow = copyLayout.bytesPerRow;
        textureDataLayout.rowsPerImage = copyLayout.rowsPerImage;

        device.GetQueue().WriteTexture(&textureCopyView, textureArrayCopyData.data(),
                                       textureArrayCopyData.size() * sizeof(RGBA8),
                                       &textureDataLayout, &copyLayout.mipSize);

        // Perform the texture to texture copy
        wgpu::TextureCopyView srcTextureCopyView =
            utils::CreateTextureCopyView(srcTexture, srcSpec.level, srcSpec.copyOrigin);
        wgpu::TextureCopyView dstTextureCopyView =
            utils::CreateTextureCopyView(dstTexture, dstSpec.level, dstSpec.copyOrigin);

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        device.GetQueue().CopyTextureForBrowser(&srcTextureCopyView, &dstTextureCopyView, &copySize,
                                                &options);

        // Update uniform buffer based on test config
        float transformBufferData[] = {
            1.0, 1.0,  // scale
            0.0, 0.0   // offset
        };

        if (options.flipY) {
            transformBufferData[1] *= -1.0;
            transformBufferData[3] += 1.0;
        }

        device.GetQueue().WriteBuffer(transformBuffer, 0, transformBufferData,
                                      sizeof(transformBufferData));

        uint32_t dstTextureConfigData[] = {
            dstSpec.format == wgpu::TextureFormat::BGRA8Unorm,  // dstFormatSwizzled
            ChannelNumberOfTextureFormat(dstSpec.format)        // dstFormatChannelNumber
        };
        device.GetQueue().WriteBuffer(dstTextureConfigBuffer, 0, dstTextureConfigData,
                                      sizeof(dstTextureConfigData));

        // Create samplers for test.
        wgpu::SamplerDescriptor textureSamplerDesc = {};
        wgpu::Sampler srcTextureSampler = device.CreateSampler(&textureSamplerDesc);
        wgpu::Sampler dstTextureSampler = device.CreateSampler(&textureSamplerDesc);

        // Create texture views for test.
        wgpu::TextureViewDescriptor srcTextureViewDesc = {};
        srcTextureViewDesc.baseMipLevel = srcSpec.level;
        srcTextureViewDesc.mipLevelCount = 1;
        wgpu::TextureView srcTextureView = srcTexture.CreateView(&srcTextureViewDesc);

        wgpu::TextureViewDescriptor dstTextureViewDesc = {};
        dstTextureViewDesc.baseMipLevel = dstSpec.level;
        dstTextureViewDesc.mipLevelCount = 1;
        wgpu::TextureView dstTextureView = dstTexture.CreateView(&dstTextureViewDesc);

        // Create bind group based on the config.
        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, testPipeline.GetBindGroupLayout(0),
                                 {{0, transformBuffer, 0, kTransformBufferSize},
                                  {1, srcTextureSampler},
                                  {2, srcTextureView},
                                  {3, dstTextureSampler},
                                  {4, dstTextureView},
                                  {5, dstTextureConfigBuffer, 0, kDstTextureConfigBufferSize}});

        // Start a pipeline to check pixel value in bit form.
        wgpu::CommandEncoder testEncoder = device.CreateCommandEncoder();

        renderPass = utils::CreateBasicRenderPass(device, dstSpec.textureSize.width,
                                                  dstSpec.textureSize.height);

        wgpu::RenderPassEncoder pass = testEncoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(testPipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.Draw(3);
        pass.EndPass();
        wgpu::CommandBuffer testCommands = testEncoder.Finish();
        queue.Submit(1, &testCommands);

        EXPECT_PIXEL_RGBA8_EQ(RGBA8::kGreen, renderPass.color, 0, 0);
    }

    utils::BasicRenderPass renderPass;
    wgpu::Buffer dstTextureConfigBuffer;  // Uniform buffer to store dst texture meta info.
    wgpu::Buffer
        transformBuffer;  // Uniform buffer to transform source texture to get correct coords.
    wgpu::RenderPipeline testPipeline;
};

// Verify CopyTextureForBrowserTests works with internal pipeline.
// The case do copy without any transform.
TEST_P(CopyTextureForBrowserTests, PassthroughCopy) {
    // Tests skip due to crbug.com/dawn/592.
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    constexpr uint32_t kWidth = 10;
    constexpr uint32_t kHeight = 1;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight};

    DoTest(textureSpec, textureSpec, {kWidth, kHeight});
}

TEST_P(CopyTextureForBrowserTests, VerifyCopyOnXDirection) {
    // Tests skip due to crbug.com/dawn/592.
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    constexpr uint32_t kWidth = 1000;
    constexpr uint32_t kHeight = 1;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight};

    DoTest(textureSpec, textureSpec, {kWidth, kHeight});
}

TEST_P(CopyTextureForBrowserTests, VerifyCopyOnYDirection) {
    // Tests skip due to crbug.com/dawn/592.
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    constexpr uint32_t kWidth = 1;
    constexpr uint32_t kHeight = 1000;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight};

    DoTest(textureSpec, textureSpec, {kWidth, kHeight});
}

TEST_P(CopyTextureForBrowserTests, VerifyCopyFromLargeTexture) {
    // Tests skip due to crbug.com/dawn/592.
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    constexpr uint32_t kWidth = 899;
    constexpr uint32_t kHeight = 999;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight};

    DoTest(textureSpec, textureSpec, {kWidth, kHeight});
}

TEST_P(CopyTextureForBrowserTests, VerifyFlipY) {
    // Tests skip due to crbug.com/dawn/592.
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    constexpr uint32_t kWidth = 901;
    constexpr uint32_t kHeight = 1001;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight};

    wgpu::CopyTextureForBrowserOptions options = {};
    options.flipY = true;
    DoTest(textureSpec, textureSpec, {kWidth, kHeight}, options);
}

TEST_P(CopyTextureForBrowserTests, VerifyFlipYInSlimTexture) {
    // Tests skip due to crbug.com/dawn/592.
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    constexpr uint32_t kWidth = 1;
    constexpr uint32_t kHeight = 1001;

    TextureSpec textureSpec;
    textureSpec.textureSize = {kWidth, kHeight};

    wgpu::CopyTextureForBrowserOptions options = {};
    options.flipY = true;
    DoTest(textureSpec, textureSpec, {kWidth, kHeight}, options);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToBGRA8Copy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::BGRA8Unorm;

    DoTest(srcTextureSpec, dstTextureSpec);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRGB10A2Copy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RGB10A2Unorm;

    DoTest(srcTextureSpec, dstTextureSpec);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRGBA16FloatCopy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RGBA16Float;

    DoTest(srcTextureSpec, dstTextureSpec);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRGBA32FloatCopy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RGBA32Float;

    DoTest(srcTextureSpec, dstTextureSpec);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRG8Copy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RG8Unorm;

    DoTest(srcTextureSpec, dstTextureSpec);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRG16Copy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RG16Float;

    DoTest(srcTextureSpec, dstTextureSpec);
}

DAWN_INSTANTIATE_TEST(CopyTextureForBrowserTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
