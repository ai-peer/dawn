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

#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

class PixelLocalStorageTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(
            !device.HasFeature(wgpu::FeatureName::PixelLocalStorageCoherent) &&
            !device.HasFeature(wgpu::FeatureName::PixelLocalStorageNonCoherent));

        supportsCoherent = device.HasFeature(wgpu::FeatureName::PixelLocalStorageCoherent);
    }

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> requiredFeatures = {};
        if (SupportsFeatures({wgpu::FeatureName::PixelLocalStorageCoherent})) {
            requiredFeatures.push_back(wgpu::FeatureName::PixelLocalStorageCoherent);
            supportsCoherent = true;
        }
        if (SupportsFeatures({wgpu::FeatureName::PixelLocalStorageNonCoherent})) {
            requiredFeatures.push_back(wgpu::FeatureName::PixelLocalStorageNonCoherent);
        }
        return requiredFeatures;
    }

    struct StorageSpec {
        uint64_t offset;
        wgpu::TextureFormat format;
        wgpu::LoadOp loadOp = wgpu::LoadOp::Clear;
        wgpu::StoreOp storeOp = wgpu::StoreOp::Store;
        wgpu::Color clearValue = {0, 0, 0, 0};
        bool discardAfterInit = false;
    };

    enum class CheckMethod {
        StorageBuffer,
        ReadStorageAttachments,
        RenderAttachment,
    };

    struct PLSSpec {
        uint64_t totalSize;
        std::vector<StorageSpec> attachments;
        CheckMethod checkMethod = CheckMethod::ReadStorageAttachments;
    };

    wgpu::ShaderModule MakeTestModule(const PLSSpec& spec) const {
        std::vector<const char*> plsTypes;
        plsTypes.resize(spec.totalSize / kPLSSlotByteSize, "u32");
        for (const auto& attachment : spec.attachments) {
            switch (attachment.format) {
                case wgpu::TextureFormat::R32Uint:
                    plsTypes[attachment.offset / kPLSSlotByteSize] = "u32";
                    break;
                case wgpu::TextureFormat::R32Sint:
                    plsTypes[attachment.offset / kPLSSlotByteSize] = "i32";
                    break;
                case wgpu::TextureFormat::R32Float:
                    plsTypes[attachment.offset / kPLSSlotByteSize] = "f32";
                    break;
                default:
                    DAWN_UNREACHABLE();
            }
        }

        std::ostringstream o;
        o << R"(
            enable chromium_experimental_pixel_local;

            @vertex fn vs() -> @builtin(position) vec4f {
                return vec4f(0, 0, 0, 0.5);
            }

        )";
        o << "struct PLS {\n";
        for (size_t i = 0; i < plsTypes.size(); i++) {
            o << "  a" << i << " : " << plsTypes[i] << ",\n";
        }
        o << "}\n";
        o << "var<pixel_local> pls : PLS;\n";
        o << "@fragment fn accumulator() {\n";
        for (size_t i = 0; i < plsTypes.size(); i++) {
            o << "    pls.a" << i << " = pls.a" << i << " + " << (i + 1) << ";\n";
        }
        o << "}\n";
        // TODO entry points to copy to storage buffer, or to render attachments.

        return utils::CreateShaderModule(device, o.str().c_str());
    }

    wgpu::PipelineLayout MakeTestLayout(const PLSSpec& spec, wgpu::BindGroupLayout bgl = {}) const {
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

        wgpu::PipelineLayoutDescriptor plDesc;
        plDesc.nextInChain = &pls;
        plDesc.bindGroupLayoutCount = 0;
        if (bgl != nullptr) {
            plDesc.bindGroupLayoutCount = 1;
            plDesc.bindGroupLayouts = &bgl;
        }

        return device.CreatePipelineLayout(&plDesc);
    }

    std::vector<wgpu::Texture> MakeTestStorageAttachments(const PLSSpec& spec) const {
        std::vector<wgpu::Texture> attachments;
        for (size_t i = 0; i < spec.attachments.size(); i++) {
            const StorageSpec& attachmentSpec = spec.attachments[i];

            wgpu::TextureDescriptor desc;
            desc.format = attachmentSpec.format;
            desc.size = {1, 1};
            desc.usage = wgpu::TextureUsage::StorageAttachment | wgpu::TextureUsage::CopySrc |
                         wgpu::TextureUsage::CopyDst;
            if (attachmentSpec.discardAfterInit) {
                desc.usage |= wgpu::TextureUsage::RenderAttachment;
            }

            wgpu::Texture attachment = device.CreateTexture(&desc);

            // Initialize the attachment with 1s if LoadOp is Load, copying from another texture
            // so that we avoid adding the extra RenderAttachment usage to the storage attachment.
            if (attachmentSpec.loadOp == wgpu::LoadOp::Load) {
                desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
                wgpu::Texture clearedTexture = device.CreateTexture(&desc);

                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

                // The pass that clears clearedTexture.
                utils::ComboRenderPassDescriptor rpDesc({clearedTexture.CreateView()});
                rpDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
                rpDesc.cColorAttachments[0].clearValue = attachmentSpec.clearValue;
                wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rpDesc);
                pass.End();

                // Copy clearedTexture -> attachment.
                wgpu::ImageCopyTexture src = utils::CreateImageCopyTexture(clearedTexture);
                wgpu::ImageCopyTexture dst = utils::CreateImageCopyTexture(attachment);
                wgpu::Extent3D copySize = {1, 1, 1};
                encoder.CopyTextureToTexture(&src, &dst, &copySize);

                wgpu::CommandBuffer commands = encoder.Finish();
                queue.Submit(1, &commands);
            }

            // Discard after initialization to check that the lazy zero init is actually triggered
            // (and it's not just that the resource happened to be zeroes already).
            if (attachmentSpec.discardAfterInit) {
                utils::ComboRenderPassDescriptor rpDesc({attachment.CreateView()});
                rpDesc.cColorAttachments[0].loadOp = wgpu::LoadOp::Load;
                rpDesc.cColorAttachments[0].storeOp = wgpu::StoreOp::Discard;

                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rpDesc);
                pass.End();
                wgpu::CommandBuffer commands = encoder.Finish();
                queue.Submit(1, &commands);
            }

            attachments.push_back(attachment);
        }

        return attachments;
    }

    wgpu::RenderPassEncoder BeginTestRenderPass(
        const PLSSpec& spec,
        const wgpu::CommandEncoder& encoder,
        const std::vector<wgpu::Texture>& storageAttachments) const {
        std::vector<wgpu::RenderPassStorageAttachment> attachmentDescs;
        for (size_t i = 0; i < spec.attachments.size(); i++) {
            const StorageSpec& attachmentSpec = spec.attachments[i];

            wgpu::RenderPassStorageAttachment attachment;
            attachment.storage = storageAttachments[i].CreateView();
            attachment.offset = attachmentSpec.offset;
            attachment.loadOp = attachmentSpec.loadOp;
            attachment.storeOp = attachmentSpec.storeOp;
            attachment.clearValue = attachmentSpec.clearValue;
            attachmentDescs.push_back(attachment);
        }

        wgpu::RenderPassPixelLocalStorage rpPlsDesc;
        rpPlsDesc.totalPixelLocalStorageSize = spec.totalSize;
        rpPlsDesc.storageAttachmentCount = attachmentDescs.size();
        rpPlsDesc.storageAttachments = attachmentDescs.data();

        wgpu::RenderPassDescriptor rpDesc;
        rpDesc.nextInChain = &rpPlsDesc;
        rpDesc.colorAttachmentCount = 0;  // TODO
        rpDesc.depthStencilAttachment = nullptr;
        return encoder.BeginRenderPass(&rpDesc);
    }

    uint32_t ComputeExpectedValue(const PLSSpec& spec, size_t slot) {
        for (const StorageSpec& attachment : spec.attachments) {
            if (attachment.offset / kPLSSlotByteSize != slot) {
                continue;
            }

            // Compute the expected value depending on load/store ops by "replaying" the operations
            // that would be done.
            uint32_t expectedValue = 0;
            if (!attachment.discardAfterInit) {
                expectedValue = attachment.clearValue.r;
            }
            expectedValue += (slot + 1) * kIterations;

            if (attachment.storeOp == wgpu::StoreOp::Discard) {
                expectedValue = 0;
            }

            return expectedValue;
        }

        // This is not an explicit storage attachment.
        return (slot + 1) * kIterations;
    }

    void CheckByReadingStorageAttachments(const PLSSpec& spec,
                                          const std::vector<wgpu::Texture>& storageAttachments) {
        for (size_t i = 0; i < spec.attachments.size(); i++) {
            const StorageSpec& attachmentSpec = spec.attachments[i];
            uint32_t slot = attachmentSpec.offset / kPLSSlotByteSize;

            uint32_t expectedValue = ComputeExpectedValue(spec, slot);

            switch (spec.attachments[i].format) {
                case wgpu::TextureFormat::R32Float:
                    EXPECT_TEXTURE_EQ(float(expectedValue), storageAttachments[i], {0, 0});
                    break;
                case wgpu::TextureFormat::R32Uint:
                case wgpu::TextureFormat::R32Sint:
                    EXPECT_TEXTURE_EQ(expectedValue, storageAttachments[i], {0, 0});
                    break;
                default:
                    DAWN_UNREACHABLE();
            }
        }
    }

    void DoTest(const PLSSpec& spec) {
        wgpu::ShaderModule module = MakeTestModule(spec);

        // Make the pipeline that will draw a point that adds i to the i-th slot of the PLS.
        wgpu::RenderPipeline accumulatorPipeline;
        {
            utils::ComboRenderPipelineDescriptor desc;
            desc.layout = MakeTestLayout(spec);
            desc.vertex.module = module;
            desc.vertex.entryPoint = "vs";
            desc.cFragment.module = module;
            desc.cFragment.entryPoint = "accumulator";
            desc.cFragment.targetCount = 0;  // TODO
            desc.primitive.topology = wgpu::PrimitiveTopology::PointList;
            accumulatorPipeline = device.CreateRenderPipeline(&desc);
        }

        // TODO test pipeline and its bindgroup.

        std::vector<wgpu::Texture> storageAttachments = MakeTestStorageAttachments(spec);

        // Build the render pass with the specified storage attachments
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = BeginTestRenderPass(spec, encoder, storageAttachments);

        // Draw the points accumulating to PLS, with a PLS barrier if needed.
        pass.SetPipeline(accumulatorPipeline);
        if (supportsCoherent) {
            pass.Draw(kIterations);
        } else {
            for (uint32_t i = 0; i < kIterations; i++) {
                pass.Draw(1);
                pass.PixelLocalStorageBarrier();
            }
        }

        // TODO If final pipeline, bind it and set the bindgroup.

        pass.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        CheckByReadingStorageAttachments(spec, storageAttachments);
        // TODO other check methods.
        // Youpi!
    }

    static constexpr uint32_t kIterations = 10;
    bool supportsCoherent;
};

