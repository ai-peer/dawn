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

#include <limits>
#include <string>

#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

class CompatValidationTest : public ValidationTest {
  protected:
    bool UseCompatibilityMode() const override { return true; }
};

TEST_F(CompatValidationTest, CanNotCreateCubeArrayTextureView) {
    wgpu::TextureDescriptor descriptor;
    descriptor.size = {1, 1, 6};
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding;
    wgpu::Texture cubeTexture = device.CreateTexture(&descriptor);

    {
        wgpu::TextureViewDescriptor cubeViewDescriptor;
        cubeViewDescriptor.dimension = wgpu::TextureViewDimension::Cube;
        cubeViewDescriptor.format = wgpu::TextureFormat::RGBA8Unorm;

        cubeTexture.CreateView(&cubeViewDescriptor);
    }

    {
        wgpu::TextureViewDescriptor cubeArrayViewDescriptor;
        cubeArrayViewDescriptor.dimension = wgpu::TextureViewDimension::CubeArray;
        cubeArrayViewDescriptor.format = wgpu::TextureFormat::RGBA8Unorm;

        ASSERT_DEVICE_ERROR(cubeTexture.CreateView(&cubeArrayViewDescriptor));
    }

    cubeTexture.Destroy();
}

TEST_F(CompatValidationTest, CanNotCreatePipelineWithDifferentPerTargetBlendStateOrWriteMask) {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        @vertex fn vs() -> @builtin(position) vec4f {
            return vec4f(0);
        }

        struct FragmentOut {
            @location(0) fragColor0 : vec4f,
            @location(1) fragColor1 : vec4f,
            @location(2) fragColor2 : vec4f,
        }

        @fragment fn fs() -> FragmentOut {
            var output : FragmentOut;
            output.fragColor0 = vec4f(0);
            output.fragColor1 = vec4f(0);
            output.fragColor2 = vec4f(0);
            return output;
        }
    )");

    utils::ComboRenderPipelineDescriptor testDescriptor;
    testDescriptor.layout = {};
    testDescriptor.vertex.module = module;
    testDescriptor.vertex.entryPoint = "vs";
    testDescriptor.cFragment.module = module;
    testDescriptor.cFragment.entryPoint = "fs";
    testDescriptor.cFragment.targetCount = 3;
    testDescriptor.cTargets[1].format = wgpu::TextureFormat::Undefined;

    for (int i = 0; i < 10; ++i) {
        wgpu::BlendState blend0;
        wgpu::BlendState blend2;

        // Blend state intentionally omitted for target 1
        testDescriptor.cTargets[0].blend = &blend0;
        testDescriptor.cTargets[2].blend = &blend2;

        bool expectError = true;
        switch (i) {
            case 0:  // default
                expectError = false;
                break;
            case 1:  // no blend
                testDescriptor.cTargets[0].blend = nullptr;
                break;
            case 2:  // no blend second target
                testDescriptor.cTargets[2].blend = nullptr;
                break;
            case 3:  // color.operation
                blend2.color.operation = wgpu::BlendOperation::Subtract;
                break;
            case 4:  // color.srcFactor
                blend2.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
                break;
            case 5:  // color.dstFactor
                blend2.color.dstFactor = wgpu::BlendFactor::DstAlpha;
                break;
            case 6:  // alpha.operation
                blend2.alpha.operation = wgpu::BlendOperation::Subtract;
                break;
            case 7:  // alpha.srcFactor
                blend2.alpha.srcFactor = wgpu::BlendFactor::SrcAlpha;
                break;
            case 8:  // alpha.dstFactor
                blend2.alpha.dstFactor = wgpu::BlendFactor::DstAlpha;
                break;
            case 9:  // writeMask
                testDescriptor.cTargets[2].writeMask = wgpu::ColorWriteMask::Green;
                break;
            default:
                UNREACHABLE();
        }

        if (expectError) {
            ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&testDescriptor));
        } else {
            device.CreateRenderPipeline(&testDescriptor);
        }
    }
}

