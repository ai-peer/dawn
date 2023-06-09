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

#include <string_view>
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

constexpr uint32_t kWorkgroupSizeX = 8;
constexpr uint32_t kWorkgroupSizeY = 8;

constexpr std::string_view kBlitDepth32FloatToBufferShaders = R"(
@group(0) @binding(0) var src_tex : texture_depth_2d_array;
@group(0) @binding(1) var<storage, read_write> dst_buf : array<f32>;

struct Params {
    // copyExtent
    srcOrigin: vec3u,
    pad0: u32,
    srcExtent: vec3u,
    pad1: u32,

    // GPUImageDataLayout
    indicesPerRow: u32,
    rowsPerImage: u32,
    indicesOffset: u32,
};

@group(0) @binding(2) var<uniform> params : Params;

override workgroupSizeX: u32;
override workgroupSizeY: u32;

// Load the depth value and write to storage buffer.
@compute @workgroup_size(workgroupSizeX, workgroupSizeY, 1) fn main(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOrigin + params.srcExtent;
    let coord = id + params.srcOrigin;
    if (any(coord >= srcBoundary)) {
        return;
    }

    let dstOffset = params.indicesOffset + id.x + id.y * params.indicesPerRow + id.z * params.indicesPerRow * params.rowsPerImage;
    dst_buf[dstOffset] = textureLoad(src_tex, coord.xy, coord.z, 0);
}

)";

// ShaderF16 extension is only enabled by GL_AMD_gpu_shader_half_float for GL
// so we should not use it generally for the emulation.
// As a result we are using f32 and array<u32> to do all the math and byte manipulation.
// If we have 2-byte scalar type (f16, u16) it can be a bit easier when writing to the storage
// buffer.

constexpr std::string_view kBlitDepth16UnormToBufferShaders = R"(
@group(0) @binding(0) var src_tex : texture_depth_2d_array;
@group(0) @binding(1) var<storage, read_write> dst_buf : array<u32>;

struct Params {
    // copyExtent
    srcOrigin: vec3u,
    pad0: u32,
    srcExtent: vec3u,
    pad1: u32,

    // GPUImageDataLayout
    indicesPerRow: u32,
    rowsPerImage: u32,
    indicesOffset: u32,
};

@group(0) @binding(2) var<uniform> params : Params;

// Range of v is [0.0, 1.0]
// TODO: use pack2x16unorm
fn getUnorm16Bits(v: f32) -> u32 {
    var bits: u32 = u32(v * 65535.0);
    return bits;
}

override workgroupSizeX: u32;
override workgroupSizeY: u32;

// Load the depth value and write to storage buffer.
// Each thread is responsible for reading 2 u16 values and packing them into 1 u32 value.
@compute @workgroup_size(workgroupSizeX, workgroupSizeY, 1) fn main(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOrigin + params.srcExtent;
    let coord0 = vec3u(id.x * 2, id.y, id.z) + params.srcOrigin;

    if (any(coord0 >= srcBoundary)) {
        return;
    }

    let v0: f32 = textureLoad(src_tex, coord0.xy, coord0.z, 0);
    let r0: u32 = getUnorm16Bits(v0);

    let dstOffset = params.indicesOffset + id.x + id.y * params.indicesPerRow + id.z * params.indicesPerRow * params.rowsPerImage;

    var result: u32 = r0;
    let coord1 = coord0 + vec3u(1, 0, 0);
    if (coord1.x < srcBoundary.x) {
        // Make sure coord1 is still within the copy boundary
        // then read and write this value.
        let v1: f32 = textureLoad(src_tex, coord1.xy, coord1.z, 0);
        let r1: u32 = getUnorm16Bits(v1);
        result |= (r1 << 16);
    } else {
        // Otherwise, srcExtent.x is an odd number and this thread is at right edge of the texture
        // To preserve the original buffer content, we need to read from the buffer and pack it
        // together with r0 to avoid it being overwritten.
        // TODO(dawn:1782): profiling against making a separate pass for this edge case
        // as it require reading from dst_buf.
        let original: u32 = dst_buf[dstOffset];
        result |= original & 0xffff0000;
    }

    dst_buf[dstOffset] = result;
}
)";

