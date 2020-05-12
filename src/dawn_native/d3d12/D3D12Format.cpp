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

#include "dawn_native/opengl/D3D12Format.h"

namespace dawn_native { namespace d3d12 {

    D3D12FormatTable BuildD3D12FormatTable() {
        D3D12FormatTable table;

        // After C++17, this can be a constexpr lambda which populates the table.
        auto AddFormat = [&table](wgpu::TextureFormat dawnFormat, DXGI_FORMAT baseFormat,
                                  DXGI_FORMAT format, DXGI_FORMAT srvFormat) {
            size_t index = ComputeFormatIndex(dawnFormat);
            ASSERT(index < table.size());

            table[index].baseFormat = baseFormat;
            table[index].format = format;
            table[index].srvFormat = srvFormat;
        };

        // clang-format off

        AddFormat(wgpu::TextureFormat::R8Unorm,             DXGI_FORMAT_R8_TYPELESS,                DXGI_FORMAT_R8_UNORM,               DXGI_FORMAT_R8_UNORM);
        AddFormat(wgpu::TextureFormat::R8Snorm,             DXGI_FORMAT_R8_TYPELESS,                DXGI_FORMAT_R8_SNORM,               DXGI_FORMAT_R8_SNORM);
        AddFormat(wgpu::TextureFormat::R8Uint,              DXGI_FORMAT_R8_TYPELESS,                DXGI_FORMAT_R8_UINT,                DXGI_FORMAT_R8_UINT);
        AddFormat(wgpu::TextureFormat::R8Sint,              DXGI_FORMAT_R8_TYPELESS,                DXGI_FORMAT_R8_SINT,                DXGI_FORMAT_R8_SINT);

        AddFormat(wgpu::TextureFormat::R16Uint,             DXGI_FORMAT_R16_TYPELESS,               DXGI_FORMAT_R16_UINT,               DXGI_FORMAT_R16_UINT);
        AddFormat(wgpu::TextureFormat::R16Sint,             DXGI_FORMAT_R16_TYPELESS,               DXGI_FORMAT_R16_SINT,               DXGI_FORMAT_R16_SINT);
        AddFormat(wgpu::TextureFormat::R16Float,            DXGI_FORMAT_R16_TYPELESS,               DXGI_FORMAT_R16_FLOAT,              DXGI_FORMAT_R16_FLOAT);

        AddFormat(wgpu::TextureFormat::RG8Unorm,            DXGI_FORMAT_R8G8_TYPELESS,              DXGI_FORMAT_R8G8_UNORM,             DXGI_FORMAT_R8G8_UNORM);
        AddFormat(wgpu::TextureFormat::RG8Snorm,            DXGI_FORMAT_R8G8_TYPELESS,              DXGI_FORMAT_R8G8_SNORM,             DXGI_FORMAT_R8G8_SNORM);
        AddFormat(wgpu::TextureFormat::RG8Uint,             DXGI_FORMAT_R8G8_TYPELESS,              DXGI_FORMAT_R8G8_UINT,              DXGI_FORMAT_R8G8_UINT);
        AddFormat(wgpu::TextureFormat::RG8Sint,             DXGI_FORMAT_R8G8_TYPELESS,              DXGI_FORMAT_R8G8_SINT,              DXGI_FORMAT_R8G8_SINT);

        AddFormat(wgpu::TextureFormat::R32Uint,             DXGI_FORMAT_R32_TYPELESS,               DXGI_FORMAT_R32_UINT,               DXGI_FORMAT_R32_UINT);
        AddFormat(wgpu::TextureFormat::R32Sint,             DXGI_FORMAT_R32_TYPELESS,               DXGI_FORMAT_R32_SINT,               DXGI_FORMAT_R32_SINT);
        AddFormat(wgpu::TextureFormat::R32Float,            DXGI_FORMAT_R32_TYPELESS,               DXGI_FORMAT_R32_FLOAT,              DXGI_FORMAT_R32_FLOAT);

        AddFormat(wgpu::TextureFormat::RG16Uint,            DXGI_FORMAT_R16G16_TYPELESS,            DXGI_FORMAT_R16G16_UINT,            DXGI_FORMAT_R16G16_UINT);
        AddFormat(wgpu::TextureFormat::RG16Sint,            DXGI_FORMAT_R16G16_TYPELESS,            DXGI_FORMAT_R16G16_SINT,            DXGI_FORMAT_R16G16_SINT);
        AddFormat(wgpu::TextureFormat::RG16Float,           DXGI_FORMAT_R16G16_TYPELESS,            DXGI_FORMAT_R16G16_FLOAT,           DXGI_FORMAT_R16G16_FLOAT);

        AddFormat(wgpu::TextureFormat::RGBA8Unorm,          DXGI_FORMAT_B8G8R8A8_TYPELESS,          DXGI_FORMAT_R8G8B8A8_UNORM,         DXGI_FORMAT_R8G8B8A8_UNORM);
        AddFormat(wgpu::TextureFormat::RGBA8UnormSrgb,      DXGI_FORMAT_B8G8R8A8_TYPELESS,          DXGI_FORMAT_R8G8B8A8_UNORM,         DXGI_FORMAT_R8G8B8A8_UNORM);
        AddFormat(wgpu::TextureFormat::RGBA8Snorm,          DXGI_FORMAT_B8G8R8A8_TYPELESS,          DXGI_FORMAT_R8G8B8A8_SNORM,         DXGI_FORMAT_R8G8B8A8_SNORM);
        AddFormat(wgpu::TextureFormat::RGBA8Uint,           DXGI_FORMAT_B8G8R8A8_TYPELESS,          DXGI_FORMAT_R8G8B8A8_UINT,          DXGI_FORMAT_R8G8B8A8_UINT);
        AddFormat(wgpu::TextureFormat::RGBA8Sint,           DXGI_FORMAT_B8G8R8A8_TYPELESS,          DXGI_FORMAT_R8G8B8A8_SINT,          DXGI_FORMAT_R8G8B8A8_SINT);
        AddFormat(wgpu::TextureFormat::BGRA8Unorm,          DXGI_FORMAT_B8G8R8A8_TYPELESS,          DXGI_FORMAT_B8G8R8A8_UNORM,         DXGI_FORMAT_B8G8R8A8_UNORM);
        AddFormat(wgpu::TextureFormat::BGRA8UnormSrgb,      DXGI_FORMAT_B8G8R8A8_TYPELESS,          DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);

        AddFormat(wgpu::TextureFormat::RGB10A2Unorm,        DXGI_FORMAT_R10G10B10A2_TYPELESS,       DXGI_FORMAT_R10G10B10A2_UNORM,      DXGI_FORMAT_R10G10B10A2_UNORM);

        AddFormat(wgpu::TextureFormat::RG11B10Float,        DXGI_FORMAT_R11G11B10_FLOAT,            DXGI_FORMAT_R11G11B10_FLOAT,        DXGI_FORMAT_R11G11B10_FLOAT);

        AddFormat(wgpu::TextureFormat::RG32Uint,            DXGI_FORMAT_R32G32_TYPELESS,            DXGI_FORMAT_R32G32_UINT,            DXGI_FORMAT_R32G32_UINT);
        AddFormat(wgpu::TextureFormat::RG32Sint,            DXGI_FORMAT_R32G32_TYPELESS,            DXGI_FORMAT_R32G32_SINT,            DXGI_FORMAT_R32G32_SINT);
        AddFormat(wgpu::TextureFormat::RG32Float,           DXGI_FORMAT_R32G32_TYPELESS,            DXGI_FORMAT_R32G32_FLOAT,           DXGI_FORMAT_R32G32_FLOAT);

        AddFormat(wgpu::TextureFormat::RGBA16Uint,          DXGI_FORMAT_R16G16B16A16_TYPELESS,      DXGI_FORMAT_R16G16B16A16_UINT,      DXGI_FORMAT_R16G16B16A16_UINT);
        AddFormat(wgpu::TextureFormat::RGBA16Sint,          DXGI_FORMAT_R16G16B16A16_TYPELESS,      DXGI_FORMAT_R16G16B16A16_SINT,      DXGI_FORMAT_R16G16B16A16_SINT);
        AddFormat(wgpu::TextureFormat::RGBA16Float,         DXGI_FORMAT_R16G16B16A16_TYPELESS,      DXGI_FORMAT_R16G16B16A16_FLOAT,     DXGI_FORMAT_R16G16B16A16_FLOAT);

        AddFormat(wgpu::TextureFormat::RGBA32Uint,          DXGI_FORMAT_R32G32B32A32_TYPELESS,      DXGI_FORMAT_R32G32B32A32_UINT,      DXGI_FORMAT_R32G32B32A32_UINT);
        AddFormat(wgpu::TextureFormat::RGBA32Sint,          DXGI_FORMAT_R32G32B32A32_TYPELESS,      DXGI_FORMAT_R32G32B32A32_SINT,      DXGI_FORMAT_R32G32B32A32_SINT);
        AddFormat(wgpu::TextureFormat::RGBA32Float,         DXGI_FORMAT_R32G32B32A32_TYPELESS,      DXGI_FORMAT_R32G32B32A32_FLOAT,     DXGI_FORMAT_R32G32B32A32_FLOAT);

        AddFormat(wgpu::TextureFormat::Depth32Float,        DXGI_FORMAT_R32_TYPELESS,               DXGI_FORMAT_D32_FLOAT,              DXGI_FORMAT_R32_FLOAT);
        AddFormat(wgpu::TextureFormat::Depth24Plus,         DXGI_FORMAT_R32_TYPELESS,               DXGI_FORMAT_D32_FLOAT,              DXGI_FORMAT_R32_FLOAT);
        AddFormat(wgpu::TextureFormat::Depth24PlusStencil8, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,   DXGI_FORMAT_D32_FLOAT_S8X24_UINT,   DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

        AddFormat(wgpu::TextureFormat::BC1RGBAUnorm,        DXGI_FORMAT_BC1_TYPELESS,               DXGI_FORMAT_BC1_UNORM,              DXGI_FORMAT_BC1_UNORM);
        AddFormat(wgpu::TextureFormat::BC1RGBAUnormSrgb,    DXGI_FORMAT_BC1_TYPELESS,               DXGI_FORMAT_BC1_UNORM_SRGB,         DXGI_FORMAT_BC1_UNORM_SRGB);

        AddFormat(wgpu::TextureFormat::BC2RGBAUnorm,        DXGI_FORMAT_BC2_TYPELESS,               DXGI_FORMAT_BC2_UNORM,              DXGI_FORMAT_BC2_UNORM);
        AddFormat(wgpu::TextureFormat::BC2RGBAUnormSrgb,    DXGI_FORMAT_BC2_TYPELESS,               DXGI_FORMAT_BC2_UNORM_SRGB,         DXGI_FORMAT_BC2_UNORM_SRGB);

        AddFormat(wgpu::TextureFormat::BC3RGBAUnorm,        DXGI_FORMAT_BC3_TYPELESS,               DXGI_FORMAT_BC3_UNORM,              DXGI_FORMAT_BC3_UNORM);
        AddFormat(wgpu::TextureFormat::BC3RGBAUnormSrgb,    DXGI_FORMAT_BC3_TYPELESS,               DXGI_FORMAT_BC3_UNORM_SRGB,         DXGI_FORMAT_BC3_UNORM_SRGB);

        AddFormat(wgpu::TextureFormat::BC4RSnorm,           DXGI_FORMAT_BC4_TYPELESS,               DXGI_FORMAT_BC4_SNORM,              DXGI_FORMAT_BC4_SNORM);
        AddFormat(wgpu::TextureFormat::BC4RUnorm,           DXGI_FORMAT_BC4_TYPELESS,               DXGI_FORMAT_BC4_UNORM,              DXGI_FORMAT_BC4_UNORM);

        AddFormat(wgpu::TextureFormat::BC5RGSnorm,          DXGI_FORMAT_BC5_TYPELESS,               DXGI_FORMAT_BC5_SNORM,              DXGI_FORMAT_BC5_SNORM);
        AddFormat(wgpu::TextureFormat::BC5RGUnorm,          DXGI_FORMAT_BC5_TYPELESS,               DXGI_FORMAT_BC5_UNORM,              DXGI_FORMAT_BC5_UNORM);

        AddFormat(wgpu::TextureFormat::BC6HRGBSfloat,       DXGI_FORMAT_BC6H_TYPELESS,              DXGI_FORMAT_BC6H_SF16,              DXGI_FORMAT_BC6H_SF16);
        AddFormat(wgpu::TextureFormat::BC6HRGBUfloat,       DXGI_FORMAT_BC6H_TYPELESS,              DXGI_FORMAT_BC6H_UF16,              DXGI_FORMAT_BC6H_UF16);

        AddFormat(wgpu::TextureFormat::BC7RGBAUnorm,        DXGI_FORMAT_BC7_TYPELESS,               DXGI_FORMAT_BC7_UNORM,              DXGI_FORMAT_BC7_UNORM);
        AddFormat(wgpu::TextureFormat::BC7RGBAUnormSrgb,    DXGI_FORMAT_BC7_TYPELESS,               DXGI_FORMAT_BC7_UNORM_SRGB,         DXGI_FORMAT_BC7_UNORM_SRGB);

        // clang-format on

        return table;
    }

}};  // namespace dawn_native::d3d12
