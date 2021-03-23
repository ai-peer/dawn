// Copyright 2021 The Dawn Authors
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

#include "tests/unittests/validation/ValidationTest.h"

namespace {
    class ExternalTextureTest : public ValidationTest {
      public:
        wgpu::TextureDescriptor CreateDefaultTextureDescriptor() {
            wgpu::TextureDescriptor descriptor;
            descriptor.size.width = kWidth;
            descriptor.size.height = kHeight;
            descriptor.size.depth = kDefaultDepth;
            descriptor.mipLevelCount = kDefaultMipLevels;
            descriptor.sampleCount = kDefaultSampleCount;
            descriptor.dimension = wgpu::TextureDimension::e2D;
            descriptor.format = kDefaultTextureFormat;
            descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::Sampled;
            return descriptor;
        }

      private:
        static constexpr uint32_t kWidth = 32;
        static constexpr uint32_t kHeight = 32;
        static constexpr uint32_t kDefaultDepth = 1;
        static constexpr uint32_t kDefaultMipLevels = 1;
        static constexpr uint32_t kDefaultSampleCount = 1;

        static constexpr wgpu::TextureFormat kDefaultTextureFormat =
            wgpu::TextureFormat::RGBA8Unorm;
    };

    TEST_F(ExternalTextureTest, NullDescriptor) {
        ASSERT_DEVICE_ERROR(device.ImportExternalTexture(nullptr));
    }

    TEST_F(ExternalTextureTest, NullTextureView) {
        wgpu::ExternalTextureDescriptor desc;
        desc.externalTexturePlane0 = nullptr;
        ASSERT_DEVICE_ERROR(device.ImportExternalTexture(&desc));
    }

    TEST_F(ExternalTextureTest, ImportInternalTexture) {
        // Create an internal texture
        wgpu::TextureDescriptor textureDescriptor = CreateDefaultTextureDescriptor();
        wgpu::Texture internalTexture = device.CreateTexture(&textureDescriptor);

        // Create an ExternalTextureDescriptor with a view of an internal texture.
        wgpu::ExternalTextureDescriptor externalDesc;
        externalDesc.externalTexturePlane0 = internalTexture.CreateView();

        ASSERT_DEVICE_ERROR(device.ImportExternalTexture(&externalDesc));
    }

}  // namespace