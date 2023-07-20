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

#include "dawn/tests/end2end/SharedTextureMemoryTests.h"

#include "dawn/tests/MockCallback.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {

std::vector<wgpu::FeatureName> SharedTextureMemoryTests::GetRequiredFeatures() {
    auto features = GetParam().mBackend->RequiredFeatures();
    if (!SupportsFeatures(features)) {
        return {};
    }
    if (SupportsFeatures({wgpu::FeatureName::TransientAttachments})) {
        features.push_back(wgpu::FeatureName::TransientAttachments);
    }
    return features;
}

void SharedTextureMemoryNoFeatureTests::SetUp() {
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    DawnTestWithParams<SharedTextureMemoryTestParams>::SetUp();
}

void SharedTextureMemoryTests::SetUp() {
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    DawnTestWithParams<SharedTextureMemoryTestParams>::SetUp();
    DAWN_TEST_UNSUPPORTED_IF(!SupportsFeatures(GetParam().mBackend->RequiredFeatures()));
}

void SharedTextureMemoryTests::UseInRenderPass(wgpu::Device& deviceObj, wgpu::Texture& texture) {
    wgpu::CommandEncoder encoder = deviceObj.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor passDescriptor({texture.CreateView()});

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
    pass.End();
    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    deviceObj.GetQueue().Submit(1, &commandBuffer);
}

void SharedTextureMemoryTests::UseInCopy(wgpu::Device& deviceObj, wgpu::Texture& texture) {
    wgpu::CommandEncoder encoder = deviceObj.CreateCommandEncoder();
    wgpu::ImageCopyTexture source;
    source.texture = texture;

    // Create a destination buffer, large enough for 1 texel of any format.
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = 128;
    bufferDesc.usage = wgpu::BufferUsage::CopyDst;

    wgpu::ImageCopyBuffer destination;
    destination.buffer = device.CreateBuffer(&bufferDesc);

    wgpu::Extent3D size = {1, 1, 1};
    encoder.CopyTextureToBuffer(&source, &destination, &size);

    wgpu::CommandBuffer commandBuffer = encoder.Finish();
    deviceObj.GetQueue().Submit(1, &commandBuffer);
}

wgpu::CommandBuffer SharedTextureMemoryTests::MakeFourColorsClearCommandBuffer(
    wgpu::Device& deviceObj,
    wgpu::Texture& texture) {
    wgpu::ShaderModule module = utils::CreateShaderModule(deviceObj, R"(
      struct VertexOut {
          @builtin(position) position : vec4f,
          @location(0) uv : vec2f,
      }

      struct VertexIn {
          @location(0) uv : vec2f,
      }

      @vertex fn vert_main(@builtin(vertex_index) VertexIndex : u32) -> VertexOut {
          let pos = array(
            vec2( 1.0,  1.0),
            vec2( 1.0, -1.0),
            vec2(-1.0, -1.0),
            vec2( 1.0,  1.0),
            vec2(-1.0, -1.0),
            vec2(-1.0,  1.0),
          );

          let uv = array(
            vec2(1.0, 0.0),
            vec2(1.0, 1.0),
            vec2(0.0, 1.0),
            vec2(1.0, 0.0),
            vec2(0.0, 1.0),
            vec2(0.0, 0.0),
          );
          return VertexOut(vec4f(pos[VertexIndex], 0.0, 1.0), uv[VertexIndex]);
      }

      @fragment fn frag_main(in: VertexIn) -> @location(0) vec4f {
          if (in.uv.x < 0.5) {
            if (in.uv.y < 0.5) {
              return vec4f(0.0, 1.0, 0.0, 1.0);
            } else {
              return vec4f(1.0, 0.0, 0.0, 1.0);
            }
          } else {
            if (in.uv.y < 0.5) {
              return vec4f(0.0, 0.0, 1.0, 1.0);
            } else {
              return vec4f(1.0, 1.0, 0.0, 1.0);
            }
          }
      }
    )");

    utils::ComboRenderPipelineDescriptor pipelineDesc;
    pipelineDesc.vertex.module = module;
    pipelineDesc.vertex.entryPoint = "vert_main";
    pipelineDesc.cFragment.module = module;
    pipelineDesc.cFragment.entryPoint = "frag_main";
    pipelineDesc.cTargets[0].format = texture.GetFormat();

    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDesc);

    wgpu::CommandEncoder encoder = deviceObj.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor passDescriptor({texture.CreateView()});
    passDescriptor.cColorAttachments[0].storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
    pass.SetPipeline(pipeline);
    pass.Draw(6);
    pass.End();
    return encoder.Finish();
}

std::pair<wgpu::CommandBuffer, wgpu::Texture>
SharedTextureMemoryTests::MakeCheckFourColorsCommandBuffer(wgpu::Device& deviceObj,
                                                           wgpu::Texture& texture) {
    wgpu::ShaderModule module = utils::CreateShaderModule(deviceObj, R"(
      @vertex fn vert_main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
          let pos = array(
            vec2( 1.0,  1.0),
            vec2( 1.0, -1.0),
            vec2(-1.0, -1.0),
            vec2( 1.0,  1.0),
            vec2(-1.0, -1.0),
            vec2(-1.0,  1.0),
          );
          return vec4f(pos[VertexIndex], 0.0, 1.0);
      }

      @group(0) @binding(0) var t: texture_2d<f32>;

      @fragment fn frag_main(@builtin(position) coord_in: vec4<f32>) -> @location(0) vec4f {
        return textureLoad(t, vec2u(coord_in.xy), 0);
      }
    )");

    wgpu::TextureDescriptor textureDesc = {};
    textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    textureDesc.size = {texture.GetWidth(), texture.GetHeight(), texture.GetDepthOrArrayLayers()};

    wgpu::Texture colorTarget = deviceObj.CreateTexture(&textureDesc);

    utils::ComboRenderPipelineDescriptor pipelineDesc;
    pipelineDesc.vertex.module = module;
    pipelineDesc.vertex.entryPoint = "vert_main";
    pipelineDesc.cFragment.module = module;
    pipelineDesc.cFragment.entryPoint = "frag_main";
    pipelineDesc.cTargets[0].format = colorTarget.GetFormat();

    wgpu::RenderPipeline pipeline = deviceObj.CreateRenderPipeline(&pipelineDesc);

    wgpu::BindGroup bindGroup = utils::MakeBindGroup(deviceObj, pipeline.GetBindGroupLayout(0),
                                                     {{0, texture.CreateView()}});

    wgpu::CommandEncoder encoder = deviceObj.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor passDescriptor({colorTarget.CreateView()});
    passDescriptor.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    passDescriptor.cColorAttachments[0].clearValue = {1.0, 1.0, 1.0, 1.0};
    passDescriptor.cColorAttachments[0].storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.Draw(6);
    pass.End();
    return {encoder.Finish(), colorTarget};
}

