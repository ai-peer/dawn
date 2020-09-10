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

#include <array>
#include "common/Constants.h"
#include "common/Math.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/TextureFormatUtils.h"
#include "utils/WGPUHelpers.h"

class DepthBiasTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // Draw a square in the bottom left quarter of the screen at z = 0.3
        mVertexModuleBias = utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
    #version 450
    void main() {
        const vec2 pos[6] = vec2[6](vec2(-1.f, -1.f), vec2(0.f, -1.f), vec2(-1.f,  0.f),
                                    vec2(-1.f,  0.f), vec2(0.f, -1.f), vec2( 0.f,  0.f));
        gl_Position = vec4(pos[gl_VertexIndex], 0.3f, 1.f);
    })");

        // Draw a square in the bottom left quarter of the screen sloping from 0 to 0.5
        mVertexModuleSlope =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
    #version 450
    void main() {
        const vec3 pos[6] = vec3[6](vec3(-1.f, -1.f, 0.f ), vec3(0.f, -1.f, 0.f), vec3(-1.f,  0.f, 0.5f),
                                    vec3(-1.f,  0.f, 0.5f), vec3(0.f, -1.f, 0.f), vec3( 0.f,  0.f, 0.5f));
        gl_Position = vec4(pos[gl_VertexIndex], 1.f);
    })");

        mFragmentModule = utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
    #version 450
    void main() {
    })");
    }

    void RunDepthBiasTest(wgpu::Texture depthTexture,
                          wgpu::TextureFormat depthFormat,
                          wgpu::ShaderModule vertexModule,
                          int32_t bias,
                          float biasSlopeScale,
                          float biasClap) {
        // Create a render pass which clears depth to 0
        utils::ComboRenderPassDescriptor renderPassDesc({}, depthTexture.CreateView());
        renderPassDesc.cDepthStencilAttachmentInfo.clearDepth = 0.f;

        // Create a render pipeline to render a bottom-left quad
        utils::ComboRenderPipelineDescriptor renderPipelineDesc(device);

        renderPipelineDesc.cRasterizationState.depthBias = bias;
        renderPipelineDesc.cRasterizationState.depthBiasSlopeScale = biasSlopeScale;
        renderPipelineDesc.cRasterizationState.depthBiasClamp = biasClap;

        renderPipelineDesc.vertexStage.module = vertexModule;
        renderPipelineDesc.cVertexState.indexFormat = wgpu::IndexFormat::Undefined;
        renderPipelineDesc.cFragmentStage.module = mFragmentModule;
        renderPipelineDesc.cDepthStencilState.format = depthFormat;
        renderPipelineDesc.cDepthStencilState.depthWriteEnabled = true;
        renderPipelineDesc.depthStencilState = &renderPipelineDesc.cDepthStencilState;
        renderPipelineDesc.colorStateCount = 0;

        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);

        // Draw the quad (two triangles)
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPassDesc);
        pass.SetPipeline(pipeline);
        pass.Draw(6);
        pass.EndPass();

        wgpu::CommandBuffer commands = commandEncoder.Finish();
        queue.Submit(1, &commands);
    }

    // Floating point depth buffers use the following formula to calculate bias
    // bias = depthBias * 2 ** (exponent(max z of primitive) - number of bits in mantissa) +
    //        slopeScale * maxSlope
    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage-depth-bias
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdSetDepthBias.html
    // https://developer.apple.com/documentation/metal/mtlrendercommandencoder/1516269-setdepthbias
    //
    // To get a final bias of 0.1 for primitives with z = 0.3, we can use
    // depthBias = 0.1 / (2 ** (-2 - 23)) = 3355443
    static constexpr int32_t kPointOneBiasForPointThreeZOnFloat = 3355443;

    wgpu::ShaderModule mVertexModuleBias;
    wgpu::ShaderModule mVertexModuleSlope;
    wgpu::ShaderModule mFragmentModule;
};