constexpr std::string_view kBlitStencil8ToBufferShaders = R"(
@group(0) @binding(0) var src_tex : texture_2d_array<u32>;
@group(0) @binding(1) var<storage, read_write> dst_buf : array<u32>;

struct Params {
    // copyExtent
    srcOrigin: vec3u,
    pad0: u32,
    srcExtent: vec3u,
    pad1: u32,

    // GPUImageDataLayout
    indicesPerRow: u32,
    rowsPerImage: u32,
    indicesOffset: u32,
};

@group(0) @binding(2) var<uniform> params : Params;

override workgroupSizeX: u32;
override workgroupSizeY: u32;

// Load the stencil value and write to storage buffer.
// Each thread is responsible for reading 4 u8 values and packing them into 1 u32 value.
@compute @workgroup_size(workgroupSizeX, workgroupSizeY, 1) fn main(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOrigin + params.srcExtent;

    let coord0 = vec3u(id.x * 4, id.y, id.z) + params.srcOrigin;

    if (any(coord0 >= srcBoundary)) {
        return;
    }

    let r0: u32 = 0x000000ff & textureLoad(src_tex, coord0.xy, coord0.z, 0).r;

    let dstOffset = params.indicesOffset + id.x + id.y * params.indicesPerRow + id.z * params.indicesPerRow * params.rowsPerImage;

    var result: u32 = r0;

    let coord4 = coord0 + vec3u(4, 0, 0);
    if (coord4.x <= srcBoundary.x) {
        // All 4 texels for this thread are within texture bounds.
        for (var i = 1u; i < 4u; i = i + 1u) {
            let coordi = coord0 + vec3u(i, 0, 0);
            let ri: u32 = 0x000000ff & textureLoad(src_tex, coordi.xy, coordi.z, 0).r;
            result |= ri << (i * 8u);
        }
    } else {
        // Otherwise, srcExtent.x is not a multiply of 4 and this thread is at right edge of the texture
        // To preserve the original buffer content, we need to read from the buffer and pack it together with other values.
        let original: u32 = dst_buf[dstOffset];
        result += original & 0xffffff00;

        for (var i = 1u; i < 4u; i = i + 1u) {
            let coordi = coord0 + vec3u(i, 0, 0);
            if (coordi.x >= srcBoundary.x) {
                break;
            }
            let ri: u32 = 0x000000ff & textureLoad(src_tex, coordi.xy, coordi.z, 0).r;
            result |= ri << (i * 8u);
        }
    }

    dst_buf[dstOffset] = result;
}
)";

// constexpr char kBlitTextureToBufferShaders[] = R"(
// @group(0) @binding(0) var src_tex : texture_2d_array<f32>;
// @group(0) @binding(1) var<storage, read_write> dst_buf : array<u32>;

// struct Params {
//     // copyExtent
//     srcOrigin: vec3u,
//     pad0: u32,
//     srcExtent: vec3u,
//     pad1: u32,

//     // GPUImageDataLayout
//     indicesPerRow: u32,
//     rowsPerImage: u32,
//     indicesOffset: u32,
// };

// @group(0) @binding(2) var<uniform> params : Params;

// override workgroupSizeX: u32;
// override workgroupSizeY: u32;

// // fn getSnorm8BitTest(v: f32) -> u32 {
// //     var ri = i32(floor( 0.5 + 127.0 * min(1.0, max(-1.0, v)) ));
// //     if (ri < 0) {
// //         ri += 256;
// //     }
// //     return u32(ri);
// // }

// // fn packResult(coord0: vec3u, srcBoundary: vec3u) -> u32 {
// //     var result: u32 = 0u;

