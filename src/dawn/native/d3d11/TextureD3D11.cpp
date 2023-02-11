// Copyright 2017 The Dawn Authors
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

#include "dawn/native/d3d11/TextureD3D11.h"

#include <algorithm>
#include <utility>

#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/EnumMaskIterator.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/ResourceMemoryAllocation.h"
#include "dawn/native/ToBackend.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"
// #include "dawn/native/d3d11/D3D11on12Util.h"
#include "dawn/native/d3d11/D3D11Error.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/Forward.h"
// #include "dawn/native/d3d11/HeapD3D11.h"
// #include "dawn/native/d3d11/ResourceAllocatorManagerD3D11.h"
// #include "dawn/native/d3d11/StagingDescriptorAllocatorD3D11.h"
// #include "dawn/native/d3d11/TextureCopySplitter.h"
// #include "dawn/native/d3d11/UtilsD3D11.h"

namespace dawn::native::d3d11 {

namespace {

D3D11_USAGE D3D11TextureUsage(wgpu::TextureUsage usage) {
    return D3D11_USAGE_DEFAULT;
}

bool IsDepthStencilFormat(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::Depth32Float:
        case wgpu::TextureFormat::Depth24Plus:
        case wgpu::TextureFormat::Stencil8:
        case wgpu::TextureFormat::Depth24PlusStencil8:
        case wgpu::TextureFormat::Depth32FloatStencil8:
            return true;
        default:
            return false;
    }
}

UINT D3D11TextureBindFlags(wgpu::TextureUsage usage, wgpu::TextureFormat format) {
    bool isDepthStencilFormat = IsDepthStencilFormat(format);
    UINT bindFlags = 0;
    if (usage & wgpu::TextureUsage::CopySrc) {
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (usage & wgpu::TextureUsage::CopyDst) {
        bindFlags |= D3D11_BIND_RENDER_TARGET;
    }
    if (usage & wgpu::TextureUsage::TextureBinding) {
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (usage & wgpu::TextureUsage::StorageBinding) {
        bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }
    if (usage & wgpu::TextureUsage::RenderAttachment) {
        bindFlags |= isDepthStencilFormat ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_RENDER_TARGET;
    }
    return bindFlags;
}

UINT D3D11TextureCPUAccessFlags(wgpu::TextureUsage usage) {
    return 0;
}

UINT D3D11TextureMiscFlags(wgpu::TextureUsage usage) {
    UINT miscFlags = 0;
    return miscFlags;
}

// TODO: share this with D3D12
DXGI_FORMAT D3D11TypelessTextureFormat(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::R8Unorm:
        case wgpu::TextureFormat::R8Snorm:
        case wgpu::TextureFormat::R8Uint:
        case wgpu::TextureFormat::R8Sint:
            return DXGI_FORMAT_R8_TYPELESS;

        case wgpu::TextureFormat::R16Uint:
        case wgpu::TextureFormat::R16Sint:
        case wgpu::TextureFormat::R16Float:
        case wgpu::TextureFormat::Depth16Unorm:
            return DXGI_FORMAT_R16_TYPELESS;

        case wgpu::TextureFormat::RG8Unorm:
        case wgpu::TextureFormat::RG8Snorm:
        case wgpu::TextureFormat::RG8Uint:
        case wgpu::TextureFormat::RG8Sint:
            return DXGI_FORMAT_R8G8_TYPELESS;

        case wgpu::TextureFormat::R32Uint:
        case wgpu::TextureFormat::R32Sint:
        case wgpu::TextureFormat::R32Float:
            return DXGI_FORMAT_R32_TYPELESS;

        case wgpu::TextureFormat::RG16Uint:
        case wgpu::TextureFormat::RG16Sint:
        case wgpu::TextureFormat::RG16Float:
            return DXGI_FORMAT_R16G16_TYPELESS;

        case wgpu::TextureFormat::RGBA8Unorm:
        case wgpu::TextureFormat::RGBA8UnormSrgb:
        case wgpu::TextureFormat::RGBA8Snorm:
        case wgpu::TextureFormat::RGBA8Uint:
        case wgpu::TextureFormat::RGBA8Sint:
            return DXGI_FORMAT_R8G8B8A8_TYPELESS;

        case wgpu::TextureFormat::BGRA8Unorm:
        case wgpu::TextureFormat::BGRA8UnormSrgb:
            return DXGI_FORMAT_B8G8R8A8_TYPELESS;

        case wgpu::TextureFormat::RGB10A2Unorm:
            return DXGI_FORMAT_R10G10B10A2_TYPELESS;

        case wgpu::TextureFormat::RG11B10Ufloat:
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case wgpu::TextureFormat::RGB9E5Ufloat:
            return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;

        case wgpu::TextureFormat::RG32Uint:
        case wgpu::TextureFormat::RG32Sint:
        case wgpu::TextureFormat::RG32Float:
            return DXGI_FORMAT_R32G32_TYPELESS;

        case wgpu::TextureFormat::RGBA16Uint:
        case wgpu::TextureFormat::RGBA16Sint:
        case wgpu::TextureFormat::RGBA16Float:
            return DXGI_FORMAT_R16G16B16A16_TYPELESS;

        case wgpu::TextureFormat::RGBA32Uint:
        case wgpu::TextureFormat::RGBA32Sint:
        case wgpu::TextureFormat::RGBA32Float:
            return DXGI_FORMAT_R32G32B32A32_TYPELESS;

        case wgpu::TextureFormat::Depth32Float:
        case wgpu::TextureFormat::Depth24Plus:
            return DXGI_FORMAT_R32_TYPELESS;

        // DXGI_FORMAT_D24_UNORM_S8_UINT is the smallest format supported on D3D12 that has stencil,
        // for which the typeless equivalent is DXGI_FORMAT_R24G8_TYPELESS.
        case wgpu::TextureFormat::Stencil8:
            return DXGI_FORMAT_R24G8_TYPELESS;
        case wgpu::TextureFormat::Depth24PlusStencil8:
        case wgpu::TextureFormat::Depth32FloatStencil8:
            return DXGI_FORMAT_R32G8X24_TYPELESS;

        case wgpu::TextureFormat::BC1RGBAUnorm:
        case wgpu::TextureFormat::BC1RGBAUnormSrgb:
            return DXGI_FORMAT_BC1_TYPELESS;

        case wgpu::TextureFormat::BC2RGBAUnorm:
        case wgpu::TextureFormat::BC2RGBAUnormSrgb:
            return DXGI_FORMAT_BC2_TYPELESS;

        case wgpu::TextureFormat::BC3RGBAUnorm:
        case wgpu::TextureFormat::BC3RGBAUnormSrgb:
            return DXGI_FORMAT_BC3_TYPELESS;

        case wgpu::TextureFormat::BC4RSnorm:
        case wgpu::TextureFormat::BC4RUnorm:
            return DXGI_FORMAT_BC4_TYPELESS;

        case wgpu::TextureFormat::BC5RGSnorm:
        case wgpu::TextureFormat::BC5RGUnorm:
            return DXGI_FORMAT_BC5_TYPELESS;

        case wgpu::TextureFormat::BC6HRGBFloat:
        case wgpu::TextureFormat::BC6HRGBUfloat:
            return DXGI_FORMAT_BC6H_TYPELESS;

        case wgpu::TextureFormat::BC7RGBAUnorm:
        case wgpu::TextureFormat::BC7RGBAUnormSrgb:
            return DXGI_FORMAT_BC7_TYPELESS;

        case wgpu::TextureFormat::ETC2RGB8Unorm:
        case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
        case wgpu::TextureFormat::ETC2RGB8A1Unorm:
        case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
        case wgpu::TextureFormat::ETC2RGBA8Unorm:
        case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
        case wgpu::TextureFormat::EACR11Unorm:
        case wgpu::TextureFormat::EACR11Snorm:
        case wgpu::TextureFormat::EACRG11Unorm:
        case wgpu::TextureFormat::EACRG11Snorm:

        case wgpu::TextureFormat::ASTC4x4Unorm:
        case wgpu::TextureFormat::ASTC4x4UnormSrgb:
        case wgpu::TextureFormat::ASTC5x4Unorm:
        case wgpu::TextureFormat::ASTC5x4UnormSrgb:
        case wgpu::TextureFormat::ASTC5x5Unorm:
        case wgpu::TextureFormat::ASTC5x5UnormSrgb:
        case wgpu::TextureFormat::ASTC6x5Unorm:
        case wgpu::TextureFormat::ASTC6x5UnormSrgb:
        case wgpu::TextureFormat::ASTC6x6Unorm:
        case wgpu::TextureFormat::ASTC6x6UnormSrgb:
        case wgpu::TextureFormat::ASTC8x5Unorm:
        case wgpu::TextureFormat::ASTC8x5UnormSrgb:
        case wgpu::TextureFormat::ASTC8x6Unorm:
        case wgpu::TextureFormat::ASTC8x6UnormSrgb:
        case wgpu::TextureFormat::ASTC8x8Unorm:
        case wgpu::TextureFormat::ASTC8x8UnormSrgb:
        case wgpu::TextureFormat::ASTC10x5Unorm:
        case wgpu::TextureFormat::ASTC10x5UnormSrgb:
        case wgpu::TextureFormat::ASTC10x6Unorm:
        case wgpu::TextureFormat::ASTC10x6UnormSrgb:
        case wgpu::TextureFormat::ASTC10x8Unorm:
        case wgpu::TextureFormat::ASTC10x8UnormSrgb:
        case wgpu::TextureFormat::ASTC10x10Unorm:
        case wgpu::TextureFormat::ASTC10x10UnormSrgb:
        case wgpu::TextureFormat::ASTC12x10Unorm:
        case wgpu::TextureFormat::ASTC12x10UnormSrgb:
        case wgpu::TextureFormat::ASTC12x12Unorm:
        case wgpu::TextureFormat::ASTC12x12UnormSrgb:

        case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
        case wgpu::TextureFormat::Undefined:
            UNREACHABLE();
    }
}

}  // namespace

// TODO: share this with D3D12
DXGI_FORMAT D3D11TextureFormat(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::R8Unorm:
            return DXGI_FORMAT_R8_UNORM;
        case wgpu::TextureFormat::R8Snorm:
            return DXGI_FORMAT_R8_SNORM;
        case wgpu::TextureFormat::R8Uint:
            return DXGI_FORMAT_R8_UINT;
        case wgpu::TextureFormat::R8Sint:
            return DXGI_FORMAT_R8_SINT;

        case wgpu::TextureFormat::R16Uint:
            return DXGI_FORMAT_R16_UINT;
        case wgpu::TextureFormat::R16Sint:
            return DXGI_FORMAT_R16_SINT;
        case wgpu::TextureFormat::R16Float:
            return DXGI_FORMAT_R16_FLOAT;
        case wgpu::TextureFormat::RG8Unorm:
            return DXGI_FORMAT_R8G8_UNORM;
        case wgpu::TextureFormat::RG8Snorm:
            return DXGI_FORMAT_R8G8_SNORM;
        case wgpu::TextureFormat::RG8Uint:
            return DXGI_FORMAT_R8G8_UINT;
        case wgpu::TextureFormat::RG8Sint:
            return DXGI_FORMAT_R8G8_SINT;

        case wgpu::TextureFormat::R32Uint:
            return DXGI_FORMAT_R32_UINT;
        case wgpu::TextureFormat::R32Sint:
            return DXGI_FORMAT_R32_SINT;
        case wgpu::TextureFormat::R32Float:
            return DXGI_FORMAT_R32_FLOAT;
        case wgpu::TextureFormat::RG16Uint:
            return DXGI_FORMAT_R16G16_UINT;
        case wgpu::TextureFormat::RG16Sint:
            return DXGI_FORMAT_R16G16_SINT;
        case wgpu::TextureFormat::RG16Float:
            return DXGI_FORMAT_R16G16_FLOAT;
        case wgpu::TextureFormat::RGBA8Unorm:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case wgpu::TextureFormat::RGBA8UnormSrgb:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case wgpu::TextureFormat::RGBA8Snorm:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case wgpu::TextureFormat::RGBA8Uint:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case wgpu::TextureFormat::RGBA8Sint:
            return DXGI_FORMAT_R8G8B8A8_SINT;
        case wgpu::TextureFormat::BGRA8Unorm:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case wgpu::TextureFormat::BGRA8UnormSrgb:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case wgpu::TextureFormat::RGB10A2Unorm:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case wgpu::TextureFormat::RG11B10Ufloat:
            return DXGI_FORMAT_R11G11B10_FLOAT;
        case wgpu::TextureFormat::RGB9E5Ufloat:
            return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;

        case wgpu::TextureFormat::RG32Uint:
            return DXGI_FORMAT_R32G32_UINT;
        case wgpu::TextureFormat::RG32Sint:
            return DXGI_FORMAT_R32G32_SINT;
        case wgpu::TextureFormat::RG32Float:
            return DXGI_FORMAT_R32G32_FLOAT;
        case wgpu::TextureFormat::RGBA16Uint:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case wgpu::TextureFormat::RGBA16Sint:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case wgpu::TextureFormat::RGBA16Float:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case wgpu::TextureFormat::RGBA32Uint:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case wgpu::TextureFormat::RGBA32Sint:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case wgpu::TextureFormat::RGBA32Float:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;

        case wgpu::TextureFormat::Depth16Unorm:
            return DXGI_FORMAT_D16_UNORM;
        case wgpu::TextureFormat::Depth32Float:
        case wgpu::TextureFormat::Depth24Plus:
            return DXGI_FORMAT_D32_FLOAT;
        // DXGI_FORMAT_D24_UNORM_S8_UINT is the smallest format supported on D3D12 that has stencil.
        case wgpu::TextureFormat::Stencil8:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case wgpu::TextureFormat::Depth24PlusStencil8:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case wgpu::TextureFormat::Depth32FloatStencil8:
            return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        case wgpu::TextureFormat::BC1RGBAUnorm:
            return DXGI_FORMAT_BC1_UNORM;
        case wgpu::TextureFormat::BC1RGBAUnormSrgb:
            return DXGI_FORMAT_BC1_UNORM_SRGB;
        case wgpu::TextureFormat::BC2RGBAUnorm:
            return DXGI_FORMAT_BC2_UNORM;
        case wgpu::TextureFormat::BC2RGBAUnormSrgb:
            return DXGI_FORMAT_BC2_UNORM_SRGB;
        case wgpu::TextureFormat::BC3RGBAUnorm:
            return DXGI_FORMAT_BC3_UNORM;
        case wgpu::TextureFormat::BC3RGBAUnormSrgb:
            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case wgpu::TextureFormat::BC4RSnorm:
            return DXGI_FORMAT_BC4_SNORM;
        case wgpu::TextureFormat::BC4RUnorm:
            return DXGI_FORMAT_BC4_UNORM;
        case wgpu::TextureFormat::BC5RGSnorm:
            return DXGI_FORMAT_BC5_SNORM;
        case wgpu::TextureFormat::BC5RGUnorm:
            return DXGI_FORMAT_BC5_UNORM;
        case wgpu::TextureFormat::BC6HRGBFloat:
            return DXGI_FORMAT_BC6H_SF16;
        case wgpu::TextureFormat::BC6HRGBUfloat:
            return DXGI_FORMAT_BC6H_UF16;
        case wgpu::TextureFormat::BC7RGBAUnorm:
            return DXGI_FORMAT_BC7_UNORM;
        case wgpu::TextureFormat::BC7RGBAUnormSrgb:
            return DXGI_FORMAT_BC7_UNORM_SRGB;

        case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
            return DXGI_FORMAT_NV12;

        case wgpu::TextureFormat::ETC2RGB8Unorm:
        case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
        case wgpu::TextureFormat::ETC2RGB8A1Unorm:
        case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
        case wgpu::TextureFormat::ETC2RGBA8Unorm:
        case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
        case wgpu::TextureFormat::EACR11Unorm:
        case wgpu::TextureFormat::EACR11Snorm:
        case wgpu::TextureFormat::EACRG11Unorm:
        case wgpu::TextureFormat::EACRG11Snorm:

        case wgpu::TextureFormat::ASTC4x4Unorm:
        case wgpu::TextureFormat::ASTC4x4UnormSrgb:
        case wgpu::TextureFormat::ASTC5x4Unorm:
        case wgpu::TextureFormat::ASTC5x4UnormSrgb:
        case wgpu::TextureFormat::ASTC5x5Unorm:
        case wgpu::TextureFormat::ASTC5x5UnormSrgb:
        case wgpu::TextureFormat::ASTC6x5Unorm:
        case wgpu::TextureFormat::ASTC6x5UnormSrgb:
        case wgpu::TextureFormat::ASTC6x6Unorm:
        case wgpu::TextureFormat::ASTC6x6UnormSrgb:
        case wgpu::TextureFormat::ASTC8x5Unorm:
        case wgpu::TextureFormat::ASTC8x5UnormSrgb:
        case wgpu::TextureFormat::ASTC8x6Unorm:
        case wgpu::TextureFormat::ASTC8x6UnormSrgb:
        case wgpu::TextureFormat::ASTC8x8Unorm:
        case wgpu::TextureFormat::ASTC8x8UnormSrgb:
        case wgpu::TextureFormat::ASTC10x5Unorm:
        case wgpu::TextureFormat::ASTC10x5UnormSrgb:
        case wgpu::TextureFormat::ASTC10x6Unorm:
        case wgpu::TextureFormat::ASTC10x6UnormSrgb:
        case wgpu::TextureFormat::ASTC10x8Unorm:
        case wgpu::TextureFormat::ASTC10x8UnormSrgb:
        case wgpu::TextureFormat::ASTC10x10Unorm:
        case wgpu::TextureFormat::ASTC10x10UnormSrgb:
        case wgpu::TextureFormat::ASTC12x10Unorm:
        case wgpu::TextureFormat::ASTC12x10UnormSrgb:
        case wgpu::TextureFormat::ASTC12x12Unorm:
        case wgpu::TextureFormat::ASTC12x12UnormSrgb:

        case wgpu::TextureFormat::Undefined:
            UNREACHABLE();
    }
}

