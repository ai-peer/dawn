// Copyright 2019 The Dawn Authors
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
#include "utils/DawnHelpers.h"

class NonzeroTextureCreationTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
    }
};

// Test that texture clears to 1's because toggle is enabled.
TEST_P(NonzeroTextureCreationTests, TextureCreationClearsOneBits) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = 128;
    descriptor.size.height = 128;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
    descriptor.mipLevelCount = 1;
    descriptor.usage = dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
    dawn::Texture texture = device.CreateTexture(&descriptor);

    RGBA8 filled(255, 255, 255, 255);
    EXPECT_PIXEL_RGBA8_EQ(filled, texture, 0, 0);
}

// TODO(natlee@microsoft.com): This test fails right now because texture
// does not get lazy cleared to 0's and old memory is getting recycled.
// Test that texture clears to 0's because toggle is disabled.
TEST_P(NonzeroTextureCreationTests, TextureCreationClearsZeroBits) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = 128;
    descriptor.size.height = 128;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.format = dawn::TextureFormat::R8G8B8A8Unorm;
    descriptor.mipLevelCount = 1;
    descriptor.usage = dawn::TextureUsageBit::OutputAttachment | dawn::TextureUsageBit::TransferSrc;
    dawn::Texture texture = device.CreateTexture(&descriptor);

    // RGBA8 unfilled(0, 0, 0, 0);
    // EXPECT_PIXEL_RGBA8_EQ(unfilled, texture, 0, 0);
}

DAWN_INSTANTIATE_TEST(NonzeroTextureCreationTests,
                      ForceWorkaround(VulkanBackend,
                                      "nonzero_clear_resources_on_creation_for_testing"));