// //     let coord4 = coord0 + vec3u(4, 0, 0);
// //     if (coord4.x <= srcBoundary.x) {
// //         // All 4 texels for this thread are within texture bounds.
// //         for (var i = 1u; i < 4u; i += 1u) {
// //             let coordi = coord0 + vec3u(i, 0, 0);
// //             v[i] = textureLoad(src_tex, coordi.xy, coordi.z, 0).r;
// //         }
// //         result = pack4x8snorm(v);
// //     } else {
// //         // Otherwise, srcExtent.x is not a multiply of 4 and this thread is at right edge of
// the texture
// //         // To preserve the original buffer content, we need to read from the buffer and pack
// it together with other values.
// //         let original: u32 = dst_buf[dstOffset];

// //         var mask: u32 = 0x00;

// //         for (var i = 1u; i < 4u; i += 1u) {
// //             let coordi = coord0 + vec3u(i, 0, 0);
// //             if (coordi.x >= srcBoundary.x) {
// //                 break;
// //             }
// //             v[i] = textureLoad(src_tex, coordi.xy, coordi.z, 0).r;
// //             mask |= 0xffu << (i * 2);
// //         }

// //         result = (original & mask) | (pack4x8snorm(v) & ~mask);
// //     }

// //     return result;
// // }

// // Load the stencil value and write to storage buffer.
// // Each thread is responsible for reading 4 u8 values and packing them into 1 u32 value.
// @compute @workgroup_size(workgroupSizeX, workgroupSizeY, 1) fn
// main(@builtin(global_invocation_id) id : vec3u) {
//     let srcBoundary = params.srcOrigin + params.srcExtent;

//     let coord0 = vec3u(id.x * 4, id.y, id.z) + params.srcOrigin;

//     if (any(coord0 >= srcBoundary)) {
//         return;
//     }

//     var v: vec4<f32>;
//     v[0] = textureLoad(src_tex, coord0.xy, coord0.z, 0).r;

//     let dstOffset = params.indicesOffset + id.x + id.y * params.indicesPerRow + id.z *
//     params.indicesPerRow * params.rowsPerImage;

//     var result: u32 = 0;

//     // result = 0x000000ff & getSnorm8BitTest(v[0]);   // test

//     let coord4 = coord0 + vec3u(4, 0, 0);
//     if (coord4.x <= srcBoundary.x) {
//         // All 4 texels for this thread are within texture bounds.
//         for (var i = 1u; i < 4u; i += 1u) {
//             let coordi = coord0 + vec3u(i, 0, 0);
//             v[i] = textureLoad(src_tex, coordi.xy, coordi.z, 0).r;

//             // //test
//             // result |= (0xff & getSnorm8BitTest(v[i])) << (i * 8);
//         }
//         result = pack4x8snorm(v);
//     } else {
//         // Otherwise, srcExtent.x is not a multiply of 4 and this thread is at right edge of the
//         texture
//         // To preserve the original buffer content, we need to read from the buffer and pack it
//         together with other values. let original: u32 = dst_buf[dstOffset];

//         var mask: u32 = 0x00;

//         for (var i = 1u; i < 4u; i += 1u) {
//             let coordi = coord0 + vec3u(i, 0, 0);
//             if (coordi.x >= srcBoundary.x) {
//                 break;
//             }
//             v[i] = textureLoad(src_tex, coordi.xy, coordi.z, 0).r;
//             mask |= 0xffu << (i * 2);
//         }

//         result = (original & mask) | (pack4x8snorm(v) & ~mask);
//     }

//     dst_buf[dstOffset] = result;
// }
// )";

///////////////////////////////////////////////////////////////////////////////////

// constexpr char concat?

template <std::string_view const&... Strs>
struct join {
    // Join all strings into a single std::array of chars
    static constexpr auto impl() noexcept {
        constexpr std::size_t len = (Strs.size() + ... + 0);
        std::array<char, len + 1> a{};
        auto append = [i = 0, &a](auto const& s) mutable {
            for (auto c : s) {
                a[i++] = c;
            }
        };
        (append(Strs), ...);
        a[len] = 0;
        return a;
    }
    // Give the joined string static storage
    static constexpr auto arr = impl();
    // View as a std::string_view
    static constexpr std::string_view value{arr.data(), arr.size() - 1};
};
// Helper to get the value out
template <std::string_view const&... Strs>
static constexpr auto join_v = join<Strs...>::value;