void SharedTextureMemoryTests::CheckFourColors(wgpu::Device& deviceObj,
                                               wgpu::TextureFormat format,
                                               wgpu::Texture& colorTarget) {
    wgpu::Origin3D tl = {colorTarget.GetWidth() / 4, colorTarget.GetHeight() / 4};
    wgpu::Origin3D bl = {colorTarget.GetWidth() / 4, 3 * colorTarget.GetHeight() / 4};
    wgpu::Origin3D tr = {3 * colorTarget.GetWidth() / 4, colorTarget.GetHeight() / 4};
    wgpu::Origin3D br = {3 * colorTarget.GetWidth() / 4, 3 * colorTarget.GetHeight() / 4};

    switch (format) {
        case wgpu::TextureFormat::RGBA8Unorm:
        case wgpu::TextureFormat::BGRA8Unorm:
        case wgpu::TextureFormat::RGB10A2Unorm:
        case wgpu::TextureFormat::RGBA16Float:
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kGreen, colorTarget, tl, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kRed, colorTarget, bl, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kBlue, colorTarget, tr, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kYellow, colorTarget, br, {1, 1});
            break;
        case wgpu::TextureFormat::RG16Float:
        case wgpu::TextureFormat::RG8Unorm:
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kGreen, colorTarget, tl, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kRed, colorTarget, bl, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kBlack, colorTarget, tr, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kYellow, colorTarget, br, {1, 1});
            break;
        case wgpu::TextureFormat::R16Float:
        case wgpu::TextureFormat::R8Unorm:
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kBlack, colorTarget, tl, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kRed, colorTarget, bl, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kBlack, colorTarget, tr, {1, 1});
            EXPECT_TEXTURE_EQ(deviceObj, &utils::RGBA8::kRed, colorTarget, br, {1, 1});
            break;
        default:
            UNREACHABLE();
    }
}

// Allow tests to be uninstantiated since it's possible no backends are available.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SharedTextureMemoryNoFeatureTests);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SharedTextureMemoryTests);

namespace {

// Test that creating shared texture memory without the required features is an error.
// Using the memory thereafter produces errors.
TEST_P(SharedTextureMemoryNoFeatureTests, CreationWithoutFeature) {
    // Create external texture memories with an error filter.
    // We should see a message that the feature is not enabled.
    device.PushErrorScope(wgpu::ErrorFilter::Validation);
    const auto& memories = GetParam().mBackend->CreateSharedTextureMemories(device);

    testing::MockCallback<WGPUErrorCallback> popErrorScopeCallback;
    EXPECT_CALL(popErrorScopeCallback,
                Call(WGPUErrorType_Validation, testing::HasSubstr("is not enabled"), this));

    device.PopErrorScope(popErrorScopeCallback.Callback(),
                         popErrorScopeCallback.MakeUserdata(this));

    for (wgpu::SharedTextureMemory memory : memories) {
        wgpu::SharedTextureMemoryProperties properties;
        ASSERT_DEVICE_ERROR_MSG(memory.GetProperties(&properties),
                                testing::HasSubstr("is invalid"));

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.usage = properties.usage;
        textureDesc.size = properties.size;

        ASSERT_DEVICE_ERROR_MSG(wgpu::Texture texture = memory.CreateTexture(&textureDesc),
                                testing::HasSubstr("is invalid"));

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = true;

        ASSERT_DEVICE_ERROR_MSG(memory.BeginAccess(texture, &beginDesc),
                                testing::HasSubstr("is invalid"));

        wgpu::SharedTextureMemoryEndAccessState endState = {};
        ASSERT_DEVICE_ERROR_MSG(memory.EndAccess(texture, &endState),
                                testing::HasSubstr("is invalid"));
    }
}

// Test that it is an error the import a shared texture memory with no chained struct.
TEST_P(SharedTextureMemoryTests, ImportSharedTextureMemoryNoChain) {
    wgpu::SharedTextureMemoryDescriptor desc;
    ASSERT_DEVICE_ERROR_MSG(
        wgpu::SharedTextureMemory memory = device.ImportSharedTextureMemory(&desc),
        testing::HasSubstr("chain"));
}

// Test that it is an error the import a shared fence with no chained struct.
// Also test that ExportInfo reports an Undefined type for the error fence.
TEST_P(SharedTextureMemoryTests, ImportSharedFenceNoChain) {
    wgpu::SharedFenceDescriptor desc;
    ASSERT_DEVICE_ERROR_MSG(wgpu::SharedFence fence = device.ImportSharedFence(&desc),
                            testing::HasSubstr("Unsupported"));

    wgpu::SharedFenceExportInfo exportInfo;
    exportInfo.type = static_cast<wgpu::SharedFenceType>(1234);  // should be overrwritten

    // Expect that exporting the fence info writes Undefined, and generates an error.
    ASSERT_DEVICE_ERROR(fence.ExportInfo(&exportInfo));
    EXPECT_EQ(exportInfo.type, wgpu::SharedFenceType::Undefined);
}

// Test that it is an error the import a shared texture memory when the device is destroyed
TEST_P(SharedTextureMemoryTests, ImportSharedTextureMemoryDeviceDestroyed) {
    device.Destroy();

    wgpu::SharedTextureMemoryDescriptor desc;
    ASSERT_DEVICE_ERROR_MSG(
        wgpu::SharedTextureMemory memory = device.ImportSharedTextureMemory(&desc),
        testing::HasSubstr("lost"));
}

// Test that it is an error the import a shared fence when the device is destroyed
TEST_P(SharedTextureMemoryTests, ImportSharedFenceDeviceDestroyed) {
    device.Destroy();

    wgpu::SharedFenceDescriptor desc;
    ASSERT_DEVICE_ERROR_MSG(wgpu::SharedFence fence = device.ImportSharedFence(&desc),
                            testing::HasSubstr("lost"));
}

// Test calling GetProperties with an invalid chained struct. An error is
// generated, but the properties are still populated.
TEST_P(SharedTextureMemoryTests, GetPropertiesInvalidChain) {
    wgpu::SharedTextureMemory memory = GetParam().mBackend->CreateSharedTextureMemory(device);

    wgpu::SharedFenceVkSemaphoreOpaqueFDExportInfo vkInfo;
    wgpu::SharedTextureMemoryProperties properties1;
    properties1.nextInChain = &vkInfo;
    ASSERT_DEVICE_ERROR(memory.GetProperties(&properties1));

    wgpu::SharedTextureMemoryProperties properties2;
    memory.GetProperties(&properties2);

    EXPECT_EQ(properties1.usage, properties2.usage);
    EXPECT_EQ(properties1.size.width, properties2.size.width);
    EXPECT_EQ(properties1.size.height, properties2.size.height);
    EXPECT_EQ(properties1.size.depthOrArrayLayers, properties2.size.depthOrArrayLayers);
    EXPECT_EQ(properties1.format, properties2.format);
}

// Test that texture usages must be a subset of the shared texture memory's usage.
TEST_P(SharedTextureMemoryTests, UsageValidation) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        // SharedTextureMemory should never support TransientAttachment.
        ASSERT_EQ(properties.usage & wgpu::TextureUsage::TransientAttachment, 0);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;