// // TODO: share this with D3D12
// MaybeError ValidateTextureDescriptorCanBeWrapped(const TextureDescriptor* descriptor) {
//     DAWN_INVALID_IF(descriptor->dimension != wgpu::TextureDimension::e2D,
//                     "Texture dimension (%s) is not %s.", descriptor->dimension,
//                     wgpu::TextureDimension::e2D);

//     DAWN_INVALID_IF(descriptor->mipLevelCount != 1, "Mip level count (%u) is not 1.",
//                     descriptor->mipLevelCount);

//     DAWN_INVALID_IF(descriptor->size.depthOrArrayLayers != 1, "Array layer count (%u) is not 1.",
//                     descriptor->size.depthOrArrayLayers);

//     DAWN_INVALID_IF(descriptor->sampleCount != 1, "Sample count (%u) is not 1.",
//                     descriptor->sampleCount);

//     return {};
// }

// // TODO: share this with D3D12
// MaybeError ValidateD3D11TextureCanBeWrapped(ID3D11Resource* d3d11Resource,
//                                             const TextureDescriptor* dawnDescriptor) {
//     const D3D11_RESOURCE_DESC d3dDescriptor = d3d11Resource->GetDesc();
//     DAWN_INVALID_IF(
//         (dawnDescriptor->size.width != d3dDescriptor.Width) ||
//             (dawnDescriptor->size.height != d3dDescriptor.Height) ||
//             (dawnDescriptor->size.depthOrArrayLayers != 1),
//         "D3D11 texture size (Width: %u, Height: %u, DepthOrArraySize: 1) doesn't match Dawn "
//         "descriptor size (width: %u, height: %u, depthOrArrayLayers: %u).",
//         d3dDescriptor.Width, d3dDescriptor.Height, dawnDescriptor->size.width,
//         dawnDescriptor->size.height, dawnDescriptor->size.depthOrArrayLayers);

