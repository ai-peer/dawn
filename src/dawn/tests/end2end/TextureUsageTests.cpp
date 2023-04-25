// Copyright 2023 The Dawn Authors
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

#include <vector>

#include "dawn/common/Assert.h"
#include "dawn/tests/DawnTest.h"

// Tests backends on which memoryless textures are not supported.
class MemorylessTexturesNotSupportedTest : public DawnTest {};

TEST_P(MemorylessTexturesNotSupportedTest, TransientAttachmentCausesError) {
    wgpu::TextureDescriptor textureDesc;
    textureDesc.usage = wgpu::TextureUsage::TransientAttachment;
    textureDesc.size = {1024, 1, 1};
    textureDesc.format = wgpu::TextureFormat::R8Unorm;
    ASSERT_DEVICE_ERROR(device.CreateTexture(&textureDesc));
}

DAWN_INSTANTIATE_TEST(MemorylessTexturesNotSupportedTest,
                      D3D12Backend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

// Tests backends on which memoryless textures are supported.
class MemorylessTexturesSupportedTest : public DawnTest {
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        return {wgpu::FeatureName::MemorylessTextures};
    }
};

TEST_P(MemorylessTexturesSupportedTest, TransientAttachmentSupported) {
    wgpu::TextureDescriptor textureDesc;
    textureDesc.usage = wgpu::TextureUsage::TransientAttachment;
    textureDesc.size = {1024, 1, 1};
    textureDesc.format = wgpu::TextureFormat::R8Unorm;
    wgpu::Texture texture = device.CreateTexture(&textureDesc);
    ASSERT(texture);
}

DAWN_INSTANTIATE_TEST(MemorylessTexturesSupportedTest, MetalBackend());
