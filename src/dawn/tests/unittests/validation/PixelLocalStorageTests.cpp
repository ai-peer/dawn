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

#include "dawn/tests/unittests/validation/ValidationTest.h"

#include "dawn/common/NonCopyable.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

class PixelLocalStorageDisabledTest : public ValidationTest {};

// Check that creating a StorageAttachment texture is disallowed without the extension.
TEST_F(PixelLocalStorageDisabledTest, StorageAttachmentTextureNotAllowed) {
    wgpu::TextureDescriptor desc;
    desc.size = {1, 1, 1};
    desc.format = wgpu::TextureFormat::RGBA8Unorm;
    desc.usage = wgpu::TextureUsage::TextureBinding;

    // Control case: creating the texture without StorageAttachment is allowed.
    device.CreateTexture(&desc);

    // Error case: creating the texture without StorageAttachment is disallowed.
    desc.usage = wgpu::TextureUsage::StorageAttachment;
    ASSERT_DEVICE_ERROR(device.CreateTexture(&desc));
}

// Check that creating a pipeline layout with a PipelineLayoutPixelLocalStorage is disallowed
// without the extension.
TEST_F(PixelLocalStorageDisabledTest, PipelineLayoutPixelLocalStorageDisallowed) {
    wgpu::PipelineLayoutDescriptor desc;
    desc.bindGroupLayoutCount = 0;

    // Control case: creating the pipeline layout without the PLS is allowed.
    device.CreatePipelineLayout(&desc);

    // Error case: creating the pipeline layout with a PLS is disallowed even if it is empty.
    wgpu::PipelineLayoutPixelLocalStorage pls;
    pls.totalPixelLocalStorageSize = 0;
    pls.storageAttachmentCount = 0;
    desc.nextInChain = &pls;

    ASSERT_DEVICE_ERROR(device.CreatePipelineLayout(&desc));
}

// Check that a render pass with a RenderPassPixelLocalStorage is disallowed without the extension.
TEST_F(PixelLocalStorageDisabledTest, RenderPassPixelLocalStorageDisallowed) {
    utils::BasicRenderPass rp = utils::CreateBasicRenderPass(device, 1, 1);

    // Control case: beginning the render pass without the PLS is allowed.
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rp.renderPassInfo);
        pass.End();
        encoder.Finish();
    }

    // Error case: beginning the render pass without the PLS is disallowed, even if it is empty.
    {
        wgpu::RenderPassPixelLocalStorage pls;
        pls.totalPixelLocalStorageSize = 0;
        pls.storageAttachmentCount = 0;
        rp.renderPassInfo.nextInChain = &pls;

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rp.renderPassInfo);
        pass.End();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Check that PixelLocalStorageBarrier() is disallowed without the extension.
TEST_F(PixelLocalStorageDisabledTest, PixelLocalStorageBarrierDisallowed) {
    utils::BasicRenderPass rp = utils::CreateBasicRenderPass(device, 1, 1);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rp.renderPassInfo);
    pass.PixelLocalStorageBarrier();
    pass.End();
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

struct OffsetAndFormat {
    uint64_t offset;
    wgpu::TextureFormat format;
};
struct PLSSpec {
    uint64_t totalSize;
    std::vector<OffsetAndFormat> attachments;
    bool active = true;
};

constexpr std::array<wgpu::TextureFormat, 3> kStorageAttachmentFormats = {
    wgpu::TextureFormat::R32Float,
    wgpu::TextureFormat::R32Uint,
    wgpu::TextureFormat::R32Sint,
};
bool IsStorageAttachmentFormat(wgpu::TextureFormat format) {
    return std::find(kStorageAttachmentFormats.begin(), kStorageAttachmentFormats.end(), format) !=
           kStorageAttachmentFormats.end();
}

struct ComboTestPLSRenderPassDescriptor : NonMovable {
    std::array<wgpu::RenderPassStorageAttachment, 8> storageAttachments;
    wgpu::RenderPassPixelLocalStorage pls;
    wgpu::RenderPassColorAttachment colorAttachment;
    wgpu::RenderPassDescriptor rpDesc;
};

class PixelLocalStorageTest : public ValidationTest {
  protected:
    WGPUDevice CreateTestDevice(native::Adapter dawnAdapter,
                                wgpu::DeviceDescriptor descriptor) override {
        // TODO(dawn:1704): Do we need to test both extensions?
        wgpu::FeatureName requiredFeatures[1] = {wgpu::FeatureName::PixelLocalStorageNonCoherent};
        descriptor.requiredFeatures = requiredFeatures;
        descriptor.requiredFeatureCount = 1;
        return dawnAdapter.CreateDevice(&descriptor);
    }