// Test that the various supported PLS format work for accumulation.
TEST_P(PixelLocalStorageTests, Formats) {
    for (const auto format : {wgpu::TextureFormat::R32Uint, wgpu::TextureFormat::R32Sint,
                              wgpu::TextureFormat::R32Float}) {
        PLSSpec spec = {4, {{0, format}}};
        DoTest(spec);
    }
}

// Tests the storage attachment load ops
TEST_P(PixelLocalStorageTests, LoadOp) {
    // Test LoadOp::Clear with a couple values.
    {
        PLSSpec spec = {4, {{0, wgpu::TextureFormat::R32Uint}}};
        spec.attachments[0].loadOp = wgpu::LoadOp::Clear;

        spec.attachments[0].clearValue.r = 42;
        DoTest(spec);

        spec.attachments[0].clearValue.r = 38;
        DoTest(spec);
    }

    // Test LoadOp::Load (the test helper clears the texture to clearValue).
    {
        PLSSpec spec = {4, {{0, wgpu::TextureFormat::R32Uint}}};
        spec.attachments[0].clearValue.r = 18;
        spec.attachments[0].loadOp = wgpu::LoadOp::Load;
        DoTest(spec);
    }
}

// Tests the storage attachment store ops
TEST_P(PixelLocalStorageTests, StoreOp) {
    // Test StoreOp::Store.
    {
        PLSSpec spec = {4, {{0, wgpu::TextureFormat::R32Uint}}};
        spec.attachments[0].storeOp = wgpu::StoreOp::Store;
        DoTest(spec);
    }

    // Test StoreOp::Discard.
    {
        PLSSpec spec = {4, {{0, wgpu::TextureFormat::R32Uint}}};
        spec.attachments[0].storeOp = wgpu::StoreOp::Discard;
        DoTest(spec);
    }
}

