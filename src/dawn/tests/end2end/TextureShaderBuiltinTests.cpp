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

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

constexpr static unsigned int kRTSize = 64;
constexpr wgpu::TextureFormat kDefaultFormat = wgpu::TextureFormat::RGBA8Unorm;
// constexpr uint32_t kBytesPerTexel = 4;

wgpu::Texture Create2DTexture(wgpu::Device device,
                              uint32_t width,
                              uint32_t height,
                              uint32_t arrayLayerCount,
                              uint32_t mipLevelCount,
                              uint32_t sampleCount,
                              wgpu::TextureUsage usage) {
    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size.width = width;
    descriptor.size.height = height;
    descriptor.size.depthOrArrayLayers = arrayLayerCount;
    descriptor.sampleCount = sampleCount;
    descriptor.format = kDefaultFormat;
    descriptor.mipLevelCount = mipLevelCount;
    descriptor.usage = usage;
    return device.CreateTexture(&descriptor);
}

// wgpu::Texture Create3DTexture(wgpu::Device device,
//                               wgpu::Extent3D size,
//                               uint32_t mipLevelCount,
//                               wgpu::TextureUsage usage) {
//     wgpu::TextureDescriptor descriptor;
//     descriptor.dimension = wgpu::TextureDimension::e3D;
//     descriptor.size = size;
//     descriptor.sampleCount = 1;
//     descriptor.format = kDefaultFormat;
//     descriptor.mipLevelCount = mipLevelCount;
//     descriptor.usage = usage;
//     return device.CreateTexture(&descriptor);
// }

wgpu::ShaderModule CreateDefaultComputeShaderModule(wgpu::Device device) {
    return utils::CreateShaderModule(device, R"(
@group(0) @binding(0) var src_tex : texture_2d_array<f32>;
@group(0) @binding(1) var<storage, read_write> dst_buf : array<u32>;

@compute @workgroup_size(1, 1, 1) fn main() {
    dst_buf[0] = textureNumLayers(src_tex);
    dst_buf[1] = textureNumLevels(src_tex);
    // dst_buf[2] = textureNumSamples(src_tex);
    // textureDimension
}
    )");
}

wgpu::ShaderModule CreateDefaultVertexShaderModule(wgpu::Device device) {
    return utils::CreateShaderModule(device, R"(
            struct VertexOut {
                @location(0) texCoord : vec2f,
                @builtin(position) position : vec4f,
            }

            @vertex
            fn main(@builtin(vertex_index) VertexIndex : u32) -> VertexOut {
                var output : VertexOut;
                var pos = array(
                                            vec2f(-2., -2.),
                                            vec2f(-2.,  2.),
                                            vec2f( 2., -2.),
                                            vec2f(-2.,  2.),
                                            vec2f( 2., -2.),
                                            vec2f( 2.,  2.));
                var texCoord = array(
                                                 vec2f(0., 0.),
                                                 vec2f(0., 1.),
                                                 vec2f(1., 0.),
                                                 vec2f(0., 1.),
                                                 vec2f(1., 0.),
                                                 vec2f(1., 1.));
                output.position = vec4f(pos[VertexIndex], 0., 1.);
                output.texCoord = texCoord[VertexIndex];
                return output;
            }
        )");
}

class TextureShaderBuiltinTests : public DawnTest {
  protected:
    // Generates an arbitrary pixel value per-layer-per-level, used for the "actual" uploaded
    // textures and the "expected" results.
    static int GenerateTestPixelValue(uint32_t layer, uint32_t level) {
        return static_cast<int>(level * 10) + static_cast<int>(layer + 1);
    }

    void SetUp() override {
        DawnTest::SetUp();

        mRenderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

        wgpu::FilterMode kFilterMode = wgpu::FilterMode::Nearest;
        wgpu::MipmapFilterMode kMipmapFilterMode = wgpu::MipmapFilterMode::Nearest;
        wgpu::AddressMode kAddressMode = wgpu::AddressMode::ClampToEdge;

        wgpu::SamplerDescriptor samplerDescriptor = {};
        samplerDescriptor.minFilter = kFilterMode;
        samplerDescriptor.magFilter = kFilterMode;
        samplerDescriptor.mipmapFilter = kMipmapFilterMode;
        samplerDescriptor.addressModeU = kAddressMode;
        samplerDescriptor.addressModeV = kAddressMode;
        samplerDescriptor.addressModeW = kAddressMode;
        mSampler = device.CreateSampler(&samplerDescriptor);

        mVSModule = CreateDefaultVertexShaderModule(device);
    }

    void InitTexture(uint32_t arrayLayerCount, uint32_t mipLevelCount, uint32_t sampleCount) {
        ASSERT(arrayLayerCount > 0 && mipLevelCount > 0);
        ASSERT(sampleCount == 1 || sampleCount == 4);

        const uint32_t textureWidthLevel0 = 1 << mipLevelCount;
        const uint32_t textureHeightLevel0 = 1 << mipLevelCount;
        constexpr wgpu::TextureUsage kUsage =
            wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
        mTexture = Create2DTexture(device, textureWidthLevel0, textureHeightLevel0, arrayLayerCount,
                                   mipLevelCount, sampleCount, kUsage);

        mDefaultTextureViewDescriptor.dimension = wgpu::TextureViewDimension::e2DArray;
        mDefaultTextureViewDescriptor.format = kDefaultFormat;
        mDefaultTextureViewDescriptor.baseMipLevel = 0;
        mDefaultTextureViewDescriptor.mipLevelCount = mipLevelCount;
        mDefaultTextureViewDescriptor.baseArrayLayer = 0;
        mDefaultTextureViewDescriptor.arrayLayerCount = arrayLayerCount;
    }

    wgpu::Sampler mSampler;
    wgpu::Texture mTexture;
    wgpu::TextureViewDescriptor mDefaultTextureViewDescriptor;
    wgpu::ShaderModule mVSModule;
    utils::BasicRenderPass mRenderPass;
};

// Test drawing a rect with a 2D array texture.
TEST_P(TextureShaderBuiltinTests, Default2DArrayTexture) {
    // // TODO(cwallez@chromium.org) understand what the issue is
    // DAWN_SUPPRESS_TEST_IF(IsVulkan() && IsNvidia());

    // constexpr uint32_t kNumElements = 2;

    constexpr uint32_t kLayers = 3;
    constexpr uint32_t kMipLevels = 2;
    constexpr uint32_t kSampleCount = 1;
    InitTexture(kLayers, kMipLevels, kSampleCount);

    wgpu::TextureViewDescriptor descriptor;
    descriptor.dimension = wgpu::TextureViewDimension::e2DArray;
    wgpu::TextureView textureView = mTexture.CreateView(&descriptor);

    const uint32_t expected[] = {
        kLayers,
        kMipLevels,
    };

    wgpu::BufferDescriptor bufferDesc;
    // bufferDesc.size = kNumElements * sizeof(uint32_t);
    bufferDesc.size = sizeof(expected);
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer buffer = device.CreateBuffer(&bufferDesc);

    wgpu::ComputePipelineDescriptor pipelineDescriptor;
    pipelineDescriptor.compute.module = CreateDefaultComputeShaderModule(device);
    pipelineDescriptor.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDescriptor);

    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {{0, textureView}, {1, buffer}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.DispatchWorkgroups(1);
        pass.End();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_RANGE_EQ(expected, buffer, 0, sizeof(expected) / sizeof(uint32_t));
}

DAWN_INSTANTIATE_TEST(TextureShaderBuiltinTests,
                      D3D11Backend(),
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

}  // anonymous namespace
}  // namespace dawn