        for (wgpu::TextureUsage usage : {
                 wgpu::TextureUsage::CopySrc,
                 wgpu::TextureUsage::CopyDst,
                 wgpu::TextureUsage::TextureBinding,
                 wgpu::TextureUsage::StorageBinding,
                 wgpu::TextureUsage::RenderAttachment,
             }) {
            textureDesc.usage = usage;

            // `usage` is valid if it is in the shared texture memory properties.
            if (usage & properties.usage) {
                memory.CreateTexture(&textureDesc);
            } else {
                ASSERT_DEVICE_ERROR(memory.CreateTexture(&textureDesc));
            }
        }
    }
}

// Test that it is an error if the texture format doesn't match the shared texture memory.
TEST_P(SharedTextureMemoryTests, FormatValidation) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format != wgpu::TextureFormat::RGBA8Unorm
                                 ? wgpu::TextureFormat::RGBA8Unorm
                                 : wgpu::TextureFormat::RGBA16Float;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        ASSERT_DEVICE_ERROR_MSG(memory.CreateTexture(&textureDesc),
                                testing::HasSubstr("doesn't match descriptor format"));
    }
}

// Test that it is an error if the texture size doesn't match the shared texture memory.
TEST_P(SharedTextureMemoryTests, SizeValidation) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.usage = properties.usage;

        textureDesc.size = {properties.size.width + 1, properties.size.height,
                            properties.size.depthOrArrayLayers};
        ASSERT_DEVICE_ERROR_MSG(memory.CreateTexture(&textureDesc),
                                testing::HasSubstr("doesn't match descriptor size"));

        textureDesc.size = {properties.size.width, properties.size.height + 1,
                            properties.size.depthOrArrayLayers};
        ASSERT_DEVICE_ERROR_MSG(memory.CreateTexture(&textureDesc),
                                testing::HasSubstr("doesn't match descriptor size"));

        textureDesc.size = {properties.size.width, properties.size.height,
                            properties.size.depthOrArrayLayers + 1};
        ASSERT_DEVICE_ERROR_MSG(memory.CreateTexture(&textureDesc), testing::HasSubstr("is not 1"));
    }
}

// Test that it is an error if the texture mip level count is not 1.
TEST_P(SharedTextureMemoryTests, MipLevelValidation) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.usage = properties.usage;
        textureDesc.size = properties.size;
        textureDesc.mipLevelCount = 1u;

        memory.CreateTexture(&textureDesc);

        textureDesc.mipLevelCount = 2u;
        ASSERT_DEVICE_ERROR_MSG(memory.CreateTexture(&textureDesc),
                                testing::HasSubstr("(2) is not 1"));
    }
}

// Test that it is an error if the texture sample count is not 1.
TEST_P(SharedTextureMemoryTests, SampleCountValidation) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.usage = properties.usage;
        textureDesc.size = properties.size;
        textureDesc.sampleCount = 1u;

        memory.CreateTexture(&textureDesc);

        textureDesc.sampleCount = 4u;
        ASSERT_DEVICE_ERROR_MSG(memory.CreateTexture(&textureDesc),
                                testing::HasSubstr("(4) is not 1"));
    }
}

