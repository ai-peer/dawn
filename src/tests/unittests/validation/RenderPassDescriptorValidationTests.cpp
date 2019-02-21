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

namespace {

class RenderPassDescriptorValidationTest : public ValidationTest {
public:
    bool CanCreateRenderPassEncoder(const dawn::RenderPassDescriptor* descriptor) {
        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(descriptor);
        return renderPassEncoder.Get() != nullptr;
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
    ASSERT_FALSE(CanCreateRenderPassEncoder(nullptr));
}

// A render pass with only one color or one depth attachment is ok
TEST_F(RenderPassDescriptorValidationTest, OneAttachment) {
    // One color attachment
    {
        dawn::TextureView color = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = color;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;
        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }
    // One depth-stencil attachment
    {
        dawn::TextureView depthStencil = Create2DAttachment(device, 1, 1, dawn::TextureFormat::D32FloatS8Uint);
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = depthStencil;
        depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 0;
        renderPass.colorAttachments = nullptr;
        renderPass.depthStencilAttachment = &depthStencilAttachment;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }
}

// Test OOB color attachment indices are handled
TEST_F(RenderPassDescriptorValidationTest, ColorAttachmentOutOfBounds) {
    // For setting the color attachment, control case
    {
        dawn::TextureView color = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
        dawn::RenderPassColorAttachmentDescriptor colorAttachments[kMaxColorAttachments];
        colorAttachments[kMaxColorAttachments - 1].attachment = color;
        colorAttachments[kMaxColorAttachments - 1].resolveTarget = nullptr;
        colorAttachments[kMaxColorAttachments - 1].clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachments[kMaxColorAttachments - 1].loadOp = dawn::LoadOp::Clear;
        colorAttachments[kMaxColorAttachments - 1].storeOp = dawn::StoreOp::Store;
        dawn::RenderPassColorAttachmentDescriptor* colorAttachmentInfo[] =
            {nullptr, nullptr, nullptr, &colorAttachments[kMaxColorAttachments - 1]};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = kMaxColorAttachments;
        renderPass.colorAttachments = colorAttachmentInfo;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }
    // For setting the color attachment, OOB
    {
        dawn::TextureView color = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
        dawn::RenderPassColorAttachmentDescriptor colorAttachments[kMaxColorAttachments + 1];
        colorAttachments[kMaxColorAttachments].attachment = color;
        colorAttachments[kMaxColorAttachments].resolveTarget = nullptr;
        colorAttachments[kMaxColorAttachments].clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachments[kMaxColorAttachments].loadOp = dawn::LoadOp::Clear;
        colorAttachments[kMaxColorAttachments].storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachmentInfo[] =
            {nullptr, nullptr, nullptr, nullptr, &colorAttachments[kMaxColorAttachments]};

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = kMaxColorAttachments + 1;
        renderPass.colorAttachments = colorAttachmentInfo;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
    }
}

// Attachments must have the same size
TEST_F(RenderPassDescriptorValidationTest, SizeMustMatch) {
    dawn::TextureView color1x1A = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
    dawn::TextureView color1x1B = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
    dawn::TextureView color2x2 = Create2DAttachment(device, 2, 2, dawn::TextureFormat::R8G8B8A8Unorm);

    dawn::RenderPassColorAttachmentDescriptor colorAttachment1x1A;
    colorAttachment1x1A.attachment = color1x1A;
    colorAttachment1x1A.resolveTarget = nullptr;
    colorAttachment1x1A.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    colorAttachment1x1A.loadOp = dawn::LoadOp::Clear;
    colorAttachment1x1A.storeOp = dawn::StoreOp::Store;

    dawn::RenderPassColorAttachmentDescriptor colorAttachment1x1B;
    colorAttachment1x1B.attachment = color1x1B;
    colorAttachment1x1B.resolveTarget = nullptr;
    colorAttachment1x1B.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    colorAttachment1x1B.loadOp = dawn::LoadOp::Clear;
    colorAttachment1x1B.storeOp = dawn::StoreOp::Store;

    dawn::RenderPassColorAttachmentDescriptor colorAttachment2x2;
    colorAttachment2x2.attachment = color2x2;
    colorAttachment2x2.resolveTarget = nullptr;
    colorAttachment2x2.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    colorAttachment2x2.loadOp = dawn::LoadOp::Clear;
    colorAttachment2x2.storeOp = dawn::StoreOp::Store;

    dawn::TextureView depthStencil1x1 = Create2DAttachment(device, 1, 1, dawn::TextureFormat::D32FloatS8Uint);
    dawn::TextureView depthStencil2x2 = Create2DAttachment(device, 2, 2, dawn::TextureFormat::D32FloatS8Uint);

    dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment1x1;
    depthStencilAttachment1x1.attachment = depthStencil1x1;
    depthStencilAttachment1x1.depthLoadOp = dawn::LoadOp::Clear;
    depthStencilAttachment1x1.stencilLoadOp = dawn::LoadOp::Clear;
    depthStencilAttachment1x1.clearDepth = 1.0f;
    depthStencilAttachment1x1.clearStencil = 0;
    depthStencilAttachment1x1.depthStoreOp = dawn::StoreOp::Store;
    depthStencilAttachment1x1.stencilStoreOp = dawn::StoreOp::Store;

    dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment2x2;
    depthStencilAttachment2x2.attachment = depthStencil2x2;
    depthStencilAttachment2x2.depthLoadOp = dawn::LoadOp::Clear;
    depthStencilAttachment2x2.stencilLoadOp = dawn::LoadOp::Clear;
    depthStencilAttachment2x2.clearDepth = 1.0f;
    depthStencilAttachment2x2.clearStencil = 0;
    depthStencilAttachment2x2.depthStoreOp = dawn::StoreOp::Store;
    depthStencilAttachment2x2.stencilStoreOp = dawn::StoreOp::Store;

    // Control case: all the same size (1x1)
    {
        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment1x1A,
                                                                         &colorAttachment1x1B};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 2;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }

