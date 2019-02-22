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

#include "common/Constants.h"

#include "utils/DawnHelpers.h"

namespace {

class RenderPassDescriptorValidationTest : public ValidationTest {
  public:
    void AssertBeginRenderPassSuccess(const dawn::RenderPassDescriptor* descriptor) {
        dawn::CommandEncoder commandEncoder = TestBeginRenderPass(descriptor);
        commandEncoder.Finish();
    }
    void AssertBeginRenderPassError(const dawn::RenderPassDescriptor* descriptor) {
        dawn::CommandEncoder commandEncoder = TestBeginRenderPass(descriptor);
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

  private:
    dawn::CommandEncoder TestBeginRenderPass(const dawn::RenderPassDescriptor* descriptor) {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(descriptor);
        renderPassEncoder.EndPass();
        return commandEncoder;
    }
};

dawn::Texture CreateTexture(dawn::Device& device,
                            dawn::TextureDimension dimension,
                            dawn::TextureFormat format,
                            uint32_t width,
                            uint32_t height,
                            uint32_t arrayLayerCount,
                            uint32_t mipLevelCount) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dimension;
    descriptor.size.width = width;
    descriptor.size.height = height;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = arrayLayerCount;
    descriptor.sampleCount = 1;
    descriptor.format = format;
    descriptor.mipLevelCount = mipLevelCount;
    descriptor.usage = dawn::TextureUsageBit::OutputAttachment;

