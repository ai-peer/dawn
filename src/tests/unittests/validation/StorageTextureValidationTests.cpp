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

#include "tests/unittests/validation/ValidationTest.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class StorageTextureValidationTests : public ValidationTest {
  protected:
    const char* GetGLSLImageFormatQualifier(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
                return "r8";
            case wgpu::TextureFormat::R8Snorm:
                return "r8_snorm";
            case wgpu::TextureFormat::R8Uint:
                return "r8ui";
            case wgpu::TextureFormat::R8Sint:
                return "r8i";
            case wgpu::TextureFormat::R16Uint:
                return "r16ui";
            case wgpu::TextureFormat::R16Sint:
                return "r16i";
            case wgpu::TextureFormat::R16Float:
                return "r16f";
            case wgpu::TextureFormat::RG8Unorm:
                return "rg8";
            case wgpu::TextureFormat::RG8Snorm:
                return "rg8_snorm";
            case wgpu::TextureFormat::RG8Uint:
                return "rg8ui";
            case wgpu::TextureFormat::RG8Sint:
                return "rg8i";
            case wgpu::TextureFormat::R32Float:
                return "r32f";
            case wgpu::TextureFormat::R32Uint:
                return "r32ui";
            case wgpu::TextureFormat::R32Sint:
                return "r32i";
            case wgpu::TextureFormat::RG16Uint:
                return "rg16ui";
            case wgpu::TextureFormat::RG16Sint:
                return "rg16i";
            case wgpu::TextureFormat::RG16Float:
                return "rg16f";
            case wgpu::TextureFormat::RGBA8Unorm:
                return "rgba8";
            case wgpu::TextureFormat::RGBA8Snorm:
                return "rgba8_snorm";
            case wgpu::TextureFormat::RGBA8Uint:
                return "rgba8ui";
            case wgpu::TextureFormat::RGBA8Sint:
                return "rgba8i";
            case wgpu::TextureFormat::RGB10A2Unorm:
                return "rgb10_a2";
            case wgpu::TextureFormat::RG11B10Float:
                return "r11f_g11f_b10f";
            case wgpu::TextureFormat::RG32Float:
                return "rg32f";
            case wgpu::TextureFormat::RG32Uint:
                return "rg32ui";
            case wgpu::TextureFormat::RG32Sint:
                return "rg32i";
            case wgpu::TextureFormat::RGBA16Uint:
                return "rgba16ui";
            case wgpu::TextureFormat::RGBA16Sint:
                return "rgba16i";
            case wgpu::TextureFormat::RGBA16Float:
                return "rgba16f";
            case wgpu::TextureFormat::RGBA32Float:
                return "rgba32f";
            case wgpu::TextureFormat::RGBA32Uint:
                return "rgba32ui";
            case wgpu::TextureFormat::RGBA32Sint:
                return "rgba32i";
            default:
                EXPECT_TRUE(false);
                return "";
        }
    }

    const char* GetTextureComponentTypePrefix(wgpu::TextureFormat textureFormat) {
        switch (textureFormat) {
            case wgpu::TextureFormat::R8Unorm:
            case wgpu::TextureFormat::R8Snorm:
            case wgpu::TextureFormat::R16Float:
            case wgpu::TextureFormat::RG8Unorm:
            case wgpu::TextureFormat::RG8Snorm:
            case wgpu::TextureFormat::R32Float:
            case wgpu::TextureFormat::RG16Float:
            case wgpu::TextureFormat::RGBA8Unorm:
            case wgpu::TextureFormat::RGBA8Snorm:
            case wgpu::TextureFormat::RGB10A2Unorm:
            case wgpu::TextureFormat::RG11B10Float:
            case wgpu::TextureFormat::RG32Float:
            case wgpu::TextureFormat::RGBA16Float:
            case wgpu::TextureFormat::RGBA32Float:
                return "";

            case wgpu::TextureFormat::R8Uint:
            case wgpu::TextureFormat::R16Uint:
            case wgpu::TextureFormat::RG8Uint:
            case wgpu::TextureFormat::R32Uint:
            case wgpu::TextureFormat::RG16Uint:
            case wgpu::TextureFormat::RGBA8Uint:
            case wgpu::TextureFormat::RG32Uint:
            case wgpu::TextureFormat::RGBA16Uint:
            case wgpu::TextureFormat::RGBA32Uint:
                return "u";

            case wgpu::TextureFormat::R8Sint:
            case wgpu::TextureFormat::R16Sint:
            case wgpu::TextureFormat::RG8Sint:
            case wgpu::TextureFormat::R32Sint:
            case wgpu::TextureFormat::RG16Sint:
            case wgpu::TextureFormat::RGBA8Sint:
            case wgpu::TextureFormat::RG32Sint:
            case wgpu::TextureFormat::RGBA16Sint:
            case wgpu::TextureFormat::RGBA32Sint:
                return "i";
            default:
                EXPECT_TRUE(false);
                return "";
        }
    }

    std::string CreateComputeShaderWithStorageTexture(wgpu::BindingType storageTextureBindingType,
                                                      wgpu::TextureFormat textureFormat) {
        const char* memoryQualifier = "";
        switch (storageTextureBindingType) {
            case wgpu::BindingType::ReadonlyStorageTexture:
                memoryQualifier = "readonly";
                break;
            case wgpu::BindingType::WriteonlyStorageTexture:
                memoryQualifier = "writeonly";
                break;
            default:
                EXPECT_TRUE(false);
                break;
        }

        const char* prefix = GetTextureComponentTypePrefix(textureFormat);

        std::ostringstream ostream;
        ostream << "#version 450\n"
                   "layout (set = 0, binding = 0, "
                << GetGLSLImageFormatQualifier(textureFormat) << ") uniform " << memoryQualifier
                << " " << prefix
                << "image2D image0;\n"
                   "layout(std430, set = 0, binding = 0) buffer Buf { uint buf; };\n"
                << "void main() {\n";

        switch (storageTextureBindingType) {
            case wgpu::BindingType::ReadonlyStorageTexture:
                ostream << prefix
                        << "vec4 pixel = imageLoad(image0, ivec2(gl_LocalInvocationID.xy));\n"
                           "buf = uint(pixel.x);\n";
                break;
            case wgpu::BindingType::WriteonlyStorageTexture:
                ostream << "imageStore(image0, ivec2(gl_LocalInvocationID.xy), " << prefix
                        << "vec4(0, 0, 0, "
                           "0));\n";
                break;
            default:
                EXPECT_TRUE(false);
                break;
        }

        ostream << "}\n";

        return ostream.str();
    }

    wgpu::ShaderModule mDefaultVSModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
        #version 450
        void main() {
            gl_Position = vec4(0.f, 0.f, 0.f, 1.f);
        })");
    wgpu::ShaderModule mDefaultFSModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
        #version 450
        layout(location = 0) out vec4 fragColor;
        void main() {
            fragColor = vec4(1.f, 0.f, 0.f, 1.f);
        })");
};