// Test that it is an error if the texture dimension is not 2D.
TEST_P(SharedTextureMemoryTests, DimensionValidation) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.usage = properties.usage;
        textureDesc.size = properties.size;

        textureDesc.dimension = wgpu::TextureDimension::e1D;
        ASSERT_DEVICE_ERROR_MSG(memory.CreateTexture(&textureDesc),
                                testing::HasSubstr("is not TextureDimension::e2D"));

        textureDesc.dimension = wgpu::TextureDimension::e3D;
        ASSERT_DEVICE_ERROR_MSG(memory.CreateTexture(&textureDesc),
                                testing::HasSubstr("is not TextureDimension::e2D"));
    }
}

// Test that it is an error to call BeginAccess twice in a row on the same texture and memory.
TEST_P(SharedTextureMemoryTests, DoubleBeginAccess) {
    wgpu::SharedTextureMemory memory = GetParam().mBackend->CreateSharedTextureMemory(device);

    wgpu::SharedTextureMemoryProperties properties;
    memory.GetProperties(&properties);

    wgpu::TextureDescriptor textureDesc = {};
    textureDesc.format = properties.format;
    textureDesc.size = properties.size;
    textureDesc.usage = properties.usage;

    wgpu::Texture texture = memory.CreateTexture(&textureDesc);

    wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
    beginDesc.initialized = true;

    // It should be an error to BeginAccess twice in a row.
    memory.BeginAccess(texture, &beginDesc);
    ASSERT_DEVICE_ERROR_MSG(memory.BeginAccess(texture, &beginDesc),
                            testing::HasSubstr("Cannot begin access with"));
}

// Test that it is an error to call BeginAccess twice in a row on two textures from the same memory.
TEST_P(SharedTextureMemoryTests, DoubleBeginAccessSeparateTextures) {
    wgpu::SharedTextureMemory memory = GetParam().mBackend->CreateSharedTextureMemory(device);

    wgpu::SharedTextureMemoryProperties properties;
    memory.GetProperties(&properties);

    wgpu::TextureDescriptor textureDesc = {};
    textureDesc.format = properties.format;
    textureDesc.size = properties.size;
    textureDesc.usage = properties.usage;

    wgpu::Texture texture1 = memory.CreateTexture(&textureDesc);
    wgpu::Texture texture2 = memory.CreateTexture(&textureDesc);

    wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
    beginDesc.initialized = true;

    // It should be an error to BeginAccess twice in a row.
    memory.BeginAccess(texture1, &beginDesc);
    ASSERT_DEVICE_ERROR_MSG(memory.BeginAccess(texture2, &beginDesc),
                            testing::HasSubstr("Cannot begin access with"));
}

// Test that it is an error to call EndAccess twice in a row on the same memory.
TEST_P(SharedTextureMemoryTests, DoubleEndAccess) {
    wgpu::SharedTextureMemory memory = GetParam().mBackend->CreateSharedTextureMemory(device);

    wgpu::SharedTextureMemoryProperties properties;
    memory.GetProperties(&properties);

    wgpu::TextureDescriptor textureDesc = {};
    textureDesc.format = properties.format;
    textureDesc.size = properties.size;
    textureDesc.usage = properties.usage;

    wgpu::Texture texture = memory.CreateTexture(&textureDesc);

    wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
    beginDesc.initialized = true;

    memory.BeginAccess(texture, &beginDesc);

    wgpu::SharedTextureMemoryEndAccessState endState = {};
    memory.EndAccess(texture, &endState);

    // Invalid to end access a second time.
    ASSERT_DEVICE_ERROR_MSG(memory.EndAccess(texture, &endState),
                            testing::HasSubstr("Cannot end access"));
}

// Test that it is an error to call EndAccess without a preceding BeginAccess.
TEST_P(SharedTextureMemoryTests, EndAccessWithoutBegin) {
    wgpu::SharedTextureMemory memory = GetParam().mBackend->CreateSharedTextureMemory(device);

    wgpu::SharedTextureMemoryProperties properties;
    memory.GetProperties(&properties);

    wgpu::TextureDescriptor textureDesc = {};
    textureDesc.format = properties.format;
    textureDesc.size = properties.size;
    textureDesc.usage = properties.usage;

    wgpu::Texture texture = memory.CreateTexture(&textureDesc);

    wgpu::SharedTextureMemoryEndAccessState endState = {};
    ASSERT_DEVICE_ERROR_MSG(memory.EndAccess(texture, &endState),
                            testing::HasSubstr("Cannot end access"));
}

// Test that it is an error to use the texture on the queue without a preceding BeginAccess.
TEST_P(SharedTextureMemoryTests, UseWithoutBegin) {
    DAWN_TEST_UNSUPPORTED_IF(HasToggleEnabled("skip_validation"));

    wgpu::SharedTextureMemory memory = GetParam().mBackend->CreateSharedTextureMemory(device);

    wgpu::SharedTextureMemoryProperties properties;
    memory.GetProperties(&properties);

    wgpu::TextureDescriptor textureDesc = {};
    textureDesc.format = properties.format;
    textureDesc.size = properties.size;
    textureDesc.usage = properties.usage;

    wgpu::Texture texture = memory.CreateTexture(&textureDesc);

    if (textureDesc.usage & wgpu::TextureUsage::RenderAttachment) {
        ASSERT_DEVICE_ERROR_MSG(UseInRenderPass(device, texture),
                                testing::HasSubstr("without current access"));
    } else if (properties.format != wgpu::TextureFormat::R8BG8Biplanar420Unorm) {
        if (textureDesc.usage & wgpu::TextureUsage::CopySrc) {
            ASSERT_DEVICE_ERROR_MSG(UseInCopy(device, texture),
                                    testing::HasSubstr("without current access"));
        }
        if (textureDesc.usage & wgpu::TextureUsage::CopyDst) {
            wgpu::Extent3D writeSize = {1, 1, 1};
            wgpu::ImageCopyTexture dest = {};
            dest.texture = texture;
            wgpu::TextureDataLayout dataLayout = {};
            uint64_t data[2];
            ASSERT_DEVICE_ERROR_MSG(
                device.GetQueue().WriteTexture(&dest, &data, sizeof(data), &dataLayout, &writeSize),
                testing::HasSubstr("without current access"));
        }
    }
}

