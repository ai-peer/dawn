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

#include "dawn_native/CopyTextureForBrowserHelper.h"

#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Device.h"
#include "dawn_native/InternalPipelineStore.h"
#include "dawn_native/Queue.h"
#include "dawn_native/RenderPassEncoder.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/Sampler.h"
#include "dawn_native/Texture.h"
#include "dawn_native/ValidationUtils_autogen.h"
#include "dawn_native/utils/WGPUHelpers.h"

#include <unordered_set>

namespace dawn_native {
    namespace {

        static const char sCopyTextureForBrowserShader[] = R"(
            [[block]] struct Uniforms {                 // offset   align   size
                u_scale: vec2<f32>;                     // 0        8       8
                u_offset: vec2<f32>;                    // 8        8       8
                u_steps_mask: u32;                      // 16       4       4  
                // implicit padding;                    // 20               12
                u_conversionMatrix: mat3x3<f32>;        // 32       16      48
                u_gamma_decoding_gamma: vec4<f32>;      // 80       16      16      
                u_gamma_decoding_linear: vec3<f32>;     // 96       16      12
                u_src_color_space_is_extended: u32;     // 108      4       4
                u_gamma_encoding_gamma: vec4<f32>;      // 112      16      16
                u_gamma_encoding_linear: vec3<f32>;     // 128      16      12
                u_dst_color_space_is_extended: u32;     // 140      4       4
            };

            [[binding(0), group(0)]] var<uniform> uniforms : Uniforms;

            struct VertexOutputs {
                [[location(0)]] texcoords : vec2<f32>;
                [[builtin(position)]] position : vec4<f32>;
            };

            // Chromium uses unified equation to construct gamma decoding function
            // and gamma encoding function.
            // The logic is:
            //  if x < epsilon
            //      linear = C * x + F
            //  nonlinear = pow(A * x + B, G) + E
            // (https://source.chromium.org/chromium/chromium/src/+/main:ui/gfx/color_transform.cc;l=541)
            // Expand the equation with sign() to make it handle all gamma conversions.
            fn gamma_conversion(v: vec3<f32>,
                                linearParams: vec3<f32>,
                                gammaParams: vec4<f32>, colorSpaceIsExtended: u32) -> vec3<f32> {
                let signFactor = select(vec3<f32>(1.0), sign(v), colorSpaceIsExtended > 0u);
                let value = select(v, abs(v), colorSpaceIsExtended > 0u);

                // Linear part: C * x + F
                let C = linearParams.y;
                let F = linearParams.z;
                let linear_part = signFactor * (vec3<f32>(C) * value + vec3<f32>(F));

                // Gamma part: pow(A * x + B, G) + E
                let A = gammaParams.x;
                let B = gammaParams.y;
                let G = gammaParams.z;
                let E = gammaParams.w;
                let gamma_part = signFactor * (pow(vec3<f32>(A) * value + vec3<f32>(B),
                                                  vec3<f32>(G)) + vec3<f32>(E));

                let epsilon = linearParams.x;
                return select(gamma_part, linear_part, value < vec3<f32>(epsilon));
            }

            [[stage(vertex)]]
            fn vs_main(
                [[builtin(vertex_index)]] VertexIndex : u32
            ) -> VertexOutputs {
                var texcoord = array<vec2<f32>, 3>(
                    vec2<f32>(-0.5, 0.0),
                    vec2<f32>( 1.5, 0.0),
                    vec2<f32>( 0.5, 2.0));

                var output : VertexOutputs;
                output.position = vec4<f32>((texcoord[VertexIndex] * 2.0 - vec2<f32>(1.0, 1.0)), 0.0, 1.0);

                // Y component of scale is calculated by the copySizeHeight / textureHeight. Only
                // flipY case can get negative number.
                var flipY = uniforms.u_scale.y < 0.0;

                // Texture coordinate takes top-left as origin point. We need to map the
                // texture to triangle carefully.
                if (flipY) {
                    // We need to get the mirror positions(mirrored based on y = 0.5) on flip cases.
                    // Adopt transform to src texture and then mapping it to triangle coord which
                    // do a +1 shift on Y dimension will help us got that mirror position perfectly.
                    output.texcoords = (texcoord[VertexIndex] * uniforms.u_scale + uniforms.u_offset) *
                        vec2<f32>(1.0, -1.0) + vec2<f32>(0.0, 1.0);
                } else {
                    // For the normal case, we need to get the exact position.
                    // So mapping texture to triangle firstly then adopt the transform.
                    output.texcoords = (texcoord[VertexIndex] *
                        vec2<f32>(1.0, -1.0) + vec2<f32>(0.0, 1.0)) *
                        uniforms.u_scale + uniforms.u_offset;
                }

                return output;
            }

