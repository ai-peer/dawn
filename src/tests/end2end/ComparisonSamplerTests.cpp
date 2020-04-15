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

class ComparisonSamplerTest : public DawnTest {
  protected:
    void TestSetUp() override {
        DawnTest::TestSetUp();

        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                void main() {
                    const vec2 pos[3] = vec2[3](vec2(-1.f, -1.f), vec2(3.f, -1.f), vec2(-1.f, 3.f));
                    gl_Position = vec4(pos[gl_VertexIndex], 0.f, 1.f);
                }
            )");

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
                #version 450
                layout(set = 0, binding = 0) uniform samplerShadow samp;
                layout(set = 0, binding = 1) uniform texture2D tex;
                layout(set = 0, binding = 2) uniform Uniforms {
                    float compareRef;
                };

                layout(location = 0) out vec4 samplerResult;

                void main() {
                    samplerResult = vec4(texture(sampler2DShadow(tex, samp), vec3(0.5, 0.5, compareRef)));
                }
            )");

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ComparisonSampler},
                     {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture},
                     {2, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        pipelineDescriptor.layout = utils::MakeBasicPipelineLayout(device, &bgl);

        mRenderPipeline = device.CreateRenderPipeline(&pipelineDescriptor);

        wgpu::BufferDescriptor uniformBufferDesc = {
            .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
            .size = sizeof(float),
        };
        mUniformBuffer = device.CreateBuffer(&uniformBufferDesc);

        wgpu::BufferDescriptor textureUploadDesc = {
            .usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst,
            .size = sizeof(float),
        };
        mTextureUploadBuffer = device.CreateBuffer(&textureUploadDesc);

        wgpu::TextureDescriptor inputTextureDesc = {
            .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled,
            .size = {1, 1, 1},
            .format = wgpu::TextureFormat::R32Float,
        };

        mInputTexture = device.CreateTexture(&inputTextureDesc);

        wgpu::TextureDescriptor outputTextureDesc = {
            .usage = wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc,
            .size = {1, 1, 1},
            .format = wgpu::TextureFormat::RGBA8Unorm,
        };

        mOutputTexture = device.CreateTexture(&outputTextureDesc);
    }

    void DoCompareRefTest(float compareRef,
                          wgpu::CompareFunction compare,
                          std::vector<float> textureValues) {
        mUniformBuffer.SetSubData(0, sizeof(float), &compareRef);

        wgpu::SamplerDescriptor samplerDesc = {
            .compare = compare,
        };
        wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);

        wgpu::BindGroup bindGroup =
            utils::MakeBindGroup(device, mRenderPipeline.GetBindGroupLayout(0),
                                 {
                                     {0, sampler},
                                     {1, mInputTexture.CreateView()},
                                     {2, mUniformBuffer},
                                 });

        for (float textureValue : textureValues) {
            bool success = false;
            switch (compare) {
                case wgpu::CompareFunction::Never:
                    success = false;
                    break;
                case wgpu::CompareFunction::Less:
                    success = compareRef < textureValue;
                    break;
                case wgpu::CompareFunction::LessEqual:
                    success = compareRef <= textureValue;
                    break;
                case wgpu::CompareFunction::Greater:
                    success = compareRef > textureValue;
                    break;
                case wgpu::CompareFunction::GreaterEqual:
                    success = compareRef >= textureValue;
                    break;
                case wgpu::CompareFunction::Equal:
                    success = compareRef == textureValue;
                    break;
                case wgpu::CompareFunction::NotEqual:
                    success = compareRef != textureValue;
                    break;
                case wgpu::CompareFunction::Always:
                    success = true;
                    break;
                default:
                    UNREACHABLE();
                    break;
            }

            wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();

            // Set the input depth texture to the provided texture value
            {
                mTextureUploadBuffer.SetSubData(0, sizeof(float), &textureValue);
                wgpu::BufferCopyView bufferCopyView = {
                    .buffer = mTextureUploadBuffer,
                    .offset = 0,
                    .rowPitch = kTextureRowPitchAlignment,
                    .imageHeight = 1,
                };
                wgpu::TextureCopyView textureCopyView = {
                    .texture = mInputTexture,
                    .origin = {0, 0, 0},
                };
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

            EXPECT_PIXEL_RGBA8_EQ(success ? RGBA8(255, 255, 255, 255) : RGBA8(0, 0, 0, 0),
                                  mOutputTexture, 0, 0);
        }
    }

  private:
    wgpu::RenderPipeline mRenderPipeline;
    wgpu::Buffer mUniformBuffer;
    wgpu::Buffer mTextureUploadBuffer;
    wgpu::Texture mInputTexture;
    wgpu::Texture mOutputTexture;
};

TEST_P(ComparisonSamplerTest, CompareFunctions) {
    // Test a "normal" ref value between 0 and 1; as well as negative and > 1 refs.
    for (float compareRef : {-0.1, 0.4, 1.2}) {
        // Test negative, 0, below the ref, equal to, above the ref, 1, and above 1.
        std::vector<float> values = {-0.2, 0.0, 0.3, 0.4, 0.5, 1.0, 1.3};

        DoCompareRefTest(compareRef, wgpu::CompareFunction::Never, values);
        DoCompareRefTest(compareRef, wgpu::CompareFunction::Less, values);
        DoCompareRefTest(compareRef, wgpu::CompareFunction::LessEqual, values);
        DoCompareRefTest(compareRef, wgpu::CompareFunction::Greater, values);
        DoCompareRefTest(compareRef, wgpu::CompareFunction::GreaterEqual, values);
        DoCompareRefTest(compareRef, wgpu::CompareFunction::Equal, values);
        DoCompareRefTest(compareRef, wgpu::CompareFunction::NotEqual, values);
        DoCompareRefTest(compareRef, wgpu::CompareFunction::Always, values);
    }
}

DAWN_INSTANTIATE_TEST(ComparisonSamplerTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      VulkanBackend());