// Validate read-only storage textures can be declared in vertex and fragment
// shaders, while writeonly storage textures can't.
TEST_F(StorageTextureValidationTests, RenderPipeline) {
    // Readonly storage texture can be declared in a vertex shader.
    {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform readonly image2D image0;
            void main() {
                gl_Position = imageLoad(image0, ivec2(gl_VertexIndex, 0));
            })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.layout = nullptr;
        descriptor.vertexStage.module = vsModule;
        descriptor.cFragmentStage.module = mDefaultFSModule;
        device.CreateRenderPipeline(&descriptor);
    }

    // Read-only storage textures can be declared in a fragment shader.
    {
        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform readonly image2D image0;
            layout(location = 0) out vec4 fragColor;
            void main() {
                fragColor = imageLoad(image0, ivec2(gl_FragCoord.xy));
            })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.layout = nullptr;
        descriptor.vertexStage.module = mDefaultVSModule;
        descriptor.cFragmentStage.module = fsModule;
        device.CreateRenderPipeline(&descriptor);
    }

    // Write-only storage textures cannot be declared in a vertex shader.
    {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform writeonly image2D image0;
            void main() {
                imageStore(image0, ivec2(gl_VertexIndex, 0), vec4(1.f, 0.f, 0.f, 1.f));
            })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.layout = nullptr;
        descriptor.vertexStage.module = vsModule;
        descriptor.cFragmentStage.module = mDefaultFSModule;
        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Write-only storage textures cannot be declared in a fragment shader.
    {
        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform writeonly image2D image0;
            void main() {
                imageStore(image0, ivec2(gl_FragCoord.xy), vec4(1.f, 0.f, 0.f, 1.f));
            })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.layout = nullptr;
        descriptor.vertexStage.module = mDefaultVSModule;
        descriptor.cFragmentStage.module = fsModule;
        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }
}

