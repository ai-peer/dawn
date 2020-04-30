// Copyright 2020 The Dawn Authors
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

#include "common/Assert.h"
#include "common/Constants.h"
#include "tests/DawnTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class DepthSamplingTest : public DawnTest {
  protected:
    void TestSetUp() override {
        DawnTest::TestSetUp();

        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                void main() {
                    gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
                    gl_PointSize = 1.0;
                }
            )");

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
                #version 450
                layout(set = 0, binding = 0) uniform sampler samp;
                layout(set = 0, binding = 1) uniform texture2D tex;

                layout(location = 0) out float samplerResult;

                void main() {
                    samplerResult = texture(sampler2D(tex, samp), vec2(0.5, 0.5)).r;
                }
            )");

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler},
                     {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture}});

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        pipelineDescriptor.layout = utils::MakeBasicPipelineLayout(device, &bgl);
        pipelineDescriptor.primitiveTopology = wgpu::PrimitiveTopology::PointList;
        pipelineDescriptor.cColorStates[0].format = wgpu::TextureFormat::R32Float;

        mRenderPipeline = device.CreateRenderPipeline(&pipelineDescriptor);

        wgpu::BufferDescriptor textureUploadDesc;
        textureUploadDesc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        textureUploadDesc.size = sizeof(float);
        mTextureUploadBuffer = device.CreateBuffer(&textureUploadDesc);

        wgpu::TextureDescriptor outputTextureDesc;
        outputTextureDesc.usage =
            wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc;
        outputTextureDesc.size = {1, 1, 1};
        outputTextureDesc.format = wgpu::TextureFormat::R32Float;
        mOutputTexture = device.CreateTexture(&outputTextureDesc);
    }

    void DoTest(wgpu::TextureFormat textureFormat, std::vector<float> textureValues) {
        wgpu::TextureDescriptor inputTextureDesc;
        inputTextureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled |
                                 wgpu::TextureUsage::OutputAttachment;
        inputTextureDesc.size = {1, 1, 1};
        inputTextureDesc.format = textureFormat;
        wgpu::Texture inputTexture = device.CreateTexture(&inputTextureDesc);

        wgpu::SamplerDescriptor samplerDesc;
        wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, mRenderPipeline.GetBindGroupLayout(0),
                                 {
                                     {0, sampler},
                                     {1, inputTexture.CreateView()},
                                 });

        for (float textureValue : textureValues) {
            wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();

            // Set the input depth texture to the provided texture value
            if (textureValue >= 0.0 && textureValue <= 1.0) {
                // For valid loadOp values, use a loadOp.
                utils::ComboRenderPassDescriptor passDescriptor({}, inputTexture.CreateView());
                passDescriptor.cDepthStencilAttachmentInfo.clearDepth = textureValue;

                wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
                pass.EndPass();
            } else {
                if (IsOpenGL()) {
                    // TODO(enga): We don't support copying to depth textures yet on OpenGL.
                    return;
                }
                mTextureUploadBuffer.SetSubData(0, sizeof(float), &textureValue);
                wgpu::BufferCopyView bufferCopyView;
                bufferCopyView.buffer = mTextureUploadBuffer;
                bufferCopyView.offset = 0;
                bufferCopyView.bytesPerRow = kTextureBytesPerRowAlignment;
                bufferCopyView.rowsPerImage = 1;
                wgpu::TextureCopyView textureCopyView;
                textureCopyView.texture = inputTexture;
                textureCopyView.origin = {0, 0, 0};
                wgpu::Extent3D copySize = {1, 1, 1};
                commandEncoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);
            }

            // Render into the output texture
            {
                utils::ComboRenderPassDescriptor passDescriptor({mOutputTexture.CreateView()});
                wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
                pass.SetPipeline(mRenderPipeline);
                pass.SetBindGroup(0, bindGroup);
                pass.Draw(3);
                pass.EndPass();
            }

            wgpu::CommandBuffer commands = commandEncoder.Finish();
            queue.Submit(1, &commands);

            EXPECT_PIXEL_FLOAT_EQ(textureValue, mOutputTexture, 0, 0);
        }
    }

  private:
    wgpu::RenderPipeline mRenderPipeline;
    wgpu::Buffer mTextureUploadBuffer;
    wgpu::Texture mOutputTexture;
};

// Test that sampling with all of the compare functions works.
TEST_P(DepthSamplingTest, Depth32Float) {
    // Test negative, 0, between 0 and 1, 1, and above 1.
    std::vector<float> values = {-0.2, 0.0, 0.37, 1.0, 1.3};

    DoTest(wgpu::TextureFormat::Depth32Float, values);
}

DAWN_INSTANTIATE_TEST(DepthSamplingTest,
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend(),
                      D3D12Backend());