// Test lazy zero initialization of the storage attachments.
TEST_P(PixelLocalStorageTests, ZeroInit) {
    // Discard causes the storage attachment to be lazy zeroed.
    {
        PLSSpec spec = {4, {{0, wgpu::TextureFormat::R32Uint}}};
        spec.attachments[0].storeOp = wgpu::StoreOp::Discard;
        DoTest(spec);
    }

    // Discard before using as a storage attachment, it should be lazy-cleared.
    // XXX
    // {
    //     PLSSpec spec = {4, {{0, wgpu::TextureFormat::R32Uint}}};
    //     spec.attachments[0].clearValue.r = 18;
    //     spec.attachments[0].discardAfterInit = true;
    //     DoTest(spec);
    // }
}

// Test many explicit storage attachments.
TEST_P(PixelLocalStorageTests, MultipleStorageAttachments) {
    PLSSpec spec = {16,
                    {
                        {0, wgpu::TextureFormat::R32Sint},
                        {4, wgpu::TextureFormat::R32Uint},
                        {8, wgpu::TextureFormat::R32Float},
                        {12, wgpu::TextureFormat::R32Sint},
                    }};
    DoTest(spec);
}

// Test explicit storage attachments in inverse offset order
TEST_P(PixelLocalStorageTests, InvertedOffsetOrder) {
    PLSSpec spec = {8,
                    {
                        {4, wgpu::TextureFormat::R32Uint},
                        {0, wgpu::TextureFormat::R32Sint},
                    }};
    DoTest(spec);
}

// TEST PLAN TIME!
//
// - Multiple explicit attachments at offset, with holes
// - Implicit attachments
// - Only implicit attachments
// - Mix of render and PLS

DAWN_INSTANTIATE_TEST(PixelLocalStorageTests, MetalBackend());

}  // anonymous namespace
}  // namespace dawn