// Validate both read-only and write-only storage textures can be declared in
// compute shaders.
TEST_F(StorageTextureValidationTests, ComputePipeline) {
    // Read-only storage textures can be declared in a compute shader.
    {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform readonly image2D image0;
            layout(std430, set = 0, binding = 0) buffer Buf { uint buf; };
            void main() {
                vec4 pixel = imageLoad(image0, ivec2(gl_LocalInvocationID.xy));
                buf = uint(pixel.x);
            })");

        wgpu::ComputePipelineDescriptor descriptor;
        descriptor.layout = nullptr;
        descriptor.computeStage.module = csModule;
        descriptor.computeStage.entryPoint = "main";

        device.CreateComputePipeline(&descriptor);
    }

    // Write-only storage textures can be declared in a compute shader.
    {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform writeonly image2D image0;
            void main() {
                imageStore(image0, ivec2(gl_LocalInvocationID.xy), vec4(0.f, 0.f, 0.f, 0.f));
            })");

        wgpu::ComputePipelineDescriptor descriptor;
        descriptor.layout = nullptr;
        descriptor.computeStage.module = csModule;
        descriptor.computeStage.entryPoint = "main";

        device.CreateComputePipeline(&descriptor);
    }
}

// Validate read-write storage textures have not been supported yet.
TEST_F(StorageTextureValidationTests, ReadWriteStorageTexture) {
    // Read-write storage textures cannot be declared in a vertex shader by default.
    {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform image2D image0;
            void main() {
                vec4 pixel = imageLoad(image0, ivec2(gl_VertexIndex, 0));
                imageStore(image0, ivec2(gl_VertexIndex, 0), pixel * 2);
            })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.layout = nullptr;
        descriptor.vertexStage.module = vsModule;
        descriptor.cFragmentStage.module = mDefaultFSModule;
        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Read-write storage textures cannot be declared in a fragment shader by default.
    {
        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform image2D image0;
            void main() {
                vec4 pixel = imageLoad(image0, ivec2(gl_FragCoord.xy));
                imageStore(image0, ivec2(gl_FragCoord.xy), pixel * 2);
            })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.layout = nullptr;
        descriptor.vertexStage.module = mDefaultVSModule;
        descriptor.cFragmentStage.module = fsModule;
        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Read-write storage textures cannot be declared in a compute shader by default.
    {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
            #version 450
            layout(set = 0, binding = 0, rgba8) uniform image2D image0;
            void main() {
                vec4 pixel = imageLoad(image0, ivec2(gl_LocalInvocationID.xy));
                imageStore(image0, ivec2(gl_LocalInvocationID.xy), pixel * 2);
            })");

        wgpu::ComputePipelineDescriptor descriptor;
        descriptor.layout = nullptr;
        descriptor.computeStage.module = csModule;
        descriptor.computeStage.entryPoint = "main";

        ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&descriptor));
    }
}

