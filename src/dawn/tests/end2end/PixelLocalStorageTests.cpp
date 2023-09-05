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
    }

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> requiredFeatures = {};
        if (SupportsFeatures({wgpu::FeatureName::PixelLocalStorageCoherent})) {
            requiredFeatures.push_back(wgpu::FeatureName::PixelLocalStorageCoherent);
        }
        if (SupportsFeatures({wgpu::FeatureName::PixelLocalStorageNonCoherent})) {
            requiredFeatures.push_back(wgpu::FeatureName::PixelLocalStorageNonCoherent);
        }
        return requiredFeatures;
    }
};

TEST_P(PixelLocalStorageTests, Foo) {
    const wgpu::TextureFormat kFormat = wgpu::TextureFormat::R32Uint;

    wgpu::TextureDescriptor tDesc;
    tDesc.format = kFormat;
    tDesc.size = {1, 1};
    tDesc.usage = wgpu::TextureUsage::StorageAttachment | wgpu::TextureUsage::CopySrc;
    wgpu::Texture tex = device.CreateTexture(&tDesc);

    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        enable chromium_experimental_pixel_local;

        @vertex fn vs() -> @builtin(position) vec4f {
            return vec4f(0, 0, 0, 0.5);
        }

        struct PLS {
            value : u32,
        };
        var<pixel_local> pls : PLS;
        @fragment fn fs() {
            pls.value = pls.value + 1;
        }
    )");

    wgpu::PipelineLayoutStorageAttachment plAttachment;
    plAttachment.offset = 0;
    plAttachment.format = kFormat;

    wgpu::PipelineLayoutPixelLocalStorage plPlsDesc;
    plPlsDesc.totalPixelLocalStorageSize = 4;
    plPlsDesc.storageAttachmentCount = 1;
    plPlsDesc.storageAttachments = &plAttachment;

    wgpu::PipelineLayoutDescriptor plDesc;
    plDesc.nextInChain = &plPlsDesc;
    plDesc.bindGroupLayoutCount = 0;
    wgpu::PipelineLayout pl = device.CreatePipelineLayout(&plDesc);

    utils::ComboRenderPipelineDescriptor pDesc;
    pDesc.layout = pl;
    pDesc.vertex.module = module;
    pDesc.vertex.entryPoint = "vs";
    pDesc.cFragment.module = module;
    pDesc.cFragment.entryPoint = "fs";
    pDesc.cFragment.targetCount = 0;
    pDesc.primitive.topology = wgpu::PrimitiveTopology::PointList;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pDesc);

    wgpu::RenderPassStorageAttachment rpAttachment;
    rpAttachment.storage = tex.CreateView();
    rpAttachment.offset = 0;
    rpAttachment.loadOp = wgpu::LoadOp::Clear;
    rpAttachment.clearValue = {0, 0, 0, 0};
    rpAttachment.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassPixelLocalStorage rpPlsDesc;
    rpPlsDesc.totalPixelLocalStorageSize = 4;
    rpPlsDesc.storageAttachmentCount = 1;
    rpPlsDesc.storageAttachments = &rpAttachment;

    wgpu::RenderPassDescriptor rpDesc;
    rpDesc.nextInChain = &rpPlsDesc;
    rpDesc.colorAttachmentCount = 0;
    rpDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rpDesc);
    pass.SetPipeline(pipeline);
    pass.Draw(1000);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    queue.Submit(1, &commands);

    EXPECT_TEXTURE_EQ(uint32_t(1000), tex, {0, 0});
}

// TEST PLAN TIME!
//

DAWN_INSTANTIATE_TEST(PixelLocalStorageTests, MetalBackend());

}  // anonymous namespace
}  // namespace dawn
