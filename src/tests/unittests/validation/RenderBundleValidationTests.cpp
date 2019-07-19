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

#include "tests/unittests/validation/ValidationTest.h"

#include "common/Constants.h"

#include "utils/ComboRenderBundleEncoderDescriptor.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

namespace {

    class RenderBundleValidationTest : public ValidationTest {
      protected:
        void SetUp() override {
            ValidationTest::SetUp();

            vsModule = utils::CreateShaderModule(device, utils::ShaderStage::Vertex, R"(
              #version 450
              layout (set = 0, binding = 0) uniform vertexUniformBuffer {
                  mat2 transform;
              };
              void main() {
                  const vec2 pos[3] = vec2[3](vec2(-1.f, -1.f), vec2(1.f, -1.f), vec2(-1.f, 1.f));
                  gl_Position = vec4(transform * pos[gl_VertexIndex], 0.f, 1.f);
              })");

            fsModule = utils::CreateShaderModule(device, utils::ShaderStage::Fragment, R"(
              #version 450
              layout (set = 1, binding = 0) uniform fragmentUniformBuffer {
                  vec4 color;
              };
              layout(location = 0) out vec4 fragColor;
              void main() {
                  fragColor = color;
              })");

            dawn::BindGroupLayout bgls[] = {
                utils::MakeBindGroupLayout(
                    device, {{0, dawn::ShaderStageBit::Vertex, dawn::BindingType::UniformBuffer}}),
                utils::MakeBindGroupLayout(device, {{0, dawn::ShaderStageBit::Fragment,
                                                     dawn::BindingType::UniformBuffer}})};

            dawn::PipelineLayoutDescriptor pipelineLayoutDesc;
            pipelineLayoutDesc.bindGroupLayoutCount = 2;
            pipelineLayoutDesc.bindGroupLayouts = bgls;

            pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

            utils::ComboRenderPipelineDescriptor descriptor(device);
            descriptor.layout = pipelineLayout;
            descriptor.cVertexStage.module = vsModule;
            descriptor.cFragmentStage.module = fsModule;

            pipeline = device.CreateRenderPipeline(&descriptor);

            float data[4];
            dawn::Buffer buffer = utils::CreateBufferFromData(device, data, 4 * sizeof(float),
                                                              dawn::BufferUsageBit::Uniform);

            bg0 = utils::MakeBindGroup(device, bgls[0], {{0, buffer, 0, 4 * sizeof(float)}});
            bg1 = utils::MakeBindGroup(device, bgls[1], {{0, buffer, 0, 4 * sizeof(float)}});
        }

        dawn::ShaderModule vsModule;
        dawn::ShaderModule fsModule;
        dawn::PipelineLayout pipelineLayout;
        dawn::RenderPipeline pipeline;
        dawn::BindGroup bg0;
        dawn::BindGroup bg1;
    };

    // Test creating and encoding an empty render bundle.
    TEST_F(RenderBundleValidationTest, Empty) {
        DummyRenderPass renderPass(device);

        utils::ComboRenderBundleEncoderDescriptor desc = {};
        desc.colorFormatsCount = 1;
        *desc.cColorFormats[0] = renderPass.attachmentFormat;

        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        commandEncoder.Finish();
    }