//     const DXGI_FORMAT dxgiFormatFromDescriptor = D3D11TextureFormat(dawnDescriptor->format);
//     DAWN_INVALID_IF(dxgiFormatFromDescriptor != d3dDescriptor.Format,
//                     "D3D11 texture format (%x) is not compatible with Dawn descriptor format
//                     (%s).", d3dDescriptor.Format, dawnDescriptor->format);

//     DAWN_INVALID_IF(d3dDescriptor.MipLevels != 1,
//                     "D3D11 texture number of miplevels (%u) is not 1.", d3dDescriptor.MipLevels);

//     DAWN_INVALID_IF(d3dDescriptor.DepthOrArraySize != 1, "D3D11 texture array size (%u) is
//     not 1.",
//                     d3dDescriptor.DepthOrArraySize);

//     // Shared textures cannot be multi-sample so no need to check those.
//     ASSERT(d3dDescriptor.SampleDesc.Count == 1);
//     ASSERT(d3dDescriptor.SampleDesc.Quality == 0);

//     return {};
// }

// static
ResultOrError<Ref<Texture>> Texture::Create(Device* device, const TextureDescriptor* descriptor) {
    Ref<Texture> dawnTexture =
        AcquireRef(new Texture(device, descriptor, TextureState::OwnedInternal));

    DAWN_INVALID_IF(dawnTexture->GetFormat().IsMultiPlanar(),
                    "Cannot create a multi-planar formatted texture directly");

    DAWN_TRY(dawnTexture->InitializeAsInternalTexture());
    return std::move(dawnTexture);
}

// // static
// ResultOrError<Ref<Texture>> Texture::CreateExternalImage(
//     Device* device,
//     const TextureDescriptor* descriptor,
//     ComPtr<ID3D11Resource> d3d11Texture,
//     std::vector<Ref<Fence>> waitFences,
//     Ref<D3D11on12ResourceCacheEntry> d3d11on12Resource,
//     bool isSwapChainTexture,
//     bool isInitialized) {
//     Ref<Texture> dawnTexture =
//         AcquireRef(new Texture(device, descriptor, TextureState::OwnedExternal));

//     DAWN_TRY(
//         dawnTexture->InitializeAsExternalTexture(std::move(d3d11Texture), std::move(waitFences),
//                                                  std::move(d3d11on12Resource),
//                                                  isSwapChainTexture));

//     // Importing a multi-planar format must be initialized. This is required because
//     // a shared multi-planar format cannot be initialized by Dawn.
//     DAWN_INVALID_IF(
//         !isInitialized && dawnTexture->GetFormat().IsMultiPlanar(),
//         "Cannot create a texture with a multi-planar format (%s) with uninitialized data.",
//         dawnTexture->GetFormat().format);

//     dawnTexture->SetIsSubresourceContentInitialized(isInitialized,
//                                                     dawnTexture->GetAllSubresources());
//     return std::move(dawnTexture);
// }

// static
ResultOrError<Ref<Texture>> Texture::Create(Device* device,
                                            const TextureDescriptor* descriptor,
                                            ComPtr<ID3D11Resource> d3d11Texture) {
    Ref<Texture> dawnTexture =
        AcquireRef(new Texture(device, descriptor, TextureState::OwnedExternal));
    DAWN_TRY(dawnTexture->InitializeAsSwapChainTexture(std::move(d3d11Texture)));
    return std::move(dawnTexture);
}

// MaybeError Texture::InitializeAsExternalTexture(ComPtr<ID3D11Resource> d3d11Texture,
//                                                 std::vector<Ref<Fence>> waitFences,
//                                                 Ref<D3D11on12ResourceCacheEntry>
//                                                 d3d11on12Resource, bool isSwapChainTexture) {
//     D3D11_RESOURCE_DESC desc = d3d11Texture->GetDesc();
//     mD3D11ResourceFlags = desc.Flags;

//     AllocationInfo info;
//     info.mMethod = AllocationMethod::kExternal;
//     // When creating the ResourceHeapAllocation, the resource heap is set to nullptr because the
//     // texture is owned externally. The texture's owning entity must remain responsible for
//     // memory management.
//     mResourceAllocation = {info, 0, std::move(d3d11Texture), nullptr};

//     mWaitFences = std::move(waitFences);
//     mD3D11on12Resource = std::move(d3d11on12Resource);
//     mSwapChainTexture = isSwapChainTexture;

//     SetLabelHelper("Dawn_ExternalTexture");

//     return {};
// }

