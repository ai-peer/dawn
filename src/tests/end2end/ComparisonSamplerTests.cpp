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
#include "utils/WGPUHelpers.h"

class ComparisonSamplerTest : public DawnTest {
  protected:
    void TestSetUp() override {
        DawnTest::TestSetUp();

        wgpu::ShaderModule shaderModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
                #version 450
                layout(set = 0, binding = 0) uniform samplerShadow samp;
                layout(set = 0, binding = 1) uniform texture2D tex;
                layout(set = 0, binding = 2) uniform Uniforms {
                    float compareRef;
                };
                layout(set = 0, binding = 3, std430) buffer Output {
                    float samplerResult;
                };

                void main() {
                    samplerResult = texture(sampler2DShadow(tex, samp), vec3(0.5, 0.5, compareRef));
                }
            )");

        wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::ComparisonSampler},
                     {1, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture},
                     {2, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer},
                     {3, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer}});

        wgpu::ComputePipelineDescriptor pipelineDescriptor = {
            .layout = utils::MakeBasicPipelineLayout(device, &bgl),
            .computeStage = {
                .module = shaderModule,
                .entryPoint = "main",
            },
        };
        mComputePipeline = device.CreateComputePipeline(&pipelineDescriptor);

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
            .usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled |
                     wgpu::TextureUsage::OutputAttachment,
            .size = {1, 1, 1},
            .format = wgpu::TextureFormat::Depth32Float,
        };
        mInputTexture = device.CreateTexture(&inputTextureDesc);

        wgpu::BufferDescriptor outputBufferDesc = {
            .usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc,
            .size = 4,
        };
        mOutputBuffer = device.CreateBuffer(&outputBufferDesc);
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
            utils::MakeBindGroup(device, mComputePipeline.GetBindGroupLayout(0),
                                 {
                                     {0, sampler},
                                     {1, mInputTexture.CreateView()},
                                     {2, mUniformBuffer},
                                     {3, mOutputBuffer},
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
            if (textureValue >= 0.0 && textureValue <= 1.0) {
                // For valid loadOp values, use a loadOp.
                utils::ComboRenderPassDescriptor passDescriptor({}, mInputTexture.CreateView());
                passDescriptor.cDepthStencilAttachmentInfo.clearDepth = textureValue;

                wgpu::RenderPassEncoder pass = commandEncoder.BeginRenderPass(&passDescriptor);
                pass.EndPass();
            } else {
                if (IsOpenGL()) {
                    // TODO(enga): We don't support copying to depth textures yet on OpenGL.
                    return;
                }
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

            // Sample into the output buffer
            {
                wgpu::ComputePassEncoder pass = commandEncoder.BeginComputePass();
                pass.SetPipeline(mComputePipeline);
                pass.SetBindGroup(0, bindGroup);
                pass.Dispatch(1);
                pass.EndPass();
            }

            wgpu::CommandBuffer commands = commandEncoder.Finish();
            queue.Submit(1, &commands);

            float float0 = 0.0;
            float float1 = 1.0;
            uint32_t expected = success ? *reinterpret_cast<uint32_t*>(&float1)
                                        : *reinterpret_cast<uint32_t*>(&float0);
            EXPECT_BUFFER_U32_EQ(expected, mOutputBuffer, 0);
        }
    }

  private:
    wgpu::ComputePipeline mComputePipeline;
    wgpu::Buffer mUniformBuffer;
    wgpu::Buffer mTextureUploadBuffer;
    wgpu::Texture mInputTexture;
    wgpu::Buffer mOutputBuffer;
};

// Test that sampling with all of the compare functions works.
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

// TODO(crbug.com/dawn/367): Does not work on D3D12 because we need to reinterpret the texture view
// as R32Float to sample it. See tables here:
// https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/hardware-support-for-direct3d-12-1-formats
DAWN_INSTANTIATE_TEST(ComparisonSamplerTest, MetalBackend(), OpenGLBackend(), VulkanBackend());