            [[binding(1), group(0)]] var mySampler: sampler;
            [[binding(2), group(0)]] var myTexture: texture_2d<f32>;

            [[stage(fragment)]]
            fn fs_main(
                [[location(0)]] texcoord : vec2<f32>
            ) -> [[location(0)]] vec4<f32> {
                // Clamp the texcoord and discard the out-of-bound pixels.
                var clampedTexcoord =
                    clamp(texcoord, vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0));
                if (!all(clampedTexcoord == texcoord)) {
                    discard;
                }

                // Swizzling of texture formats when sampling / rendering is handled by the
                // hardware so we don't need special logic in this shader. This is covered by tests.
                var srcColor = textureSample(myTexture, mySampler, texcoord);

                // Unpremultiply step. Appling color space conversion op on premultiplied source texture
                // also needs to unpremultiply first.
                if (bool(uniforms.u_steps_mask & 0x01u)) {
                    if (srcColor.a != 0.0) {
                        srcColor = vec4<f32>(srcColor.rgb / srcColor.a, srcColor.a);
                    }
                }

                // Linearize the source color using the source color space’s
                // transfer function if it is non-linear.
                if (bool(uniforms.u_steps_mask & 0x02u)) {
                    srcColor = vec4<f32>(gamma_conversion(srcColor.rgb,
                                                          uniforms.u_gamma_decoding_linear,
                                                          uniforms.u_gamma_decoding_gamma,
                                                          uniforms.u_src_color_space_is_extended),
                                         srcColor.a);
                }

                // Convert unpremultiplied, linear source colors to the destination gamut by
                // multiplying by a 3x3 matrix. Calculate transformFromXYZD50 * transformToXYZD50
                // in CPU side and upload the final result in uniforms.
                if (bool(uniforms.u_steps_mask & 0x04u)) {
                    srcColor = vec4<f32>(uniforms.u_conversionMatrix * srcColor.rgb, srcColor.a);
                }

                // Encode that color using the inverse of the destination color
                // space’s transfer function if it is non-linear.
                if (bool(uniforms.u_steps_mask & 0x08u)) {
                    srcColor = vec4<f32>(gamma_conversion(srcColor.rgb,
                                                          uniforms.u_gamma_encoding_linear,
                                                          uniforms.u_gamma_encoding_gamma,
                                                          uniforms.u_dst_color_space_is_extended),
                                         srcColor.a);
                }

                // Premultiply step.
                if (bool(uniforms.u_steps_mask & 0x10u)) {
                    srcColor = vec4<f32>(srcColor.rgb * srcColor.a, srcColor.a);
                }