MaybeError Texture::InitializeAsInternalTexture() {
    Device* device = ToBackend(GetDevice());

    bool applyForceClearCopyableDepthStencilTextureOnCreationToggle =
        device->IsToggleEnabled(Toggle::D3D12ForceClearCopyableDepthStencilTextureOnCreation) &&
        GetFormat().HasDepthOrStencil() && (GetInternalUsage() & wgpu::TextureUsage::CopyDst);
    if (applyForceClearCopyableDepthStencilTextureOnCreationToggle) {
        AddInternalUsage(wgpu::TextureUsage::RenderAttachment);
    }

    // // This will need to be much more nuanced when WebGPU has
    // // texture view compatibility rules.
    // const bool needsTypelessFormat =
    //     (GetDevice()->IsToggleEnabled(Toggle::D3D12AlwaysUseTypelessFormatsForCastableTexture) &&
    //      GetViewFormats().any()) ||
    //     (GetFormat().HasDepthOrStencil() &&
    //      (GetInternalUsage() & wgpu::TextureUsage::TextureBinding) != 0);
    const bool needsTypelessFormat = false;

    DXGI_FORMAT dxgiFormat = needsTypelessFormat ? D3D11TypelessTextureFormat(GetFormat().format)
                                                 : D3D11TextureFormat(GetFormat().format);

    switch (GetDimension()) {
        case wgpu::TextureDimension::e1D: {
            D3D11_TEXTURE1D_DESC textureDescriptor;
            textureDescriptor.Width = GetSize().width;
            textureDescriptor.MipLevels = static_cast<UINT16>(GetNumMipLevels());
            textureDescriptor.ArraySize = 1;
            textureDescriptor.Format = dxgiFormat;
            textureDescriptor.Usage = D3D11TextureUsage(GetUsage());
            textureDescriptor.BindFlags = D3D11TextureBindFlags(GetUsage(), GetFormat().format);
            textureDescriptor.CPUAccessFlags = D3D11TextureCPUAccessFlags(GetUsage());
            textureDescriptor.MiscFlags = D3D11TextureMiscFlags(GetUsage());
            ComPtr<ID3D11Texture1D> texture;
            DAWN_TRY(CheckHRESULT(
                device->GetD3D11Device()->CreateTexture1D(&textureDescriptor, nullptr, &texture),
                "D3D11 create texture"));
            mResource = texture;
            break;
        }
        case wgpu::TextureDimension::e2D: {
            D3D11_TEXTURE2D_DESC textureDescriptor;
            textureDescriptor.Width = GetSize().width;
            textureDescriptor.Height = GetSize().height;
            textureDescriptor.MipLevels = static_cast<UINT16>(GetNumMipLevels());
            textureDescriptor.ArraySize = GetArrayLayers();
            textureDescriptor.Format = dxgiFormat;
            textureDescriptor.SampleDesc.Count = GetSampleCount();
            textureDescriptor.SampleDesc.Quality = 0;
            textureDescriptor.Usage = D3D11TextureUsage(GetUsage());
            textureDescriptor.BindFlags = D3D11TextureBindFlags(GetUsage(), GetFormat().format);
            textureDescriptor.CPUAccessFlags = D3D11TextureCPUAccessFlags(GetUsage());
            textureDescriptor.MiscFlags = D3D11TextureMiscFlags(GetUsage());
            ComPtr<ID3D11Texture2D> texture;
            DAWN_TRY(CheckHRESULT(
                device->GetD3D11Device()->CreateTexture2D(&textureDescriptor, nullptr, &texture),
                "D3D11 create texture"));
            mResource = texture;
            break;
        }
        case wgpu::TextureDimension::e3D: {
            D3D11_TEXTURE3D_DESC textureDescriptor;
            textureDescriptor.Width = GetSize().width;
            textureDescriptor.Height = GetSize().height;
            textureDescriptor.Depth = GetSize().depthOrArrayLayers;
            textureDescriptor.MipLevels = static_cast<UINT16>(GetNumMipLevels());
            textureDescriptor.Format = dxgiFormat;
            textureDescriptor.Usage = D3D11TextureUsage(GetUsage());
            textureDescriptor.BindFlags = D3D11TextureBindFlags(GetUsage(), GetFormat().format);
            textureDescriptor.CPUAccessFlags = D3D11TextureCPUAccessFlags(GetUsage());
            textureDescriptor.MiscFlags = D3D11TextureMiscFlags(GetUsage());
            ComPtr<ID3D11Texture3D> texture;
            DAWN_TRY(CheckHRESULT(
                device->GetD3D11Device()->CreateTexture3D(&textureDescriptor, nullptr, &texture),
                "D3D11 create texture"));
            mResource = texture;
            break;
        }
    }

    SetLabelImpl();

    if (applyForceClearCopyableDepthStencilTextureOnCreationToggle) {
        CommandRecordingContext* commandContext;
        DAWN_TRY_ASSIGN(commandContext, device->GetPendingCommandContext());
        DAWN_TRY(ClearTexture(commandContext, GetAllSubresources(), TextureBase::ClearValue::Zero));
    }

    if (device->IsToggleEnabled(Toggle::NonzeroClearResourcesOnCreationForTesting)) {
        CommandRecordingContext* commandContext;
        DAWN_TRY_ASSIGN(commandContext, device->GetPendingCommandContext());

        DAWN_TRY(
            ClearTexture(commandContext, GetAllSubresources(), TextureBase::ClearValue::NonZero));
    }

    return {};
}

MaybeError Texture::InitializeAsSwapChainTexture(ComPtr<ID3D11Resource> d3d11Texture) {
    // AllocationInfo info;
    // info.mMethod = AllocationMethod::kExternal;
    // // When creating the ResourceHeapAllocation, the resource heap is set to nullptr because the
    // // texture is owned externally. The texture's owning entity must remain responsible for
    // // memory management.
    // mResourceAllocation = {info, 0, std::move(d3d11Texture), nullptr};

    mResource = std::move(d3d11Texture);
    SetLabelHelper("Dawn_SwapChainTexture");

    return {};
}

Texture::Texture(Device* device, const TextureDescriptor* descriptor, TextureState state)
    : TextureBase(device, descriptor, state) {}

Texture::~Texture() {}

void Texture::DestroyImpl() {
    TextureBase::DestroyImpl();

    // ToBackend(GetDevice())->DeallocateMemory(mResourceAllocation);

    // // Set mSwapChainTexture to false to prevent ever calling ID3D11SharingContract::Present
    // again. mSwapChainTexture = false;

    // // Now that the texture has been destroyed, it should release the d3d11on12 resource refptr.
    // mD3D11on12Resource = nullptr;
}

// ResultOrError<ExecutionSerial> Texture::EndAccess() {
//     ASSERT(mD3D11on12Resource == nullptr);

//     Device* device = ToBackend(GetDevice());

//     // Synchronize if texture access wasn't synchronized already due to ExecuteCommandLists.
//     if (!mSignalFenceValue.has_value()) {
//         // Needed to ensure that command allocator doesn't get destroyed before pending commands
//         // are submitted due to calling NextSerial(). No-op if there are no pending commands.
//         DAWN_TRY(device->ExecutePendingCommandContext());
//         // If there were pending commands that used this texture mSignalFenceValue will be set,
//         // but if it's still not set, generate a signal fence after waiting on wait fences.
//         if (!mSignalFenceValue.has_value()) {
//             DAWN_TRY(SynchronizeImportedTextureBeforeUse());
//             DAWN_TRY(SynchronizeImportedTextureAfterUse());
//         }
//         DAWN_TRY(device->NextSerial());
//         ASSERT(mSignalFenceValue.has_value());
//     }

//     ExecutionSerial ret = mSignalFenceValue.value();
//     ASSERT(ret <= device->GetLastSubmittedCommandSerial());
//     // Explicitly call reset() since std::move() on optional doesn't make it std::nullopt.
//     mSignalFenceValue.reset();
//     return ret;
// }

DXGI_FORMAT Texture::GetD3D11Format() const {
    return D3D11TextureFormat(GetFormat().format);
}

// ID3D11Resource* Texture::GetD3D11Resource() const {
//     return mResourceAllocation.GetD3D11Resource();
// }