    return device.CreateTexture(&descriptor);
}

dawn::TextureView Create2DAttachment(dawn::Device& device,
                                     uint32_t width,
                                     uint32_t height,
                                     dawn::TextureFormat format) {
    dawn::Texture texture = CreateTexture(
        device, dawn::TextureDimension::e2D, format, width, height, 1, 1);
    return texture.CreateDefaultTextureView();
}

// Using BeginRenderPass with no attachments isn't valid
TEST_F(RenderPassDescriptorValidationTest, Empty) {
    {
        utils::ComboRenderPassDescriptor renderPass({}, nullptr);
        AssertBeginRenderPassError(&renderPass.desc);
    }

    {
        utils::ComboRenderPassDescriptor renderPass({nullptr, nullptr}, nullptr);
        AssertBeginRenderPassError(&renderPass.desc);
    }
}

// A render pass with only one color or one depth attachment is ok
TEST_F(RenderPassDescriptorValidationTest, OneAttachment) {
    // One color attachment
    {
        dawn::TextureView color = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
        utils::ComboRenderPassDescriptor renderPass({color});

        AssertBeginRenderPassSuccess(&renderPass.desc);
    }
    // One depth-stencil attachment
    {
        dawn::TextureView depthStencil = Create2DAttachment(device, 1, 1, dawn::TextureFormat::D32FloatS8Uint);
        utils::ComboRenderPassDescriptor renderPass({}, depthStencil);

        AssertBeginRenderPassSuccess(&renderPass.desc);
    }
}

// Test OOB color attachment indices are handled
TEST_F(RenderPassDescriptorValidationTest, ColorAttachmentOutOfBounds) {
    dawn::TextureView color = Create2DAttachment(device, 1, 1,
                                                 dawn::TextureFormat::R8G8B8A8Unorm);

    // For setting the color attachment, control case
    {
        utils::ComboRenderPassDescriptor renderPass({nullptr, nullptr, nullptr, color});
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }
    // For setting the color attachment, OOB
    {
        // We cannot use utils::ComboRenderPassDescriptor here because it only supports at most
        // kMaxColorAttachments(4) color attachments.
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = color;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {nullptr, nullptr, nullptr,
                                                                         nullptr, &colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 5;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;
        AssertBeginRenderPassError(&renderPass);
    }
}

// Attachments must have the same size
TEST_F(RenderPassDescriptorValidationTest, SizeMustMatch) {
    dawn::TextureView color1x1A = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
    dawn::TextureView color1x1B = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
    dawn::TextureView color2x2 = Create2DAttachment(device, 2, 2, dawn::TextureFormat::R8G8B8A8Unorm);

    dawn::TextureView depthStencil1x1 = Create2DAttachment(device, 1, 1, dawn::TextureFormat::D32FloatS8Uint);
    dawn::TextureView depthStencil2x2 = Create2DAttachment(device, 2, 2, dawn::TextureFormat::D32FloatS8Uint);

    // Control case: all the same size (1x1)
    {
        utils::ComboRenderPassDescriptor renderPass({color1x1A, color1x1B}, depthStencil1x1);
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }

    // One of the color attachments has a different size
    {
        utils::ComboRenderPassDescriptor renderPass({color1x1A, color2x2});
        AssertBeginRenderPassError(&renderPass.desc);
    }

    // The depth stencil attachment has a different size
    {
        utils::ComboRenderPassDescriptor renderPass({color1x1A, color1x1B}, depthStencil2x2);
        AssertBeginRenderPassError(&renderPass.desc);
    }
}

// Attachments formats must match whether they are used for color or depth-stencil
TEST_F(RenderPassDescriptorValidationTest, FormatMismatch) {
    dawn::TextureView color = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
    dawn::TextureView depthStencil = Create2DAttachment(device, 1, 1, dawn::TextureFormat::D32FloatS8Uint);

    // Using depth-stencil for color
    {
        utils::ComboRenderPassDescriptor renderPass({depthStencil});
        AssertBeginRenderPassError(&renderPass.desc);
    }

    // Using color for depth-stencil
    {
        utils::ComboRenderPassDescriptor renderPass({}, color);
        AssertBeginRenderPassError(&renderPass.desc);
    }
}

// Currently only texture views with arrayLayerCount == 1 are allowed to be color and depth stencil
// attachments
TEST_F(RenderPassDescriptorValidationTest, TextureViewLayerCountForColorAndDepthStencil) {
    constexpr uint32_t kLevelCount = 1;
    constexpr uint32_t kSize = 32;
    constexpr dawn::TextureFormat kColorFormat = dawn::TextureFormat::R8G8B8A8Unorm;
    constexpr dawn::TextureFormat kDepthStencilFormat = dawn::TextureFormat::D32FloatS8Uint;

    constexpr uint32_t kArrayLayers = 10;

    dawn::Texture colorTexture = CreateTexture(
        device, dawn::TextureDimension::e2D, kColorFormat, kSize, kSize, kArrayLayers, kLevelCount);
    dawn::Texture depthStencilTexture = CreateTexture(
        device, dawn::TextureDimension::e2D, kDepthStencilFormat, kSize, kSize, kArrayLayers,
        kLevelCount);

    dawn::TextureViewDescriptor baseDescriptor;
    baseDescriptor.dimension = dawn::TextureViewDimension::e2DArray;
    baseDescriptor.baseArrayLayer = 0;
    baseDescriptor.arrayLayerCount = kArrayLayers;
    baseDescriptor.baseMipLevel = 0;
    baseDescriptor.mipLevelCount = kLevelCount;

    // Using 2D array texture view with arrayLayerCount > 1 is not allowed for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.arrayLayerCount = 5;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({colorTextureView});
        //AssertBeginRenderPassError(&renderPass.desc);
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass.desc);
        renderPassEncoder.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }

    // Using 2D array texture view with arrayLayerCount > 1 is not allowed for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.arrayLayerCount = 5;

        dawn::TextureView depthStencilView = depthStencilTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({}, depthStencilView);
        AssertBeginRenderPassError(&renderPass.desc);
    }

    // Using 2D array texture view that covers the first layer of the texture is OK for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.baseArrayLayer = 0;
        descriptor.arrayLayerCount = 1;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({colorTextureView});
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }

    // Using 2D array texture view that covers the first layer is OK for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.baseArrayLayer = 0;
        descriptor.arrayLayerCount = 1;

        dawn::TextureView depthStencilView = depthStencilTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({}, depthStencilView);
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }

    // Using 2D array texture view that covers the last layer is OK for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.baseArrayLayer = kArrayLayers - 1;
        descriptor.arrayLayerCount = 1;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({colorTextureView});
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }

    // Using 2D array texture view that covers the last layer is OK for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.baseArrayLayer = kArrayLayers - 1;
        descriptor.arrayLayerCount = 1;

        dawn::TextureView depthStencilView = depthStencilTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({}, depthStencilView);
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }
}