constexpr std::string_view kCommon = R"(
struct Params {
    // copyExtent
    srcOrigin: vec3u,
    packTexelCount: u32,
    srcExtent: vec3u,
    pad1: u32,

    // GPUImageDataLayout
    indicesPerRow: u32,
    rowsPerImage: u32,
    indicesOffset: u32,
};

// TODO: seperate
@group(0) @binding(0) var src_tex : texture_2d_array<f32>;
@group(0) @binding(1) var<storage, read_write> dst_buf : array<u32>;

@group(0) @binding(2) var<uniform> params : Params;

override workgroupSizeX: u32;
override workgroupSizeY: u32;

// Load the snorm value and write to storage buffer.
// Each thread is responsible for reading 1 byte and packing them into a 4-byte u32.
@compute @workgroup_size(workgroupSizeX, workgroupSizeY, 1) fn main
(@builtin(global_invocation_id) id : vec3u) {
    let srcBoundary = params.srcOrigin + params.srcExtent;

    let coord0 = vec3u(id.x * params.packTexelCount, id.y, id.z) + params.srcOrigin;

    if (any(coord0 >= srcBoundary)) {
        return;
    }

    let dstOffset = params.indicesOffset + id.x + id.y * params.indicesPerRow + id.z * params.indicesPerRow * params.rowsPerImage;

    // Result bits to store into dst_buf
    var result: u32 = 0;
)";

constexpr std::string_view kCommonEnd = R"(
    dst_buf[dstOffset] = result;
}
)";

constexpr std::string_view kPack4Into1 = R"(
    // Storing snorm8 texel values
    // later called by pack4x8snorm to convert to u32.
    var v: vec4<f32>;
    v[0] = textureLoad(src_tex, coord0.xy, coord0.z, 0).r;

    let coord4 = coord0 + vec3u(4, 0, 0);
    if (coord4.x <= srcBoundary.x) {
        // All 4 texels for this thread are within texture bounds.
        for (var i = 1u; i < 4u; i += 1u) {
            let coordi = coord0 + vec3u(i, 0, 0);
            v[i] = textureLoad(src_tex, coordi.xy, coordi.z, 0).r;
        }
        result = pack4x8snorm(v);
    } else {
        // Otherwise, srcExtent.x is not a multiply of 4 and this thread is at right edge of the texture
        // To preserve the original buffer content, we need to read from the buffer and pack it together with other values.
        let original: u32 = dst_buf[dstOffset];

        var mask: u32 = 0x00u;

        for (var i = 1u; i < 4u; i += 1u) {
            let coordi = coord0 + vec3u(i, 0, 0);
            if (coordi.x >= srcBoundary.x) {
                break;
            }
            v[i] = textureLoad(src_tex, coordi.xy, coordi.z, 0).r;
            mask |= 0xffu << (i * 2);
        }

        result = (original & mask) | (pack4x8snorm(v) & ~mask);
    }
)";

constexpr std::string_view kPack2Into1 = R"(
    // Storing snorm8 texel values
    // later called by pack4x8snorm to convert to u32.
    var v: vec4<f32>;
    let texel0 = textureLoad(src_tex, coord0.xy, coord0.z, 0).rg;
    v[0] = texel0.r;
    v[1] = texel0.g;

    let coord1 = coord0 + vec3u(1, 0, 0);
    if (coord1.x <= srcBoundary.x) {
        // Make sure coord1 is still within the copy boundary.
        let texel1 = textureLoad(src_tex, coord1.xy, coord1.z, 0).rg;
        v[2] = texel1.r;
        v[3] = texel1.g;
        result = pack4x8snorm(v);
    } else {
        // Otherwise, srcExtent.x is not a multiply of 2 and this thread is at right edge of the texture
        // To preserve the original buffer content, we need to read from the buffer and pack it together with other values.
        let original: u32 = dst_buf[dstOffset];
        let mask = 0xffff0000u;
        result = (original & mask) | (pack4x8snorm(v) & ~mask);
    }
)";