// Fences are tracked by BeginAccess regardless of whether or not the operation
// was successful. In error conditions, the same fences are returned in EndAccess, so that
// the caller can free them (the implementation did not consume them), and the wait condition
// isn't dropped on the floor entirely.
// If there are invalid nested accesses, forwarding waits for the invalid accesses still occurs.
// The mental model is that there is a stack of scopes per (memory, texture) pair.
TEST_P(SharedTextureMemoryTests, AccessStack) {
    const auto& memories = GetParam().mBackend->CreateSharedTextureMemories(device);
    ASSERT_GT(memories.size(), 1u);

    for (size_t i = 0; i < memories.size(); ++i) {
        wgpu::SharedTextureMemoryProperties properties;
        memories[i].GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        // Create multiple textures for use in the test.
        wgpu::Texture texture1 = memories[i].CreateTexture(&textureDesc);
        wgpu::Texture texture2 = memories[i].CreateTexture(&textureDesc);
        wgpu::Texture texture3 = memories[i].CreateTexture(&textureDesc);
        wgpu::Texture texture4 = memories[i].CreateTexture(&textureDesc);
        wgpu::Texture texture5 = memories[i].CreateTexture(&textureDesc);

        std::vector<wgpu::SharedFence> fences;
        std::vector<uint64_t> signaledValues;

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        wgpu::SharedTextureMemoryEndAccessState endState;
        beginDesc.initialized = true;

        auto CheckFencesMatch = [&](const wgpu::SharedTextureMemoryBeginAccessDescriptor& begin,
                                    const wgpu::SharedTextureMemoryEndAccessState& end) {
            ASSERT_EQ(begin.fenceCount, end.fenceCount);
            for (size_t j = 0; j < end.fenceCount; ++j) {
                EXPECT_EQ(begin.fences[j].Get(), end.fences[j].Get());
                EXPECT_EQ(begin.signaledValues[j], end.signaledValues[j]);
            }
        };

        // Begin/EndAccess to generate multiple fences.
        while (fences.size() < 7) {
            endState = {};
            memories[i].BeginAccess(texture1, &beginDesc);
            memories[i].EndAccess(texture1, &endState);

            ASSERT_GT(endState.fenceCount, 0u);
            for (size_t j = 0; j < endState.fenceCount; ++j) {
                fences.push_back(std::move(endState.fences[j]));
                signaledValues.push_back(endState.signaledValues[j]);
            }
        }

        // Begin access on memories[i], texture1 using the first fence.
        auto ti1BeginDesc = beginDesc = {};
        ti1BeginDesc.initialized = true;
        ti1BeginDesc.fenceCount = 1;
        ti1BeginDesc.fences = &fences[0];
        ti1BeginDesc.signaledValues = &signaledValues[0];
        memories[i].BeginAccess(texture1, &ti1BeginDesc);

        // Begin access on memories[i], texture2 with no fences.
        auto ti2BeginDesc = beginDesc = {};
        ti2BeginDesc.fenceCount = 0;
        ASSERT_DEVICE_ERROR(memories[i].BeginAccess(texture2, &ti2BeginDesc));

        // Begin access on memories[i], texture3 with two fences.
        auto ti3BeginDesc = beginDesc = {};
        ti3BeginDesc.fenceCount = 2;
        ti3BeginDesc.fences = &fences[1];
        ti3BeginDesc.signaledValues = &signaledValues[1];
        ASSERT_DEVICE_ERROR(memories[i].BeginAccess(texture3, &ti3BeginDesc));

        auto tj3BeginDesc = beginDesc = {};
        if (i + 1 < memories.size()) {
            // Begin access on memories[i + 1], texture3 with one fence.
            tj3BeginDesc.fenceCount = 1;
            tj3BeginDesc.fences = &fences[3];
            tj3BeginDesc.signaledValues = &signaledValues[3];
            ASSERT_DEVICE_ERROR(memories[i + 1].BeginAccess(texture3, &tj3BeginDesc));
        }

        // End access on memories[i], texture2.
        // Expect the same fences from the BeginAccess operation.
        ASSERT_DEVICE_ERROR(memories[i].EndAccess(texture2, &endState));
        CheckFencesMatch(ti2BeginDesc, endState);

        // End access on memories[i], texture1. The begin was valid.
        // This should be valid too.
        memories[i].EndAccess(texture1, &endState);

        // Begin access on memories[i], texture4 with two fences.
        auto ti4BeginDesc = beginDesc = {};
        ti4BeginDesc.initialized = true;
        ti4BeginDesc.fenceCount = 1;
        ti4BeginDesc.fences = &fences[4];
        ti4BeginDesc.signaledValues = &signaledValues[4];
        memories[i].BeginAccess(texture4, &ti4BeginDesc);

        auto tj5BeginDesc = beginDesc = {};
        if (i + 1 < memories.size()) {
            // Begin access on memories[i + 1], texture5 with one fence.
            tj5BeginDesc.fenceCount = 1;
            tj5BeginDesc.fences = &fences[6];
            tj5BeginDesc.signaledValues = &signaledValues[6];
            ASSERT_DEVICE_ERROR(memories[i + 1].BeginAccess(texture5, &tj5BeginDesc));

            // End access on memories[i + 1], texture3.
            ASSERT_DEVICE_ERROR(memories[i + 1].EndAccess(texture3, &endState));
            CheckFencesMatch(tj3BeginDesc, endState);
        }

        // End access on memories[i], texture3.
        ASSERT_DEVICE_ERROR(memories[i].EndAccess(texture3, &endState));
        CheckFencesMatch(ti3BeginDesc, endState);

        // EndAccess on memories[i], texture4.
        memories[i].EndAccess(texture4, &endState);

        if (i + 1 < memories.size()) {
            // End access on memories[i + 1], texture5.
            ASSERT_DEVICE_ERROR(memories[i + 1].EndAccess(texture5, &endState));
            CheckFencesMatch(tj5BeginDesc, endState);
        }
    }
}