// D3D11_RESOURCE_FLAGS Texture::GetD3D11ResourceFlags() const {
//     return mD3D11ResourceFlags;
// }

DXGI_FORMAT Texture::GetD3D11CopyableSubresourceFormat(Aspect aspect) const {
    ASSERT(GetFormat().aspects & aspect);

    switch (GetFormat().format) {
        case wgpu::TextureFormat::Depth24PlusStencil8:
        case wgpu::TextureFormat::Depth32FloatStencil8:
        case wgpu::TextureFormat::Stencil8:
            switch (aspect) {
                case Aspect::Depth:
                    return DXGI_FORMAT_R32_FLOAT;
                case Aspect::Stencil:
                    return DXGI_FORMAT_R8_UINT;
                default:
                    UNREACHABLE();
            }
        default:
            ASSERT(HasOneBit(GetFormat().aspects));
            return GetD3D11Format();
    }
}

// MaybeError Texture::SynchronizeImportedTextureBeforeUse() {
//     if (mD3D11on12Resource != nullptr) {
//         DAWN_TRY(mD3D11on12Resource->AcquireKeyedMutex());
//     }
//     // Perform the wait only on the first call.
//     Device* device = ToBackend(GetDevice());
//     for (Ref<Fence>& fence : mWaitFences) {
//         DAWN_TRY(CheckHRESULT(device->GetCommandQueue()->Wait(fence->GetD3D11Fence(),
//                                                               fence->GetFenceValue()),
//                               "D3D11 fence wait"););
//         // Keep D3D11 fence alive since we'll clear the waitFences list below.
//         device->ReferenceUntilUnused(fence->GetD3D11Fence());
//     }
//     mWaitFences.clear();
//     return {};
// }

// MaybeError Texture::SynchronizeImportedTextureAfterUse() {
//     // In PIX's D3D11-only mode, there is no way to determine frame boundaries
//     // for WebGPU since Dawn does not manage DXGI swap chains. Without assistance,
//     // PIX will wait forever for a present that never happens.
//     // If we know we're dealing with a swapbuffer texture, inform PIX we've
//     // "presented" the texture so it can determine frame boundaries and use its
//     // contents for the UI.
//     Device* device = ToBackend(GetDevice());
//     if (mSwapChainTexture) {
//         ID3D11SharingContract* d3dSharingContract = device->GetSharingContract();
//         if (d3dSharingContract != nullptr) {
//             d3dSharingContract->Present(mResourceAllocation.GetD3D11Resource(), 0, 0);
//         }
//     }
//     if (mD3D11on12Resource != nullptr) {
//         DAWN_TRY(mD3D11on12Resource->ReleaseKeyedMutex());
//     } else {
//         // NextSerial() will be called after this - this is also checked in EndAccess().
//         mSignalFenceValue = device->GetPendingCommandSerial();
//     }
//     return {};
// }

// void Texture::TrackUsageAndTransitionNow(CommandRecordingContext* commandContext,
//                                          wgpu::TextureUsage usage,
//                                          const SubresourceRange& range) {
//     TrackUsageAndTransitionNow(commandContext, D3D11TextureUsage(usage, GetFormat()), range);
// }

// void Texture::TrackAllUsageAndTransitionNow(CommandRecordingContext* commandContext,
//                                             wgpu::TextureUsage usage) {
//     TrackUsageAndTransitionNow(commandContext, D3D11TextureUsage(usage, GetFormat()),
//                                GetAllSubresources());
// }

// void Texture::TrackAllUsageAndTransitionNow(CommandRecordingContext* commandContext,
//                                             D3D11_RESOURCE_STATES newState) {
//     TrackUsageAndTransitionNow(commandContext, newState, GetAllSubresources());
// }

// void Texture::TrackUsageAndTransitionNow(CommandRecordingContext* commandContext,
//                                          D3D11_RESOURCE_STATES newState,
//                                          const SubresourceRange& range) {
//     if (mResourceAllocation.GetInfo().mMethod != AllocationMethod::kExternal) {
//         // Track the underlying heap to ensure residency.
//         Heap* heap = ToBackend(mResourceAllocation.GetResourceHeap());
//         commandContext->TrackHeapUsage(heap, GetDevice()->GetPendingCommandSerial());
//     }

//     std::vector<D3D11_RESOURCE_BARRIER> barriers;

//     // TODO(enga): Consider adding a Count helper.
//     uint32_t aspectCount = 0;
//     for (Aspect aspect : IterateEnumMask(range.aspects)) {
//         aspectCount++;
//         DAWN_UNUSED(aspect);
//     }

//     barriers.reserve(range.levelCount * range.layerCount * aspectCount);

//     TransitionUsageAndGetResourceBarrier(commandContext, &barriers, newState, range);
//     if (barriers.size()) {
//         commandContext->GetCommandList()->ResourceBarrier(barriers.size(), barriers.data());
//     }
// }

// void Texture::TransitionSubresourceRange(std::vector<D3D11_RESOURCE_BARRIER>* barriers,
//                                          const SubresourceRange& range,
//                                          StateAndDecay* state,
//                                          D3D11_RESOURCE_STATES newState,
//                                          ExecutionSerial pendingCommandSerial) const {
//     D3D11_RESOURCE_STATES lastState = state->lastState;

//     // If the transition is from-UAV-to-UAV, then a UAV barrier is needed.
//     // If one of the usages isn't UAV, then other barriers are used.
//     bool needsUAVBarrier = lastState == D3D11_RESOURCE_STATE_UNORDERED_ACCESS &&
//                            newState == D3D11_RESOURCE_STATE_UNORDERED_ACCESS;

//     if (needsUAVBarrier) {
//         D3D11_RESOURCE_BARRIER barrier;
//         barrier.Type = D3D11_RESOURCE_BARRIER_TYPE_UAV;
//         barrier.Flags = D3D11_RESOURCE_BARRIER_FLAG_NONE;
//         barrier.UAV.pResource = GetD3D11Resource();
//         barriers->push_back(barrier);
//         return;
//     }

//     // Reuse the subresource(s) directly and avoid transition when it isn't needed, and
//     // return false.
//     if (lastState == newState) {
//         return;
//     }

//     // The COMMON state represents a state where no write operations can be pending, and
//     // where all pixels are uncompressed. This makes it possible to transition to and
//     // from some states without synchronization (i.e. without an explicit
//     // ResourceBarrier call). Textures can be implicitly promoted to 1) a single write
//     // state, or 2) multiple read states. Textures will implicitly decay to the COMMON
//     // state when all of the following are true: 1) the texture is accessed on a command
//     // list, 2) the ExecuteCommandLists call that uses that command list has ended, and
//     // 3) the texture was promoted implicitly to a read-only state and is still in that
//     // state.
//     //
//     https://docs.microsoft.com/en-us/windows/desktop/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#implicit-state-transitions

//     // To track implicit decays, we must record the pending serial on which that
//     // transition will occur. When that texture is used again, the previously recorded
//     // serial must be compared to the last completed serial to determine if the texture
//     // has implicity decayed to the common state.
//     if (state->isValidToDecay && pendingCommandSerial > state->lastDecaySerial) {
//         lastState = D3D11_RESOURCE_STATE_COMMON;
//     }

//     // Update the tracked state.
//     state->lastState = newState;

//     // Destination states that qualify for an implicit promotion for a
//     // non-simultaneous-access texture: NON_PIXEL_SHADER_RESOURCE,
//     // PIXEL_SHADER_RESOURCE, COPY_SRC, COPY_DEST.
//     {
//         const D3D11_RESOURCE_STATES kD3D11PromotableReadOnlyStates =
//             D3D11_RESOURCE_STATE_COPY_SOURCE | D3D11_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
//             D3D11_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

//         if (lastState == D3D11_RESOURCE_STATE_COMMON) {
//             if (IsSubset(newState, kD3D11PromotableReadOnlyStates)) {
//                 // Implicit texture state decays can only occur when the texture was implicitly
//                 // transitioned to a read-only state. isValidToDecay is needed to differentiate
//                 // between resources that were implictly or explicitly transitioned to a
//                 // read-only state.
//                 state->isValidToDecay = true;
//                 state->lastDecaySerial = pendingCommandSerial;
//                 return;
//             } else if (newState == D3D11_RESOURCE_STATE_COPY_DEST) {
//                 state->isValidToDecay = false;
//                 return;
//             }
//         }
//     }

