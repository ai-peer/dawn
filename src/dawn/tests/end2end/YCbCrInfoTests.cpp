// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <vector>

#include "dawn/native/VulkanBackend.h"
#include "dawn/tests/DawnTest.h"

#if DAWN_PLATFORM_IS(ANDROID)
#include <android/hardware_buffer.h>
#endif  // DAWN_PLATFORM_IS(ANDROID)

namespace dawn {
namespace {

constexpr uint32_t kWidth = 32u;
constexpr uint32_t kHeight = 32u;
constexpr uint32_t kDefaultMipLevels = 6u;
constexpr wgpu::TextureFormat kDefaultTextureFormat = wgpu::TextureFormat::RGBA8Unorm;

wgpu::Texture Create2DArrayTexture(wgpu::Device& device,
                                   uint32_t arrayLayerCount,
                                   uint32_t width = kWidth,
                                   uint32_t height = kHeight,
                                   uint32_t mipLevelCount = kDefaultMipLevels,
                                   uint32_t sampleCount = 1) {
    wgpu::TextureDescriptor descriptor;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size.width = width;
    descriptor.size.height = height;
    descriptor.size.depthOrArrayLayers = arrayLayerCount;
    descriptor.sampleCount = sampleCount;
    descriptor.format = kDefaultTextureFormat;
    descriptor.mipLevelCount = mipLevelCount;
    descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment;
    return device.CreateTexture(&descriptor);
}

wgpu::TextureViewDescriptor CreateDefaultViewDescriptor(wgpu::TextureViewDimension dimension) {
    wgpu::TextureViewDescriptor descriptor;
    descriptor.format = kDefaultTextureFormat;
    descriptor.dimension = dimension;
    descriptor.baseMipLevel = 0;
    if (dimension != wgpu::TextureViewDimension::e1D) {
        descriptor.mipLevelCount = kDefaultMipLevels;
    }
    descriptor.baseArrayLayer = 0;
    descriptor.arrayLayerCount = 1;
    return descriptor;
}

class YCbCrInfoTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        // Skip all tests if ycbcr sampler feature is not supported
        DAWN_TEST_UNSUPPORTED_IF(!SupportsFeatures({wgpu::FeatureName::YCbCrVulkanSamplers}));
    }

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> requiredFeatures = {};
        if (SupportsFeatures({wgpu::FeatureName::StaticSamplers}) &&
            SupportsFeatures({wgpu::FeatureName::YCbCrVulkanSamplers})) {
            requiredFeatures.push_back(wgpu::FeatureName::YCbCrVulkanSamplers);
        }
        return requiredFeatures;
    }
};

// Test that it is possible to create the sampler with ycbcr vulkan descriptor.
TEST_P(YCbCrInfoTest, YCbCrSamplerValidWhenFeatureEnabled) {
    wgpu::SamplerDescriptor samplerDesc = {};
    native::vulkan::YCbCrVulkanDescriptor yCbCrDesc = {};
    yCbCrDesc.vulkanYCbCrInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    yCbCrDesc.vulkanYCbCrInfo.pNext = nullptr;
    yCbCrDesc.vulkanYCbCrInfo.format = VK_FORMAT_R8G8B8A8_UNORM;

    samplerDesc.nextInChain = &yCbCrDesc;

    device.CreateSampler(&samplerDesc);
}

// Test that it is possible to create the sampler with ycbcr vulkan descriptor with only vulkan
// format set.
TEST_P(YCbCrInfoTest, YCbCrSamplerValidWithOnlyVkFormat) {
    wgpu::SamplerDescriptor samplerDesc = {};
    native::vulkan::YCbCrVulkanDescriptor yCbCrDesc = {};
    yCbCrDesc.vulkanYCbCrInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    yCbCrDesc.vulkanYCbCrInfo.pNext = nullptr;
    yCbCrDesc.vulkanYCbCrInfo.format = VK_FORMAT_R8G8B8A8_UNORM;

#if DAWN_PLATFORM_IS(ANDROID)
    VkExternalFormatANDROID vulkanExternalFormat = {};
    vulkanExternalFormat.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    vulkanExternalFormat.pNext = nullptr;
    // format is set as VK_FORMAT.
    vulkanExternalFormat.externalFormat = 0;

    yCbCrDesc.vulkanYCbCrInfo.pNext = &vulkanExternalFormat;
#endif  // DAWN_PLATFORM_IS(ANDROID)

    samplerDesc.nextInChain = &yCbCrDesc;

    device.CreateSampler(&samplerDesc);
}