// Test that it is an error to call BeginAccess on a texture that wasn't created from the same
// memory.
TEST_P(SharedTextureMemoryTests, MismatchingMemory) {
    const auto& memories = GetParam().mBackend->CreateSharedTextureMemories(device);
    wgpu::SharedTextureMemory otherMemory = GetParam().mBackend->CreateSharedTextureMemory(device);
    for (size_t i = 0; i < memories.size(); ++i) {
        wgpu::SharedTextureMemoryProperties properties;
        memories[i].GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        wgpu::Texture texture = memories[i].CreateTexture(&textureDesc);

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = true;

        ASSERT_DEVICE_ERROR_MSG(otherMemory.BeginAccess(texture, &beginDesc),
                                testing::HasSubstr("cannot be used with"));

        // End access so the access scope is balanced.
        wgpu::SharedTextureMemoryEndAccessState endState;
        ASSERT_DEVICE_ERROR_MSG(otherMemory.EndAccess(texture, &endState),
                                testing::HasSubstr("cannot be used with"));
    }
}

// Test that it is valid (does not crash) if the memory is dropped while a texture access has begun.
TEST_P(SharedTextureMemoryTests, TextureAccessOutlivesMemory) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = true;

        // Begin access on a texture, and drop the memory.
        wgpu::Texture texture = memory.CreateTexture(&textureDesc);
        memory.BeginAccess(texture, &beginDesc);
        memory = nullptr;

        // Use the texture on the GPU; it should not crash.
        if (textureDesc.usage & wgpu::TextureUsage::RenderAttachment) {
            UseInRenderPass(device, texture);
        } else if (properties.format != wgpu::TextureFormat::R8BG8Biplanar420Unorm) {
            ASSERT(textureDesc.usage & wgpu::TextureUsage::CopySrc);
            UseInCopy(device, texture);
        }
    }
}

// Test that if the texture is uninitialized, it is cleared on first use.
TEST_P(SharedTextureMemoryTests, UninitializedTextureIsCleared) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        // Skipped for multiplanar formats because those must be initialized on import.
        if (properties.format == wgpu::TextureFormat::R8BG8Biplanar420Unorm) {
            continue;
        }

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        wgpu::Texture texture = memory.CreateTexture(&textureDesc);

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = false;
        memory.BeginAccess(texture, &beginDesc);

        // Use the texture on the GPU which should lazy clear it.
        if (textureDesc.usage & wgpu::TextureUsage::RenderAttachment) {
            UseInRenderPass(device, texture);
        } else {
            ASSERT(textureDesc.usage & wgpu::TextureUsage::CopySrc);
            UseInCopy(device, texture);
        }

        wgpu::SharedTextureMemoryEndAccessState endState = {};
        endState.initialized = false;  // should be overrwritten
        memory.EndAccess(texture, &endState);
        // The texture should be initialized now.
        EXPECT_TRUE(endState.initialized);
    }
}

// Test that if the texture is uninitialized, EndAccess writes the state
// out as uninitialized.
TEST_P(SharedTextureMemoryTests, UninitializedOnEndAccess) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
        wgpu::SharedTextureMemoryProperties properties;
        memory.GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        // Test basic begin+end access exports the state as uninitialized
        // if it starts as uninitialized. Skipped for multiplanar formats
        // because those must be initialized on import.
        if (textureDesc.format != wgpu::TextureFormat::R8BG8Biplanar420Unorm) {
            wgpu::Texture texture = memory.CreateTexture(&textureDesc);

            wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
            beginDesc.initialized = false;
            memory.BeginAccess(texture, &beginDesc);

            wgpu::SharedTextureMemoryEndAccessState endState = {};
            endState.initialized = true;  // should be overrwritten
            memory.EndAccess(texture, &endState);
            EXPECT_FALSE(endState.initialized);
        }

        // Test begin access as initialized, then uninitializing the texture
        // exports the state as uninitialized on end access. Requires render
        // attachment usage to uninitialize.
        if (properties.usage & wgpu::TextureUsage::RenderAttachment) {
            wgpu::Texture texture = memory.CreateTexture(&textureDesc);

            wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
            beginDesc.initialized = true;
            memory.BeginAccess(texture, &beginDesc);

            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            utils::ComboRenderPassDescriptor passDescriptor({texture.CreateView()});
            passDescriptor.cColorAttachments[0].storeOp = wgpu::StoreOp::Discard;

            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
            pass.End();
            wgpu::CommandBuffer commandBuffer = encoder.Finish();
            device.GetQueue().Submit(1, &commandBuffer);

            wgpu::SharedTextureMemoryEndAccessState endState = {};
            endState.initialized = true;  // should be overrwritten
            memory.EndAccess(texture, &endState);
            EXPECT_FALSE(endState.initialized);
        }
    }
}