    // Test successfully creating and encoding a render bundle into a command buffer.
    TEST_F(RenderBundleValidationTest, SimpleSuccess) {
        DummyRenderPass renderPass(device);

        utils::ComboRenderBundleEncoderDescriptor desc = {};
        desc.colorFormatsCount = 1;
        *desc.cColorFormats[0] = renderPass.attachmentFormat;

        // Simple case.
        {
            dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
            renderBundleEncoder.SetPipeline(pipeline);
            renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
            renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
            renderBundleEncoder.Draw(3, 0, 0, 0);
            dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.EndPass();
            commandEncoder.Finish();
        }

        // Mixed commands. Some are in the bundle, some in the pass.
        {
            dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
            renderBundleEncoder.SetPipeline(pipeline);
            renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
            renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
            dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.Draw(3, 0, 0, 0);
            pass.EndPass();
            commandEncoder.Finish();
        }

        // Mixed commands. Some are in the bundle, some in the pass.
        {
            dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
            renderBundleEncoder.SetPipeline(pipeline);
            renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
            dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.SetBindGroup(1, bg1, 0, nullptr);
            pass.Draw(3, 0, 0, 0);
            pass.EndPass();
            commandEncoder.Finish();
        }

        // Mixed commands. Some are in the bundle, some in the pass.
        {
            dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
            renderBundleEncoder.SetPipeline(pipeline);
            renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
            dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.SetBindGroup(1, bg1, 0, nullptr);
            pass.ExecuteBundles(1, &renderBundle);
            pass.Draw(3, 0, 0, 0);
            pass.EndPass();
            commandEncoder.Finish();
        }

        // Mixed commands. Some are in the bundle, some in the pass.
        {
            dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
            renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
            renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
            dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.SetPipeline(pipeline);
            pass.ExecuteBundles(1, &renderBundle);
            pass.Draw(3, 0, 0, 0);
            pass.EndPass();
            commandEncoder.Finish();
        }
    }

    // Test creating and encoding multiple render bundles.
    TEST_F(RenderBundleValidationTest, MultipleBundles) {
        DummyRenderPass renderPass(device);

        utils::ComboRenderBundleEncoderDescriptor desc = {};
        desc.colorFormatsCount = 1;
        *desc.cColorFormats[0] = renderPass.attachmentFormat;

        dawn::RenderBundle renderBundles[2] = {};

        dawn::RenderBundleEncoder renderBundleEncoder0 = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder0.SetPipeline(pipeline);
        renderBundleEncoder0.SetBindGroup(0, bg0, 0, nullptr);
        renderBundles[0] = renderBundleEncoder0.Finish();

        dawn::RenderBundleEncoder renderBundleEncoder1 = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder1.SetBindGroup(1, bg1, 0, nullptr);
        renderBundleEncoder1.Draw(3, 1, 0, 0);
        renderBundles[1] = renderBundleEncoder1.Finish();

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(2, renderBundles);
        pass.EndPass();
        commandEncoder.Finish();
    }

    // Test that is is valid to execute a render bundle more than once.
    TEST_F(RenderBundleValidationTest, ExecuteMultipleTimes) {
        DummyRenderPass renderPass(device);

        utils::ComboRenderBundleEncoderDescriptor desc = {};
        desc.colorFormatsCount = 1;
        *desc.cColorFormats[0] = renderPass.attachmentFormat;

        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.SetPipeline(pipeline);
        renderBundleEncoder.SetBindGroup(0, bg0, 0, nullptr);
        renderBundleEncoder.SetBindGroup(1, bg1, 0, nullptr);
        renderBundleEncoder.Draw(3, 1, 0, 0);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
        pass.ExecuteBundles(1, &renderBundle);
        pass.ExecuteBundles(1, &renderBundle);
        pass.ExecuteBundles(1, &renderBundle);
        pass.EndPass();
        commandEncoder.Finish();
    }

    // Test that it is an error to call Finish() on a render bundle encoder twice.
    TEST_F(RenderBundleValidationTest, FinishTwice) {
        utils::ComboRenderBundleEncoderDescriptor desc = {};
        desc.colorFormatsCount = 1;
        *desc.cColorFormats[0] = dawn::TextureFormat::RGBA8Uint;

        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.Finish();
        ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
    }

    // Test that it is invalid to create a render bundle with no texture formats
    TEST_F(RenderBundleValidationTest, RequiresAtLeastOneTextureFormat) {
        // Test failure case.
        {
            utils::ComboRenderBundleEncoderDescriptor desc = {};
            ASSERT_DEVICE_ERROR(device.CreateRenderBundleEncoder(&desc));
        }

        // Test success with one color format.
        {
            utils::ComboRenderBundleEncoderDescriptor desc = {};
            desc.colorFormatsCount = 1;
            *desc.cColorFormats[0] = dawn::TextureFormat::RGBA8Uint;
            device.CreateRenderBundleEncoder(&desc);
        }

        // Test success with a depth stencil format.
        {
            utils::ComboRenderBundleEncoderDescriptor desc = {};
            desc.cDepthStencilFormat = dawn::TextureFormat::Depth24PlusStencil8;
            desc.depthStencilFormat = &desc.cDepthStencilFormat;
            device.CreateRenderBundleEncoder(&desc);
        }
    }