// Test that it is possible to create the sampler with ycbcr vulkan descriptor with only external
// format set.
TEST_P(YCbCrInfoTest, YCbCrSamplerValidWithOnlyExternalFormat) {
    wgpu::SamplerDescriptor samplerDesc = {};
    native::vulkan::YCbCrVulkanDescriptor yCbCrDesc = {};
    yCbCrDesc.vulkanYCbCrInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    yCbCrDesc.vulkanYCbCrInfo.pNext = nullptr;
    // format is set as externalFormat.
    yCbCrDesc.vulkanYCbCrInfo.format = VK_FORMAT_UNDEFINED;

#if DAWN_PLATFORM_IS(ANDROID)
    VkExternalFormatANDROID vulkanExternalFormat = {};
    vulkanExternalFormat.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    vulkanExternalFormat.pNext = nullptr;
    vulkanExternalFormat.externalFormat = 5;

    yCbCrDesc.vulkanYCbCrInfo.pNext = &vulkanExternalFormat;
#endif  // DAWN_PLATFORM_IS(ANDROID)

    samplerDesc.nextInChain = &yCbCrDesc;

    device.CreateSampler(&samplerDesc);
}

// Test that it is NOT possible to create the sampler with ycbcr vulkan descriptor and no format
// set.
TEST_P(YCbCrInfoTest, YCbCrSamplerInvalidWithNoFormat) {
    wgpu::SamplerDescriptor samplerDesc = {};
    native::vulkan::YCbCrVulkanDescriptor yCbCrDesc = {};
    yCbCrDesc.vulkanYCbCrInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    yCbCrDesc.vulkanYCbCrInfo.pNext = nullptr;
    yCbCrDesc.vulkanYCbCrInfo.format = VK_FORMAT_UNDEFINED;

#if DAWN_PLATFORM_IS(ANDROID)
    VkExternalFormatANDROID vulkanExternalFormat = {};
    vulkanExternalFormat.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    vulkanExternalFormat.pNext = nullptr;
    vulkanExternalFormat.externalFormat = 0;

    yCbCrDesc.vulkanYCbCrInfo.pNext = &vulkanExternalFormat;
#endif  // DAWN_PLATFORM_IS(ANDROID)

    samplerDesc.nextInChain = &yCbCrDesc;

    ASSERT_DEVICE_ERROR(device.CreateSampler(&samplerDesc));
}

// Test that it is possible to create texture view with ycbcr vulkan descriptor.
TEST_P(YCbCrInfoTest, YCbCrTextureViewValidWhenFeatureEnabled) {
    wgpu::Texture texture = Create2DArrayTexture(device, 1);

    wgpu::TextureViewDescriptor descriptor =
        CreateDefaultViewDescriptor(wgpu::TextureViewDimension::e2D);
    descriptor.arrayLayerCount = 1;

    native::vulkan::YCbCrVulkanDescriptor yCbCrDesc = {};
    yCbCrDesc.vulkanYCbCrInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    yCbCrDesc.vulkanYCbCrInfo.pNext = nullptr;
    yCbCrDesc.vulkanYCbCrInfo.format = VK_FORMAT_R8G8B8A8_UNORM;

    descriptor.nextInChain = &yCbCrDesc;

    texture.CreateView(&descriptor);
}