// Test rendering to a texture memory on one device, then sampling it using another device.
// Encode the commands after performing BeginAccess.
TEST_P(SharedTextureMemoryTests, RenderThenSampleEncodeAfterBeginAccess) {
    std::vector<wgpu::Device> devices = {device, CreateDevice()};

    for (const auto& memories :
         GetParam().mBackend->CreatePerDeviceSharedTextureMemories(devices)) {
        wgpu::SharedTextureMemoryProperties properties;
        memories[0].GetProperties(&properties);

        constexpr wgpu::TextureUsage kRequiredUsage =
            wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        if ((properties.usage & kRequiredUsage) != kRequiredUsage) {
            continue;
        }

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        // Clear the texture

        wgpu::Texture texture = memories[0].CreateTexture(&textureDesc);

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = false;
        memories[0].BeginAccess(texture, &beginDesc);

        wgpu::CommandBuffer commandBuffer = MakeFourColorsClearCommandBuffer(devices[0], texture);
        devices[0].GetQueue().Submit(1, &commandBuffer);

        wgpu::SharedTextureMemoryEndAccessState endState = {};
        memories[0].EndAccess(texture, &endState);

        // Sample from the texture

        std::vector<wgpu::SharedFence> sharedFences;
        for (size_t i = 0; i < endState.fenceCount; ++i) {
            sharedFences.push_back(
                GetParam().mBackend->ImportFenceTo(devices[1], endState.fences[i]));
        }
        beginDesc.fenceCount = endState.fenceCount;
        beginDesc.fences = sharedFences.data();
        beginDesc.signaledValues = endState.signaledValues;
        beginDesc.initialized = endState.initialized;

        texture = memories[1].CreateTexture(&textureDesc);

        memories[1].BeginAccess(texture, &beginDesc);

        wgpu::Texture colorTarget;
        std::tie(commandBuffer, colorTarget) =
            MakeCheckFourColorsCommandBuffer(devices[1], texture);
        devices[1].GetQueue().Submit(1, &commandBuffer);
        memories[1].EndAccess(texture, &endState);

        CheckFourColors(devices[1], texture.GetFormat(), colorTarget);
    }
}

// Test rendering to a texture memory on one device, then sampling it using another device.
// Encode the commands before performing BeginAccess (the access is only held during) QueueSubmit.
TEST_P(SharedTextureMemoryTests, RenderThenSampleEncodeBeforeBeginAccess) {
    std::vector<wgpu::Device> devices = {device, CreateDevice()};
    for (const auto& memories :
         GetParam().mBackend->CreatePerDeviceSharedTextureMemories(devices)) {
        wgpu::SharedTextureMemoryProperties properties;
        memories[0].GetProperties(&properties);

        constexpr wgpu::TextureUsage kRequiredUsage =
            wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        if ((properties.usage & kRequiredUsage) != kRequiredUsage) {
            continue;
        }

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        // Create two textures from each memory.
        wgpu::Texture textures[] = {memories[0].CreateTexture(&textureDesc),
                                    memories[1].CreateTexture(&textureDesc)};

        // Make two command buffers, one that clears the texture, another that samples.
        wgpu::CommandBuffer commandBuffer0 =
            MakeFourColorsClearCommandBuffer(devices[0], textures[0]);
        auto [commandBuffer1, colorTarget] =
            MakeCheckFourColorsCommandBuffer(devices[1], textures[1]);

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = false;
        memories[0].BeginAccess(textures[0], &beginDesc);

        devices[0].GetQueue().Submit(1, &commandBuffer0);

        wgpu::SharedTextureMemoryEndAccessState endState = {};
        memories[0].EndAccess(textures[0], &endState);

        std::vector<wgpu::SharedFence> sharedFences;
        for (size_t i = 0; i < endState.fenceCount; ++i) {
            sharedFences.push_back(
                GetParam().mBackend->ImportFenceTo(devices[1], endState.fences[i]));
        }
        beginDesc.fenceCount = endState.fenceCount;
        beginDesc.fences = sharedFences.data();
        beginDesc.signaledValues = endState.signaledValues;
        beginDesc.initialized = endState.initialized;

        memories[1].BeginAccess(textures[1], &beginDesc);
        devices[1].GetQueue().Submit(1, &commandBuffer1);
        memories[1].EndAccess(textures[1], &endState);

        CheckFourColors(devices[1], textures[1].GetFormat(), colorTarget);
    }
}

// Test rendering to a texture memory on one device, then sampling it using another device.
// Destroy the texture from the first device after submitting the commands, but before perorming
// EndAccess. The second device should still be able to wait on the first device and see the
// results.
TEST_P(SharedTextureMemoryTests, RenderThenTextureDestroyBeforeEndAccessThenSample) {
    std::vector<wgpu::Device> devices = {device, CreateDevice()};
    for (const auto& memories :
         GetParam().mBackend->CreatePerDeviceSharedTextureMemories(devices)) {
        wgpu::SharedTextureMemoryProperties properties;
        memories[0].GetProperties(&properties);

        constexpr wgpu::TextureUsage kRequiredUsage =
            wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        if ((properties.usage & kRequiredUsage) != kRequiredUsage) {
            continue;
        }

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        // Create two textures from each memory.
        wgpu::Texture textures[] = {memories[0].CreateTexture(&textureDesc),
                                    memories[1].CreateTexture(&textureDesc)};

        // Make two command buffers, one that clears the texture, another that samples.
        wgpu::CommandBuffer commandBuffer0 =
            MakeFourColorsClearCommandBuffer(devices[0], textures[0]);
        auto [commandBuffer1, colorTarget] =
            MakeCheckFourColorsCommandBuffer(devices[1], textures[1]);

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = false;
        memories[0].BeginAccess(textures[0], &beginDesc);

        devices[0].GetQueue().Submit(1, &commandBuffer0);

        // Destroy the texture before performing EndAccess.
        textures[0].Destroy();

        wgpu::SharedTextureMemoryEndAccessState endState = {};
        memories[0].EndAccess(textures[0], &endState);

        std::vector<wgpu::SharedFence> sharedFences;
        for (size_t i = 0; i < endState.fenceCount; ++i) {
            sharedFences.push_back(
                GetParam().mBackend->ImportFenceTo(devices[1], endState.fences[i]));
        }
        beginDesc.fenceCount = endState.fenceCount;
        beginDesc.fences = sharedFences.data();
        beginDesc.signaledValues = endState.signaledValues;
        beginDesc.initialized = endState.initialized;

        memories[1].BeginAccess(textures[1], &beginDesc);
        devices[1].GetQueue().Submit(1, &commandBuffer1);
        memories[1].EndAccess(textures[1], &endState);

        CheckFourColors(devices[1], textures[1].GetFormat(), colorTarget);
    }
}