//     D3D11_RESOURCE_BARRIER barrier;
//     barrier.Type = D3D11_RESOURCE_BARRIER_TYPE_TRANSITION;
//     barrier.Flags = D3D11_RESOURCE_BARRIER_FLAG_NONE;
//     barrier.Transition.pResource = GetD3D11Resource();
//     barrier.Transition.StateBefore = lastState;
//     barrier.Transition.StateAfter = newState;

//     bool isFullRange = range.baseArrayLayer == 0 && range.baseMipLevel == 0 &&
//                        range.layerCount == GetArrayLayers() &&
//                        range.levelCount == GetNumMipLevels() &&
//                        range.aspects == GetFormat().aspects;

//     // Use a single transition for all subresources if possible.
//     if (isFullRange) {
//         barrier.Transition.Subresource = D3D11_RESOURCE_BARRIER_ALL_SUBRESOURCES;
//         barriers->push_back(barrier);
//     } else {
//         for (Aspect aspect : IterateEnumMask(range.aspects)) {
//             for (uint32_t arrayLayer = 0; arrayLayer < range.layerCount; ++arrayLayer) {
//                 for (uint32_t mipLevel = 0; mipLevel < range.levelCount; ++mipLevel) {
//                     barrier.Transition.Subresource = GetSubresourceIndex(
//                         range.baseMipLevel + mipLevel, range.baseArrayLayer + arrayLayer,
//                         aspect);
//                     barriers->push_back(barrier);
//                 }
//             }
//         }
//     }

//     state->isValidToDecay = false;
// }

// void Texture::HandleTransitionSpecialCases(CommandRecordingContext* commandContext) {
//     // Externally allocated textures can be written from other graphics queues. Hence, they must
//     be
//     // acquired before command list submission to ensure work from the other queues has finished.
//     // See CommandRecordingContext::ExecuteCommandList.
//     if (mResourceAllocation.GetInfo().mMethod == AllocationMethod::kExternal) {
//         commandContext->AddToSharedTextureList(this);
//     }
// }

// void Texture::TransitionUsageAndGetResourceBarrier(CommandRecordingContext* commandContext,
//                                                    std::vector<D3D11_RESOURCE_BARRIER>* barrier,
//                                                    wgpu::TextureUsage usage,
//                                                    const SubresourceRange& range) {
//     TransitionUsageAndGetResourceBarrier(commandContext, barrier,
//                                          D3D11TextureUsage(usage, GetFormat()), range);
// }

// void Texture::TransitionUsageAndGetResourceBarrier(CommandRecordingContext* commandContext,
//                                                    std::vector<D3D11_RESOURCE_BARRIER>* barriers,
//                                                    D3D11_RESOURCE_STATES newState,
//                                                    const SubresourceRange& range) {
//     HandleTransitionSpecialCases(commandContext);

//     const ExecutionSerial pendingCommandSerial =
//     ToBackend(GetDevice())->GetPendingCommandSerial();

//     mSubresourceStateAndDecay.Update(range, [&](const SubresourceRange& updateRange,
//                                                 StateAndDecay* state) {
//         TransitionSubresourceRange(barriers, updateRange, state, newState, pendingCommandSerial);
//     });
// }

// void Texture::TrackUsageAndGetResourceBarrierForPass(CommandRecordingContext* commandContext,
//                                                      std::vector<D3D11_RESOURCE_BARRIER>*
//                                                      barriers, const TextureSubresourceUsage&
//                                                      textureUsages) {
//     if (mResourceAllocation.GetInfo().mMethod != AllocationMethod::kExternal) {
//         // Track the underlying heap to ensure residency.
//         Heap* heap = ToBackend(mResourceAllocation.GetResourceHeap());
//         commandContext->TrackHeapUsage(heap, GetDevice()->GetPendingCommandSerial());
//     }

//     HandleTransitionSpecialCases(commandContext);

//     const ExecutionSerial pendingCommandSerial =
//     ToBackend(GetDevice())->GetPendingCommandSerial();

//     mSubresourceStateAndDecay.Merge(
//         textureUsages,
//         [&](const SubresourceRange& mergeRange, StateAndDecay* state, wgpu::TextureUsage usage) {
//             // Skip if this subresource is not used during the current pass
//             if (usage == wgpu::TextureUsage::None) {
//                 return;
//             }

//             D3D11_RESOURCE_STATES newState = D3D11TextureUsage(usage, GetFormat());
//             TransitionSubresourceRange(barriers, mergeRange, state, newState,
//             pendingCommandSerial);
//         });
// }

D3D11_RENDER_TARGET_VIEW_DESC Texture::GetRTVDescriptor(const Format& format,
                                                        uint32_t mipLevel,
                                                        uint32_t baseSlice,
                                                        uint32_t sliceCount) const {
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = D3D11TextureFormat(format.format);
    if (IsMultisampledTexture()) {
        ASSERT(GetDimension() == wgpu::TextureDimension::e2D);
        ASSERT(GetNumMipLevels() == 1);
        ASSERT(sliceCount == 1);
        ASSERT(baseSlice == 0);
        ASSERT(mipLevel == 0);
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
        return rtvDesc;
    }
    switch (GetDimension()) {
        case wgpu::TextureDimension::e2D:
            // Currently we always use D3D11_TEX2D_ARRAY_RTV because we cannot specify base
            // array layer and layer count in D3D11_TEX2D_RTV. For 2D texture views, we treat
            // them as 1-layer 2D array textures. (Just like how we treat SRVs)
            // https://docs.microsoft.com/en-us/windows/desktop/api/d3d11/ns-d3d11-d3d11_tex2d_rtv
            // https://docs.microsoft.com/en-us/windows/desktop/api/d3d11/ns-d3d11-d3d11_tex2d_array
            // _rtv
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = mipLevel;
            rtvDesc.Texture2DArray.FirstArraySlice = baseSlice;
            rtvDesc.Texture2DArray.ArraySize = sliceCount;
            break;
        case wgpu::TextureDimension::e3D:
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
            rtvDesc.Texture3D.MipSlice = mipLevel;
            rtvDesc.Texture3D.FirstWSlice = baseSlice;
            rtvDesc.Texture3D.WSize = sliceCount;
            break;
        case wgpu::TextureDimension::e1D:
            UNREACHABLE();
            break;
    }
    return rtvDesc;
}

D3D11_DEPTH_STENCIL_VIEW_DESC Texture::GetDSVDescriptor(uint32_t mipLevel,
                                                        uint32_t baseArrayLayer,
                                                        uint32_t layerCount,
                                                        Aspect aspects,
                                                        bool depthReadOnly,
                                                        bool stencilReadOnly) const {
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;

    dsvDesc.Format = GetD3D11Format();
    dsvDesc.Flags = 0;
    if (depthReadOnly && aspects & Aspect::Depth) {
        dsvDesc.Flags |= D3D11_DSV_READ_ONLY_DEPTH;
    }
    if (stencilReadOnly && aspects & Aspect::Stencil) {
        dsvDesc.Flags |= D3D11_DSV_READ_ONLY_STENCIL;
    }

    if (IsMultisampledTexture()) {
        ASSERT(GetNumMipLevels() == 1);
        ASSERT(layerCount == 1);
        ASSERT(baseArrayLayer == 0);
        ASSERT(mipLevel == 0);
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
    } else {
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.FirstArraySlice = baseArrayLayer;
        dsvDesc.Texture2DArray.ArraySize = layerCount;
        dsvDesc.Texture2DArray.MipSlice = mipLevel;
    }

    return dsvDesc;
}