constexpr std::string_view kPack1Into1 = R"(
    // Storing snorm8 texel values
    // later called by pack4x8snorm to convert to u32.
    var v: vec4<f32>;
    let texel0 = textureLoad(src_tex, coord0.xy, coord0.z, 0);
    v[0] = texel0.r;
    v[1] = texel0.g;
    v[2] = texel0.b;
    v[3] = texel0.a;

    result = pack4x8snorm(v);
)";

constexpr std::string_view kBlitR8Unorm = join_v<kCommon, kPack4Into1, kCommonEnd>;
constexpr std::string_view kBlitRG8Unorm = join_v<kCommon, kPack2Into1, kCommonEnd>;
constexpr std::string_view kBlitRGBA8Unorm = join_v<kCommon, kPack1Into1, kCommonEnd>;

ResultOrError<Ref<ComputePipelineBase>> GetOrCreateTextureToBufferPipeline(DeviceBase* device,
                                                                           const Format& format) {
    // TODO: store
    // InternalPipelineStore* store = device->GetInternalPipelineStore();
    // if (store->blitStencil8ToBufferComputePipeline != nullptr) {
    //     return store->blitStencil8ToBufferComputePipeline;
    // }

    ShaderModuleWGSLDescriptor wgslDesc = {};
    ShaderModuleDescriptor shaderModuleDesc = {};
    shaderModuleDesc.nextInChain = &wgslDesc;

    wgpu::TextureSampleType textureSampleType;
    switch (format.format) {
        case wgpu::TextureFormat::R8Snorm:
            wgslDesc.code = kBlitR8Unorm.data();
            textureSampleType = wgpu::TextureSampleType::Float;
            break;
        case wgpu::TextureFormat::RG8Snorm:
            wgslDesc.code = kBlitRG8Unorm.data();
            textureSampleType = wgpu::TextureSampleType::Float;
            break;
        case wgpu::TextureFormat::RGBA8Snorm:
            wgslDesc.code = kBlitRGBA8Unorm.data();
            textureSampleType = wgpu::TextureSampleType::Float;
            break;
        case wgpu::TextureFormat::Depth16Unorm:
            wgslDesc.code = kBlitDepth16UnormToBufferShaders.data();
            textureSampleType = wgpu::TextureSampleType::Depth;
            break;
        case wgpu::TextureFormat::Depth32Float:
            wgslDesc.code = kBlitDepth32FloatToBufferShaders.data();
            textureSampleType = wgpu::TextureSampleType::Depth;
            break;
        case wgpu::TextureFormat::Stencil8:
        case wgpu::TextureFormat::Depth24PlusStencil8:
            wgslDesc.code = kBlitStencil8ToBufferShaders.data();
            textureSampleType = wgpu::TextureSampleType::Uint;
            break;
        default:
            UNREACHABLE();
    }

    Ref<ShaderModuleBase> shaderModule;
    DAWN_TRY_ASSIGN(shaderModule, device->CreateShaderModule(&shaderModuleDesc));

    Ref<BindGroupLayoutBase> bindGroupLayout;
    DAWN_TRY_ASSIGN(bindGroupLayout,
                    utils::MakeBindGroupLayout(
                        device,
                        {
                            {0, wgpu::ShaderStage::Compute, textureSampleType,
                             wgpu::TextureViewDimension::e2DArray},
                            {1, wgpu::ShaderStage::Compute, kInternalStorageBufferBinding},
                            {2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},
                        },
                        /* allowInternalBinding */ true));

    Ref<PipelineLayoutBase> pipelineLayout;
    DAWN_TRY_ASSIGN(pipelineLayout, utils::MakeBasicPipelineLayout(device, bindGroupLayout));

    ComputePipelineDescriptor computePipelineDescriptor = {};
    computePipelineDescriptor.layout = pipelineLayout.Get();
    computePipelineDescriptor.compute.module = shaderModule.Get();
    computePipelineDescriptor.compute.entryPoint = "main";

    constexpr std::array<ConstantEntry, 2> constants = {{
        {nullptr, "workgroupSizeX", kWorkgroupSizeX},
        {nullptr, "workgroupSizeY", kWorkgroupSizeY},
    }};
    computePipelineDescriptor.compute.constantCount = constants.size();
    computePipelineDescriptor.compute.constants = constants.data();

    Ref<ComputePipelineBase> pipeline;
    DAWN_TRY_ASSIGN(pipeline, device->CreateComputePipeline(&computePipelineDescriptor));
    // store->blitStencil8ToBufferComputePipeline = pipeline;
    return pipeline;
}

}  // anonymous namespace