// Only 2D texture views with mipLevelCount == 1 are allowed to be color attachments
TEST_F(RenderPassDescriptorValidationTest, TextureViewLevelCountForColorAndDepthStencil) {
    constexpr uint32_t kArrayLayers = 1;
    constexpr uint32_t kSize = 32;
    constexpr dawn::TextureFormat kColorFormat = dawn::TextureFormat::R8G8B8A8Unorm;
    constexpr dawn::TextureFormat kDepthStencilFormat = dawn::TextureFormat::D32FloatS8Uint;

    constexpr uint32_t kLevelCount = 4;

    dawn::Texture colorTexture = CreateTexture(
        device, dawn::TextureDimension::e2D, kColorFormat, kSize, kSize, kArrayLayers, kLevelCount);
    dawn::Texture depthStencilTexture = CreateTexture(
        device, dawn::TextureDimension::e2D, kDepthStencilFormat, kSize, kSize, kArrayLayers,
        kLevelCount);

    dawn::TextureViewDescriptor baseDescriptor;
    baseDescriptor.dimension = dawn::TextureViewDimension::e2D;
    baseDescriptor.baseArrayLayer = 0;
    baseDescriptor.arrayLayerCount = kArrayLayers;
    baseDescriptor.baseMipLevel = 0;
    baseDescriptor.mipLevelCount = kLevelCount;

    // Using 2D texture view with mipLevelCount > 1 is not allowed for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.mipLevelCount = 2;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({colorTextureView});
        AssertBeginRenderPassError(&renderPass.desc);
    }

    // Using 2D texture view with mipLevelCount > 1 is not allowed for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.mipLevelCount = 2;

        dawn::TextureView depthStencilView = depthStencilTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({}, depthStencilView);
        AssertBeginRenderPassError(&renderPass.desc);
    }

    // Using 2D texture view that covers the first level of the texture is OK for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.baseMipLevel = 0;
        descriptor.mipLevelCount = 1;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({colorTextureView});
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }

    // Using 2D texture view that covers the first level is OK for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.baseMipLevel = 0;
        descriptor.mipLevelCount = 1;

        dawn::TextureView depthStencilView = depthStencilTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({}, depthStencilView);
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }

    // Using 2D texture view that covers the last level is OK for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.baseMipLevel = kLevelCount - 1;
        descriptor.mipLevelCount = 1;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({colorTextureView});
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }

    // Using 2D texture view that covers the last level is OK for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.baseMipLevel = kLevelCount - 1;
        descriptor.mipLevelCount = 1;

        dawn::TextureView depthStencilView = depthStencilTexture.CreateTextureView(&descriptor);
        utils::ComboRenderPassDescriptor renderPass({}, depthStencilView);
        AssertBeginRenderPassSuccess(&renderPass.desc);
    }
}

// Tests on the resolve target of RenderPassColorAttachmentDescriptor.
// TODO(jiawei.shao@intel.com): add more tests when we support multisample color attachments.
TEST_F(RenderPassDescriptorValidationTest, ResolveTarget) {
    constexpr uint32_t kArrayLayers = 1;
    constexpr uint32_t kSize = 32;
    constexpr dawn::TextureFormat kColorFormat = dawn::TextureFormat::R8G8B8A8Unorm;

    constexpr uint32_t kLevelCount = 1;

    dawn::Texture colorTexture = CreateTexture(
        device, dawn::TextureDimension::e2D, kColorFormat, kSize, kSize, kArrayLayers, kLevelCount);

    dawn::Texture resolveTexture = CreateTexture(
        device, dawn::TextureDimension::e2D, kColorFormat, kSize, kSize, kArrayLayers, kLevelCount);

    // It is not allowed to set resolve target when the sample count of the color attachment is 1.
    {
        dawn::TextureView colorTextureView = colorTexture.CreateDefaultTextureView();
        dawn::TextureView resolveTargetTextureView = resolveTexture.CreateDefaultTextureView();

        utils::ComboRenderPassDescriptor renderPass({colorTextureView});
        renderPass.colorAttachmentsInfoPtr[0]->resolveTarget = resolveTargetTextureView;
        AssertBeginRenderPassError(&renderPass.desc);
    }
}

// TODO(cwallez@chromium.org): Constraints on attachment aliasing?

} // anonymous namespace