                return srcColor;
            }
        )";

        struct Uniform {
            float scaleX;
            float scaleY;
            float offsetX;
            float offsetY;
            uint32_t stepsMask = 0;
            const std::array<uint32_t, 3> padding = {};  // 12 bytes padding
            std::array<float, 12> conversionMatrix = {};
            std::array<float, 7> gammaDecodingParams = {};  // vec4 gamma_part, vec3 linear_part
            uint32_t srcColorSpaceIsExtended = 0;
            std::array<float, 7> gammaEncodingParams = {};
            uint32_t dstColorSpaceIsExtended = 0;
        };
        static_assert(sizeof(Uniform) == 144, "");

        // TODO(crbug.com/dawn/856): Expand copyTextureForBrowser to support any
        // non-depth, non-stencil, non-compressed texture format pair copy. Now this API
        // supports CopyImageBitmapToTexture normal format pairs.
        MaybeError ValidateCopyTextureFormatConversion(const wgpu::TextureFormat srcFormat,
                                                       const wgpu::TextureFormat dstFormat) {
            switch (srcFormat) {
                case wgpu::TextureFormat::BGRA8Unorm:
                case wgpu::TextureFormat::RGBA8Unorm:
                    break;
                default:
                    return DAWN_FORMAT_VALIDATION_ERROR(
                        "Source texture format (%s) is not supported.", srcFormat);
            }

            switch (dstFormat) {
                case wgpu::TextureFormat::R8Unorm:
                case wgpu::TextureFormat::R16Float:
                case wgpu::TextureFormat::R32Float:
                case wgpu::TextureFormat::RG8Unorm:
                case wgpu::TextureFormat::RG16Float:
                case wgpu::TextureFormat::RG32Float:
                case wgpu::TextureFormat::RGBA8Unorm:
                case wgpu::TextureFormat::BGRA8Unorm:
                case wgpu::TextureFormat::RGB10A2Unorm:
                case wgpu::TextureFormat::RGBA16Float:
                case wgpu::TextureFormat::RGBA32Float:
                    break;
                default:
                    return DAWN_FORMAT_VALIDATION_ERROR(
                        "Destination texture format (%s) is not supported.", dstFormat);
            }

            return {};
        }

        MaybeError ValidateCopyTextureColorSpaceConversion(wgpu::ColorSpace srcColorSpace,
                                                           wgpu::ColorSpace dstColorSpace) {
            DAWN_TRY(ValidateColorSpace(srcColorSpace));
            DAWN_TRY(ValidateColorSpace(dstColorSpace));

            switch (srcColorSpace) {
                case wgpu::ColorSpace::SRGB:
                case wgpu::ColorSpace::DisplayP3:
                    break;
                default:
                    return DAWN_FORMAT_VALIDATION_ERROR("Source color space (%s) is not supported.",
                                                        srcColorSpace);
            }

            switch (dstColorSpace) {
                case wgpu::ColorSpace::SRGB:
                    break;
                default:
                    return DAWN_FORMAT_VALIDATION_ERROR(
                        "Destination color space (%s) is not supported.", dstColorSpace);
            }

            return {};
        }

        RenderPipelineBase* GetCachedPipeline(InternalPipelineStore* store,
                                              wgpu::TextureFormat dstFormat) {
            auto pipeline = store->copyTextureForBrowserPipelines.find(dstFormat);
            if (pipeline != store->copyTextureForBrowserPipelines.end()) {
                return pipeline->second.Get();
            }
            return nullptr;
        }

        ResultOrError<RenderPipelineBase*> GetOrCreateCopyTextureForBrowserPipeline(
            DeviceBase* device,
            wgpu::TextureFormat dstFormat) {
            InternalPipelineStore* store = device->GetInternalPipelineStore();

            if (GetCachedPipeline(store, dstFormat) == nullptr) {
                // Create vertex shader module if not cached before.
                if (store->copyTextureForBrowser == nullptr) {
                    DAWN_TRY_ASSIGN(
                        store->copyTextureForBrowser,
                        utils::CreateShaderModule(device, sCopyTextureForBrowserShader));
                }

                ShaderModuleBase* shaderModule = store->copyTextureForBrowser.Get();

                // Prepare vertex stage.
                VertexState vertex = {};
                vertex.module = shaderModule;
                vertex.entryPoint = "vs_main";

                // Prepare frgament stage.
                FragmentState fragment = {};
                fragment.module = shaderModule;
                fragment.entryPoint = "fs_main";

                // Prepare color state.
                ColorTargetState target = {};
                target.format = dstFormat;

                // Create RenderPipeline.
                RenderPipelineDescriptor renderPipelineDesc = {};

                // Generate the layout based on shader modules.
                renderPipelineDesc.layout = nullptr;

                renderPipelineDesc.vertex = vertex;
                renderPipelineDesc.fragment = &fragment;

                renderPipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;

                fragment.targetCount = 1;
                fragment.targets = &target;

                Ref<RenderPipelineBase> pipeline;
                DAWN_TRY_ASSIGN(pipeline, device->CreateRenderPipeline(&renderPipelineDesc));
                store->copyTextureForBrowserPipelines.insert({dstFormat, std::move(pipeline)});
            }

            return GetCachedPipeline(store, dstFormat);
        }

        // Row major 3x3 matrix
        struct ColorSpaceInfo {
            std::array<float, 9> toXYZD50;    // 3x3 row major transform matrix
            std::array<float, 9> fromXYZD50;  // inverse transform matrix of toXYZD50, precomputed
            std::array<float, 7> gammaDecodingParams;  // Follow { A, B, G, E, epsilon, C, F } order
            std::array<float, 7> gammaEncodingParams;  // inverse op of decoding, precomputed
            bool isNonLinear;
            bool isExtended;  // For extended color space.
        };

        static constexpr size_t kSupportedColorSpaceCount = 2;
        static constexpr std::array<ColorSpaceInfo, kSupportedColorSpaceCount> ColorSpaceTable = {{
            // sRGB,
            // Got primary attributes from https://drafts.csswg.org/css-color/#predefined-sRGB
            // Use matries from
            // http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html#WSMatrices
            // Get gamma-linear conversion params from https://en.wikipedia.org/wiki/SRGB with some
            // mathmatics.
            {
                //
                {{
                    //
                    0.4360747, 0.3850649, 0.1430804,  //
                    0.2225045, 0.7168786, 0.0606169,  //
                    0.0139322, 0.0971045, 0.7141733   //
                }},

                {{
                    //
                    3.1338561, -1.6168667, -0.4906146,  //
                    -0.9787684, 1.9161415, 0.0334540,   //
                    0.0719453, -0.2289914, 1.4052427    //
                }},

                // {A, B, G, E, epsilon, C, F, }
                {{1.0 / 1.055, 0.055 / 1.055, 2.4, 0.0, 4.045e-02, 1.0 / 12.92, 0.0}},

                {{1.13711 /*pow(1.055, 2.4)*/, 0.0, 1.0 / 2.4, -0.055, 3.1308e-03, 12.92f, 0.0}},

                true,
                true  //
            },

            // Display P3, got primary attributes from
            // https://www.w3.org/TR/css-color-4/#valdef-color-display-p3
            // Use equations found in
            // http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html,
            // Use Bradford method to do D65 to D50 transform.
            // Get matries with help of http://www.russellcottrell.com/photo/matrixCalculator.htm
            // Gamma-linear conversion params is the same as Srgb.
            {
                //
                {{
                    //
                    0.5151114, 0.2919612, 0.1571274,  //
                    0.2411865, 0.6922440, 0.0665695,  //
                    -0.0010491, 0.0418832, 0.7842659  //
                }},

                {{
                    //
                    2.4039872, -0.9898498, -0.3976181,  //
                    -0.8422138, 1.7988188, 0.0160511,   //
                    0.0481937, -0.0973889, 1.2736887    //
                }},

                // {A, B, G, E, epsilon, C, F, }
                {{1.0 / 1.055, 0.055 / 1.055, 2.4, 0.0, 4.045e-02, 1.0 / 12.92, 0.0}},

                {{1.13711 /*pow(1.055, 2.4)*/, 0.0, 1.0 / 2.4, -0.055, 3.1308e-03, 12.92f, 0.0}},

                true,
                false  //
            }
            //
        }};

        std::array<float, 12> GetConversionMatrix(wgpu::ColorSpace src, wgpu::ColorSpace dst) {
            ASSERT(src < wgpu::ColorSpace::Invalid);
            ASSERT(dst < wgpu::ColorSpace::Invalid);

            std::array<float, 9> toXYZD50 = ColorSpaceTable[static_cast<uint32_t>(src)].toXYZD50;
            std::array<float, 9> fromXYZD50 =
                ColorSpaceTable[static_cast<uint32_t>(dst)].fromXYZD50;

            // Fuse the transform matrix. The color space transformation equation is:
            // Pixels = fromXYZD50 * toXYZD50 * Pixels.
            // Calculate fromXYZD50 * toXYZD50 to simplify
            // Add a padding in each row for Mat3x3 in wgsl uniform(mat3x3, Align(16), Size(48)).
            std::array<float, 12> fuseMatrix = {};

            // Mat3x3 * Mat3x3
            for (uint32_t row = 0; row < 3; ++row) {
                for (uint32_t col = 0; col < 3; ++col) {
                    // fuseMatrix has 1 float padding each row.
                    // Transpose the matrix from row major to column major for wgsl.
                    fuseMatrix[col * 4 + row] = fromXYZD50[row * 3 + 0] * toXYZD50[col] +
                                                fromXYZD50[row * 3 + 1] * toXYZD50[3 + col] +
                                                fromXYZD50[row * 3 + 2] * toXYZD50[3 * 2 + col];
                }
            }

            return fuseMatrix;
        }
    }  // anonymous namespace

    MaybeError ValidateCopyTextureForBrowser(DeviceBase* device,
                                             const ImageCopyTexture* source,
                                             const ImageCopyTexture* destination,
                                             const Extent3D* copySize,
                                             const CopyTextureForBrowserOptions* options) {
        DAWN_TRY(device->ValidateObject(source->texture));
        DAWN_TRY(device->ValidateObject(destination->texture));

        DAWN_TRY_CONTEXT(ValidateImageCopyTexture(device, *source, *copySize),
                         "validating the ImageCopyTexture for the source");
        DAWN_TRY_CONTEXT(ValidateImageCopyTexture(device, *destination, *copySize),
                         "validating the ImageCopyTexture for the destination");

        DAWN_TRY_CONTEXT(ValidateTextureCopyRange(device, *source, *copySize),
                         "validating that the copy fits in the source");
        DAWN_TRY_CONTEXT(ValidateTextureCopyRange(device, *destination, *copySize),
                         "validating that the copy fits in the destination");

        DAWN_TRY(ValidateTextureToTextureCopyCommonRestrictions(*source, *destination, *copySize));

        DAWN_INVALID_IF(source->origin.z > 0, "Source has a non-zero z origin (%u).",
                        source->origin.z);
        DAWN_INVALID_IF(copySize->depthOrArrayLayers > 1,
                        "Copy is for more than one array layer (%u)", copySize->depthOrArrayLayers);

        DAWN_INVALID_IF(
            source->texture->GetSampleCount() > 1 || destination->texture->GetSampleCount() > 1,
            "The source texture sample count (%u) or the destination texture sample count (%u) is "
            "not 1.",
            source->texture->GetSampleCount(), destination->texture->GetSampleCount());

        DAWN_TRY(ValidateCanUseAs(source->texture, wgpu::TextureUsage::CopySrc));
        DAWN_TRY(ValidateCanUseAs(source->texture, wgpu::TextureUsage::TextureBinding));

        DAWN_TRY(ValidateCanUseAs(destination->texture, wgpu::TextureUsage::CopyDst));
        DAWN_TRY(ValidateCanUseAs(destination->texture, wgpu::TextureUsage::RenderAttachment));

        DAWN_TRY(ValidateCopyTextureFormatConversion(source->texture->GetFormat().format,
                                                     destination->texture->GetFormat().format));

        DAWN_INVALID_IF(options->nextInChain != nullptr, "nextInChain must be nullptr");

        // TODO(crbug.com/dawn/1140): Remove alphaOp and wgpu::AlphaState::DontChange.
        DAWN_TRY(ValidateAlphaOp(options->alphaOp));
        DAWN_TRY(ValidateAlphaState(options->srcTextureAlphaState));
        DAWN_TRY(ValidateAlphaState(options->dstTextureAlphaState));
        DAWN_TRY(
            ValidateCopyTextureColorSpaceConversion(options->colorSpaceConversion.srcColorSpace,
                                                    options->colorSpaceConversion.dstColorSpace));

        return {};
    }

    MaybeError DoCopyTextureForBrowser(DeviceBase* device,
                                       const ImageCopyTexture* source,
                                       const ImageCopyTexture* destination,
                                       const Extent3D* copySize,
                                       const CopyTextureForBrowserOptions* options) {
        // TODO(crbug.com/dawn/856): In D3D12 and Vulkan, compatible texture format can directly
        // copy to each other. This can be a potential fast path.

        // Noop copy
        if (copySize->width == 0 || copySize->height == 0 || copySize->depthOrArrayLayers == 0) {
            return {};
        }

        RenderPipelineBase* pipeline;
        DAWN_TRY_ASSIGN(pipeline, GetOrCreateCopyTextureForBrowserPipeline(
                                      device, destination->texture->GetFormat().format));

        // Prepare bind group layout.
        Ref<BindGroupLayoutBase> layout;
        DAWN_TRY_ASSIGN(layout, pipeline->GetBindGroupLayout(0));

        Extent3D srcTextureSize = source->texture->GetSize();

        // Prepare binding 0 resource: uniform buffer.
        Uniform uniformData = {
            copySize->width / static_cast<float>(srcTextureSize.width),
            copySize->height / static_cast<float>(srcTextureSize.height),  // scale
            source->origin.x / static_cast<float>(srcTextureSize.width),
            source->origin.y / static_cast<float>(srcTextureSize.height)  // offset
        };

        // Handle flipY. FlipY here means we flip the source texture firstly and then
        // do copy. This helps on the case which source texture is flipped and the copy
        // need to unpack the flip.
        if (options->flipY) {
            uniformData.scaleY *= -1.0;
            uniformData.offsetY += copySize->height / static_cast<float>(srcTextureSize.height);
        }

        uint32_t stepsMask = 0u;

        // Steps to do color space conversion
        // From https://skia.org/docs/user/color/
        // - unpremultiply if the source color is premultiplied alpha is not involved in color
        // management,
        //   and we need to divide it out if it’s multiplied in
        // - linearize the source color using the source color space’s transfer function
        // - convert those unpremultiplied, linear source colors to XYZ D50 gamut by multiplying by
        // a 3x3 matrix
        // - convert those XYZ D50 colors to the destination gamut by multiplying by a 3x3 matrix
        // - encode that color using the inverse of the destination color space’s transfer function
        // - premultiply by alpha if the destination is premultiplied
        // The reason to choose XYZ D50 as intermediate color space:
        // From http://www.brucelindbloom.com/index.html?WorkingSpaceInfo.html
        // "Since the Lab TIFF specification, the ICC profile specification and
        // Adobe Photoshop all use a D50"
        bool skipColorSpaceConversion = options->colorSpaceConversion.srcColorSpace ==
                                        options->colorSpaceConversion.dstColorSpace;

        if (options->srcTextureAlphaState == wgpu::AlphaState::Premultiplied) {
            if (!skipColorSpaceConversion ||
                options->dstTextureAlphaState == wgpu::AlphaState::Seperate) {
                stepsMask |= 0x01;  // Umpremultiply step
            }
        }

        if (!skipColorSpaceConversion) {
            ColorSpaceInfo srcColorSpace =
                ColorSpaceTable[static_cast<uint32_t>(options->colorSpaceConversion.srcColorSpace)];
            ColorSpaceInfo dstColorSpace =
                ColorSpaceTable[static_cast<uint32_t>(options->colorSpaceConversion.dstColorSpace)];

            if (srcColorSpace.isNonLinear) {
                stepsMask |= 0x02;  // Linearize source color
                uniformData.gammaDecodingParams = srcColorSpace.gammaDecodingParams;
                uniformData.srcColorSpaceIsExtended =
                    static_cast<uint32_t>(srcColorSpace.isExtended);
            }

            stepsMask |= 0x04;  // convert to destination gamut
            uniformData.conversionMatrix =
                GetConversionMatrix(options->colorSpaceConversion.srcColorSpace,
                                    options->colorSpaceConversion.dstColorSpace);

            if (dstColorSpace.isNonLinear) {
                stepsMask |= 0x08;  // Encode linear color to non-linear
                uniformData.gammaEncodingParams = dstColorSpace.gammaEncodingParams;
                uniformData.dstColorSpaceIsExtended =
                    static_cast<uint32_t>(dstColorSpace.isExtended);
            }
        }

        if (options->dstTextureAlphaState == wgpu::AlphaState::Premultiplied) {
            if (!skipColorSpaceConversion ||
                options->srcTextureAlphaState == wgpu::AlphaState::Seperate) {
                stepsMask |= 0x10;  // Premultiply step
            }
        }

        // TODO(crbugs.com/dawn/1140): AlphaOp will be deprecated
        if (options->alphaOp == wgpu::AlphaOp::Premultiply) {
            stepsMask |= 0x10;
        }

        if (options->alphaOp == wgpu::AlphaOp::Unpremultiply) {
            stepsMask |= 0x01;
        }

        uniformData.stepsMask = stepsMask;

        Ref<BufferBase> uniformBuffer;
        DAWN_TRY_ASSIGN(
            uniformBuffer,
            utils::CreateBufferFromData(
                device, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform, {uniformData}));

        // Prepare binding 1 resource: sampler
        // Use default configuration, filterMode set to Nearest for min and mag.
        SamplerDescriptor samplerDesc = {};
        Ref<SamplerBase> sampler;
        DAWN_TRY_ASSIGN(sampler, device->CreateSampler(&samplerDesc));

        // Prepare binding 2 resource: sampled texture
        TextureViewDescriptor srcTextureViewDesc = {};
        srcTextureViewDesc.baseMipLevel = source->mipLevel;
        srcTextureViewDesc.mipLevelCount = 1;
        srcTextureViewDesc.arrayLayerCount = 1;
        Ref<TextureViewBase> srcTextureView;
        DAWN_TRY_ASSIGN(srcTextureView,
                        device->CreateTextureView(source->texture, &srcTextureViewDesc));

        // Create bind group after all binding entries are set.
        Ref<BindGroupBase> bindGroup;
        DAWN_TRY_ASSIGN(bindGroup, utils::MakeBindGroup(
                                       device, layout,
                                       {{0, uniformBuffer}, {1, sampler}, {2, srcTextureView}}));

        // Create command encoder.
        CommandEncoderDescriptor encoderDesc = {};
        // TODO(dawn:723): change to not use AcquireRef for reentrant object creation.
        Ref<CommandEncoder> encoder = AcquireRef(device->APICreateCommandEncoder(&encoderDesc));

        // Prepare dst texture view as color Attachment.
        TextureViewDescriptor dstTextureViewDesc;
        dstTextureViewDesc.baseMipLevel = destination->mipLevel;
        dstTextureViewDesc.mipLevelCount = 1;
        dstTextureViewDesc.baseArrayLayer = destination->origin.z;
        dstTextureViewDesc.arrayLayerCount = 1;
        Ref<TextureViewBase> dstView;
        DAWN_TRY_ASSIGN(dstView,
                        device->CreateTextureView(destination->texture, &dstTextureViewDesc));

        // Prepare render pass color attachment descriptor.
        RenderPassColorAttachment colorAttachmentDesc;

        colorAttachmentDesc.view = dstView.Get();
        colorAttachmentDesc.loadOp = wgpu::LoadOp::Load;
        colorAttachmentDesc.storeOp = wgpu::StoreOp::Store;
        colorAttachmentDesc.clearColor = {0.0, 0.0, 0.0, 1.0};

        // Create render pass.
        RenderPassDescriptor renderPassDesc;
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachmentDesc;
        // TODO(dawn:723): change to not use AcquireRef for reentrant object creation.
        Ref<RenderPassEncoder> passEncoder =
            AcquireRef(encoder->APIBeginRenderPass(&renderPassDesc));

        // Start pipeline  and encode commands to complete
        // the copy from src texture to dst texture with transformation.
        passEncoder->APISetPipeline(pipeline);
        passEncoder->APISetBindGroup(0, bindGroup.Get());
        passEncoder->APISetViewport(destination->origin.x, destination->origin.y, copySize->width,
                                    copySize->height, 0.0, 1.0);
        passEncoder->APIDraw(3);
        passEncoder->APIEndPass();

        // Finsh encoding.
        // TODO(dawn:723): change to not use AcquireRef for reentrant object creation.
        Ref<CommandBufferBase> commandBuffer = AcquireRef(encoder->APIFinish());
        CommandBufferBase* submitCommandBuffer = commandBuffer.Get();

        // Submit command buffer.
        device->GetQueue()->APISubmit(1, &submitCommandBuffer);

        return {};
    }

}  // namespace dawn_native