    void InitializePLSRenderPass(ComboTestPLSRenderPassDescriptor* desc) {
        // Set up a single storage attachment.
        wgpu::TextureDescriptor storageDesc;
        storageDesc.size = {1, 1};
        storageDesc.format = wgpu::TextureFormat::R32Uint;
        storageDesc.usage = wgpu::TextureUsage::StorageAttachment;
        wgpu::Texture storage = device.CreateTexture(&storageDesc);

        desc->storageAttachments[0].storage = storage.CreateView();
        desc->storageAttachments[0].offset = 0;
        desc->storageAttachments[0].loadOp = wgpu::LoadOp::Load;
        desc->storageAttachments[0].storeOp = wgpu::StoreOp::Store;

        desc->pls.totalPixelLocalStorageSize = 4;
        desc->pls.storageAttachmentCount = 1;
        desc->pls.storageAttachments = desc->storageAttachments.data();

        // Add at least on color attachment to make the render pass valid if there's no storage
        // attachment.
        wgpu::TextureDescriptor colorDesc;
        colorDesc.size = {1, 1};
        colorDesc.format = wgpu::TextureFormat::R32Uint;
        colorDesc.usage = wgpu::TextureUsage::RenderAttachment;
        wgpu::Texture color = device.CreateTexture(&colorDesc);

        desc->colorAttachment.view = color.CreateView();
        desc->colorAttachment.loadOp = wgpu::LoadOp::Load;
        desc->colorAttachment.storeOp = wgpu::StoreOp::Store;

        desc->rpDesc.nextInChain = &desc->pls;
        desc->rpDesc.colorAttachmentCount = 1;
        desc->rpDesc.colorAttachments = &desc->colorAttachment;
    }

    void RecordRenderPass(const wgpu::RenderPassDescriptor* desc) {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(desc);
        pass.End();
        encoder.Finish();
    }

    void RecordPLSRenderPass(const PLSSpec& spec) {
        ComboTestPLSRenderPassDescriptor desc;
        InitializePLSRenderPass(&desc);

        // Convert the PLSSpec to a RenderPassPLS
        for (size_t i = 0; i < spec.attachments.size(); i++) {
            wgpu::TextureDescriptor tDesc;
            tDesc.size = {1, 1};
            tDesc.format = spec.attachments[i].format;
            tDesc.usage = wgpu::TextureUsage::StorageAttachment;
            wgpu::Texture texture = device.CreateTexture(&tDesc);

            desc.storageAttachments[i].storage = texture.CreateView();
            desc.storageAttachments[i].offset = spec.attachments[i].offset;
            desc.storageAttachments[i].loadOp = wgpu::LoadOp::Load;
            desc.storageAttachments[i].storeOp = wgpu::StoreOp::Store;
        }

        desc.pls.totalPixelLocalStorageSize = spec.totalSize;
        desc.pls.storageAttachmentCount = spec.attachments.size();

        // Add the PLS if needed and record the render pass.
        if (!spec.active) {
            desc.rpDesc.nextInChain = nullptr;
        }

        RecordRenderPass(&desc.rpDesc);
    }

    wgpu::PipelineLayout MakePipelineLayout(const PLSSpec& spec) {
        // Convert the PLSSpec to a PipelineLayoutPLS
        std::vector<wgpu::PipelineLayoutStorageAttachment> storageAttachments;
        for (const auto& attachmentSpec : spec.attachments) {
            wgpu::PipelineLayoutStorageAttachment attachment;
            attachment.format = attachmentSpec.format;
            attachment.offset = attachmentSpec.offset;
            storageAttachments.push_back(attachment);
        }

        wgpu::PipelineLayoutPixelLocalStorage pls;
        pls.totalPixelLocalStorageSize = spec.totalSize;
        pls.storageAttachmentCount = storageAttachments.size();
        pls.storageAttachments = storageAttachments.data();

        // Add the PLS if needed and make the pipeline layout.
        wgpu::PipelineLayoutDescriptor plDesc;
        plDesc.bindGroupLayoutCount = 0;
        if (spec.active) {
            plDesc.nextInChain = &pls;
        }
        return device.CreatePipelineLayout(&plDesc);
    }
};

// Check that StorageAttachment textures must be one of the supported formats.
TEST_F(PixelLocalStorageTest, TextureFormatMustSupportStorageAttachment) {
    for (wgpu::TextureFormat format : utils::kAllTextureFormats) {
        wgpu::TextureDescriptor desc;
        desc.size = {1, 1};
        desc.format = format;
        desc.usage = wgpu::TextureUsage::StorageAttachment;

        if (IsStorageAttachmentFormat(format)) {
            device.CreateTexture(&desc);
        } else {
            ASSERT_DEVICE_ERROR(device.CreateTexture(&desc));
        }
    }
}