MaybeError BlitTextureToBuffer(DeviceBase* device,
                               CommandEncoder* commandEncoder,
                               const TextureCopy& src,
                               const BufferCopy& dst,
                               const Extent3D& copyExtent) {
    const Format& format = src.texture->GetFormat();

    Ref<ComputePipelineBase> pipeline;
    switch (format.format) {
        case wgpu::TextureFormat::R8Snorm:
        case wgpu::TextureFormat::RG8Snorm:
        case wgpu::TextureFormat::RGBA8Snorm:
        case wgpu::TextureFormat::Depth16Unorm:
        case wgpu::TextureFormat::Depth32Float:
        case wgpu::TextureFormat::Stencil8:
        case wgpu::TextureFormat::Depth24PlusStencil8:
            DAWN_TRY_ASSIGN(pipeline, GetOrCreateTextureToBufferPipeline(device, format));
            break;
        // // TODO: aspect refine format??
        default:
            // Depth32FloatStencil8 is not supported on OpenGL/OpenGLES where
            // we enabled this workaround.
            // Unsupported texture format for the compute blit emulation.
            UNREACHABLE();
    }

    uint32_t texelFormatByteSize = format.GetAspectInfo(src.aspect).block.byteSize;
    // printf("\n\n\n %u \n\n\n", texelFormatByteSize);

    // DAWN_ASSERT(texelFormatByteSize == 1 || texelFormatByteSize == 2 || texelFormatByteSize ==
    // 4);
    uint32_t workgroupCountX = 1;
    uint32_t workgroupCountY = (copyExtent.height + kWorkgroupSizeY - 1) / kWorkgroupSizeY;
    uint32_t workgroupCountZ = copyExtent.depthOrArrayLayers;
    switch (texelFormatByteSize) {
        case 1:
            // One thread is responsible for writing four texel values (x, y) ~ (x+3, y).
            workgroupCountX = (copyExtent.width + 4 * kWorkgroupSizeX - 1) / (4 * kWorkgroupSizeX);
            break;
        case 2:
            // One thread is responsible for writing two texel values (x, y) and (x+1, y).
            workgroupCountX = (copyExtent.width + 2 * kWorkgroupSizeX - 1) / (2 * kWorkgroupSizeX);
            break;
        case 4:
            workgroupCountX = (copyExtent.width + kWorkgroupSizeX - 1) / kWorkgroupSizeX;
            break;
        default:
            UNREACHABLE();
    }

    Ref<BufferBase> destinationBuffer = dst.buffer;
    bool useIntermediateCopyBuffer = false;
    if (texelFormatByteSize < 4 && dst.buffer->GetSize() % 4 != 0 &&
        copyExtent.width % (4 / texelFormatByteSize) != 0) {
        // TODO: comment refine
        useIntermediateCopyBuffer = true;
        BufferDescriptor descriptor = {};
        descriptor.size = Align(dst.buffer->GetSize(), 4);
        // TODO(dawn:1485): adding CopyDst usage to add kInternalStorageBuffer usage internally.
        descriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
        DAWN_TRY_ASSIGN(destinationBuffer, device->CreateBuffer(&descriptor));
    }

    // Allow internal usages since we need to use the source as a texture binding
    // and buffer as a storage binding.
    auto scope = commandEncoder->MakeInternalUsageScope();

    Ref<BindGroupLayoutBase> bindGroupLayout;
    DAWN_TRY_ASSIGN(bindGroupLayout, pipeline->GetBindGroupLayout(0));

    Ref<BufferBase> uniformBuffer;
    {
        BufferDescriptor bufferDesc = {};
        // Uniform buffer size needs to be multiple of 16 bytes
        bufferDesc.size = sizeof(uint32_t) * 12;
        bufferDesc.usage = wgpu::BufferUsage::Uniform;
        bufferDesc.mappedAtCreation = true;
        DAWN_TRY_ASSIGN(uniformBuffer, device->CreateBuffer(&bufferDesc));

        uint32_t* params =
            static_cast<uint32_t*>(uniformBuffer->GetMappedRange(0, bufferDesc.size));
        // srcOrigin: vec3u
        params[0] = src.origin.x;
        params[1] = src.origin.y;
        // src.origin.z is set at textureView.baseArrayLayer
        params[2] = 0;
        // packTexelCount: number of texel values (1, 2, or 4) one thread packs into the dst buffer
        params[3] = 4 / texelFormatByteSize;
        // srcExtent: vec3u
        params[4] = copyExtent.width;
        params[5] = copyExtent.height;
        params[6] = copyExtent.depthOrArrayLayers;

        // Turn bytesPerRow, (bytes)offset to use array index as unit
        // We pack values into array<u32> or array<f32>
        params[8] = dst.bytesPerRow / 4;
        params[9] = dst.rowsPerImage;
        params[10] = dst.offset / 4;

        DAWN_TRY(uniformBuffer->Unmap());
    }

    TextureViewDescriptor viewDesc = {};
    // viewDesc.aspect = src.aspect;
    switch (src.aspect) {
        case Aspect::Color:
            viewDesc.aspect = wgpu::TextureAspect::All;
            break;
        case Aspect::Depth:
            viewDesc.aspect = wgpu::TextureAspect::DepthOnly;
            break;
        case Aspect::Stencil:
            viewDesc.aspect = wgpu::TextureAspect::StencilOnly;
            break;
        default:
            UNREACHABLE();
    }
    viewDesc.dimension = wgpu::TextureViewDimension::e2DArray;
    viewDesc.baseMipLevel = src.mipLevel;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = src.origin.z;
    viewDesc.arrayLayerCount = copyExtent.depthOrArrayLayers;

    Ref<TextureViewBase> srcView;
    DAWN_TRY_ASSIGN(srcView, src.texture->CreateView(&viewDesc));

    Ref<BindGroupBase> bindGroup;
    DAWN_TRY_ASSIGN(bindGroup, utils::MakeBindGroup(device, bindGroupLayout,
                                                    {
                                                        {0, srcView},
                                                        {1, destinationBuffer},
                                                        {2, uniformBuffer},
                                                    },
                                                    UsageValidationMode::Internal));

    Ref<ComputePassEncoder> pass = commandEncoder->BeginComputePass();
    pass->APISetPipeline(pipeline.Get());
    pass->APISetBindGroup(0, bindGroup.Get());
    pass->APIDispatchWorkgroups(workgroupCountX, workgroupCountY, workgroupCountZ);

    pass->APIEnd();

    if (useIntermediateCopyBuffer) {
        ASSERT(destinationBuffer->GetSize() <= dst.buffer->GetAllocatedSize());
        commandEncoder->InternalCopyBufferToBufferWithAllocatedSize(
            destinationBuffer.Get(), 0, dst.buffer.Get(), 0, destinationBuffer->GetSize());
    }

    return {};
}

}  // namespace dawn::native