TEST_F(CompatValidationTest, CanNotDrawDifferentMipsSameTextureSameBindGroup) {
    wgpu::TextureDescriptor descriptor;
    descriptor.size = {2, 1, 1};
    descriptor.mipLevelCount = 2;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Create a pipeline that will sample from 2 2D textures and output to an attachment.
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        @vertex
        fn vs(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
            var pos = array(
                vec4f(-1,  3, 0, 1),
                vec4f( 3, -1, 0, 1),
                vec4f(-1, -1, 0, 1));
            return pos[VertexIndex];
        }

        @group(0) @binding(0) var tex0 : texture_2d<f32>;
        @group(0) @binding(1) var tex1 : texture_2d<f32>;
        @group(0) @binding(2) var samp : sampler;
        @fragment
        fn fs(@builtin(position) pos: vec4f) -> @location(0) vec4f {
            let c0 = textureSample(tex0, samp, vec2f(0.5));
            let c1 = textureSample(tex1, samp, vec2f(0.5));
            return select(c0, c1, i32(pos.x) % 2 == 1);
        }
    )");
    utils::ComboRenderPipelineDescriptor pDesc;
    pDesc.vertex.module = module;
    pDesc.vertex.entryPoint = "vs";
    pDesc.cFragment.module = module;
    pDesc.cFragment.entryPoint = "fs";
    pDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pDesc);

    wgpu::TextureViewDescriptor mip0ViewDesc;
    mip0ViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    mip0ViewDesc.baseMipLevel = 0;
    mip0ViewDesc.mipLevelCount = 1;

    wgpu::TextureViewDescriptor mip1ViewDesc;
    mip1ViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    mip1ViewDesc.baseMipLevel = 1;
    mip1ViewDesc.mipLevelCount = 1;

    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {{0, texture.CreateView(&mip0ViewDesc)},
                                                      {1, texture.CreateView(&mip1ViewDesc)},
                                                      {2, device.CreateSampler()}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    utils::BasicRenderPass rp = utils::CreateBasicRenderPass(device, 4, 1);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rp.renderPassInfo);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(3);
    pass.End();

    ASSERT_DEVICE_ERROR(encoder.Finish());

    texture.Destroy();
}

TEST_F(CompatValidationTest, CanNotDrawDifferentMipsSameTextureDifferentBindGroups) {
    wgpu::TextureDescriptor descriptor;
    descriptor.size = {2, 1, 1};
    descriptor.mipLevelCount = 2;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Create a pipeline that will sample from 2 2D textures and output to an attachment.
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        @vertex
        fn vs(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
            var pos = array(
                vec4f(-1,  3, 0, 1),
                vec4f( 3, -1, 0, 1),
                vec4f(-1, -1, 0, 1));
            return pos[VertexIndex];
        }

        @group(0) @binding(0) var tex0 : texture_2d<f32>;
        @group(0) @binding(1) var samp : sampler;
        @group(1) @binding(0) var tex1 : texture_2d<f32>;

        @fragment
        fn fs(@builtin(position) pos: vec4f) -> @location(0) vec4f {
            let c0 = textureSample(tex0, samp, vec2f(0.5));
            let c1 = textureSample(tex1, samp, vec2f(0.5));
            return select(c0, c1, i32(pos.x) % 2 == 1);
        }
    )");
    utils::ComboRenderPipelineDescriptor pDesc;
    pDesc.vertex.module = module;
    pDesc.vertex.entryPoint = "vs";
    pDesc.cFragment.module = module;
    pDesc.cFragment.entryPoint = "fs";
    pDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pDesc);

    wgpu::TextureViewDescriptor mip0ViewDesc;
    mip0ViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    mip0ViewDesc.baseMipLevel = 0;
    mip0ViewDesc.mipLevelCount = 1;

    wgpu::TextureViewDescriptor mip1ViewDesc;
    mip1ViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    mip1ViewDesc.baseMipLevel = 1;
    mip1ViewDesc.mipLevelCount = 1;

    wgpu::BindGroup bindGroup0 =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                             {{0, texture.CreateView(&mip0ViewDesc)}, {1, device.CreateSampler()}});

    wgpu::BindGroup bindGroup1 = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(1),
                                                      {{0, texture.CreateView(&mip1ViewDesc)}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    utils::BasicRenderPass rp = utils::CreateBasicRenderPass(device, 4, 1);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rp.renderPassInfo);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup0);
    pass.SetBindGroup(1, bindGroup1);
    pass.Draw(3);
    pass.End();

    ASSERT_DEVICE_ERROR(encoder.Finish());

    texture.Destroy();
}

