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

#include "dawn/tests/white_box/SharedTextureMemoryTests.h"

#include "dawn/tests/MockCallback.h"
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

void SharedTextureMemoryTests::SetUp() {
    DawnTestWithParams<SharedTextureMemoryTestParams>::SetUp();
    DAWN_TEST_UNSUPPORTED_IF(UsesWire());
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

wgpu::CommandBuffer SharedTextureMemoryTests::MakeClearCommandBuffer(wgpu::Device& deviceObj,
                                                                     wgpu::Texture& texture,
                                                                     wgpu::Color clearValue) {
    wgpu::CommandEncoder encoder = deviceObj.CreateCommandEncoder();
    utils::ComboRenderPassDescriptor passDescriptor({texture.CreateView()});
    passDescriptor.cColorAttachments[0].loadOp = wgpu::LoadOp::Clear;
    passDescriptor.cColorAttachments[0].clearValue = clearValue;
    passDescriptor.cColorAttachments[0].storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&passDescriptor);
    pass.End();
    return encoder.Finish();
}

wgpu::CommandBuffer SharedTextureMemoryTests::MakeSampleCommandBuffer(wgpu::Device& deviceObj,
                                                                      wgpu::Texture& texture,
                                                                      wgpu::Color expectedColor) {
    // TODO
    wgpu::CommandEncoder encoder = deviceObj.CreateCommandEncoder();
    return encoder.Finish();
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
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
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
}

// Test that it is an error to call BeginAccess twice in a row on two textures from the same memory.
TEST_P(SharedTextureMemoryTests, DoubleBeginAccessSeparateTextures) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
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
}

// Test that it is an error to call EndAccess twice in a row on the same memory.
TEST_P(SharedTextureMemoryTests, DoubleEndAccess) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
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
}

// Test that it is an error to call EndAccess without a preceding BeginAccess.
TEST_P(SharedTextureMemoryTests, EndAccessWithoutBegin) {
    for (wgpu::SharedTextureMemory memory :
         GetParam().mBackend->CreateSharedTextureMemories(device)) {
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
}

// Test that it is an error to call BeginAccess on a texture that wasn't created from the same
// memory.
TEST_P(SharedTextureMemoryTests, MismatchingMemory) {
    const auto& memories1 = GetParam().mBackend->CreateSharedTextureMemories(device);
    const auto& memories2 = GetParam().mBackend->CreateSharedTextureMemories(device);
    for (size_t i = 0; i < memories1.size(); ++i) {
        wgpu::SharedTextureMemoryProperties properties;
        memories1[i].GetProperties(&properties);

        wgpu::TextureDescriptor textureDesc = {};
        textureDesc.format = properties.format;
        textureDesc.size = properties.size;
        textureDesc.usage = properties.usage;

        wgpu::Texture texture = memories1[i].CreateTexture(&textureDesc);

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = true;

        ASSERT_DEVICE_ERROR_MSG(memories2[i].BeginAccess(texture, &beginDesc),
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

        wgpu::CommandBuffer commandBuffer =
            MakeClearCommandBuffer(devices[0], texture, {1.0, 0.5, 0.0, 1.0});
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

        commandBuffer = MakeSampleCommandBuffer(devices[1], texture, {1.0, 0.5, 0.0, 1.0});
        devices[1].GetQueue().Submit(1, &commandBuffer);

        memories[1].EndAccess(texture, &endState);
    }
}

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
        wgpu::CommandBuffer commandBuffers[] = {
            MakeClearCommandBuffer(devices[0], textures[0], {1.0, 0.5, 0.0, 1.0}),
            MakeSampleCommandBuffer(devices[1], textures[1], {1.0, 0.5, 0.0, 1.0})};

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = false;
        memories[0].BeginAccess(textures[0], &beginDesc);

        devices[0].GetQueue().Submit(1, &commandBuffers[0]);

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
        devices[1].GetQueue().Submit(1, &commandBuffers[1]);
        memories[1].EndAccess(textures[1], &endState);
    }
}

TEST_P(SharedTextureMemoryTests, RenderThenDestroyBeforeEndAccessThenSample) {
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
        wgpu::CommandBuffer commandBuffers[] = {
            MakeClearCommandBuffer(devices[0], textures[0], {1.0, 0.5, 0.0, 1.0}), nullptr};

        wgpu::SharedTextureMemoryBeginAccessDescriptor beginDesc = {};
        beginDesc.initialized = false;
        memories[0].BeginAccess(textures[0], &beginDesc);

        devices[0].GetQueue().Submit(1, &commandBuffers[0]);

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
        memories[1].EndAccess(textures[1], &endState);
    }
}

}  // anonymous namespace
}  // namespace dawn
