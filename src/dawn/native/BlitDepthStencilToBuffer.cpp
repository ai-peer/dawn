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
@group(0) @binding(1) var<storage, read_write> dst_buf : array<f32>;

struct Params {
    // copyExtent
    srcOrigin: vec2u,
    srcExtent: vec2u,

    // GPUImageDataLayout
    indicesPerRow: u32,
    rowsPerImage: u32,
    offset: u32,
};

// TODO: this might need to be a searpate group
@group(0) @binding(2) var<uniform> params : Params;

// Load the depth value and write to storage buffer.
// @compute @workgroup_size(64, 1, 1) fn blit_depth_to_buffer(@builtin(global_invocation_id) id : vec3u) {
@compute @workgroup_size(8, 8, 1) fn blit_depth_to_buffer(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOrigin + params.srcExtent;
    let coord = id.xy + params.srcOrigin;
    if (coord.x >= srcBoundary.x || coord.y >= srcBoundary.y) {
        return;
    }

    let dstOffset = params.offset + id.x + id.y * params.indicesPerRow;
    dst_buf[dstOffset] = textureLoad(src_tex, coord, 0);
}

// TODO: other format?

)";

// ShaderF16 is only enabled by GL_AMD_gpu_shader_half_float, should not used generally

// Read 2 texels at a time, write them to 4 byte per element array
// TODO: might need padding at the end for BufferGL

constexpr char kBlitDepth16UnormToBufferShaders[] = R"(
@group(0) @binding(0) var src_tex : texture_depth_2d;
@group(0) @binding(1) var<storage, read_write> dst_buf : array<u32>;

struct Params {
    // copyExtent
    srcOrigin: vec2u,
    srcExtent: vec2u,

    // GPUImageDataLayout
    indicesPerRow: u32,
    rowsPerImage: u32,
    offset: u32,
};

// TODO: this might need to be a searpate group
@group(0) @binding(2) var<uniform> params : Params;

// v is a subnormal value, [0, 1]
fn getUnorm16Bits(v: f32) -> u32 {
    var bits: u32 = u32(v * 65535.0);
    return bits;
}

// Load the depth value and write to storage buffer.
// @compute @workgroup_size(64, 1, 1) fn blit_depth_to_buffer(@builtin(global_invocation_id) id : vec3u) {
@compute @workgroup_size(8, 8, 1) fn blit_depth_to_buffer(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOrigin + params.srcExtent;
    let coord0 = vec2u(id.x * 2, id.y) + params.srcOrigin;

    // TODO: edge condition (odd number width)
    if (coord0.x >= srcBoundary.x - 1 || coord0.y >= srcBoundary.y) {
        return;
    }
    let coord1 = coord0 + vec2u(1, 0);

    var v0: f32 = textureLoad(src_tex, coord0, 0);
    var v1: f32 = textureLoad(src_tex, coord1, 0);

    var r0: u32 = getUnorm16Bits(v0);
    var r1: u32 = getUnorm16Bits(v1);

    var result: u32 = (r0 << 16) + r1;

    let dstOffset = params.offset + id.x + id.y * params.indicesPerRow;
    dst_buf[dstOffset] = result;
    // dst_buf[0] = result;
}
)";

ResultOrError<Ref<ComputePipelineBase>> CreateDepthBlitComputePipeline(DeviceBase* device,
                                                                       const char* shaderSource) {
    ShaderModuleWGSLDescriptor wgslDesc = {};
    ShaderModuleDescriptor shaderModuleDesc = {};
    shaderModuleDesc.nextInChain = &wgslDesc;
    wgslDesc.source = shaderSource;

    Ref<ShaderModuleBase> shaderModule;
    DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&shaderModuleDesc));

    Ref<BindGroupLayoutBase> bindGroupLayout;
    DAWN_TRY_ASSIGN(bindGroupLayout,
                    utils::MakeBindGroupLayout(
                        device,
                        {
                            {0, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::Depth,
                             wgpu::TextureViewDimension::e2D},
                            {1, wgpu::ShaderStage::Compute, kInternalStorageBufferBinding},

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

ResultOrError<Ref<ComputePipelineBase>> GetOrCreateDepth32FloatToBufferPipeline(
    DeviceBase* device) {
    // InternalPipelineStore* store = device->GetInternalPipelineStore();
    // if (store->blitRG8ToDepth16UnormPipeline != nullptr) {
    //     return store->blitRG8ToDepth16UnormPipeline;
    // }

    Ref<ComputePipelineBase> pipeline;
    DAWN_TRY_ASSIGN(pipeline,
                    CreateDepthBlitComputePipeline(device, kBlitDepth32FloatToBufferShaders));

    // store->blitRG8ToDepth16UnormPipeline = pipeline;
    return pipeline;
}

ResultOrError<Ref<ComputePipelineBase>> GetOrCreateDepth16UnormToBufferPipeline(
    DeviceBase* device) {
    // cached pipeline

    Ref<ComputePipelineBase> pipeline;
    DAWN_TRY_ASSIGN(pipeline,
                    CreateDepthBlitComputePipeline(device, kBlitDepth16UnormToBufferShaders));
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
    const Format& format = src.texture->GetFormat();
    // const TexelBlockInfo& blockInfo =
    //         format.GetAspectInfo(src.aspect).block;
    // ASSERT(format.format == wgpu::TextureFormat::Depth16Unorm);

    Ref<ComputePipelineBase> pipeline;
    uint32_t workgroupCountX = 1;
    uint32_t workgroupCountY = 1;
    switch (format.format) {
        case wgpu::TextureFormat::Depth16Unorm:
            // TODO: odd edge case
            // One thread is writing two texel values (x, y) and (x+1, y).
            workgroupCountX = (copyExtent.width + 15) / 16;
            workgroupCountY = (copyExtent.width + 7) / 8;
            DAWN_TRY_ASSIGN(pipeline, GetOrCreateDepth16UnormToBufferPipeline(device));
            break;
        case wgpu::TextureFormat::Depth32Float:
            workgroupCountX = (copyExtent.width + 7) / 8;
            workgroupCountY = (copyExtent.width + 7) / 8;
            DAWN_TRY_ASSIGN(pipeline, GetOrCreateDepth32FloatToBufferPipeline(device));
            break;
        default:
            // Other format (Depth32FloatStencil8) is not supported
            UNREACHABLE();
    }

    // Allow internal usages since we need to use the source as a texture binding, and
    // the destination as a render attachment.
    auto scope = commandEncoder->MakeInternalUsageScope();

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
        // srcOrigin: vec2u
        params[0] = src.origin.x;
        params[1] = src.origin.y;
        // srcExtent: vec2u
        params[2] = copyExtent.width;
        params[3] = copyExtent.height;
        // BufferCopy
        // params[4] = dst.bytesPerRow;
        // params[4] = dst.bytesPerRow / blockInfo.byteSize;
        params[4] = dst.bytesPerRow / 4;  // depth16unorm 2 per thread 2*2byte, depth32float 4byte
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
    pass->APIDispatchWorkgroups(workgroupCountX, workgroupCountY, 1);
    // pass->APIDispatchWorkgroups(copyExtent.width * copyExtent.height);
    // pass->End();
    pass->APIEnd();

    return {};
}

}  // namespace dawn::native
