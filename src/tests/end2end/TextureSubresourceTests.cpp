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

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class TextureSubresourceTest : public DawnTest {
  public:
    static constexpr uint32_t kSize = 4u;
    static constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;

    wgpu::Texture CreateTexture(uint32_t mipLevelCount,
                                uint32_t arrayLayerCount,
                                wgpu::TextureUsage usage) {
        wgpu::TextureDescriptor texDesc;
        texDesc.dimension = wgpu::TextureDimension::e2D;
        texDesc.size = {kSize, kSize, arrayLayerCount};
        texDesc.sampleCount = 1;
        texDesc.mipLevelCount = mipLevelCount;
        texDesc.usage = usage;
        texDesc.format = kFormat;
        return device.CreateTexture(&texDesc);
    }

    wgpu::TextureView CreateTextureView(wgpu::Texture texture,
                                        uint32_t baseMipLevel,
                                        uint32_t baseArrayLayer) {
        wgpu::TextureViewDescriptor viewDesc;
        viewDesc.format = kFormat;
        viewDesc.baseArrayLayer = baseArrayLayer;
        viewDesc.arrayLayerCount = 1;
        viewDesc.baseMipLevel = baseMipLevel;
        viewDesc.mipLevelCount = 1;
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        return texture.CreateView(&viewDesc);
    }

    void SetUp() override {
        DawnTest::SetUp();

        utils::ComboRenderPipelineDescriptor pipelineDesc(device);
        pipelineDesc.vertexStage.module =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                layout (location = 0) out vec2 fUV;
                void main() {
                    const vec2 pos[6] = vec2[6](
                        vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(-1.f, 1.f),
                        vec2(-1.f, -1.f), vec2(1.f, -1.f), vec2(1.f, 1.f));
                    const vec2 uv[6] = vec2[6](
                        vec2(0.f, 1.f), vec2(1.f, 0.f), vec2(0.f, 0.f),
                        vec2(0.f, 1.f), vec2(1.f, 1.f), vec2(1.f, 0.f));
                    fUV = uv[gl_VertexIndex];
                    gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
                 })");
        pipelineDesc.cFragmentStage.module =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
                #version 450
                layout (set = 0, binding = 0) uniform sampler samp;
                layout (set = 0, binding = 1) uniform texture2D tex;
                layout (location = 0) in vec2 fUV;
                layout (location = 0) out vec4 fragColor;
                void main() {
                    fragColor = texture(sampler2D(tex, samp), fUV);
                })");
        pipelineDesc.primitiveTopology = wgpu::PrimitiveTopology::TriangleStrip;
        pipelineDesc.cColorStates[0].format = kFormat;

        mSampleAndDrawPipeline = device.CreateRenderPipeline(&pipelineDesc);
    }

    void DrawTriangle(const wgpu::TextureView& view) {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                void main() {
                    const vec2 pos[3] = vec2[3](
                        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, -1.f));
                    gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
                 })");

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
                #version 450
                layout(location = 0) out vec4 fragColor;
                void main() {
                    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
                })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.vertexStage.module = vsModule;
        descriptor.cFragmentStage.module = fsModule;
        descriptor.primitiveTopology = wgpu::PrimitiveTopology::TriangleStrip;
        descriptor.cColorStates[0].format = kFormat;

        wgpu::RenderPipeline rp = device.CreateRenderPipeline(&descriptor);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        utils::ComboRenderPassDescriptor renderPassDesc({view});
        renderPassDesc.cColorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
        pass.SetPipeline(rp);
        pass.Draw(3);
        pass.EndPass();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

    void SampleAndDraw(wgpu::CommandEncoder encoder,
                       const wgpu::TextureView& samplerView,
                       const wgpu::TextureView& renderView) {
        wgpu::SamplerDescriptor samplerDescriptor = {};
        wgpu::Sampler sampler = device.CreateSampler(&samplerDescriptor);

        wgpu::BindGroupLayout bgl = mSampleAndDrawPipeline.GetBindGroupLayout(0);
        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, bgl, {{0, sampler}, {1, samplerView}});

        utils::ComboRenderPassDescriptor renderPassDesc({renderView});
        renderPassDesc.cColorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
        pass.SetPipeline(mSampleAndDrawPipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.Draw(6);
        pass.EndPass();
    }

    void SampleAndDraw(const wgpu::TextureView& samplerView, const wgpu::TextureView& renderView) {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        SampleAndDraw(encoder, samplerView, renderView);
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
    }

  protected:
    wgpu::RenderPipeline mSampleAndDrawPipeline;
};