// Check that StorageAttachment textures must have a sample count of 1.
TEST_F(PixelLocalStorageTest, TextureMustBeSingleSampled) {
    wgpu::TextureDescriptor desc;
    desc.size = {1, 1};
    desc.format = wgpu::TextureFormat::R32Uint;
    desc.usage = wgpu::TextureUsage::StorageAttachment;

    // Control case: sampleCount = 1 is valid.
    desc.sampleCount = 1;
    device.CreateTexture(&desc);

    // Error case: sampledCount != 1 is an error.
    desc.sampleCount = 4;
    ASSERT_DEVICE_ERROR(device.CreateTexture(&desc));
}

// Check that the format in PLS must be one of the enabled ones.
TEST_F(PixelLocalStorageTest, PLSStateFormatMustSupportStorageAttachment) {
    for (wgpu::TextureFormat format : utils::kFormatsInCoreSpec) {
        PLSSpec spec = {4, {{0, format}}};

        // Not that BeginRenderPass is not tested here as a different test checks that the
        // StorageAttachment texture must indeed have been created with the StorageAttachment usage.
        if (IsStorageAttachmentFormat(format)) {
            MakePipelineLayout(spec);
        } else {
            ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        }
    }
}

// Check that the total size must be a multiple of 4.
TEST_F(PixelLocalStorageTest, PLSStateTotalSizeMultipleOf4) {
    // Control case: total size is a multiple of 4.
    {
        PLSSpec spec = {4, {}};
        MakePipelineLayout(spec);
        RecordPLSRenderPass(spec);
    }

    // Control case: total size isn't a multiple of 4.
    {
        PLSSpec spec = {2, {}};
        ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        ASSERT_DEVICE_ERROR(RecordPLSRenderPass(spec));
    }
}

// Check that the total size must be less than 16.
// TODO(dawn:1704): Have a proper limit for totalSize.
TEST_F(PixelLocalStorageTest, PLSStateTotalLessThan16) {
    // Control case: total size is a multiple of 4.
    {
        PLSSpec spec = {16, {}};
        MakePipelineLayout(spec);
        RecordPLSRenderPass(spec);
    }

    // Control case: total size isn't a multiple of 4.
    {
        PLSSpec spec = {20, {}};
        ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        ASSERT_DEVICE_ERROR(RecordPLSRenderPass(spec));
    }
}

// Check that the offset of a storage attachment must be a multiple of 4.
TEST_F(PixelLocalStorageTest, PLSStateOffsetMultipleOf4) {
    // Control case: offset is a multiple of 4.
    {
        PLSSpec spec = {8, {{4, wgpu::TextureFormat::R32Uint}}};
        MakePipelineLayout(spec);
        RecordPLSRenderPass(spec);
    }

    // Error case: offset isn't a multiple of 4.
    {
        PLSSpec spec = {8, {{2, wgpu::TextureFormat::R32Uint}}};
        ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        ASSERT_DEVICE_ERROR(RecordPLSRenderPass(spec));
    }
}

// Check that the storage attachment is in bounds of the total size.
TEST_F(PixelLocalStorageTest, PLSStateAttachmentInBoundsOfTotalSize) {
    // Note that all storage attachment formats are currently 4 byte wide.

    // Control case: 0 + 4 <= 4
    {
        PLSSpec spec = {4, {{0, wgpu::TextureFormat::R32Uint}}};
        MakePipelineLayout(spec);
        RecordPLSRenderPass(spec);
    }

    // Error case: 4 + 4 > 4
    {
        PLSSpec spec = {4, {{4, wgpu::TextureFormat::R32Uint}}};
        ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        ASSERT_DEVICE_ERROR(RecordPLSRenderPass(spec));
    }

    // Control case: 8 + 4 <= 12
    {
        PLSSpec spec = {12, {{8, wgpu::TextureFormat::R32Uint}}};
        MakePipelineLayout(spec);
        RecordPLSRenderPass(spec);
    }

    // Error case: 12 + 4 > 12
    {
        PLSSpec spec = {4, {{12, wgpu::TextureFormat::R32Uint}}};
        ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        ASSERT_DEVICE_ERROR(RecordPLSRenderPass(spec));
    }

    // Check that overflows don't incorrectly pass the validation.
    {
        PLSSpec spec = {4, {{uint64_t(0) - uint64_t(4), wgpu::TextureFormat::R32Uint}}};
        ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        ASSERT_DEVICE_ERROR(RecordPLSRenderPass(spec));
    }
}