MaybeError Texture::ClearTexture(CommandRecordingContext* commandContext,
                                 const SubresourceRange& range,
                                 TextureBase::ClearValue clearValue) {
    // ID3D11GraphicsCommandList* commandList = commandContext->GetCommandList();

    // Device* device = ToBackend(GetDevice());

    // uint8_t clearColor = (clearValue == TextureBase::ClearValue::Zero) ? 0 : 1;
    // float fClearColor = (clearValue == TextureBase::ClearValue::Zero) ? 0.f : 1.f;

    // if ((mD3D11ResourceFlags & D3D11_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0) {
    //     TrackUsageAndTransitionNow(commandContext, D3D11_RESOURCE_STATE_DEPTH_WRITE, range);

    //     for (uint32_t level = range.baseMipLevel; level < range.baseMipLevel + range.levelCount;
    //          ++level) {
    //         for (uint32_t layer = range.baseArrayLayer;
    //              layer < range.baseArrayLayer + range.layerCount; ++layer) {
    //             // Iterate the aspects individually to determine which clear flags to use.
    //             D3D11_CLEAR_FLAGS clearFlags = {};
    //             for (Aspect aspect : IterateEnumMask(range.aspects)) {
    //                 if (clearValue == TextureBase::ClearValue::Zero &&
    //                     IsSubresourceContentInitialized(
    //                         SubresourceRange::SingleMipAndLayer(level, layer, aspect))) {
    //                     // Skip lazy clears if already initialized.
    //                     continue;
    //                 }

    //                 switch (aspect) {
    //                     case Aspect::Depth:
    //                         clearFlags |= D3D11_CLEAR_FLAG_DEPTH;
    //                         break;
    //                     case Aspect::Stencil:
    //                         clearFlags |= D3D11_CLEAR_FLAG_STENCIL;
    //                         break;
    //                     default:
    //                         UNREACHABLE();
    //                 }
    //             }

    //             if (clearFlags == 0) {
    //                 continue;
    //             }

    //             CPUDescriptorHeapAllocation dsvHandle;
    //             DAWN_TRY_ASSIGN(
    //                 dsvHandle,
    //                 device->GetDepthStencilViewAllocator()->AllocateTransientCPUDescriptors());
    //             const D3D11_CPU_DESCRIPTOR_HANDLE baseDescriptor = dsvHandle.GetBaseDescriptor();
    //             D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc =
    //                 GetDSVDescriptor(level, layer, 1, range.aspects, false, false);
    //             device->GetD3D11Device()->CreateDepthStencilView(GetD3D11Resource(), &dsvDesc,
    //                                                              baseDescriptor);

    //             commandList->ClearDepthStencilView(baseDescriptor, clearFlags, fClearColor,
    //                                                clearColor, 0, nullptr);
    //         }
    //     }
    // } else if ((mD3D11ResourceFlags & D3D11_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0) {
    //     TrackUsageAndTransitionNow(commandContext, D3D11_RESOURCE_STATE_RENDER_TARGET, range);

    //     const float clearColorRGBA[4] = {fClearColor, fClearColor, fClearColor, fClearColor};

    //     ASSERT(range.aspects == Aspect::Color);
    //     for (uint32_t level = range.baseMipLevel; level < range.baseMipLevel + range.levelCount;
    //          ++level) {
    //         for (uint32_t layer = range.baseArrayLayer;
    //              layer < range.baseArrayLayer + range.layerCount; ++layer) {
    //             if (clearValue == TextureBase::ClearValue::Zero &&
    //                 IsSubresourceContentInitialized(
    //                     SubresourceRange::SingleMipAndLayer(level, layer, Aspect::Color))) {
    //                 // Skip lazy clears if already initialized.
    //                 continue;
    //             }

    //             CPUDescriptorHeapAllocation rtvHeap;
    //             DAWN_TRY_ASSIGN(
    //                 rtvHeap,
    //                 device->GetRenderTargetViewAllocator()->AllocateTransientCPUDescriptors());
    //             const D3D11_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap.GetBaseDescriptor();

    //             uint32_t baseSlice = layer;
    //             uint32_t sliceCount = 1;
    //             if (GetDimension() == wgpu::TextureDimension::e3D) {
    //                 baseSlice = 0;
    //                 sliceCount = std::max(GetDepth() >> level, 1u);
    //             }
    //             D3D11_RENDER_TARGET_VIEW_DESC rtvDesc =
    //                 GetRTVDescriptor(GetFormat(), level, baseSlice, sliceCount);
    //             device->GetD3D11Device()->CreateRenderTargetView(GetD3D11Resource(), &rtvDesc,
    //                                                              rtvHandle);
    //             commandList->ClearRenderTargetView(rtvHandle, clearColorRGBA, 0, nullptr);
    //         }
    //     }
    // } else {
    //     ASSERT(!IsMultisampledTexture());

    //     // create temp buffer with clear color to copy to the texture image
    //     TrackUsageAndTransitionNow(commandContext, D3D11_RESOURCE_STATE_COPY_DEST, range);

    //     for (Aspect aspect : IterateEnumMask(range.aspects)) {
    //         const TexelBlockInfo& blockInfo = GetFormat().GetAspectInfo(aspect).block;

    //         Extent3D largestMipSize =
    //         GetMipLevelSingleSubresourcePhysicalSize(range.baseMipLevel);

    //         uint32_t bytesPerRow =
    //             Align((largestMipSize.width / blockInfo.width) * blockInfo.byteSize,
    //                   kTextureBytesPerRowAlignment);
    //         uint64_t bufferSize = bytesPerRow * (largestMipSize.height / blockInfo.height) *
    //                               largestMipSize.depthOrArrayLayers;
    //         DynamicUploader* uploader = device->GetDynamicUploader();
    //         UploadHandle uploadHandle;
    //         DAWN_TRY_ASSIGN(uploadHandle,
    //                         uploader->Allocate(bufferSize, device->GetPendingCommandSerial(),
    //                                            blockInfo.byteSize));
    //         memset(uploadHandle.mappedBuffer, clearColor, bufferSize);

    //         for (uint32_t level = range.baseMipLevel; level < range.baseMipLevel +
    //         range.levelCount;
    //              ++level) {
    //             // compute d3d11 texture copy locations for texture and buffer
    //             Extent3D copySize = GetMipLevelSingleSubresourcePhysicalSize(level);

    //             for (uint32_t layer = range.baseArrayLayer;
    //                  layer < range.baseArrayLayer + range.layerCount; ++layer) {
    //                 if (clearValue == TextureBase::ClearValue::Zero &&
    //                     IsSubresourceContentInitialized(
    //                         SubresourceRange::SingleMipAndLayer(level, layer, aspect))) {
    //                     // Skip lazy clears if already initialized.
    //                     continue;
    //                 }

    //                 TextureCopy textureCopy;
    //                 textureCopy.texture = this;
    //                 textureCopy.origin = {0, 0, layer};
    //                 textureCopy.mipLevel = level;
    //                 textureCopy.aspect = aspect;
    //                 RecordBufferTextureCopyWithBufferHandle(
    //                     BufferTextureCopyDirection::B2T, commandList,
    //                     ToBackend(uploadHandle.stagingBuffer)->GetD3D11Resource(),
    //                     uploadHandle.startOffset, bytesPerRow, largestMipSize.height,
    //                     textureCopy, copySize);
    //             }
    //         }
    //     }
    // }
    // if (clearValue == TextureBase::ClearValue::Zero) {
    //     SetIsSubresourceContentInitialized(true, range);
    //     GetDevice()->IncrementLazyClearCountForTesting();
    // }
    return {};
}

void Texture::SetLabelHelper(const char* prefix) {
    std::string name(prefix);
    name += " " + GetLabel();
    mResource->SetPrivateData(WKPDID_D3DDebugObjectName, name.size(), name.c_str());
}

void Texture::SetLabelImpl() {
    SetLabelHelper("Dawn_InternalTexture");
}

MaybeError Texture::EnsureSubresourceContentInitialized(CommandRecordingContext* commandContext,
                                                        const SubresourceRange& range) {
    if (!ToBackend(GetDevice())->IsToggleEnabled(Toggle::LazyClearResourceOnFirstUse)) {
        return {};
    }
    if (!IsSubresourceContentInitialized(range)) {
        // If subresource has not been initialized, clear it to black as it could contain
        // dirty bits from recycled memory
        DAWN_TRY(ClearTexture(commandContext, range, TextureBase::ClearValue::Zero));
    }
    return {};
}

// bool Texture::StateAndDecay::operator==(const Texture::StateAndDecay& other) const {
//     return lastState == other.lastState && lastDecaySerial == other.lastDecaySerial &&
//            isValidToDecay == other.isValidToDecay;
// }