// Test different mipmap levels
TEST_P(TextureSubresourceTest, MipmapLevelsTest) {
    // Create a texture with 2 mipmap levels and 1 layer
    wgpu::Texture texture =
        CreateTexture(2, 1,
                      wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment |
                          wgpu::TextureUsage::CopySrc);

    // Create two views on different mipmap levels.
    wgpu::TextureView samplerView = CreateTextureView(texture, 0, 0);
    wgpu::TextureView renderView = CreateTextureView(texture, 1, 0);

    // Draw a red triangle at the bottom-left half
    DrawTriangle(samplerView);

    // Sample from one subresource and draw into another subresource in the same texture
    SampleAndDraw(samplerView, renderView);

    // Check both subresources.
    std::vector<RGBA8> mip0Expected = {
        RGBA8::kBlack, RGBA8::kBlack, RGBA8::kBlack, RGBA8::kBlack,  //
        RGBA8::kRed,   RGBA8::kBlack, RGBA8::kBlack, RGBA8::kBlack,  //
        RGBA8::kRed,   RGBA8::kRed,   RGBA8::kBlack, RGBA8::kBlack,  //
        RGBA8::kRed,   RGBA8::kRed,   RGBA8::kRed,   RGBA8::kBlack,  //
    };

    std::vector<RGBA8> mip1Expected = {
        RGBA8::kBlack, RGBA8::kBlack,  //
        RGBA8::kRed, RGBA8::kBlack,    //
    };

    EXPECT_TEXTURE_EQ(mip0Expected.data(), texture, 0, 0, kSize, kSize, 0);
    EXPECT_TEXTURE_EQ(mip1Expected.data(), texture, 0, 0, kSize / 2, kSize / 2, 1);
}

// Test generating a long mip chain in single command buffer
TEST_P(TextureSubresourceTest, LongMipMapGeneration) {
    wgpu::TextureDescriptor texDesc;
    texDesc.size = {32, 32, 1};
    texDesc.mipLevelCount = 6;
    texDesc.usage = wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment |
                    wgpu::TextureUsage::CopySrc;
    texDesc.format = kFormat;
    wgpu::Texture texture = device.CreateTexture(&texDesc);

    wgpu::TextureViewDescriptor viewDesc = {};
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    wgpu::TextureView srcView = texture.CreateView(&viewDesc);

    DrawTriangle(srcView);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    for (uint32_t i = 1; i < texDesc.mipLevelCount; ++i) {
        wgpu::TextureViewDescriptor viewDesc = {};
        viewDesc.baseMipLevel = i;
        viewDesc.mipLevelCount = 1;
        wgpu::TextureView dstView = texture.CreateView(&viewDesc);

        SampleAndDraw(encoder, srcView, dstView);
        srcView = dstView;
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    for (uint32_t i = 0; i < texDesc.mipLevelCount; ++i) {
        uint32_t mipSize = texDesc.size.width >> i;
        std::vector<RGBA8> expected(mipSize * mipSize);
        for (uint32_t y = 0; y < mipSize; ++y) {
            for (uint32_t x = 0; x < mipSize; ++x) {
                if (x + (mipSize - y) < mipSize) {
                    expected[y * mipSize + x] = RGBA8::kRed;
                } else {
                    expected[y * mipSize + x] = RGBA8::kBlack;
                }
            }
        }
        EXPECT_TEXTURE_EQ(expected.data(), texture, 0, 0, mipSize, mipSize, i);
    }
}

// Test different array layers
TEST_P(TextureSubresourceTest, ArrayLayersTest) {
    // Create a texture with 1 mipmap level and 2 layers
    wgpu::Texture texture =
        CreateTexture(1, 2,
                      wgpu::TextureUsage::Sampled | wgpu::TextureUsage::OutputAttachment |
                          wgpu::TextureUsage::CopySrc);

    // Create two views on different layers
    wgpu::TextureView samplerView = CreateTextureView(texture, 0, 0);
    wgpu::TextureView renderView = CreateTextureView(texture, 0, 1);

    // Draw a red triangle at the bottom-left half
    DrawTriangle(samplerView);

    // Sample from one subresource and draw into another subresource in the same texture
    SampleAndDraw(samplerView, renderView);

    // Check both subresources.
    std::vector<RGBA8> expected = {
        RGBA8::kBlack, RGBA8::kBlack, RGBA8::kBlack, RGBA8::kBlack,  //
        RGBA8::kRed,   RGBA8::kBlack, RGBA8::kBlack, RGBA8::kBlack,  //
        RGBA8::kRed,   RGBA8::kRed,   RGBA8::kBlack, RGBA8::kBlack,  //
        RGBA8::kRed,   RGBA8::kRed,   RGBA8::kRed,   RGBA8::kBlack,  //
    };

    EXPECT_TEXTURE_EQ(expected.data(), texture, 0, 0, kSize, kSize, 0, 0);
    EXPECT_TEXTURE_EQ(expected.data(), texture, 0, 0, kSize, kSize, 0, 1);
}

// TODO (yunchao.he@intel.com):
// * add tests for storage texture and sampler across miplevel or
// arraylayer dimensions in the same texture
//
// * add tests for copy operation upon texture subresource if needed
//
// * add tests for clear operation upon texture subresource if needed

DAWN_INSTANTIATE_TEST(TextureSubresourceTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