    // Test that a render bundle is validated with respect to commands in the render pass.
    TEST_F(RenderBundleValidationTest, ValidatedInsideRenderPass) {
        DummyRenderPass renderPass(device);

        utils::ComboRenderBundleEncoderDescriptor desc = {};
        desc.colorFormatsCount = 1;
        *desc.cColorFormats[0] = renderPass.attachmentFormat;

        dawn::RenderBundleEncoder renderBundleEncoder = device.CreateRenderBundleEncoder(&desc);
        renderBundleEncoder.Draw(3, 1, 0, 0);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        // Test the successful base case.
        {
            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bg0, 0, nullptr);
            pass.EndPass();
            commandEncoder.Finish();
        }

        // Test the failure case, when the render bundle adds an additional invalid command.
        {
            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bg0, 0, nullptr);

            pass.ExecuteBundles(1, &renderBundle);

            pass.EndPass();
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        }
    }

    // Test that encoding SetPipline with an incompatible color format produces an error.
    TEST_F(RenderBundleValidationTest, PipelineColorFormatMismatch) {
        utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
        renderBundleDesc.colorFormatsCount = 3;
        *renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
        *renderBundleDesc.cColorFormats[1] = dawn::TextureFormat::RG16Float;
        *renderBundleDesc.cColorFormats[2] = dawn::TextureFormat::R16Sint;

        utils::ComboRenderPipelineDescriptor renderPipelineDesc(device);
        renderPipelineDesc.colorStateCount = 3;
        renderPipelineDesc.cColorStates[0]->format = dawn::TextureFormat::RGBA8Unorm;
        renderPipelineDesc.cColorStates[1]->format = dawn::TextureFormat::RG16Float;
        renderPipelineDesc.cColorStates[2]->format = dawn::TextureFormat::R16Sint;
        renderPipelineDesc.layout = pipelineLayout;
        renderPipelineDesc.cVertexStage.module = vsModule;
        renderPipelineDesc.cFragmentStage.module = fsModule;

        // Test the success case.
        {
            dawn::RenderBundleEncoder renderBundleEncoder =
                device.CreateRenderBundleEncoder(&renderBundleDesc);
            dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
            renderBundleEncoder.SetPipeline(pipeline);
            renderBundleEncoder.Finish();
        }

        // Test the failure case for mismatched format types.
        {
            utils::ComboRenderPipelineDescriptor desc = renderPipelineDesc;
            desc.cColorStates[1]->format = dawn::TextureFormat::RGBA8Unorm;

            dawn::RenderBundleEncoder renderBundleEncoder =
                device.CreateRenderBundleEncoder(&renderBundleDesc);
            dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
            renderBundleEncoder.SetPipeline(pipeline);
            ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
        }

        // Test the failure case for missing format
        {
            utils::ComboRenderPipelineDescriptor desc = renderPipelineDesc;
            desc.cColorStates[2] = nullptr;

            dawn::RenderBundleEncoder renderBundleEncoder =
                device.CreateRenderBundleEncoder(&renderBundleDesc);
            dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
            renderBundleEncoder.SetPipeline(pipeline);
            ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
        }
    }

    // Test that encoding SetPipline with an incompatible depth stencil format produces an error.
    TEST_F(RenderBundleValidationTest, PipelineDepthStencilFormatMismatch) {
        utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
        renderBundleDesc.colorFormatsCount = 1;
        *renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
        renderBundleDesc.cDepthStencilFormat = dawn::TextureFormat::Depth24PlusStencil8;
        renderBundleDesc.depthStencilFormat = &renderBundleDesc.cDepthStencilFormat;

        utils::ComboRenderPipelineDescriptor renderPipelineDesc(device);
        renderPipelineDesc.colorStateCount = 1;
        renderPipelineDesc.cColorStates[0]->format = dawn::TextureFormat::RGBA8Unorm;
        renderPipelineDesc.depthStencilState = &renderPipelineDesc.cDepthStencilState;
        renderPipelineDesc.layout = pipelineLayout;
        renderPipelineDesc.cVertexStage.module = vsModule;
        renderPipelineDesc.cFragmentStage.module = fsModule;
        renderPipelineDesc.cDepthStencilState.format = dawn::TextureFormat::Depth24PlusStencil8;

        // Test the success case.
        {
            dawn::RenderBundleEncoder renderBundleEncoder =
                device.CreateRenderBundleEncoder(&renderBundleDesc);
            dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
            renderBundleEncoder.SetPipeline(pipeline);
            renderBundleEncoder.Finish();
        }

        // Test the failure case for mismatched format.
        {
            utils::ComboRenderPipelineDescriptor desc = renderPipelineDesc;
            desc.cDepthStencilState.format = dawn::TextureFormat::Depth24Plus;
            desc.depthStencilState = &desc.cDepthStencilState;

            dawn::RenderBundleEncoder renderBundleEncoder =
                device.CreateRenderBundleEncoder(&renderBundleDesc);
            dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
            renderBundleEncoder.SetPipeline(pipeline);
            ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
        }

        // Test the failure case for missing format.
        {
            utils::ComboRenderPipelineDescriptor desc = renderPipelineDesc;
            desc.depthStencilState = nullptr;

            dawn::RenderBundleEncoder renderBundleEncoder =
                device.CreateRenderBundleEncoder(&renderBundleDesc);
            dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
            renderBundleEncoder.SetPipeline(pipeline);
            ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
        }
    }

    // Test that encoding SetPipline with an incompatible sample count produces an error.
    // TODO(enga): Enable when sampleCount is supported.
    TEST_F(RenderBundleValidationTest, DISABLED_PipelineSampleCountMismatch) {
        utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
        renderBundleDesc.colorFormatsCount = 1;
        *renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
        renderBundleDesc.sampleCount = 2;

        utils::ComboRenderPipelineDescriptor renderPipelineDesc(device);
        renderPipelineDesc.colorStateCount = 1;
        renderPipelineDesc.cColorStates[0]->format = dawn::TextureFormat::RGBA8Unorm;
        renderPipelineDesc.sampleCount = 2;
        renderPipelineDesc.layout = pipelineLayout;
        renderPipelineDesc.cVertexStage.module = vsModule;
        renderPipelineDesc.cFragmentStage.module = fsModule;

        // Test the success case.
        {
            dawn::RenderBundleEncoder renderBundleEncoder =
                device.CreateRenderBundleEncoder(&renderBundleDesc);
            dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
            renderBundleEncoder.SetPipeline(pipeline);
            renderBundleEncoder.Finish();
        }

        // Test the failure case.
        {
            renderPipelineDesc.sampleCount = 1;

            dawn::RenderBundleEncoder renderBundleEncoder =
                device.CreateRenderBundleEncoder(&renderBundleDesc);
            dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&renderPipelineDesc);
            renderBundleEncoder.SetPipeline(pipeline);
            ASSERT_DEVICE_ERROR(renderBundleEncoder.Finish());
        }
    }

    // Test that encoding ExecuteBundles with an incompatible color format produces an error.
    TEST_F(RenderBundleValidationTest, RenderPassColorFormatMismatch) {
        utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
        renderBundleDesc.colorFormatsCount = 3;
        *renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
        *renderBundleDesc.cColorFormats[1] = dawn::TextureFormat::RG16Float;
        *renderBundleDesc.cColorFormats[2] = dawn::TextureFormat::R16Sint;

        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        dawn::TextureDescriptor textureDesc = {};
        textureDesc.usage = dawn::TextureUsageBit::OutputAttachment;
        textureDesc.size = dawn::Extent3D({400, 400, 1});

        textureDesc.format = dawn::TextureFormat::RGBA8Unorm;
        dawn::Texture tex0 = device.CreateTexture(&textureDesc);

        textureDesc.format = dawn::TextureFormat::RG16Float;
        dawn::Texture tex1 = device.CreateTexture(&textureDesc);

        textureDesc.format = dawn::TextureFormat::R16Sint;
        dawn::Texture tex2 = device.CreateTexture(&textureDesc);

        // Test the success case
        {
            utils::ComboRenderPassDescriptor renderPass({
                tex0.CreateDefaultView(),
                tex1.CreateDefaultView(),
                tex2.CreateDefaultView(),
            });

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.EndPass();
            commandEncoder.Finish();
        }

        // Test the failure case
        {
            utils::ComboRenderPassDescriptor renderPass({
                tex0.CreateDefaultView(),
                tex1.CreateDefaultView(),
                tex0.CreateDefaultView(),
            });

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        }
    }

    // Test that encoding ExecuteBundles with an incompatible depth stencil format produces an
    // error.
    TEST_F(RenderBundleValidationTest, RenderPassDepthStencilFormatMismatch) {
        utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
        renderBundleDesc.colorFormatsCount = 1;
        *renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;
        renderBundleDesc.cDepthStencilFormat = dawn::TextureFormat::Depth24Plus;
        renderBundleDesc.depthStencilFormat = &renderBundleDesc.cDepthStencilFormat;

        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        dawn::TextureDescriptor textureDesc = {};
        textureDesc.usage = dawn::TextureUsageBit::OutputAttachment;
        textureDesc.size = dawn::Extent3D({400, 400, 1});

        textureDesc.format = dawn::TextureFormat::RGBA8Unorm;
        dawn::Texture tex0 = device.CreateTexture(&textureDesc);

        textureDesc.format = dawn::TextureFormat::Depth24Plus;
        dawn::Texture tex1 = device.CreateTexture(&textureDesc);

        textureDesc.format = dawn::TextureFormat::Depth32Float;
        dawn::Texture tex2 = device.CreateTexture(&textureDesc);

        // Test the success case
        {
            utils::ComboRenderPassDescriptor renderPass({tex0.CreateDefaultView()},
                                                        tex1.CreateDefaultView());

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.EndPass();
            commandEncoder.Finish();
        }

        // Test the failure case
        {
            utils::ComboRenderPassDescriptor renderPass({tex0.CreateDefaultView()},
                                                        tex2.CreateDefaultView());

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        }
    }

    // Test that encoding ExecuteBundles with an incompatible sample count produces an error.
    // TODO(enga): Enable when sampleCount is supported.
    TEST_F(RenderBundleValidationTest, DISABLED_RenderPassSampleCountMismatch) {
        utils::ComboRenderBundleEncoderDescriptor renderBundleDesc = {};
        renderBundleDesc.colorFormatsCount = 1;
        *renderBundleDesc.cColorFormats[0] = dawn::TextureFormat::RGBA8Unorm;

        dawn::RenderBundleEncoder renderBundleEncoder =
            device.CreateRenderBundleEncoder(&renderBundleDesc);
        dawn::RenderBundle renderBundle = renderBundleEncoder.Finish();

        dawn::TextureDescriptor textureDesc = {};
        textureDesc.usage = dawn::TextureUsageBit::OutputAttachment;
        textureDesc.size = dawn::Extent3D({400, 400, 1});

        textureDesc.format = dawn::TextureFormat::RGBA8Unorm;
        dawn::Texture tex0 = device.CreateTexture(&textureDesc);

        textureDesc.sampleCount = 2;
        dawn::Texture tex1 = device.CreateTexture(&textureDesc);

        // Test the success case
        {
            utils::ComboRenderPassDescriptor renderPass({tex0.CreateDefaultView()});

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.EndPass();
            commandEncoder.Finish();
        }

        // Test the failure case
        {
            utils::ComboRenderPassDescriptor renderPass({tex1.CreateDefaultView()});

            dawn::CommandEncoder commandEncoder = device.CreateCommandEncoder();
            dawn::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&renderPass);
            pass.ExecuteBundles(1, &renderBundle);
            pass.EndPass();
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        }
    }

}  // anonymous namespace
