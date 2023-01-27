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

#include "dawn/native/BlitDepthToDepth.h"

#include "dawn/common/Assert.h"
#include "dawn/native/BindGroup.h"
#include "dawn/native/CommandEncoder.h"
#include "dawn/native/Device.h"
#include "dawn/native/InternalPipelineStore.h"
#include "dawn/native/RenderPassEncoder.h"
#include "dawn/native/RenderPipeline.h"

namespace dawn::native {

namespace {

constexpr char kBlitToDepthShaders[] = R"(

struct VertexOutputs {
  @location(0) @interpolate(flat) src_z : u32,
  @builtin(position) position : vec4<f32>,
};

// The instance_index here is not used for instancing.
// It represents the current z slice we're copying from in the
// source.
// This is a cheap way to get the z value into the shader
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
    vec4<f32>(pos[vertex_index], 0.0, 1.0)
  );
}

@group(0) @binding(0) var src_tex : texture_depth_2d_array;
@group(0) @binding(1) var<uniform> mipLevel : u32;

// Load the depth value and return it as the frag_depth.
@fragment fn blit_to_depth(input : VertexOutputs) -> @builtin(frag_depth) f32 {
  return textureLoad(
    src_tex, vec2<u32>(input.position.xy), input.src_z, mipLevel);
}

)";

ResultOrError<Ref<RenderPipelineBase>> GetOrCreateDepthBlitPipeline(DeviceBase* device,
                                                                    wgpu::TextureFormat format) {
    InternalPipelineStore* store = device->GetInternalPipelineStore();
    {
        auto it = store->depthBlitPipelines.find(format);
        if (it != store->depthBlitPipelines.end()) {
            return it->second;
        }
    }

    ShaderModuleWGSLDescriptor wgslDesc = {};
    ShaderModuleDescriptor shaderModuleDesc = {};
    shaderModuleDesc.nextInChain = &wgslDesc;
    wgslDesc.source = kBlitToDepthShaders;

    Ref<ShaderModuleBase> shaderModule;
    DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&shaderModuleDesc));

    FragmentState fragmentState = {};
    fragmentState.module = shaderModule.Get();
    fragmentState.entryPoint = "blit_to_depth";

    DepthStencilState dsState = {};
    dsState.format = format;
    dsState.depthWriteEnabled = true;

    RenderPipelineDescriptor renderPipelineDesc = {};
    renderPipelineDesc.vertex.module = shaderModule.Get();
    renderPipelineDesc.vertex.entryPoint = "vert_fullscreen_quad";
    renderPipelineDesc.depthStencil = &dsState;
    renderPipelineDesc.fragment = &fragmentState;

    Ref<RenderPipelineBase> pipeline;
    DAWN_TRY_ASSIGN(pipeline, device->CreateRenderPipeline(&renderPipelineDesc));

    store->depthBlitPipelines[format] = pipeline;
    return pipeline;
}

}  // namespace

MaybeError BlitDepthToDepth(DeviceBase* device,
                            CommandEncoder* commandEncoder,
                            const TextureCopy& src,
                            const TextureCopy& dst,
                            const Extent3D& copyExtent) {
    ASSERT(src.texture->GetFormat().HasDepth());
    ASSERT(dst.texture->GetFormat().HasDepth());

    // Allow internal usages since we need to use the source as a texture binding, and
    // the destination as a render attachment.
    auto scope = commandEncoder->MakeInternalUsageScope();

    // Create the source view. This is a view of the whole texture.
    // and textureLoad with an explicit array_index and level will be used
    // in the shader. This is to avoid driver issues where texture view subresource
    // subsetting does not work.
    Ref<TextureViewBase> srcView;
    {
        TextureViewDescriptor viewDesc = {};
        viewDesc.aspect = wgpu::TextureAspect::DepthOnly;
        viewDesc.dimension = wgpu::TextureViewDimension::e2DArray;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipLevelCount = src.texture->GetNumMipLevels();
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = src.texture->GetArrayLayers();
        DAWN_TRY_ASSIGN(srcView, src.texture->CreateView(&viewDesc));
    }

    // Upload the source mip level to a uniform buffer.
    Ref<BufferBase> mipLevelBuffer;
    {
        BufferDescriptor bufferDesc = {};
        bufferDesc.size = sizeof(uint32_t);
        bufferDesc.usage = wgpu::BufferUsage::Uniform;
        bufferDesc.mappedAtCreation = true;
        DAWN_TRY_ASSIGN(mipLevelBuffer, device->CreateBuffer(&bufferDesc));

        uint32_t* mipLevel =
            static_cast<uint32_t*>(mipLevelBuffer->GetMappedRange(0, bufferDesc.size));
        *mipLevel = src.mipLevel;
        mipLevelBuffer->Unmap();
    }

    Ref<RenderPipelineBase> pipeline;
    DAWN_TRY_ASSIGN(pipeline,
                    GetOrCreateDepthBlitPipeline(device, dst.texture->GetFormat().format));

    Ref<BindGroupLayoutBase> bgl;
    DAWN_TRY_ASSIGN(bgl, pipeline->GetBindGroupLayout(0));

    Ref<BindGroupBase> bindGroup;
    {
        std::array<BindGroupEntry, 2> bgEntries = {};
        bgEntries[0].binding = 0;
        bgEntries[0].textureView = srcView.Get();
        bgEntries[1].binding = 1;
        bgEntries[1].buffer = mipLevelBuffer.Get();

        BindGroupDescriptor bgDesc = {};
        bgDesc.layout = bgl.Get();
        bgDesc.entryCount = bgEntries.size();
        bgDesc.entries = bgEntries.data();
        DAWN_TRY_ASSIGN(bindGroup, device->CreateBindGroup(&bgDesc, UsageValidationMode::Internal));
    }

    for (uint32_t z = 0; z < copyExtent.depthOrArrayLayers; ++z) {
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

        RenderPassDepthStencilAttachment dsAttachment;
        dsAttachment.view = dstView.Get();
        dsAttachment.depthLoadOp = wgpu::LoadOp::Load;
        dsAttachment.depthStoreOp = wgpu::StoreOp::Store;
        if (dst.texture->GetFormat().HasStencil()) {
            dsAttachment.stencilLoadOp = wgpu::LoadOp::Load;
            dsAttachment.stencilStoreOp = wgpu::StoreOp::Store;
        }

        RenderPassDescriptor rpDesc = {};
        rpDesc.depthStencilAttachment = &dsAttachment;

        // Draw to perform the blit.
        Ref<RenderPassEncoder> pass = AcquireRef(commandEncoder->APIBeginRenderPass(&rpDesc));
        pass->APISetBindGroup(0, bindGroup.Get());
        pass->APISetPipeline(pipeline.Get());
        // Draw one instance, and use the source z slice as firstInstance.
        // This is a cheap way to get the z value into the shader
        // since WebGPU doesn't have push constants.
        pass->APIDraw(3, 1, 0, src.origin.z + z);
        pass->APIEnd();
    }

    return {};
}

}  // namespace dawn::native
