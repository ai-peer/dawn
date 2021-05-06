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

#include "utils/TestUtils.h"

#include "common/Assert.h"
#include "common/Constants.h"
#include "common/Math.h"
#include "utils/TextureFormatUtils.h"
#include "utils/WGPUHelpers.h"

#include <sstream>
#include <vector>

namespace utils {

    uint32_t GetMinimumBytesPerRow(wgpu::TextureFormat format, uint32_t width) {
        const uint32_t bytesPerBlock = utils::GetTexelBlockSizeInBytes(format);
        return Align(bytesPerBlock * width, kTextureBytesPerRowAlignment);
    }

    TextureDataCopyLayout GetTextureDataCopyLayoutForTexture2DAtLevel(
        wgpu::TextureFormat format,
        wgpu::Extent3D textureSizeAtLevel0,
        uint32_t mipmapLevel,
        uint32_t rowsPerImage) {
        // TODO(jiawei.shao@intel.com): support compressed texture formats
        ASSERT(utils::GetTextureFormatBlockWidth(format) == 1);

        TextureDataCopyLayout layout;

        layout.mipSize = {std::max(textureSizeAtLevel0.width >> mipmapLevel, 1u),
                          std::max(textureSizeAtLevel0.height >> mipmapLevel, 1u),
                          textureSizeAtLevel0.depthOrArrayLayers};

        layout.bytesPerRow = GetMinimumBytesPerRow(format, layout.mipSize.width);

        if (rowsPerImage == wgpu::kCopyStrideUndefined) {
            rowsPerImage = layout.mipSize.height;
        }
        layout.rowsPerImage = rowsPerImage;

        layout.bytesPerImage = layout.bytesPerRow * rowsPerImage;

        // TODO(kainino@chromium.org): Remove this intermediate variable.
        // It is currently needed because of an issue in the D3D12 copy splitter
        // (or maybe in D3D12 itself?) which requires there to be enough room in the
        // buffer for the last image to have a height of `rowsPerImage` instead of
        // the actual height.
        wgpu::Extent3D mipSizeWithHeightWorkaround = layout.mipSize;
        mipSizeWithHeightWorkaround.height =
            rowsPerImage * utils::GetTextureFormatBlockHeight(format);

        layout.byteLength = RequiredBytesInCopy(layout.bytesPerRow, rowsPerImage,
                                                mipSizeWithHeightWorkaround, format);

        const uint32_t bytesPerTexel = utils::GetTexelBlockSizeInBytes(format);
        layout.texelBlocksPerRow = layout.bytesPerRow / bytesPerTexel;
        layout.texelBlocksPerImage = layout.bytesPerImage / bytesPerTexel;
        layout.texelBlockCount = layout.byteLength / bytesPerTexel;

        return layout;
    }

    uint64_t RequiredBytesInCopy(uint64_t bytesPerRow,
                                 uint64_t rowsPerImage,
                                 wgpu::Extent3D copyExtent,
                                 wgpu::TextureFormat textureFormat) {
        uint32_t blockSize = utils::GetTexelBlockSizeInBytes(textureFormat);
        uint32_t blockWidth = utils::GetTextureFormatBlockWidth(textureFormat);
        uint32_t blockHeight = utils::GetTextureFormatBlockHeight(textureFormat);
        ASSERT(copyExtent.width % blockWidth == 0);
        uint32_t widthInBlocks = copyExtent.width / blockWidth;
        ASSERT(copyExtent.height % blockHeight == 0);
        uint32_t heightInBlocks = copyExtent.height / blockHeight;
        return RequiredBytesInCopy(bytesPerRow, rowsPerImage, widthInBlocks, heightInBlocks,
                                   copyExtent.depthOrArrayLayers, blockSize);
    }

    uint64_t RequiredBytesInCopy(uint64_t bytesPerRow,
                                 uint64_t rowsPerImage,
                                 uint64_t widthInBlocks,
                                 uint64_t heightInBlocks,
                                 uint64_t depth,
                                 uint64_t bytesPerBlock) {
        if (depth == 0) {
            return 0;
        }

        uint64_t bytesPerImage = bytesPerRow * rowsPerImage;
        uint64_t requiredBytesInCopy = bytesPerImage * (depth - 1);
        if (heightInBlocks != 0) {
            uint64_t lastRowBytes = widthInBlocks * bytesPerBlock;
            uint64_t lastImageBytes = bytesPerRow * (heightInBlocks - 1) + lastRowBytes;
            requiredBytesInCopy += lastImageBytes;
        }
        return requiredBytesInCopy;
    }

    uint64_t GetTexelCountInCopyRegion(uint64_t bytesPerRow,
                                       uint64_t rowsPerImage,
                                       wgpu::Extent3D copyExtent,
                                       wgpu::TextureFormat textureFormat) {
        return RequiredBytesInCopy(bytesPerRow, rowsPerImage, copyExtent, textureFormat) /
               utils::GetTexelBlockSizeInBytes(textureFormat);
    }