// Test adding positive bias to output
TEST_P(DepthBiasTests, PositiveBiasOnFloat) {
    DAWN_SKIP_TEST_IF(IsVulkan() && IsSwiftshader());

    // Create a depth texture
    constexpr uint32_t kWidth = 4;
    constexpr uint32_t kHeight = 4;
    wgpu::TextureDescriptor texDescriptor = {};
    texDescriptor.size = {kWidth, kHeight, 1};
    texDescriptor.format = wgpu::TextureFormat::Depth32Float;
    texDescriptor.usage = wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture depthTexture = device.CreateTexture(&texDescriptor);

    RunDepthBiasTest(depthTexture, texDescriptor.format, mVertexModuleBias,
                     kPointOneBiasForPointThreeZOnFloat, 0, 0);

    // Only the bottom left quad has depth values
    std::vector<float> expected = {
        0.0, 0.0, 0.0, 0.0,  //
        0.0, 0.0, 0.0, 0.0,  //
        0.4, 0.4, 0.0, 0.0,  //
        0.4, 0.4, 0.0, 0.0,  //
    };

    // This expectation is the test as it performs the CopyTextureToBuffer.
    EXPECT_TEXTURE_EQ(expected.data(), depthTexture, 0, 0, kWidth, kHeight, 0, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding positive bias to output with a clap
TEST_P(DepthBiasTests, PositiveBiasOnFloatWithClamp) {
    DAWN_SKIP_TEST_IF(IsVulkan() && IsSwiftshader());

    // Create a depth texture
    constexpr uint32_t kWidth = 4;
    constexpr uint32_t kHeight = 4;
    wgpu::TextureDescriptor texDescriptor = {};
    texDescriptor.size = {kWidth, kHeight, 1};
    texDescriptor.format = wgpu::TextureFormat::Depth32Float;
    texDescriptor.usage = wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture depthTexture = device.CreateTexture(&texDescriptor);

    RunDepthBiasTest(depthTexture, texDescriptor.format, mVertexModuleBias,
                     kPointOneBiasForPointThreeZOnFloat, 0, 0.06);

    // Only the bottom left quad has depth values
    std::vector<float> expected = {
        0.0,  0.0,  0.0, 0.0,  //
        0.0,  0.0,  0.0, 0.0,  //
        0.36, 0.36, 0.0, 0.0,  //
        0.36, 0.36, 0.0, 0.0,  //
    };

    // This expectation is the test as it performs the CopyTextureToBuffer.
    EXPECT_TEXTURE_EQ(expected.data(), depthTexture, 0, 0, kWidth, kHeight, 0, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding positive slop bias to output
TEST_P(DepthBiasTests, PositiveSlopeBiasOnFloat) {
    DAWN_SKIP_TEST_IF(IsVulkan() && IsSwiftshader());

    // Create a depth texture
    constexpr uint32_t kWidth = 4;
    constexpr uint32_t kHeight = 4;
    wgpu::TextureDescriptor texDescriptor = {};
    texDescriptor.size = {kWidth, kHeight, 1};
    texDescriptor.format = wgpu::TextureFormat::Depth32Float;
    texDescriptor.usage = wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture depthTexture = device.CreateTexture(&texDescriptor);

    RunDepthBiasTest(depthTexture, texDescriptor.format, mVertexModuleSlope, 0, 1, 0);

    // Only the bottom left quad has depth values
    std::vector<float> expected = {
        0.0,   0.0,   0.0, 0.0,  //
        0.0,   0.0,   0.0, 0.0,  //
        0.625, 0.625, 0.0, 0.0,  //
        0.375, 0.375, 0.0, 0.0,  //
    };

    // This expectation is the test as it performs the CopyTextureToBuffer.
    EXPECT_TEXTURE_EQ(expected.data(), depthTexture, 0, 0, kWidth, kHeight, 0, 0,
                      wgpu::TextureAspect::DepthOnly);
}

// Test adding positive slop bias to output with a clap
TEST_P(DepthBiasTests, PositiveSlopeBiasOnFloatWithClamp) {
    DAWN_SKIP_TEST_IF(IsVulkan() && IsSwiftshader());

    // Create a depth texture
    constexpr uint32_t kWidth = 4;
    constexpr uint32_t kHeight = 4;
    wgpu::TextureDescriptor texDescriptor = {};
    texDescriptor.size = {kWidth, kHeight, 1};
    texDescriptor.format = wgpu::TextureFormat::Depth32Float;
    texDescriptor.usage = wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture depthTexture = device.CreateTexture(&texDescriptor);

    RunDepthBiasTest(depthTexture, texDescriptor.format, mVertexModuleSlope, 0, 1, 0.2);

    // Only the bottom left quad has depth values
    std::vector<float> expected = {
        0.0,   0.0,   0.0, 0.0,  //
        0.0,   0.0,   0.0, 0.0,  //
        0.575, 0.575, 0.0, 0.0,  //
        0.325, 0.325, 0.0, 0.0,  //
    };

    // This expectation is the test as it performs the CopyTextureToBuffer.
    EXPECT_TEXTURE_EQ(expected.data(), depthTexture, 0, 0, kWidth, kHeight, 0, 0,
                      wgpu::TextureAspect::DepthOnly);
}

DAWN_INSTANTIATE_TEST(DepthBiasTests, D3D12Backend(), MetalBackend(), VulkanBackend());