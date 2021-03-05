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
    // Set default texture size to 256 x 256 to cover all possible RGBA8 combination for
    // color conversion tests.
    static constexpr uint64_t kDefaultTextureWidth = 256;
    static constexpr uint64_t kDefaultTextureHeight = 256;

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
                              static_cast<uint8_t>(x % 256), static_cast<uint8_t>(x % 256));
                }
            }
        }

        return textureData;
    }

    void SetUp() override {
        DawnTest::SetUp();

        testPipeline = MakeTestPipeline();

        uint32_t uniformBufferData[] = {
            0,  // copy have flipY option
            0,  // isFloat16
            0,  // isRGB10A2Unorm
            0,  // isSwizzled
            4,  // channelCount
        };

        wgpu::BufferDescriptor uniformBufferDesc = {};
        uniformBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        uniformBufferDesc.size = sizeof(uniformBufferData);
        uniformBuffer = device.CreateBuffer(&uniformBufferDesc);
    }

    // Do the bit-by-bit comparison between the source and destination texture with GPU (compute
    // shader) instead of CPU after executing CopyTextureForBrowser() to avoid the errors caused by
    // comparing a value generated on CPU to the one generated on GPU.
    wgpu::ComputePipeline MakeTestPipeline() {
        wgpu::ShaderModule csModule = utils::CreateShaderModuleFromWGSL(device, R"(
            [[block]] struct Uniforms {
                [[offset(0)]] dstTextureFlipY : u32;
                [[offset(4)]] isFloat16 : u32;
                [[offset(8)]] isRGB10A2Unorm : u32;
                [[offset(12)]] isSwizzled : u32;
                [[offset(16)]] channelCount : u32;
            };
            [[block]] struct OutputBuf {
                [[offset(0)]] result : [[stride(4)]] array<u32>;
            };
            [[group(0), binding(0)]] var src : texture_2d<f32>;
            [[group(0), binding(1)]] var dst : texture_2d<f32>;
            [[group(0), binding(2)]] var<storage_buffer> output : OutputBuf;
            [[group(0), binding(3)]] var<uniform> uniforms : Uniforms;
            [[builtin(global_invocation_id)]] var<in> GlobalInvocationID : vec3<u32>;
            // Fp16 logic from https://github.com/Maratyszcza/FP16, used by Chromium.
            // The alogrithm details can be found in chromium code base:
            // third_party/fp16/src/include/fp16/fp16.h
            fn ConvertToFp16FloatValue(fp32 : f32) -> u32 {
               // Convert fp32 value to fp16
               // TODO(crbug.com/dawn/465) : Replace these two magic numbers to
               // the hex form 0x1.0p+112f and 0x1.0p-110f after tint supports it.
               var scale_to_inf : f32 = bitcast<f32>(0x77800000);
               var scale_to_zero : f32 = bitcast<f32>(0x08800000);
               var base: f32 = abs(fp32) * scale_to_inf  * scale_to_zero;
               var w : u32 = bitcast<u32>(fp32);
               var shll_w : u32 = w + w;
               var sign : u32 = w & 0x80000000u;
               var bias : u32 = shll_w & 0xFF000000u;
               if (bias < 0x71000000u) {
                   bias = 0x71000000u;
               }

               base = bitcast<f32>((bias >> 1) + 0x7800000u) + base;
               var bits : u32 = bitcast<u32>(base);
               var exp_bits : u32 = (bits >> 13) & 0x00007C00u;
               var mantissa_bits : u32 = bits & 0x00000FFF;
               var nonsign : u32 = exp_bits + mantissa_bits;
               var fp16u_without_sign : u32;
               if (shll_w > 0xFF000000u) {
                   fp16u_without_sign = 0x7E00u;
               } else {
                   fp16u_without_sign = nonsign;
               }
               var fp16u : u32 = (sign >> 16) | fp16u_without_sign;

               // convert fp16u to f32 value
               var cw : u32  = fp16u << 16;
               var csign : u32 = cw & 0x80000000u;
               var two_cw : u32 = cw + cw;
               var exp_offset : u32 = 0xE0u << 23;
               // TODO(crbug.com/dawn/465) : Replace this magic number to
               // the hex form 0x1.0p-112f after tint supports it.
               var exp_scale : f32 = bitcast<f32>(0x7800000u);
               var normalized_value : f32 = bitcast<f32>((two_cw >> 4) + exp_offset) * exp_scale;
               var magic_mask : u32 = u32(126) << 23;
               var magic_bias : f32 = 0.5;
               var denormalized_value : f32 = bitcast<f32>((two_cw >> 17) | magic_mask) - magic_bias;
               var denormalized_cutoff : u32 = u32(1) << 27;

               var result_without_sign : u32;
               
               if (two_cw < denormalized_cutoff) {
                   result_without_sign = bitcast<u32>(denormalized_value);
               } else {
                   result_without_sign = bitcast<u32>(normalized_value);
               }

               var result : u32 = csign | result_without_sign;
               return result;
            }

            [[stage(compute), workgroup_size(1, 1, 1)]]
            fn main() -> void {
                // Current CopyTextureForBrowser only support full copy now.
                // TODO(crbug.com/dawn/465): Refactor this after CopyTextureForBrowser
                // support sub-rect copy.
                var size : vec2<i32> = textureDimensions(src);
                var dstTexCoord : vec2<i32> = vec2<i32>(GlobalInvocationID.xy);
                var srcTexCoord : vec2<i32> = dstTexCoord;
                if (uniforms.dstTextureFlipY == 1) {
                    srcTexCoord.y = size.y - dstTexCoord.y - 1;
                }

                var srcColor : vec4<f32> = textureLoad(src, srcTexCoord);
                var dstColor : vec4<f32> = textureLoad(dst, dstTexCoord);
                var success : bool = true;
            
                // float16 formats will have precision lose during fp32->fp16.
                // We emulate the whole progress of fp32->fp16->fp32 progress on
                // source texture and compare the bits.
                if (uniforms.isFloat16 == 1) {
                    var srcColorBits : vec4<u32>;
                    var dstColorBits : vec4<u32> = bitcast<vec4<u32>>(dstColor);
                    
                    // Apply conversion to each channel.
                    // TODO(crbug.com/tint/534): we have to seperate the 2-channel formats
                    // and 4-channel formats manually to workaround this issue.
                    if (uniforms.channelCount == 4) {
                        for (var i : u32 = 0u; i < 4u; i = i + 1) {
                            srcColorBits[i] = ConvertToFp16FloatValue(srcColor[i]);
                        
                            // Low 8-bit are 0x00 and can be shift out.
                            // Add a |DCHECK| like logic to guard this assumption.
                            if ((srcColorBits[i] & 0x000000FFu) != 0u ||
                                (dstColorBits[i] & 0x000000FFu) != 0u) {
                                success = false;
                            }
                            srcColorBits[i] = srcColorBits[i] >> 8;
                            dstColorBits[i] = dstColorBits[i] >> 8;
                        
                            // TODO(shaobo.yan@intel.com): There is an error in the emulate convert
                            // progress, which makes the bits compare of src and dst have 1~2 bit
                            // difference in the valid mantissa part. Manually verified the dst fp16
                            // value and it seems our emulation progress needs to be more precision.
                            // Will update the algorithm when wgsl support hex presentaion of float,
                            // e.g. 0x1.0p-227.
                            success = success &&
                                      (max(srcColorBits[i], dstColorBits[i]) -
                                       min(srcColorBits[i], dstColorBits[i]) < 64u);
                        }
                    } else { // uniforms.channleCount == 2
                        for (var i : u32 = 0u; i < 2u; i = i + 1) {
                            srcColorBits[i] = ConvertToFp16FloatValue(srcColor[i]);
                        
                            if ((srcColorBits[i] & 0x000000FFu) != 0u ||
                                (dstColorBits[i] & 0x000000FFu) != 0u) {
                                success = false;
                            }
                            srcColorBits[i] = srcColorBits[i] >> 8;
                            dstColorBits[i] = dstColorBits[i] >> 8;
                            success = success &&
                                      (max(srcColorBits[i], dstColorBits[i]) -
                                       min(srcColorBits[i], dstColorBits[i]) < 64u);
                        }
                    }
                }  elseif (uniforms.isRGB10A2Unorm == 1) {
                    var maxRGBValue : u32 = (1u << 10u) - 1u;
                    var maxAlphaValue : u32 = (1u << 2u) - 1u;
                    for (var i : u32 = 0u; i < 4u; i = i + 1) {
                        var maxComponentValue : u32;
                        if (i == 3u) { // Alpha channel
                            maxComponentValue = (1u << 2u) - 1u;
                        } else { // RGB channels
                            maxComponentValue = (1u << 10u) - 1u;
                        }

                        var expectDenormalizedColor : f32 = round(srcColor[i] * f32(maxComponentValue));
                        var denormalizedDstColor : f32 = round(dstColor[i] * f32(maxComponentValue));

                        // |round| may have different output but the error should no more than 1.
                        success = success &&
                                  max(expectDenormalizedColor, denormalizedDstColor) -
                                  min(expectDenormalizedColor, denormalizedDstColor) < 1.1;
                    }
                } elseif (uniforms.isSwizzled == 1) { // BGRA8Unorm check
                  // RGBA8Unorm to BGRA8Unorm 
                  success = success &&
                            srcColor[0] == dstColor[2] &&  // r channel
                            srcColor[1] == dstColor[1] &&  // g channel
                            srcColor[2] == dstColor[0] &&  // b channel
                            srcColor[3] == dstColor[3];    // a channel
                } else { // Other format converts shouldn't have precision difference
                    if (uniforms.channelCount == 2) { // Dst texture formats are all have rg components.
                        success = success && all(srcColor.rg == dstColor.rg);
                    } else {
                        success = success && all(srcColor == dstColor);
                    }
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

    void DoTest(const TextureSpec& srcSpec,
                const TextureSpec& dstSpec,
                const wgpu::Extent3D& copySize = {kDefaultTextureWidth, kDefaultTextureHeight},
                const wgpu::CopyTextureForBrowserOptions options = {}) {
        wgpu::TextureDescriptor srcDescriptor;
        srcDescriptor.size = srcSpec.textureSize;
        srcDescriptor.format = srcSpec.format;
        srcDescriptor.mipLevelCount = srcSpec.level + 1;
        srcDescriptor.usage =
            wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled;
        wgpu::Texture srcTexture = device.CreateTexture(&srcDescriptor);

        wgpu::Texture dstTexture;
        wgpu::TextureDescriptor dstDescriptor;
        dstDescriptor.size = dstSpec.textureSize;
        dstDescriptor.format = dstSpec.format;
        dstDescriptor.mipLevelCount = dstSpec.level + 1;
        dstDescriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled |
                              wgpu::TextureUsage::OutputAttachment;
        dstTexture = device.CreateTexture(&dstDescriptor);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        const utils::TextureDataCopyLayout copyLayout =
            utils::GetTextureDataCopyLayoutForTexture2DAtLevel(
                kTextureFormat,
                {srcSpec.textureSize.width, srcSpec.textureSize.height, copySize.depth},
                srcSpec.level);

        std::vector<RGBA8> textureArrayCopyData = GetSourceTextureData(copyLayout);
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
            options.flipY,  // copy have flipY option
            dstSpec.format == wgpu::TextureFormat::RG16Float ||
                dstSpec.format == wgpu::TextureFormat::RGBA16Float,  // isFloat16
            dstSpec.format == wgpu::TextureFormat::RGB10A2Unorm,     // isRGB10A2Unorm
            dstSpec.format == wgpu::TextureFormat::BGRA8Unorm,       // isSwizzled
            GetTextureFormatComponentCount(dstSpec.format)};         // channelCount

        device.GetQueue().WriteBuffer(uniformBuffer, 0, uniformBufferData,
                                      sizeof(uniformBufferData));

        // Create output buffer to store result
        wgpu::BufferDescriptor outputDesc;
        outputDesc.size = copySize.width * copySize.height * sizeof(uint32_t) * 2;
        outputDesc.usage =
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer outputBuffer = device.CreateBuffer(&outputDesc);

        // Create texture views for test.
        wgpu::TextureViewDescriptor srcTextureViewDesc = {};
        srcTextureViewDesc.baseMipLevel = srcSpec.level;
        wgpu::TextureView srcTextureView = srcTexture.CreateView(&srcTextureViewDesc);

        wgpu::TextureViewDescriptor dstTextureViewDesc = {};
        dstTextureViewDesc.baseMipLevel = dstSpec.level;
        wgpu::TextureView dstTextureView = dstTexture.CreateView(&dstTextureViewDesc);

        // Create bind group based on the config.
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(
            device, testPipeline.GetBindGroupLayout(0),
            {{0, srcTextureView},
             {1, dstTextureView},
             {2, outputBuffer, 0, copySize.width * copySize.height * sizeof(uint32_t) * 2},
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

        std::vector<uint32_t> expectResult(copySize.width * copySize.height, 1);
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

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToBGRA8UnormCopy) {
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

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRG8UnormCopy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RG8Unorm;

    DoTest(srcTextureSpec, dstTextureSpec);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRGBA16FloatCopy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RGBA16Float;

    DoTest(srcTextureSpec, dstTextureSpec);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRG16FloatCopy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RG16Float;

    DoTest(srcTextureSpec, dstTextureSpec);
}

TEST_P(CopyTextureForBrowserTests, VerifyRGBA8ToRGB10A2UnormCopy) {
    DAWN_SKIP_TEST_IF(IsD3D12() && IsBackendValidationEnabled());

    TextureSpec srcTextureSpec = {};

    TextureSpec dstTextureSpec;
    dstTextureSpec.format = wgpu::TextureFormat::RGB10A2Unorm;

    DoTest(srcTextureSpec, dstTextureSpec);
}

DAWN_INSTANTIATE_TEST(CopyTextureForBrowserTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