    void UnalignDynamicUploader(wgpu::Device device) {
        std::vector<uint8_t> data = {1};

        wgpu::TextureDescriptor descriptor = {};
        descriptor.size = {1, 1, 1};
        descriptor.format = wgpu::TextureFormat::R8Unorm;
        descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc;
        wgpu::Texture texture = device.CreateTexture(&descriptor);

        wgpu::ImageCopyTexture imageCopyTexture =
            utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
        wgpu::TextureDataLayout textureDataLayout =
            utils::CreateTextureDataLayout(0, wgpu::kCopyStrideUndefined);
        wgpu::Extent3D copyExtent = {1, 1, 1};

        // WriteTexture with exactly 1 byte of data.
        device.GetQueue().WriteTexture(&imageCopyTexture, data.data(), 1, &textureDataLayout,
                                       &copyExtent);
    }

    std::pair<wgpu::Buffer, uint64_t> ReadbackTextureBySampling(wgpu::Device device,
                                                                wgpu::ImageCopyTexture source,
                                                                wgpu::TextureFormat format,
                                                                wgpu::TextureDimension dimension,
                                                                wgpu::Extent3D extent3D,
                                                                wgpu::BufferUsage bufferUsage,
                                                                wgpu::CommandEncoder encoder) {
        std::ostringstream shader;

        const char* textureComponentType = utils::GetWGSLColorTextureComponentType(format);
        uint32_t componentCount = utils::GetTextureFormatComponentCount(format);

        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = 4u * componentCount * extent3D.width * extent3D.height *
                          extent3D.depthOrArrayLayers;
        bufferDesc.usage = wgpu::BufferUsage::Storage | bufferUsage;
        wgpu::Buffer buffer = device.CreateBuffer(&bufferDesc);

        const char* indexExpression = nullptr;
        switch (format) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::R8Snorm:
            case wgpu::TextureFormat::R8Uint:
            case wgpu::TextureFormat::R8Sint:
            case wgpu::TextureFormat::R16Uint:
            case wgpu::TextureFormat::R16Sint:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::BC4RUnorm:
            case wgpu::TextureFormat::BC4RSnorm:
                indexExpression = ".r";
                break;

            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG8Snorm:
            case wgpu::TextureFormat::RG8Uint:
            case wgpu::TextureFormat::RG8Sint:
            case wgpu::TextureFormat::RG16Uint:
            case wgpu::TextureFormat::RG16Sint:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::BC5RGUnorm:
            case wgpu::TextureFormat::BC5RGSnorm:
                indexExpression = ".rg";
                break;

            case wgpu::TextureFormat::RGB10A2Unorm:
            case wgpu::TextureFormat::RGB9E5Ufloat:
            case wgpu::TextureFormat::RG11B10Ufloat:
            case wgpu::TextureFormat::BC6HRGBUfloat:
            case wgpu::TextureFormat::BC6HRGBFloat:
                indexExpression = ".rgb";
                break;

            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8UnormSrgb:
            case wgpu::TextureFormat::RGBA8Snorm:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGBA32Float:
            case wgpu::TextureFormat::RGBA32Uint:
            case wgpu::TextureFormat::RGBA32Sint:
            case wgpu::TextureFormat::BC1RGBAUnorm:
            case wgpu::TextureFormat::BC1RGBAUnormSrgb:
            case wgpu::TextureFormat::BC2RGBAUnorm:
            case wgpu::TextureFormat::BC2RGBAUnormSrgb:
            case wgpu::TextureFormat::BC3RGBAUnorm:
            case wgpu::TextureFormat::BC3RGBAUnormSrgb:
            case wgpu::TextureFormat::BC7RGBAUnorm:
            case wgpu::TextureFormat::BC7RGBAUnormSrgb:
                indexExpression = ".rgba";
                break;

            case wgpu::TextureFormat::BGRA8Unorm:
            case wgpu::TextureFormat::BGRA8UnormSrgb:
                indexExpression = ".bgra";
                break;

            case wgpu::TextureFormat::Stencil8:
            case wgpu::TextureFormat::Depth32Float:
            case wgpu::TextureFormat::Depth24Plus:
            case wgpu::TextureFormat::Depth24PlusStencil8:
            case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
            case wgpu::TextureFormat::Undefined:
                UNREACHABLE();
        }