// Test rendering to a texture memory on one device, then sampling it using another device.
// Destroy the first device after submitting the commands, but before perorming
// EndAccess. The second device should still be able to wait on the first device and see the
// results.
TEST_P(SharedTextureMemoryTests, RenderThenDeviceDestroyBeforeEndAccessThenSample) {
    std::vector<wgpu::Device> devices = {device, CreateDevice()};
    for (const auto& memories :
         GetParam().mBackend->CreatePerDeviceSharedTextureMemories(devices)) {
        wgpu::SharedTextureMemoryProperties properties;
        memories[0].GetProperties(&properties);

        constexpr wgpu::TextureUsage kRequiredUsage =
            wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        if ((properties.usage & kRequiredUsage) != kRequiredUsage) {
            continue;
        }

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        // Create two textures from each memory.
        wgpu::Texture textures[] = {memories[0].CreateTexture(&textureDesc),
                                    memories[1].CreateTexture(&textureDesc)};

        // Make two command buffers, one that clears the texture, another that samples.
        wgpu::CommandBuffer commandBuffer0 =
            MakeFourColorsClearCommandBuffer(devices[0], textures[0]);
        auto [commandBuffer1, colorTarget] =
            MakeCheckFourColorsCommandBuffer(devices[1], textures[1]);

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = false;
        memories[0].BeginAccess(textures[0], &beginDesc);

        devices[0].GetQueue().Submit(1, &commandBuffer0);

        // Destroy the device before performing EndAccess.
        devices[0].Destroy();

        wgpu::SharedTextureMemoryEndAccessState endState = {};
        memories[0].EndAccess(textures[0], &endState);
        EXPECT_GT(endState.fenceCount, 0u);

        std::vector<wgpu::SharedFence> sharedFences;
        for (size_t i = 0; i < endState.fenceCount; ++i) {
            sharedFences.push_back(
                GetParam().mBackend->ImportFenceTo(devices[1], endState.fences[i]));
        }
        beginDesc.fenceCount = endState.fenceCount;
        beginDesc.fences = sharedFences.data();
        beginDesc.signaledValues = endState.signaledValues;
        beginDesc.initialized = endState.initialized;

        memories[1].BeginAccess(textures[1], &beginDesc);
        devices[1].GetQueue().Submit(1, &commandBuffer1);
        memories[1].EndAccess(textures[1], &endState);

        CheckFourColors(devices[1], textures[1].GetFormat(), colorTarget);

        // Skip remaining variants. The device is destroyed so they can't be tested.
        return;
    }
}

// Test rendering to a texture memory on one device, then sampling it using another device.
// Lose the first device after submitting the commands, but before perorming
// EndAccess. The second device should still be able to wait on the first device and see the
// results.
TEST_P(SharedTextureMemoryTests, RenderThenLoseDeviceBeforeEndAccessThenSample) {
    // TODO(crbug.cm/dawn/1745) Hangs on Metal.
    DAWN_SUPPRESS_TEST_IF(IsMetal());

    std::vector<wgpu::Device> devices = {device, CreateDevice()};
    for (const auto& memories :
         GetParam().mBackend->CreatePerDeviceSharedTextureMemories(devices)) {
        wgpu::SharedTextureMemoryProperties properties;
        memories[0].GetProperties(&properties);

        constexpr wgpu::TextureUsage kRequiredUsage =
            wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
        if ((properties.usage & kRequiredUsage) != kRequiredUsage) {
            continue;
        }

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        // Create two textures from each memory.
        wgpu::Texture textures[] = {memories[0].CreateTexture(&textureDesc),
                                    memories[1].CreateTexture(&textureDesc)};

        // Make two command buffers, one that clears the texture, another that samples.
        wgpu::CommandBuffer commandBuffer0 =
            MakeFourColorsClearCommandBuffer(devices[0], textures[0]);
        auto [commandBuffer1, colorTarget] =
            MakeCheckFourColorsCommandBuffer(devices[1], textures[1]);

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = false;
        memories[0].BeginAccess(textures[0], &beginDesc);

        devices[0].GetQueue().Submit(1, &commandBuffer0);

        // Lose the device before performing EndAccess.
        LoseDeviceForTesting(devices[0]);

        wgpu::SharedTextureMemoryEndAccessState endState = {};
        memories[0].EndAccess(textures[0], &endState);
        EXPECT_GT(endState.fenceCount, 0u);

        std::vector<wgpu::SharedFence> sharedFences;
        for (size_t i = 0; i < endState.fenceCount; ++i) {
            sharedFences.push_back(
                GetParam().mBackend->ImportFenceTo(devices[1], endState.fences[i]));
        }
        beginDesc.fenceCount = endState.fenceCount;
        beginDesc.fences = sharedFences.data();
        beginDesc.signaledValues = endState.signaledValues;
        beginDesc.initialized = endState.initialized;

        memories[1].BeginAccess(textures[1], &beginDesc);
        devices[1].GetQueue().Submit(1, &commandBuffer1);
        memories[1].EndAccess(textures[1], &endState);

        CheckFourColors(devices[1], textures[1].GetFormat(), colorTarget);

        // Skip remaining variants. The device is lost so they can't be tested.
        return;
    }
}

}  // anonymous namespace
}  // namespace dawn