// Test that it is possible to create texture view with ycbcr vulkan descriptor with only vulkan
// format set.
TEST_P(YCbCrInfoTest, YCbCrTextureViewValidWithOnlyVkFormat) {
    wgpu::Texture texture = Create2DArrayTexture(device, 1);

    wgpu::TextureViewDescriptor descriptor =
        CreateDefaultViewDescriptor(wgpu::TextureViewDimension::e2D);
    descriptor.arrayLayerCount = 1;

    native::vulkan::YCbCrVulkanDescriptor yCbCrDesc = {};
    yCbCrDesc.vulkanYCbCrInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    yCbCrDesc.vulkanYCbCrInfo.pNext = nullptr;
    yCbCrDesc.vulkanYCbCrInfo.format = VK_FORMAT_R8G8B8A8_UNORM;

#if DAWN_PLATFORM_IS(ANDROID)
    VkExternalFormatANDROID vulkanExternalFormat = {};
    vulkanExternalFormat.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    vulkanExternalFormat.pNext = nullptr;
    // format is set as VK_FORMAT.
    vulkanExternalFormat.externalFormat = 0;

    yCbCrDesc.vulkanYCbCrInfo.pNext = &vulkanExternalFormat;
#endif  // DAWN_PLATFORM_IS(ANDROID)

    descriptor.nextInChain = &yCbCrDesc;

    texture.CreateView(&descriptor);
}

// Test that it is possible to create texture view with ycbcr vulkan descriptor with only external
// format set.
TEST_P(YCbCrInfoTest, YCbCrTextureViewValidWithOnlyExternalFormat) {
    wgpu::Texture texture = Create2DArrayTexture(device, 1);

    wgpu::TextureViewDescriptor descriptor =
        CreateDefaultViewDescriptor(wgpu::TextureViewDimension::e2D);
    descriptor.arrayLayerCount = 1;

    native::vulkan::YCbCrVulkanDescriptor yCbCrDesc = {};
    yCbCrDesc.vulkanYCbCrInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    yCbCrDesc.vulkanYCbCrInfo.pNext = nullptr;
    // format is set as externalFormat.
    yCbCrDesc.vulkanYCbCrInfo.format = VK_FORMAT_UNDEFINED;

#if DAWN_PLATFORM_IS(ANDROID)
    VkExternalFormatANDROID vulkanExternalFormat = {};
    vulkanExternalFormat.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    vulkanExternalFormat.pNext = nullptr;
    vulkanExternalFormat.externalFormat = 5;

    yCbCrDesc.vulkanYCbCrInfo.pNext = &vulkanExternalFormat;
#endif  // DAWN_PLATFORM_IS(ANDROID)

    descriptor.nextInChain = &yCbCrDesc;

    texture.CreateView(&descriptor);
}

// Test that it is NOT possible to create texture view with ycbcr vulkan descriptor and no format
// set.
TEST_P(YCbCrInfoTest, YCbCrTextureViewInvalidWithNoFormat) {
    wgpu::Texture texture = Create2DArrayTexture(device, 1);

    wgpu::TextureViewDescriptor descriptor =
        CreateDefaultViewDescriptor(wgpu::TextureViewDimension::e2D);
    descriptor.arrayLayerCount = 1;

    native::vulkan::YCbCrVulkanDescriptor yCbCrDesc = {};
    yCbCrDesc.vulkanYCbCrInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    yCbCrDesc.vulkanYCbCrInfo.pNext = nullptr;
    yCbCrDesc.vulkanYCbCrInfo.format = VK_FORMAT_UNDEFINED;

#if DAWN_PLATFORM_IS(ANDROID)
    VkExternalFormatANDROID vulkanExternalFormat = {};
    vulkanExternalFormat.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    vulkanExternalFormat.pNext = nullptr;
    vulkanExternalFormat.externalFormat = 0;

    yCbCrDesc.vulkanYCbCrInfo.pNext = &vulkanExternalFormat;
#endif  // DAWN_PLATFORM_IS(ANDROID)

    descriptor.nextInChain = &yCbCrDesc;

    ASSERT_DEVICE_ERROR(texture.CreateView(&descriptor));
}

DAWN_INSTANTIATE_TEST(YCbCrInfoTest, VulkanBackend());

}  // anonymous namespace
}  // namespace dawn