TEST_F(CompatValidationTest, CanBindDifferentMipsSameTextureSameBindGroupAndFixWithoutError) {
    wgpu::TextureDescriptor descriptor;
    descriptor.size = {2, 1, 1};
    descriptor.mipLevelCount = 2;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Create a pipeline that will sample from 2 2D textures and output to an attachment.
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        @vertex
        fn vs(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
            var pos = array(
                vec4f(-1,  3, 0, 1),
                vec4f( 3, -1, 0, 1),
                vec4f(-1, -1, 0, 1));
            return pos[VertexIndex];
        }

        @group(0) @binding(0) var tex0 : texture_2d<f32>;
        @group(0) @binding(1) var tex1 : texture_2d<f32>;
        @group(0) @binding(2) var samp : sampler;
        @fragment
        fn fs(@builtin(position) pos: vec4f) -> @location(0) vec4f {
            let c0 = textureSample(tex0, samp, vec2f(0.5));
            let c1 = textureSample(tex1, samp, vec2f(0.5));
            return select(c0, c1, i32(pos.x) % 2 == 1);
        }
    )");
    utils::ComboRenderPipelineDescriptor pDesc;
    pDesc.vertex.module = module;
    pDesc.vertex.entryPoint = "vs";
    pDesc.cFragment.module = module;
    pDesc.cFragment.entryPoint = "fs";
    pDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pDesc);

    wgpu::TextureViewDescriptor mip0ViewDesc;
    mip0ViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    mip0ViewDesc.baseMipLevel = 0;
    mip0ViewDesc.mipLevelCount = 1;

    wgpu::TextureViewDescriptor mip1ViewDesc;
    mip1ViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    mip1ViewDesc.baseMipLevel = 1;
    mip1ViewDesc.mipLevelCount = 1;

    // Bindgroup with different views of same texture
    wgpu::BindGroup badBindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                        {{0, texture.CreateView(&mip0ViewDesc)},
                                                         {1, texture.CreateView(&mip1ViewDesc)},
                                                         {2, device.CreateSampler()}});

    // Bindgroup with same views of texture
    wgpu::BindGroup goodBindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                         {{0, texture.CreateView(&mip0ViewDesc)},
                                                          {1, texture.CreateView(&mip0ViewDesc)},
                                                          {2, device.CreateSampler()}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    utils::BasicRenderPass rp = utils::CreateBasicRenderPass(device, 4, 1);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rp.renderPassInfo);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, badBindGroup);
    pass.SetBindGroup(0, goodBindGroup);
    pass.Draw(3);
    pass.End();

    // No Error is expected
    encoder.Finish();

    texture.Destroy();
}

