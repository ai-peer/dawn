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
    static constexpr uint64_t kDefaultTextureWidth = 4;
    static constexpr uint64_t kDefaultTextureHeight = 4;

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
                              static_cast<uint8_t>(x % 256), static_cast<uint8_t>(255));
                }
            }
        }

        return textureData;
    }

    static uint32_t GetTextureFormatComponentCount(wgpu::TextureFormat format) {
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

        uint32_t uniformBufferData[] = {
            0,  // copy have flipY option
            0,  // dstFormatSwizzled
            4,  // Texture format component count
        };

        wgpu::BufferDescriptor uniformBufferDesc = {};
        uniformBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        uniformBufferDesc.size = sizeof(uniformBufferData);
        uniformBuffer = device.CreateBuffer(&uniformBufferDesc);
    }

    // Use compute shader to compare source texture pixels and dst texture pixels after executing
    // CopyTextureForBrowser. This will avoid errors that may occurs by using CPU to calculate the
    // float value and compared it bit-by-bit with the GPU calculated float value.
    wgpu::ComputePipeline MakeTestPipeline() {
        wgpu::ShaderModule csModule = utils::CreateShaderModuleFromWGSL(device, R"(
            [[block]] struct Uniforms {
                [[offset(0)]] dstTextureFlipY : u32;
                [[offset(4)]] dstFormatSwizzled : u32;
                [[offset(8)]] dstFormatChannelNumber : u32;
            };
            [[block]] struct OutputBuf {
                [[offset(0)]] result : [[stride(4)]] array<u32>;
            };
            [[group(0), binding(0)]] var src : texture_2d<f32>;
            [[group(0), binding(1)]] var dst : texture_2d<f32>;
            [[group(0), binding(2)]] var<storage_buffer> output : OutputBuf;
            [[group(0), binding(3)]] var<uniform> uniforms : Uniforms;
            [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;
            [[stage(compute), workgroup_size(1, 1, 1)]]
            fn main() -> void {
                // Current CopyTextureForBrowser only support full copy now.
                // TODO(dawn:465): Refactor this after CopyTextureForBrowser
                // support sub-rect copy.
                var size : vec2<i32> = textureDimensions(src);
                var dstTexCoord : vec2<i32> = vec2<i32>(GlobalInvocationID.xy);
                var srcTexCoord : vec2<i32> = dstTexCoord;
                if (uniforms.dstTextureFlipY == 1) {
                    srcTexCoord.y = size.y - dstTexCoord.y - 1;
                }

                var srcColor : vec4<f32> = textureLoad(src, srcTexCoord);
                var dstColor : vec4<f32> = textureLoad(dst, dstTexCoord);
                if (uniforms.dstFormatSwizzled > 0) {
                    // swizzle is specialized to rg<ba> <-> <b>gr<a>;
                    srcColor = srcColor.bgra;
                }

                var success : bool = true;
                var srcColorBitCast : vec4<u32> = bitcast<vec4<u32>>(srcColor);
                var dstColorBitCast : vec4<u32> = bitcast<vec4<u32>>(dstColor);
                if (uniforms.dstFormatChannelNumber == 2) {
                    success = success &&
                              (srcColorBitCast[0] == dstColorBitCast[0]) &&
                              (srcColorBitCast[1] == dstColorBitCast[1]);
                } else {
                    success = success &&
                              all(srcColorBitCast == dstColorBitCast);
                }

                var outputIndex : u32 = GlobalInvocationID.y * size.x + GlobalInvocationID.x;

                if (success) {
                    output.result[outputIndex] = u32(1);
                } else {
                    output.result[outputIndex] = u32(0);
                }
            }
         )");

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.computeStage.module = csModule;
        csDesc.computeStage.entryPoint = "main";

        return device.CreateComputePipeline(&csDesc);
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
                              wgpu::TextureUsage::Sampled | wgpu::TextureUsage::Storage;
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
        uint32_t uniformBufferData[] = {
            options.flipY,                                      // copy have flipY option
            dstSpec.format == wgpu::TextureFormat::BGRA8Unorm,  // dstFormatSwizzled
            GetTextureFormatComponentCount(dstSpec.format)};

        device.GetQueue().WriteBuffer(uniformBuffer, 0, uniformBufferData,
                                      sizeof(uniformBufferData));

        // Create output buffer to store result
        wgpu::BufferDescriptor outputDesc;
        outputDesc.size = copySize.width * copySize.height * sizeof(uint32_t);
        outputDesc.usage =
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer outputBuffer = device.CreateBuffer(&outputDesc);

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
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(
            device, testPipeline.GetBindGroupLayout(0),
            {{0, srcTextureView},
             {1, dstTextureView},
             {2, outputBuffer, 0, copySize.width * copySize.height * sizeof(uint32_t)},
             {3, uniformBuffer, 0, sizeof(uniformBufferData)}});

        // Start a pipeline to check pixel value in bit form.
        wgpu::CommandEncoder testEncoder = device.CreateCommandEncoder();

        wgpu::CommandBuffer testCommands;
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(testPipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.Dispatch(copySize.width, copySize.height);
            pass.EndPass();

            testCommands = encoder.Finish();
        }

        queue.Submit(1, &testCommands);

        std::vector<uint32_t> expectResult{};
        expectResult.resize(copySize.width * copySize.height);
        std::fill(expectResult.begin(), expectResult.end(), uint32_t(1));

        EXPECT_BUFFER_U32_RANGE_EQ(expectResult.data(), outputBuffer, 0,
                                   copySize.width * copySize.height);
    }
    wgpu::Buffer uniformBuffer;  // Uniform buffer to store dst texture meta info.
    wgpu::ComputePipeline testPipeline;
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

DAWN_INSTANTIATE_TEST(CopyTextureForBrowserTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
