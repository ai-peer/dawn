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

#include "dawn/native/BlitBufferToStencil.h"

#include <utility>

#include "dawn/common/Assert.h"
#include "dawn/native/BindGroup.h"
#include "dawn/native/CommandBuffer.h"
#include "dawn/native/CommandEncoder.h"
#include "dawn/native/Device.h"
#include "dawn/native/InternalPipelineStore.h"
#include "dawn/native/Queue.h"
#include "dawn/native/RenderPassEncoder.h"
#include "dawn/native/RenderPipeline.h"

namespace dawn::native {

namespace {

constexpr char kShaders[] = R"(

struct VertexOutputs {
  @location(0) @interpolate(flat) stencil_val : u32,
  @builtin(position) position : vec4<f32>,
};

// The instance_index here is not used for instancing.
// It represents the current stencil mask we're testing in the
// source.
// This is a cheap way to get the stencil value into the shader
// since WebGPU doesn't have push constants.
@vertex fn vert_fullscreen_quad(
  @builtin(vertex_index) vertex_index : u32,
  @builtin(instance_index) instance_index: u32,
) -> VertexOutputs {
  const pos = array<vec2<f32>, 3>(
      vec2<f32>(-1.0, -1.0),
      vec2<f32>( 3.0, -1.0),
      vec2<f32>(-1.0,  3.0));
  return VertexOutputs(
    instance_index,
    vec4<f32>(pos[vertex_index], 0.0, 1.0),
  );
}

struct Params {
  origin : vec2<u32>
};

@group(0) @binding(0) var src_tex : texture_2d<u32>;
@group(0) @binding(1) var<uniform> params : Params;

// Do nothing (but also don't discard). Used for clearing
// stencil to 0.
@fragment fn frag_noop() {}

// Discard the fragment if the source texture doesn't
// have the stencil_val.
@fragment fn frag_check_src_stencil(input : VertexOutputs) {
  // Load the source stencil value.
  let src_val : u32 = textureLoad(
    src_tex, vec2<u32>(input.position.xy) - params.origin, 0u)[0];

  // Discard it if it doesn't contain the stencil reference.
  if ((src_val & input.stencil_val) == 0u) {
    discard;
  }
}

)";

ResultOrError<InternalPipelineStore::BlitBufferToStencilPipelines>
GetOrCreatePipelines(DeviceBase* device, wgpu::TextureFormat format, BindGroupLayoutBase* bgl) {
    InternalPipelineStore* store = device->GetInternalPipelineStore();
    {
        auto it = store->blitBufferToStencilPipelines.find(format);
        if (it != store->blitBufferToStencilPipelines.end()) {
            return InternalPipelineStore::BlitBufferToStencilPipelines{it->second};
        }
    }

    Ref<PipelineLayoutBase> pipelineLayout;
    {
        PipelineLayoutDescriptor plDesc = {};
        plDesc.bindGroupLayoutCount = 1;

        plDesc.bindGroupLayouts = &bgl;
        DAWN_TRY_ASSIGN(pipelineLayout, device->CreatePipelineLayout(&plDesc));
    }

    ShaderModuleWGSLDescriptor wgslDesc = {};
    ShaderModuleDescriptor shaderModuleDesc = {};
    shaderModuleDesc.nextInChain = &wgslDesc;
    wgslDesc.source = kShaders;

    Ref<ShaderModuleBase> shaderModule;
    DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&shaderModuleDesc));

    FragmentState fragmentState = {};
    fragmentState.module = shaderModule.Get();

    DepthStencilState dsState = {};
    dsState.format = format;
    dsState.depthWriteEnabled = false;
    dsState.stencilFront.passOp = wgpu::StencilOperation::Replace;

    RenderPipelineDescriptor renderPipelineDesc = {};
    renderPipelineDesc.layout = pipelineLayout.Get();
    renderPipelineDesc.vertex.module = shaderModule.Get();
    renderPipelineDesc.vertex.entryPoint = "vert_fullscreen_quad";
    renderPipelineDesc.depthStencil = &dsState;
    renderPipelineDesc.fragment = &fragmentState;

    // Build a pipeline to clear stencil to 0. We need a pipeline, and not just a render pass load
    // op because the copy region may be a subregion of the stencil texture.
    Ref<RenderPipelineBase> clearPipeline;
    fragmentState.entryPoint = "frag_noop";
    DAWN_TRY_ASSIGN(clearPipeline, device->CreateRenderPipeline(&renderPipelineDesc));

    // Build 8 pipelines masked to replace each bit of the stencil.
    std::array<Ref<RenderPipelineBase>, 8> setStencilPipelines;
    fragmentState.entryPoint = "frag_check_src_stencil";
    for (uint32_t bit = 0; bit < 8; ++bit) {
        dsState.stencilWriteMask = 1u << bit;
        DAWN_TRY_ASSIGN(setStencilPipelines[bit],
                        device->CreateRenderPipeline(&renderPipelineDesc));
    }

    InternalPipelineStore::BlitBufferToStencilPipelines pipelines{std::move(clearPipeline),
                                                                  std::move(setStencilPipelines)};
    store->blitBufferToStencilPipelines[format] = pipelines;
    return pipelines;
}