TEST_F(CompatValidationTest, CanBindSameView2BindGroups) {
    wgpu::TextureDescriptor descriptor;
    descriptor.size = {2, 1, 1};
    descriptor.mipLevelCount = 2;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Create a pipeline that will sample from 2 2D textures and output to an attachment.
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        @vertex
        fn vs(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
            var pos = array(
                vec4f(-1,  3, 0, 1),
                vec4f( 3, -1, 0, 1),
                vec4f(-1, -1, 0, 1));
            return pos[VertexIndex];
        }

        @group(0) @binding(0) var tex0 : texture_2d<f32>;
        @group(1) @binding(0) var tex1 : texture_2d<f32>;
        @group(0) @binding(1) var samp : sampler;
        @fragment
        fn fs(@builtin(position) pos: vec4f) -> @location(0) vec4f {
            let c0 = textureSample(tex0, samp, vec2f(0.5));
            let c1 = textureSample(tex1, samp, vec2f(0.5));
            return select(c0, c1, i32(pos.x) % 2 == 1);
        }
    )");
    utils::ComboRenderPipelineDescriptor pDesc;
    pDesc.vertex.module = module;
    pDesc.vertex.entryPoint = "vs";
    pDesc.cFragment.module = module;
    pDesc.cFragment.entryPoint = "fs";
    pDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pDesc);

    wgpu::TextureViewDescriptor mip0ViewDesc;
    mip0ViewDesc.dimension = wgpu::TextureViewDimension::e2D;

    wgpu::TextureViewDescriptor mip1ViewDesc;
    mip1ViewDesc.dimension = wgpu::TextureViewDimension::e2D;

    wgpu::BindGroup bindGroup0 =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                             {{0, texture.CreateView(&mip0ViewDesc)}, {1, device.CreateSampler()}});

    // Bindgroup with same views of texture
    wgpu::BindGroup bindGroup1 = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(1),
                                                      {{0, texture.CreateView(&mip1ViewDesc)}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    utils::BasicRenderPass rp = utils::CreateBasicRenderPass(device, 4, 1);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rp.renderPassInfo);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup0);
    pass.SetBindGroup(1, bindGroup1);
    pass.Draw(3);
    pass.End();

    // No Error is expected
    encoder.Finish();

    texture.Destroy();
}

TEST_F(CompatValidationTest, NoErrorIfMultipleDifferentViewsOfTextureAreNotUsed) {
    wgpu::TextureDescriptor descriptor;
    descriptor.size = {2, 1, 1};
    descriptor.mipLevelCount = 2;
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding;
    wgpu::Texture texture = device.CreateTexture(&descriptor);

    // Create a pipeline that will sample from 2 2D textures and output to an attachment.
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        @vertex
        fn vs(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
            var pos = array(
                vec4f(-1,  3, 0, 1),
                vec4f( 3, -1, 0, 1),
                vec4f(-1, -1, 0, 1));
            return pos[VertexIndex];
        }

        @group(0) @binding(0) var tex0 : texture_2d<f32>;
        @group(1) @binding(0) var tex1 : texture_2d<f32>;
        @group(0) @binding(1) var samp : sampler;
        @fragment
        fn fs(@builtin(position) pos: vec4f) -> @location(0) vec4f {
            let c0 = textureSample(tex0, samp, vec2f(0.5));
            let c1 = textureSample(tex1, samp, vec2f(0.5));
            return select(c0, c1, i32(pos.x) % 2 == 1);
        }
    )");
    utils::ComboRenderPipelineDescriptor pDesc;
    pDesc.vertex.module = module;
    pDesc.vertex.entryPoint = "vs";
    pDesc.cFragment.module = module;
    pDesc.cFragment.entryPoint = "fs";
    pDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pDesc);

    wgpu::TextureViewDescriptor mip0ViewDesc;
    mip0ViewDesc.dimension = wgpu::TextureViewDimension::e2D;

    wgpu::TextureViewDescriptor mip1ViewDesc;
    mip1ViewDesc.dimension = wgpu::TextureViewDimension::e2D;
    mip1ViewDesc.baseMipLevel = 1;
    mip1ViewDesc.mipLevelCount = 1;

    // Bindgroup with different views of same texture
    wgpu::BindGroup bindGroup0 =
        utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                             {{0, texture.CreateView(&mip0ViewDesc)}, {1, device.CreateSampler()}});

    // Bindgroup with same views of texture
    wgpu::BindGroup bindGroup1 = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(1),
                                                      {{0, texture.CreateView(&mip1ViewDesc)}});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    utils::BasicRenderPass rp = utils::CreateBasicRenderPass(device, 4, 1);
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&rp.renderPassInfo);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup0);
    pass.SetBindGroup(1, bindGroup1);
    pass.End();

    // No Error is expected because draw was never called
    encoder.Finish();

    texture.Destroy();
}

}  // anonymous namespace
}  // namespace dawn