    // One of the color attachments has a different size
    {
        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment1x1A,
                                                                         &colorAttachment2x2};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 2;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass)); 
    }

    // The depth stencil attachment has a different size
    {
        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment1x1A,
                                                                         &colorAttachment1x1B};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 2;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = &depthStencilAttachment2x2;

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
    }
}

// Attachments formats must match whether they are used for color or depth-stencil
TEST_F(RenderPassDescriptorValidationTest, FormatMismatch) {
    dawn::TextureView color = Create2DAttachment(device, 1, 1, dawn::TextureFormat::R8G8B8A8Unorm);
    dawn::TextureView depthStencil = Create2DAttachment(device, 1, 1, dawn::TextureFormat::D32FloatS8Uint);

    // Using depth-stencil for color
    {
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = depthStencil;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using color for depth-stencil
    {
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = color;
        depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 0;
        renderPass.colorAttachments = nullptr;
        renderPass.depthStencilAttachment = &depthStencilAttachment;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
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
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = colorTextureView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D array texture view with arrayLayerCount > 1 is not allowed for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.arrayLayerCount = 5;

        dawn::TextureView depthStencilView = depthStencilTexture.CreateTextureView(&descriptor);
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = depthStencilView;
        depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 0;
        renderPass.colorAttachments = nullptr;
        renderPass.depthStencilAttachment = &depthStencilAttachment;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D array texture view that covers the first layer of the texture is OK for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.baseArrayLayer = 0;
        descriptor.arrayLayerCount = 1;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = colorTextureView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D array texture view that covers the first layer is OK for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.baseArrayLayer = 0;
        descriptor.arrayLayerCount = 1;

        dawn::TextureView depthStencilTextureView =
            depthStencilTexture.CreateTextureView(&descriptor);
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = depthStencilTextureView;
        depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 0;
        renderPass.colorAttachments = nullptr;
        renderPass.depthStencilAttachment = &depthStencilAttachment;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D array texture view that covers the last layer is OK for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.baseArrayLayer = kArrayLayers - 1;
        descriptor.arrayLayerCount = 1;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = colorTextureView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D array texture view that covers the last layer is OK for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.baseArrayLayer = kArrayLayers - 1;
        descriptor.arrayLayerCount = 1;

        dawn::TextureView depthStencilTextureView =
            depthStencilTexture.CreateTextureView(&descriptor);
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = depthStencilTextureView;
        depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 0;
        renderPass.colorAttachments = nullptr;
        renderPass.depthStencilAttachment = &depthStencilAttachment;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
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
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = colorTextureView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D texture view with mipLevelCount > 1 is not allowed for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.mipLevelCount = 2;

        dawn::TextureView depthStencilView = depthStencilTexture.CreateTextureView(&descriptor);
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = depthStencilView;
        depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 0;
        renderPass.colorAttachments = nullptr;
        renderPass.depthStencilAttachment = &depthStencilAttachment;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D texture view that covers the first level of the texture is OK for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.baseMipLevel = 0;
        descriptor.mipLevelCount = 1;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = colorTextureView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D texture view that covers the first level is OK for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.baseMipLevel = 0;
        descriptor.mipLevelCount = 1;

        dawn::TextureView depthStencilTextureView =
            depthStencilTexture.CreateTextureView(&descriptor);
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = depthStencilTextureView;
        depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 0;
        renderPass.colorAttachments = nullptr;
        renderPass.depthStencilAttachment = &depthStencilAttachment;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D texture view that covers the last level is OK for color
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kColorFormat;
        descriptor.baseMipLevel = kLevelCount - 1;
        descriptor.mipLevelCount = 1;

        dawn::TextureView colorTextureView = colorTexture.CreateTextureView(&descriptor);
        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = colorTextureView;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
    }

    // Using 2D texture view that covers the last level is OK for depth stencil
    {
        dawn::TextureViewDescriptor descriptor = baseDescriptor;
        descriptor.format = kDepthStencilFormat;
        descriptor.baseMipLevel = kLevelCount - 1;
        descriptor.mipLevelCount = 1;

        dawn::TextureView depthStencilTextureView =
            depthStencilTexture.CreateTextureView(&descriptor);
        dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
        depthStencilAttachment.attachment = depthStencilTextureView;
        depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
        depthStencilAttachment.clearDepth = 1.0f;
        depthStencilAttachment.clearStencil = 0;
        depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
        depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 0;
        renderPass.colorAttachments = nullptr;
        renderPass.depthStencilAttachment = &depthStencilAttachment;

        ASSERT_TRUE(CanCreateRenderPassEncoder(&renderPass));
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

        dawn::RenderPassColorAttachmentDescriptor colorAttachment;
        colorAttachment.attachment = colorTextureView;
        colorAttachment.resolveTarget = resolveTargetTextureView;
        colorAttachment.clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorAttachment.loadOp = dawn::LoadOp::Clear;
        colorAttachment.storeOp = dawn::StoreOp::Store;

        dawn::RenderPassColorAttachmentDescriptor* colorAttachments[] = {&colorAttachment};
        dawn::RenderPassDescriptor renderPass;
        renderPass.colorAttachmentCount = 1;
        renderPass.colorAttachments = colorAttachments;
        renderPass.depthStencilAttachment = nullptr;

        ASSERT_FALSE(CanCreateRenderPassEncoder(&renderPass));
    }
}

// TODO(cwallez@chromium.org): Constraints on attachment aliasing?

} // anonymous namespace