MaybeError BlitR8ToStencil(DeviceBase* device,
                           CommandEncoder* commandEncoder,
                           TextureBase* stencilDataTexture,
                           const TextureCopy& dst,
                           const Extent3D& copyExtent) {
    const Format& format = dst.texture->GetFormat();
    ASSERT(dst.aspect == Aspect::Stencil);

    // Allow internal usages since we need to use the destination
    // as a render attachment.
    auto scope = commandEncoder->MakeInternalUsageScope();

    // This bgl is the same for all the render pipelines.
    Ref<BindGroupLayoutBase> bgl;
    {
        std::array<BindGroupLayoutEntry, 2> bglEntries = {};
        bglEntries[0].binding = 0;
        bglEntries[0].visibility = wgpu::ShaderStage::Fragment;
        bglEntries[0].texture.sampleType = wgpu::TextureSampleType::Uint;
        bglEntries[1].binding = 1;
        bglEntries[1].visibility = wgpu::ShaderStage::Fragment;
        bglEntries[1].buffer.type = wgpu::BufferBindingType::Uniform;
        bglEntries[1].buffer.minBindingSize = 2 * sizeof(uint32_t);

        BindGroupLayoutDescriptor bglDesc = {};
        bglDesc.entryCount = bglEntries.size();
        bglDesc.entries = bglEntries.data();

        DAWN_TRY_ASSIGN(bgl, device->CreateBindGroupLayout(&bglDesc));
    }

    InternalPipelineStore::BlitBufferToStencilPipelines pipelines;
    DAWN_TRY_ASSIGN(pipelines, GetOrCreatePipelines(device, format.format, bgl.Get()));

    for (uint32_t z = 0; z < copyExtent.depthOrArrayLayers; ++z) {
        Ref<TextureViewBase> srcView;
        {
            TextureViewDescriptor viewDesc = {};
            viewDesc.dimension = wgpu::TextureViewDimension::e2D;
            viewDesc.baseArrayLayer = z;
            viewDesc.arrayLayerCount = 1;
            viewDesc.mipLevelCount = 1;
            DAWN_TRY_ASSIGN(srcView, stencilDataTexture->CreateView(&viewDesc));
        }

        Ref<TextureViewBase> dstView;
        {
            TextureViewDescriptor viewDesc = {};
            viewDesc.dimension = wgpu::TextureViewDimension::e2D;
            viewDesc.baseArrayLayer = dst.origin.z + z;
            viewDesc.arrayLayerCount = 1;
            viewDesc.baseMipLevel = dst.mipLevel;
            viewDesc.mipLevelCount = 1;
            DAWN_TRY_ASSIGN(dstView, dst.texture->CreateView(&viewDesc));
        }

        Ref<BufferBase> paramsBuffer;
        {
            BufferDescriptor bufferDesc = {};
            bufferDesc.size = sizeof(uint32_t) * 2;
            bufferDesc.usage = wgpu::BufferUsage::Uniform;
            bufferDesc.mappedAtCreation = true;
            DAWN_TRY_ASSIGN(paramsBuffer, device->CreateBuffer(&bufferDesc));

            uint32_t* params =
                static_cast<uint32_t*>(paramsBuffer->GetMappedRange(0, bufferDesc.size));
            params[0] = dst.origin.x;
            params[1] = dst.origin.y;
            paramsBuffer->Unmap();
        }

        Ref<BindGroupBase> bindGroup;
        {
            std::array<BindGroupEntry, 2> bgEntries = {};
            bgEntries[0].binding = 0;
            bgEntries[0].textureView = srcView.Get();
            bgEntries[1].binding = 1;
            bgEntries[1].buffer = paramsBuffer.Get();

            BindGroupDescriptor bgDesc = {};
            bgDesc.layout = bgl.Get();
            bgDesc.entryCount = bgEntries.size();
            bgDesc.entries = bgEntries.data();
            DAWN_TRY_ASSIGN(bindGroup, device->CreateBindGroup(&bgDesc));
        }

        RenderPassDepthStencilAttachment dsAttachment;
        dsAttachment.view = dstView.Get();
        if (format.HasDepth()) {
            dsAttachment.depthLoadOp = wgpu::LoadOp::Load;
            dsAttachment.depthStoreOp = wgpu::StoreOp::Store;
        }
        dsAttachment.stencilLoadOp = wgpu::LoadOp::Load;
        dsAttachment.stencilStoreOp = wgpu::StoreOp::Store;

        RenderPassDescriptor rpDesc = {};
        rpDesc.depthStencilAttachment = &dsAttachment;

        Ref<RenderPassEncoder> pass = AcquireRef(commandEncoder->APIBeginRenderPass(&rpDesc));
        // Bind the resources.
        pass->APISetBindGroup(0, bindGroup.Get());
        // Discard all fragments outside the copy region.
        pass->APISetScissorRect(dst.origin.x, dst.origin.y, copyExtent.width, copyExtent.height);

        // Clear the copy region to 0.
        pass->APISetStencilReference(0);
        pass->APISetPipeline(pipelines.clearPipeline.Get());
        pass->APIDraw(3, 1, 0, 0);

        // Perform 8 draws. Each will load the source stencil data, and will
        // set the bit index in the destination stencil attachment if it the
        // source also has that bit using stencil operation `Replace`.
        // If it doesn't match, the fragment will be discarded.
        pass->APISetStencilReference(255);
        for (uint32_t bit = 0; bit < 8; ++bit) {
            pass->APISetPipeline(pipelines.setStencilPipelines[bit].Get());
            // Draw one instance, and use the stencil value as firstInstance.
            // This is a cheap way to get the stencil value into the shader
            // since WebGPU doesn't have push constants.
            pass->APIDraw(3, 1, 0, 1u << bit);
        }
        pass->APIEnd();
    }
    return {};
}

}  // anonymous namespace