        shader << "type TextureComponentT = " << textureComponentType << ";\n";
        if (componentCount == 1) {
            shader << "type TexelResultT = TextureComponentT;\n";
        } else {
            shader << "type TexelResultT = vec" << std::to_string(componentCount)
                   << "<TextureComponentT>;\n";
        }
        switch (dimension) {
            case wgpu::TextureDimension::e1D:
                shader << "type TextureT = texture_1d<TextureComponentT>;\n";
                break;
            case wgpu::TextureDimension::e2D:
                if (extent3D.depthOrArrayLayers > 1) {
                    shader << "type TextureT = texture_2d_array<TextureComponentT>;\n";
                } else {
                    shader << "type TextureT = texture_2d<TextureComponentT>;\n";
                }
                break;
            case wgpu::TextureDimension::e3D:
                shader << "type TextureT = texture_3d<TextureComponentT>;\n";
                break;
        }
        shader << R"(
[[block]] struct Constants {
    [[size(16)]] origin : vec3<u32>;
    [[size(4)]] mipLevel : u32;
    [[size(4)]] width : u32;
    [[size(4)]] height : u32;
};

[[group(0), binding(0)]] var<uniform> constants : Constants;

[[group(0), binding(1)]] var source : TextureT;

[[block]] struct Result {
    values : array<TexelResultT>;
};
[[group(0), binding(2)]] var<storage> result : [[access(read_write)]] Result;

)";

        switch (dimension) {
            case wgpu::TextureDimension::e1D:
                shader << R"(
fn loadTexel(t : texture_1d<TextureComponentT>, texelCoords : vec3<i32>) -> vec4<TextureComponentT> {
    return textureLoad(t, texelCoords.x, i32(constants.mipLevel));
}
                )";
                break;
            case wgpu::TextureDimension::e2D:
                if (extent3D.depthOrArrayLayers > 1) {
                    shader << R"(
fn loadTexel(t : texture_2d_array<TextureComponentT>, texelCoords : vec3<i32>) -> vec4<TextureComponentT> {
    return textureLoad(t, texelCoords.xy, texelCoords.z, i32(constants.mipLevel));
}
                    )";
                } else {
                    shader << R"(
fn loadTexel(t : texture_2d<TextureComponentT>, texelCoords : vec3<i32>) -> vec4<TextureComponentT> {
    return textureLoad(t, texelCoords.xy, i32(constants.mipLevel));
}
                    )";
                }
                break;
            case wgpu::TextureDimension::e3D:
                shader << R"(
fn loadTexel(t : texture_3d<TextureComponentT>, texelCoords : vec3<i32>) -> vec4<TextureComponentT> {
    return textureLoad(t, texelCoords, i32(constants.mipLevel));
}
                )";
                break;
        }

        shader << R"(
[[stage(compute)]]
fn main([[builtin(global_invocation_id)]] GlobalInvocationID : vec3<u32>) {
    var flatIndex : u32 =
        constants.width * constants.height * GlobalInvocationID.z +
        constants.width * GlobalInvocationID.y +
        GlobalInvocationID.x;

    var texel : vec4<TextureComponentT> = loadTexel(source, vec3<i32>(GlobalInvocationID.xyz + constants.origin));

    result.values[flatIndex] = texel)"
               << indexExpression << ";\n}";

        wgpu::ComputePipelineDescriptor pipelineDesc;
        pipelineDesc.computeStage.entryPoint = "main";
        pipelineDesc.computeStage.module = utils::CreateShaderModule(device, shader.str().c_str());

        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pipelineDesc);

        struct Constants {
            std::array<uint32_t, 4> origin; // one extra for padding
            uint32_t mipLevel;
            uint32_t width;
            uint32_t height;
        };
        static_assert(offsetof(Constants, origin) == 0, "");
        static_assert(offsetof(Constants, mipLevel) == 16, "");
        static_assert(offsetof(Constants, width) == 20, "");
        static_assert(offsetof(Constants, height) == 24, "");

        Constants constants = {
            {source.origin.x, source.origin.y, source.origin.z, 0},
            source.mipLevel,
            extent3D.width,
            extent3D.height,
        };

        wgpu::Buffer constantsBuffer = utils::CreateBufferFromData(
            device, &constants, sizeof(constants), wgpu::BufferUsage::Uniform);

        wgpu::TextureViewDescriptor viewDesc = {};
        if (dimension == wgpu::TextureDimension::e2D) {
            if (extent3D.depthOrArrayLayers > 1) {
                viewDesc.dimension = wgpu::TextureViewDimension::e2DArray;
            } else {
                viewDesc.dimension = wgpu::TextureViewDimension::e2D;
                viewDesc.baseArrayLayer = source.origin.z;
                viewDesc.arrayLayerCount = 1u;
            }
        }

        bool madeEncoder = false;
        if (!encoder) {
            madeEncoder = true;
            encoder = device.CreateCommandEncoder();
        }

        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(
            0, utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                    {
                                        {0, constantsBuffer},
                                        {1, source.texture.CreateView(&viewDesc)},
                                        {2, buffer},
                                    }));
        pass.Dispatch(extent3D.width, extent3D.height, extent3D.depthOrArrayLayers);
        pass.EndPass();

        if (madeEncoder) {
            wgpu::CommandBuffer commandBuffer = encoder.Finish();
            device.GetQueue().Submit(1, &commandBuffer);
        }

        return std::make_pair(buffer, bufferDesc.size);
    }

}  // namespace utils