// static
Ref<TextureView> TextureView::Create(TextureBase* texture,
                                     const TextureViewDescriptor* descriptor) {
    return AcquireRef(new TextureView(texture, descriptor));
}

TextureView::TextureView(TextureBase* texture, const TextureViewDescriptor* descriptor)
    : TextureViewBase(texture, descriptor) {
    mSrvDesc.Format = D3D11TextureFormat(descriptor->format);

    const Format& textureFormat = texture->GetFormat();
    if (textureFormat.HasDepthOrStencil()) {
        // Configure the SRV descriptor to reinterpret the texture allocated as
        // TYPELESS as a single-plane shader-accessible view.
        switch (textureFormat.format) {
            case wgpu::TextureFormat::Depth32Float:
            case wgpu::TextureFormat::Depth24Plus:
                mSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
                break;
            case wgpu::TextureFormat::Depth16Unorm:
                mSrvDesc.Format = DXGI_FORMAT_R16_UNORM;
                break;
            case wgpu::TextureFormat::Stencil8: {
                Aspect aspects = SelectFormatAspects(textureFormat, descriptor->aspect);
                ASSERT(aspects != Aspect::None);
                if (!HasZeroOrOneBits(aspects)) {
                    // A single aspect is not selected. The texture view must not be
                    // sampled.
                    mSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
                    break;
                }
                switch (aspects) {
                    case Aspect::Depth:
                        mSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                        break;
                    case Aspect::Stencil:
                        mSrvDesc.Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
                break;
            }
            case wgpu::TextureFormat::Depth24PlusStencil8:
            case wgpu::TextureFormat::Depth32FloatStencil8: {
                Aspect aspects = SelectFormatAspects(textureFormat, descriptor->aspect);
                ASSERT(aspects != Aspect::None);
                if (!HasZeroOrOneBits(aspects)) {
                    // A single aspect is not selected. The texture view must not be
                    // sampled.
                    mSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
                    break;
                }
                switch (aspects) {
                    case Aspect::Depth:
                        mSrvDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
                        break;
                    case Aspect::Stencil:
                        mSrvDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
                break;
            }
            default:
                UNREACHABLE();
                break;
        }
    }

    // Per plane view formats must have the plane slice number be the index of the plane in the
    // array of textures.
    if (texture->GetFormat().IsMultiPlanar()) {
        const Aspect planeAspect = ConvertViewAspect(GetFormat(), descriptor->aspect);
        mSrvDesc.Format =
            D3D11TextureFormat(texture->GetFormat().GetAspectInfo(planeAspect).format);
    }

    // Currently we always use D3D11_TEX2D_ARRAY_SRV because we cannot specify base array layer
    // and layer count in D3D11_TEX2D_SRV. For 2D texture views, we treat them as 1-layer 2D
    // array textures.
    // Multisampled textures may only be one array layer, so we use
    // D3D11_SRV_DIMENSION_TEXTURE2DMS.
    // https://docs.microsoft.com/en-us/windows/desktop/api/d3d11/ns-d3d11-d3d11_tex2d_srv
    // https://docs.microsoft.com/en-us/windows/desktop/api/d3d11/ns-d3d11-d3d11_tex2d_array_srv
    if (GetTexture()->IsMultisampledTexture()) {
        switch (descriptor->dimension) {
            case wgpu::TextureViewDimension::e2DArray:
                ASSERT(texture->GetArrayLayers() == 1);
                [[fallthrough]];
            case wgpu::TextureViewDimension::e2D:
                ASSERT(texture->GetDimension() == wgpu::TextureDimension::e2D);
                mSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
                break;

            default:
                UNREACHABLE();
        }
    } else {
        switch (descriptor->dimension) {
            case wgpu::TextureViewDimension::e1D:
                mSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                mSrvDesc.Texture1D.MipLevels = descriptor->mipLevelCount;
                mSrvDesc.Texture1D.MostDetailedMip = descriptor->baseMipLevel;
                break;

            case wgpu::TextureViewDimension::e2D:
            case wgpu::TextureViewDimension::e2DArray:
                ASSERT(texture->GetDimension() == wgpu::TextureDimension::e2D);
                mSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                mSrvDesc.Texture2DArray.ArraySize = descriptor->arrayLayerCount;
                mSrvDesc.Texture2DArray.FirstArraySlice = descriptor->baseArrayLayer;
                mSrvDesc.Texture2DArray.MipLevels = descriptor->mipLevelCount;
                mSrvDesc.Texture2DArray.MostDetailedMip = descriptor->baseMipLevel;
                break;
            case wgpu::TextureViewDimension::Cube:
            case wgpu::TextureViewDimension::CubeArray:
                ASSERT(texture->GetDimension() == wgpu::TextureDimension::e2D);
                ASSERT(descriptor->arrayLayerCount % 6 == 0);
                mSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                mSrvDesc.TextureCubeArray.First2DArrayFace = descriptor->baseArrayLayer;
                mSrvDesc.TextureCubeArray.NumCubes = descriptor->arrayLayerCount / 6;
                mSrvDesc.TextureCubeArray.MostDetailedMip = descriptor->baseMipLevel;
                mSrvDesc.TextureCubeArray.MipLevels = descriptor->mipLevelCount;
                break;
            case wgpu::TextureViewDimension::e3D:
                ASSERT(texture->GetDimension() == wgpu::TextureDimension::e3D);
                mSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                mSrvDesc.Texture3D.MostDetailedMip = descriptor->baseMipLevel;
                mSrvDesc.Texture3D.MipLevels = descriptor->mipLevelCount;
                break;

            case wgpu::TextureViewDimension::Undefined:
                UNREACHABLE();
        }
    }
}

DXGI_FORMAT TextureView::GetD3D11Format() const {
    return D3D11TextureFormat(GetFormat().format);
}

const D3D11_SHADER_RESOURCE_VIEW_DESC& TextureView::GetSRVDescriptor() const {
    ASSERT(mSrvDesc.Format != DXGI_FORMAT_UNKNOWN);
    return mSrvDesc;
}

D3D11_RENDER_TARGET_VIEW_DESC TextureView::GetRTVDescriptor() const {
    return ToBackend(GetTexture())
        ->GetRTVDescriptor(GetFormat(), GetBaseMipLevel(), GetBaseArrayLayer(), GetLayerCount());
}

D3D11_DEPTH_STENCIL_VIEW_DESC TextureView::GetDSVDescriptor(bool depthReadOnly,
                                                            bool stencilReadOnly) const {
    ASSERT(GetLevelCount() == 1);
    return ToBackend(GetTexture())
        ->GetDSVDescriptor(GetBaseMipLevel(), GetBaseArrayLayer(), GetLayerCount(), GetAspects(),
                           depthReadOnly, stencilReadOnly);
}

D3D11_UNORDERED_ACCESS_VIEW_DESC TextureView::GetUAVDescriptor() const {
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    uavDesc.Format = GetD3D11Format();

    ASSERT(!GetTexture()->IsMultisampledTexture());
    switch (GetDimension()) {
        case wgpu::TextureViewDimension::e1D:
            uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
            uavDesc.Texture1D.MipSlice = GetBaseMipLevel();
            break;
        case wgpu::TextureViewDimension::e2D:
        case wgpu::TextureViewDimension::e2DArray:
            uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.FirstArraySlice = GetBaseArrayLayer();
            uavDesc.Texture2DArray.ArraySize = GetLayerCount();
            uavDesc.Texture2DArray.MipSlice = GetBaseMipLevel();
            break;
        case wgpu::TextureViewDimension::e3D:
            uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
            uavDesc.Texture3D.FirstWSlice = 0;
            uavDesc.Texture3D.WSize = GetTexture()->GetDepth() >> GetBaseMipLevel();
            uavDesc.Texture3D.MipSlice = GetBaseMipLevel();
            break;
        // Cube and Cubemap can't be used as storage texture. So there is no need to create UAV
        // descriptor for them.
        case wgpu::TextureViewDimension::Cube:
        case wgpu::TextureViewDimension::CubeArray:
        case wgpu::TextureViewDimension::Undefined:
            UNREACHABLE();
    }
    return uavDesc;
}

}  // namespace dawn::native::d3d11
