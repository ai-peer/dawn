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
    indicesOffset: u32,
};

@group(0) @binding(2) var<uniform> params : Params;

// Load the depth value and write to storage buffer.
@compute @workgroup_size(8, 8, 1) fn blit_depth_to_buffer(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOrigin + params.srcExtent;
    let coord = id.xy + params.srcOrigin;
    if (coord.x >= srcBoundary.x || coord.y >= srcBoundary.y) {
        return;
    }

    let dstOffset = params.indicesOffset + id.x + id.y * params.indicesPerRow;
    dst_buf[dstOffset] = textureLoad(src_tex, coord, 0);
}

)";

// ShaderF16 extension is only enabled by GL_AMD_gpu_shader_half_float for GL
// should not use it generally for the emulation.

// Read 2 texels at a time, write them to 4 byte per element array
// might need padding at the end for BufferGL?

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
    indicesOffset: u32,
};

@group(0) @binding(2) var<uniform> params : Params;

// v is a subnormal value, i.e. range is [0.0, 1.0]
fn getUnorm16Bits(v: f32) -> u32 {
    var bits: u32 = u32(v * 65535.0);
    return bits;
}

// Load the depth value and write to storage buffer.
// Each thread is responsible for reading 2 u16 values and pack them into 1 u32 value.
@compute @workgroup_size(8, 8, 1) fn blit_depth_to_buffer(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOrigin + params.srcExtent;
    let coord0 = vec2u(id.x * 2, id.y) + params.srcOrigin;

    if (coord0.x >= srcBoundary.x || coord0.y >= srcBoundary.y) {
        return;
    }

    let v0: f32 = textureLoad(src_tex, coord0, 0);
    let r0: u32 = getUnorm16Bits(v0);

    var result: u32 = r0;
    if (coord0.x + 1 < srcBoundary.x) {
        // Make sure coord0.x + 1 is still within the copy boundary
        // then read and write this value.
        // Otherwise, srcExtent.x is an odd number and this thread is at right edge of the texture
        // Skip reading and packing the other value.

        let coord1 = coord0 + vec2u(1, 0);
        let v1: f32 = textureLoad(src_tex, coord1, 0);
        let r1: u32 = getUnorm16Bits(v1);
        result = result + (r1 << 16);
    }

    let dstOffset = params.indicesOffset + id.x + id.y * params.indicesPerRow;
    dst_buf[dstOffset] = result;
}
)";

ResultOrError<Ref<ComputePipelineBase>> CreateDepthBlitComputePipeline(DeviceBase* device,
                                                                       InternalPipelineStore* store,
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

    if (shaderSource == kBlitDepth16UnormToBufferShaders) {
        store->blitDepth16UnormToBufferComputePipeline = pipeline;
    } else {
        ASSERT(shaderSource == kBlitDepth32FloatToBufferShaders);
        store->blitDepth32FloatToBufferComputePipeline = pipeline;
    }
    return pipeline;
}

ResultOrError<Ref<ComputePipelineBase>> GetOrCreateDepth32FloatToBufferPipeline(
    DeviceBase* device) {
    InternalPipelineStore* store = device->GetInternalPipelineStore();
    if (store->blitDepth32FloatToBufferComputePipeline != nullptr) {
        return store->blitDepth32FloatToBufferComputePipeline;
    }

    Ref<ComputePipelineBase> pipeline;
    DAWN_TRY_ASSIGN(
        pipeline, CreateDepthBlitComputePipeline(device, store, kBlitDepth32FloatToBufferShaders));

    return pipeline;
}

ResultOrError<Ref<ComputePipelineBase>> GetOrCreateDepth16UnormToBufferPipeline(
    DeviceBase* device) {
    InternalPipelineStore* store = device->GetInternalPipelineStore();
    if (store->blitDepth16UnormToBufferComputePipeline != nullptr) {
        return store->blitDepth16UnormToBufferComputePipeline;
    }

    Ref<ComputePipelineBase> pipeline;
    DAWN_TRY_ASSIGN(
        pipeline, CreateDepthBlitComputePipeline(device, store, kBlitDepth16UnormToBufferShaders));
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

    Ref<ComputePipelineBase> pipeline;
    uint32_t workgroupCountX = 1;
    uint32_t workgroupCountY = 1;
    switch (format.format) {
        case wgpu::TextureFormat::Depth16Unorm:
            // One thread is responsible for writing two texel values (x, y) and (x+1, y).
            // Workgroup size is 8 so timing 2 is 16
            workgroupCountX = (copyExtent.width + 15) / 16;
            workgroupCountY = (copyExtent.height + 7) / 8;
            DAWN_TRY_ASSIGN(pipeline, GetOrCreateDepth16UnormToBufferPipeline(device));
            break;
        case wgpu::TextureFormat::Depth32Float:
            workgroupCountX = (copyExtent.width + 7) / 8;
            workgroupCountY = (copyExtent.height + 7) / 8;
            DAWN_TRY_ASSIGN(pipeline, GetOrCreateDepth32FloatToBufferPipeline(device));
            break;
        default:
            // Other format (Depth32FloatStencil8) is not supported
            UNREACHABLE();
    }

    // Allow internal usages since we need to use the source as a texture binding
    // and buffer as a storage binding.
    auto scope = commandEncoder->MakeInternalUsageScope();

    Ref<BindGroupLayoutBase> bindGroupLayout;
    DAWN_TRY_ASSIGN(bindGroupLayout, pipeline->GetBindGroupLayout(0));

    Ref<TextureViewBase> srcView;
    {
        TextureViewDescriptor viewDesc = {};
        viewDesc.aspect = wgpu::TextureAspect::DepthOnly;
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        viewDesc.baseMipLevel = src.mipLevel;
        viewDesc.mipLevelCount = 1;

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

        // Turn bytesPerRow, (bytes)offset to use array index as unit
        // We use array<u32> for depth16unorm copy and array<f32> for depth32float copy
        // Both array element is 4 byte.
        params[4] = dst.bytesPerRow / 4;
        params[5] = dst.rowsPerImage;
        params[6] = dst.offset / 4;

        DAWN_TRY(uniformBuffer->Unmap());
    }

    Ref<BindGroupBase> bindGroup;
    DAWN_TRY_ASSIGN(bindGroup, utils::MakeBindGroup(device, bindGroupLayout,
                                                    {
                                                        {0, srcView},
                                                        {1, dst.buffer},
                                                        {2, uniformBuffer},
                                                    },
                                                    UsageValidationMode::Internal));

    Ref<ComputePassEncoder> pass = commandEncoder->BeginComputePass();
    pass->APISetPipeline(pipeline.Get());
    pass->APISetBindGroup(0, bindGroup.Get());
    pass->APIDispatchWorkgroups(workgroupCountX, workgroupCountY, 1);
    pass->APIEnd();

    return {};
}

}  // namespace dawn::native