// Validate it is an error to declare a read-only or write-only storage texture with any format
// that doesn't support TextureUsage::Storage texture usages.
TEST_F(StorageTextureValidationTests, StorageTextureFormat) {
    // Not include RGBA8UnormSrgb, BGRA8Unorm, BGRA8UnormSrgb because they are all related to
    // "rgba8" in GLSL / SPIR-V.
    constexpr std::array<std::pair<wgpu::TextureFormat, bool>, 32>
        kTextureFormatSupportedAsStorageTextureSpecs = {
            std::make_pair(wgpu::TextureFormat::R32Uint, true),
            std::make_pair(wgpu::TextureFormat::R32Sint, true),
            std::make_pair(wgpu::TextureFormat::R32Float, true),
            std::make_pair(wgpu::TextureFormat::RGBA8Unorm, true),
            std::make_pair(wgpu::TextureFormat::RGBA8Snorm, true),
            std::make_pair(wgpu::TextureFormat::RGBA8Uint, true),
            std::make_pair(wgpu::TextureFormat::RGBA8Sint, true),
            std::make_pair(wgpu::TextureFormat::RG32Uint, true),
            std::make_pair(wgpu::TextureFormat::RG32Sint, true),
            std::make_pair(wgpu::TextureFormat::RG32Float, true),
            std::make_pair(wgpu::TextureFormat::RGBA16Uint, true),
            std::make_pair(wgpu::TextureFormat::RGBA16Sint, true),
            std::make_pair(wgpu::TextureFormat::RGBA16Float, true),
            std::make_pair(wgpu::TextureFormat::RGBA32Uint, true),
            std::make_pair(wgpu::TextureFormat::RGBA32Sint, true),
            std::make_pair(wgpu::TextureFormat::RGBA32Float, true),
            std::make_pair(wgpu::TextureFormat::R8Unorm, false),
            std::make_pair(wgpu::TextureFormat::R8Snorm, false),
            std::make_pair(wgpu::TextureFormat::R8Uint, false),
            std::make_pair(wgpu::TextureFormat::R8Sint, false),
            std::make_pair(wgpu::TextureFormat::R16Uint, false),
            std::make_pair(wgpu::TextureFormat::R16Sint, false),
            std::make_pair(wgpu::TextureFormat::R16Float, false),
            std::make_pair(wgpu::TextureFormat::RG8Unorm, false),
            std::make_pair(wgpu::TextureFormat::RG8Snorm, false),
            std::make_pair(wgpu::TextureFormat::RG8Uint, false),
            std::make_pair(wgpu::TextureFormat::RG8Sint, false),
            std::make_pair(wgpu::TextureFormat::RG16Uint, false),
            std::make_pair(wgpu::TextureFormat::RG16Sint, false),
            std::make_pair(wgpu::TextureFormat::RG16Float, false),
            std::make_pair(wgpu::TextureFormat::RGB10A2Unorm, false),
            std::make_pair(wgpu::TextureFormat::RG11B10Float, false),
        };

    constexpr std::array<wgpu::BindingType, 2> kStorageTextureBindingTypes = {
        wgpu::BindingType::ReadonlyStorageTexture, wgpu::BindingType::WriteonlyStorageTexture};

    for (wgpu::BindingType storageTextureBindingType : kStorageTextureBindingTypes) {
        for (const auto& textureFormatAsStorageTextureSpec :
             kTextureFormatSupportedAsStorageTextureSpecs) {
            wgpu::TextureFormat format = textureFormatAsStorageTextureSpec.first;
            bool supportStorageTexture = textureFormatAsStorageTextureSpec.second;

            std::string computeShader =
                CreateComputeShaderWithStorageTexture(storageTextureBindingType, format);
            wgpu::ShaderModule csModule = utils::CreateShaderModule(
                device, utils::SingleShaderStage::Compute, computeShader.c_str());

            wgpu::ComputePipelineDescriptor descriptor;
            descriptor.layout = nullptr;
            descriptor.computeStage.module = csModule;
            descriptor.computeStage.entryPoint = "main";

            if (supportStorageTexture) {
                device.CreateComputePipeline(&descriptor);
            } else {
                ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&descriptor));
            }
        }
    }
}