// Check that collisions between storage attachments are not allowed.
TEST_F(PixelLocalStorageTest, PLSStateCollisionsDisallowed) {
    // Control case: no collisions, all is good!
    {
        PLSSpec spec = {8, {{0, wgpu::TextureFormat::R32Uint}, {4, wgpu::TextureFormat::R32Uint}}};
        MakePipelineLayout(spec);
        RecordPLSRenderPass(spec);
    }
    // Control case: no collisions, boo!
    {
        PLSSpec spec = {8, {{0, wgpu::TextureFormat::R32Uint}, {0, wgpu::TextureFormat::R32Uint}}};
        ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        ASSERT_DEVICE_ERROR(RecordPLSRenderPass(spec));
    }
    {
        PLSSpec spec = {8,
                        {{0, wgpu::TextureFormat::R32Uint},
                         {4, wgpu::TextureFormat::R32Uint},
                         {0, wgpu::TextureFormat::R32Uint}}};
        ASSERT_DEVICE_ERROR(MakePipelineLayout(spec));
        ASSERT_DEVICE_ERROR(RecordPLSRenderPass(spec));
    }
}

// Check that using an error view as storage attachment is an error.
TEST_F(PixelLocalStorageTest, RenderPassStorageAttachmentErrorView) {
    ComboTestPLSRenderPassDescriptor desc;
    InitializePLSRenderPass(&desc);

    wgpu::TextureDescriptor tDesc;
    tDesc.size = {1, 1};
    tDesc.usage = wgpu::TextureUsage::StorageAttachment;
    tDesc.format = wgpu::TextureFormat::R32Uint;
    wgpu::Texture t = device.CreateTexture(&tDesc);

    wgpu::TextureViewDescriptor viewDesc;

    // Control case: valid texture view.
    desc.storageAttachments[0].storage = t.CreateView(&viewDesc);
    RecordRenderPass(&desc.rpDesc);

    // Error case: invalid texture view because of the base array layer.
    viewDesc.baseArrayLayer = 10;
    ASSERT_DEVICE_ERROR(desc.storageAttachments[0].storage = t.CreateView(&viewDesc));
    ASSERT_DEVICE_ERROR(RecordRenderPass(&desc.rpDesc));
}

// Check that using a multi-subresource view as a storage attachment is an error (layers and levels
// cases).
TEST_F(PixelLocalStorageTest, RenderPassStorageAttachmentSingleSubresource) {
    ComboTestPLSRenderPassDescriptor desc;
    InitializePLSRenderPass(&desc);

    wgpu::TextureDescriptor colorDesc;
    colorDesc.size = {2, 2};
    colorDesc.usage = wgpu::TextureUsage::RenderAttachment;
    colorDesc.format = wgpu::TextureFormat::R32Uint;

    // Replace the render pass attachment with a 2x2 texture for mip level testing.
    desc.colorAttachment.view = device.CreateTexture(&colorDesc).CreateView();

    // Control case: single subresource view.
    wgpu::TextureDescriptor tDesc;
    tDesc.size = {2, 2};
    tDesc.usage = wgpu::TextureUsage::StorageAttachment;
    tDesc.format = wgpu::TextureFormat::R32Uint;

    desc.storageAttachments[0].storage = device.CreateTexture(&tDesc).CreateView();
    RecordRenderPass(&desc.rpDesc);

    // Error case: two array layers.
    tDesc.size.depthOrArrayLayers = 2;
    desc.storageAttachments[0].storage = device.CreateTexture(&tDesc).CreateView();
    ASSERT_DEVICE_ERROR(RecordRenderPass(&desc.rpDesc));

    // Error case: two mip levels.
    tDesc.size.depthOrArrayLayers = 1;
    tDesc.mipLevelCount = 2;
    desc.storageAttachments[0].storage = device.CreateTexture(&tDesc).CreateView();
    ASSERT_DEVICE_ERROR(RecordRenderPass(&desc.rpDesc));
}

// TODO(dawn:1704): Implement the test plan below.
// -- BeginRenderPass
// Check that same texture subresource cannot be used twice in BeginRenderPass (either render
// attachment or storage attachment). Check that using a storage attachment is a writable state (and
// cannot be used with usage as texture binding) Check that the storage attachment view's size must
// match the size of other attachments. Check that the storage attachment view must be from a
// StorageAttachment texture. Check that the load and store ops must not be undefined. Check that if
// the load op is clear, the clear value doesn't have nans.

// -- PixelLocalStorageBarrier
// Check PLS barrier only allowed if there is PLS (not empty).

// -- Check Attachment state compatibility
// Empty PLS vs. no PLS ok.
// Empty vs. totalSize > 0 not ok.
// attachment vs. implicit not ok.
// attachment different format not ok.
// attachment in different order ok.

// TODO(dawn:1704): Add tests for limits

}  // anonymous namespace
}  // namespace dawn
