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

#include "tests/unittests/validation/ValidationTest.h"

namespace {

class TextureViewValidationTest : public ValidationTest {
};

constexpr uint32_t kWidth = 32u;
constexpr uint32_t kHeight = 32u;
constexpr uint32_t kDefaultArrayLayers = 6u;
constexpr uint32_t kDefaultMipLevels = 6u;

constexpr dawn::TextureFormat kDefaultTextureFormat = dawn::TextureFormat::R8G8B8A8Unorm;

dawn::Texture Create2DArrayTexture(dawn::Device& device, uint32_t arrayLayers = kDefaultArrayLayers, dawn::TextureFormat format = kDefaultTextureFormat) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = kWidth;
    descriptor.size.height = kHeight;
    descriptor.size.depth = 1;
    descriptor.arrayLayer = arrayLayers;
    descriptor.format = format;
    descriptor.mipLevel = kDefaultMipLevels;
    descriptor.usage = dawn::TextureUsageBit::Sampled;
    return device.CreateTexture(&descriptor);
}

// Test creating texture view on a 2D non-array texture
TEST_F(TextureViewValidationTest, CreateTextureViewOnTexture2D) {
    dawn::Texture texture = Create2DArrayTexture(device, 1);

    dawn::TextureViewDescriptor descriptor;
    descriptor.format = kDefaultTextureFormat;
    descriptor.baseMipLevel = 0;
    descriptor.levelCount = kDefaultMipLevels;
    descriptor.baseArrayLayer = 0;

    // It is OK to create a 2D texture view on a 2D texture.
    {
        descriptor.dimension = dawn::TextureViewDimension::e2D;
        descriptor.layerCount = 1;
        texture.CreateTextureView(&descriptor);
    }

    // It is an error to specify the layer count of the texture view > 1 when texture view dimension is 2D.
    {
        descriptor.layerCount = 2;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }

    // It is OK to create a 1-layer 2D array texture view on a 2D texture.
    {
        descriptor.dimension = dawn::TextureViewDimension::e2DArray;
        descriptor.layerCount = 1;
        texture.CreateTextureView(&descriptor);
    }

    // It is an error to specify levelCount == 0.
    {
        descriptor.levelCount = 0;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }

    // It is an error to make the mip level out of range.
    {
        descriptor.baseMipLevel = kDefaultMipLevels - 1;
        descriptor.levelCount = 2;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }
}

// Test creating texture view on a 2D array texture
TEST_F(TextureViewValidationTest, CreateTextureViewOnTexture2DArray) {
    dawn::Texture texture = Create2DArrayTexture(device);

    dawn::TextureViewDescriptor descriptor;
    descriptor.format = kDefaultTextureFormat;
    descriptor.baseMipLevel = 0;
    descriptor.levelCount = kDefaultMipLevels;
    descriptor.baseArrayLayer = 0;

    // It is OK to create a 2D texture view on a 2D array texture.
    {
        descriptor.dimension = dawn::TextureViewDimension::e2D;
        descriptor.layerCount = 1;
        texture.CreateTextureView(&descriptor);
    }

    // It is OK to create a 2D array texture view on a 2D array texture.
    {
        descriptor.dimension = dawn::TextureViewDimension::e2DArray;
        descriptor.layerCount = kDefaultArrayLayers;
        texture.CreateTextureView(&descriptor);
    }

    // It is an error to specify layerCount == 0.
    {
        descriptor.layerCount = 0;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }

    // It is an error to make the array layer out of range.
    {
        descriptor.layerCount = kDefaultArrayLayers + 1;
        ASSERT_DEVICE_ERROR(texture.CreateTextureView(&descriptor));
    }
}

}