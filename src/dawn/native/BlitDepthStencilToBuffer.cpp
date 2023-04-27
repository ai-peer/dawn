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

#include "dawn/native/BlitDepthStencilToBuffer.h"

#include <utility>

#include "dawn/common/Assert.h"
#include "dawn/native/BindGroup.h"
#include "dawn/native/CommandBuffer.h"
#include "dawn/native/CommandEncoder.h"
#include "dawn/native/ComputePassEncoder.h"
#include "dawn/native/ComputePipeline.h"
#include "dawn/native/Device.h"
#include "dawn/native/InternalPipelineStore.h"
#include "dawn/native/Queue.h"
#include "dawn/native/utils/WGPUHelpers.h"

namespace dawn::native {

namespace {

constexpr char kBlitDepth32FloatToBufferShaders[] = R"(
@group(0) @binding(0) var src_tex : texture_depth_2d;
// @group(0) @binding(0) var src_tex : texture_2d<f32>;
@group(0) @binding(1) var<storage, read_write> dst_buf : array<f32>;

struct TextureDataLayout {
    // copyExtent
    srcOffset: vec2u,
    srcExtent: vec2u,

    // GPUImageDataLayout
    bytesPerRow: u32,
    rowsPerImage: u32,
    offset: u32,
};

// TODO: this might need to be a searpate group
@group(0) @binding(2) var<uniform> params : TextureDataLayout;

// Load the depth value and write to storage buffer.
// @compute @workgroup_size(64, 1, 1) fn blit_depth_to_buffer(@builtin(global_invocation_id) id : vec3u) {
@compute @workgroup_size(8, 8, 1) fn blit_depth_to_buffer(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOffset + params.srcExtent;
    let coord = id.xy + params.srcOffset;
    if (coord.x >= srcBoundary.x || coord.y >= srcBoundary.y) {
        return;
    }

    let dstOffset = params.offset + id.x + id.y * params.bytesPerRow;
    dst_buf[dstOffset] = textureLoad(src_tex, coord, 0);
}

// TODO: other format?

)";

ResultOrError<Ref<ComputePipelineBase>> GetOrCreateDepth32FloatToBufferPipeline(
    DeviceBase* device) {
    // InternalPipelineStore* store = device->GetInternalPipelineStore();
    // if (store->blitRG8ToDepth16UnormPipeline != nullptr) {
    //     return store->blitRG8ToDepth16UnormPipeline;
    // }

    ShaderModuleWGSLDescriptor wgslDesc = {};
    ShaderModuleDescriptor shaderModuleDesc = {};
    shaderModuleDesc.nextInChain = &wgslDesc;
    wgslDesc.source = kBlitDepth32FloatToBufferShaders;

    Ref<ShaderModuleBase> shaderModule;
    DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&shaderModuleDesc));

    Ref<BindGroupLayoutBase> bindGroupLayout;
    DAWN_TRY_ASSIGN(
        bindGroupLayout,
        utils::MakeBindGroupLayout(
            device,
            {
                // {0, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::UnfilterableFloat,
                // wgpu::TextureViewDimension::e2D},
                {0, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::Depth,
                 wgpu::TextureViewDimension::e2D},
                {1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},
                // {1, wgpu::ShaderStage::Compute, kInternalStorageBufferBinding},

                // uniform
                {2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},
            },
            /* allowInternalBinding */ true));

    Ref<PipelineLayoutBase> pipelineLayout;
    DAWN_TRY_ASSIGN(pipelineLayout, utils::MakeBasicPipelineLayout(device, bindGroupLayout));

    ComputePipelineDescriptor computePipelineDescriptor = {};
    computePipelineDescriptor.layout = pipelineLayout.Get();
    computePipelineDescriptor.compute.module = shaderModule.Get();
    computePipelineDescriptor.compute.entryPoint = "blit_depth_to_buffer";

    Ref<ComputePipelineBase> pipeline;
    DAWN_TRY_ASSIGN(pipeline, device->CreateComputePipeline(&computePipelineDescriptor));

    // store->blitRG8ToDepth16UnormPipeline = pipeline;
    return pipeline;
}

}  // anonymous namespace

MaybeError BlitDepthToBuffer(DeviceBase* device,
                             CommandEncoder* commandEncoder,
                             const TextureCopy& src,
                             const BufferCopy& dst,
                             //  BufferBase* buffer,
                             //  const TextureDataLayout& dst,
                             const Extent3D& copyExtent) {
    // const Format& format = src.texture->GetFormat();
    // ASSERT(format.format == wgpu::TextureFormat::Depth16Unorm);

    // Allow internal usages since we need to use the source as a texture binding, and
    // the destination as a render attachment.
    auto scope = commandEncoder->MakeInternalUsageScope();

    Ref<ComputePipelineBase> pipeline;
    DAWN_TRY_ASSIGN(pipeline, GetOrCreateDepth32FloatToBufferPipeline(device));

    Ref<BindGroupLayoutBase> bindGroupLayout;
    DAWN_TRY_ASSIGN(bindGroupLayout, pipeline->GetBindGroupLayout(0));

    Ref<TextureViewBase> srcView;
    {
        TextureViewDescriptor viewDesc = {};
        viewDesc.aspect = wgpu::TextureAspect::DepthOnly;
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        viewDesc.baseMipLevel = src.mipLevel;
        viewDesc.mipLevelCount = 1u;

        DAWN_TRY_ASSIGN(srcView, src.texture->CreateView(&viewDesc));
    }

    // TODO: reuse uniform buffer
    Ref<BufferBase> uniformBuffer;
    {
        BufferDescriptor bufferDesc = {};
        // bufferDesc.size = sizeof(uint32_t) * 7;
        bufferDesc.size = sizeof(uint32_t) * 8;  // multiple of 16 bytes
        bufferDesc.usage = wgpu::BufferUsage::Uniform;
        bufferDesc.mappedAtCreation = true;
        DAWN_TRY_ASSIGN(uniformBuffer, device->CreateBuffer(&bufferDesc));

        uint32_t* params =
            static_cast<uint32_t*>(uniformBuffer->GetMappedRange(0, bufferDesc.size));
        // srcOffset: vec2u
        params[0] = src.origin.x;
        params[1] = src.origin.y;
        // srcExtent: vec2u
        params[2] = copyExtent.width;
        params[3] = copyExtent.height;
        // BufferCopy
        params[4] = dst.bytesPerRow;
        params[5] = dst.rowsPerImage;
        params[6] = dst.offset;

        DAWN_TRY(uniformBuffer->Unmap());
    }

    Ref<BindGroupBase> bindGroup;
    DAWN_TRY_ASSIGN(bindGroup,
                    utils::MakeBindGroup(device, bindGroupLayout,
                                         {
                                             {0, srcView},
                                             {1, dst.buffer},
                                             // {1, buffer},

                                             // TODO: copy texture layout, extent uniform buffer
                                             {2, uniformBuffer},
                                         },
                                         UsageValidationMode::Internal));

    // TODO: depthOrArrayLayers?

    Ref<ComputePassEncoder> pass = commandEncoder->BeginComputePass();
    pass->APISetPipeline(pipeline.Get());
    pass->APISetBindGroup(0, bindGroup.Get());
    // dispatch workgroup size depends on the texture copyExtent
    // 1D?
    // pass->APIDispatchWorkgroups(copyExtent.width, copyExtent.height, 1);
    pass->APIDispatchWorkgroups(copyExtent.width * copyExtent.height);
    pass->APIEnd();

    return {};
}

}  // namespace dawn::native