MaybeError BlitStagingBufferToStencil(DeviceBase* device,
                                      BufferBase* buffer,
                                      const TextureDataLayout& src,
                                      const TextureCopy& dst,
                                      const Extent3D& copyExtent) {
    TextureDescriptor stencilDataTextureDesc = {};
    stencilDataTextureDesc.format = wgpu::TextureFormat::R8Uint;
    stencilDataTextureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    stencilDataTextureDesc.size = copyExtent;

    Ref<TextureBase> stencilDataTexture;
    DAWN_TRY_ASSIGN(stencilDataTexture, device->CreateTexture(&stencilDataTextureDesc));
    {
        TextureCopy r8Dst;
        r8Dst.texture = stencilDataTexture.Get();
        r8Dst.mipLevel = 0;
        r8Dst.origin = {};
        r8Dst.aspect = Aspect::Color;
        DAWN_TRY(device->CopyFromStagingToTexture(buffer, src, r8Dst, copyExtent));
    }

    Ref<CommandEncoderBase> commandEncoder;
    DAWN_TRY_ASSIGN(commandEncoder, device->CreateCommandEncoder());

    DAWN_TRY(
        BlitR8ToStencil(device, commandEncoder.Get(), stencilDataTexture.Get(), dst, copyExtent));

    Ref<CommandBufferBase> commandBuffer;
    DAWN_TRY_ASSIGN(commandBuffer, commandEncoder->Finish());

    CommandBufferBase* commands = commandBuffer.Get();
    device->GetQueue()->APISubmit(1, &commands);
    return {};
}

MaybeError BlitBufferToStencil(DeviceBase* device,
                               CommandEncoder* commandEncoder,
                               BufferBase* buffer,
                               const TextureDataLayout& src,
                               const TextureCopy& dst,
                               const Extent3D& copyExtent) {
    TextureDescriptor stencilDataTextureDesc = {};
    stencilDataTextureDesc.format = wgpu::TextureFormat::R8Uint;
    stencilDataTextureDesc.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
    stencilDataTextureDesc.size = copyExtent;

    Ref<TextureBase> stencilDataTexture;
    DAWN_TRY_ASSIGN(stencilDataTexture, device->CreateTexture(&stencilDataTextureDesc));
    {
        ImageCopyBuffer bufferSrc;
        bufferSrc.buffer = buffer;
        bufferSrc.layout = src;

        ImageCopyTexture textureDst;
        textureDst.texture = stencilDataTexture.Get();
        commandEncoder->APICopyBufferToTexture(&bufferSrc, &textureDst, &copyExtent);
    }

    DAWN_TRY(BlitR8ToStencil(device, commandEncoder, stencilDataTexture.Get(), dst, copyExtent));
    return {};
}

}  // namespace dawn::native
