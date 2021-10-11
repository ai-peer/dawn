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

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

#include "tests/unittests/validation/ValidationTest.h"

constexpr static uint32_t kSize = 4;

namespace {

    class RenderPipelineAndPassCompatibilityTests : public ValidationTest {
      public:
        wgpu::Texture CreateTexture(wgpu::TextureFormat format) {
            wgpu::TextureDescriptor descriptor = {};
            descriptor.dimension = wgpu::TextureDimension::e2D;
            descriptor.size = {kSize, kSize, 1};
            descriptor.format = format;
            descriptor.usage = wgpu::TextureUsage::RenderAttachment;
            descriptor.mipLevelCount = 1;
            descriptor.sampleCount = 1;
            return device.CreateTexture(&descriptor);
        }

        wgpu::RenderPipeline CreatePipeline(wgpu::TextureFormat format,
                                            bool enableDepthWrite,
                                            bool enableStencilWrite) {
            // Create a NoOp pipeline
            utils::ComboRenderPipelineDescriptor pipelineDescriptor;
            pipelineDescriptor.vertex.module = utils::CreateShaderModule(device, R"(
                [[stage(vertex)]] fn main() -> [[builtin(position)]] vec4<f32> {
                    return vec4<f32>();
                })");
            pipelineDescriptor.cFragment.module = utils::CreateShaderModule(device, R"(
                [[stage(fragment)]] fn main() {
                })");
            pipelineDescriptor.cFragment.targets = nullptr;
            pipelineDescriptor.cFragment.targetCount = 0;

            // Enable depth/stencil write if needed
            wgpu::DepthStencilState* depthStencil = pipelineDescriptor.EnableDepthStencil(format);
            if (enableDepthWrite) {
                depthStencil->depthWriteEnabled = true;
            }
            if (enableStencilWrite) {
                depthStencil->stencilFront.failOp = wgpu::StencilOperation::Replace;
            }
            return device.CreateRenderPipeline(&pipelineDescriptor);
        }

        utils::ComboRenderPassDescriptor CreateRenderPassDescriptor(wgpu::TextureFormat format,
                                                                    bool depthReadOnly,
                                                                    bool stencilReadOnly) {
            wgpu::Texture depthStencilTexture = CreateTexture(format);

            utils::ComboRenderPassDescriptor passDescriptor({}, depthStencilTexture.CreateView());
            if (depthReadOnly) {
                passDescriptor.cDepthStencilAttachmentInfo.depthReadOnly = true;
                passDescriptor.cDepthStencilAttachmentInfo.depthLoadOp = wgpu::LoadOp::Load;
                passDescriptor.cDepthStencilAttachmentInfo.depthStoreOp = wgpu::StoreOp::Store;
            }

            if (stencilReadOnly) {
                passDescriptor.cDepthStencilAttachmentInfo.stencilReadOnly = true;
                passDescriptor.cDepthStencilAttachmentInfo.stencilLoadOp = wgpu::LoadOp::Load;
                passDescriptor.cDepthStencilAttachmentInfo.stencilStoreOp = wgpu::StoreOp::Store;
            }

            return passDescriptor;
        }
    };

    // Test depthWrite/stencilWrite in DepthStencilState in pipeline vs
    // depthReadOnly/stencilReadOnly in DepthStencilAttachment in pass
    TEST_F(RenderPipelineAndPassCompatibilityTests, ReadOnlyDepthStencilAttachment) {
        constexpr static wgpu::TextureFormat kFormat = wgpu::TextureFormat::Depth24PlusStencil8;
        // If the format has both depth and stencil aspects, depthReadOnly and stencilReadOnly
        // should be the same. So it is not necessary to set two separate booleans like
        // depthReadOnlyInPass and stencilReadOnlyInPass.
        for (bool depthStencilReadOnlyInPass : {true, false}) {
            for (bool depthWriteInPipeline : {true, false}) {
                for (bool stencilWriteInPipeline : {true, false}) {
                    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                    utils::ComboRenderPassDescriptor passDescriptor = CreateRenderPassDescriptor(
                        kFormat, depthStencilReadOnlyInPass, depthStencilReadOnlyInPass);
                    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
                    wgpu::RenderPipeline pipeline =
                        CreatePipeline(kFormat, depthWriteInPipeline, stencilWriteInPipeline);
                    pass.SetPipeline(pipeline);
                    pass.Draw(3);
                    pass.EndPass();
                    if (depthStencilReadOnlyInPass &&
                        (depthWriteInPipeline || stencilWriteInPipeline)) {
                        ASSERT_DEVICE_ERROR(encoder.Finish());
                    } else {
                        encoder.Finish();
                    }
                }
            }
        }
    }

    // TODO(dawn:485): add more tests. For example, depth/stencil attachment should be designated if
    // depth/stenci test is enabled.

}  // anonymous namespace
