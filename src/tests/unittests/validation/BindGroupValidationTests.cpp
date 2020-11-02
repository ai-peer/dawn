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

#include "tests/unittests/validation/ValidationTest.h"

#include "common/Assert.h"
#include "common/Constants.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

class BindGroupValidationTest : public ValidationTest {
  public:
    wgpu::Texture CreateTexture(wgpu::TextureUsage usage,
                                wgpu::TextureFormat format,
                                uint32_t layerCount) {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {16, 16, layerCount};
        descriptor.sampleCount = 1;
        descriptor.mipLevelCount = 1;
        descriptor.usage = usage;
        descriptor.format = format;

        return device.CreateTexture(&descriptor);
    }

    void SetUp() override {
        ValidationTest::SetUp();

        // Create objects to use as resources inside test bind groups.
        {
            wgpu::BufferDescriptor descriptor;
            descriptor.size = 1024;
            descriptor.usage = wgpu::BufferUsage::Uniform;
            mUBO = device.CreateBuffer(&descriptor);
        }
        {
            wgpu::BufferDescriptor descriptor;
            descriptor.size = 1024;
            descriptor.usage = wgpu::BufferUsage::Storage;
            mSSBO = device.CreateBuffer(&descriptor);
        }
        {
            wgpu::SamplerDescriptor descriptor = utils::GetDefaultSamplerDescriptor();
            mSampler = device.CreateSampler(&descriptor);
        }
        {
            mSampledTexture =
                CreateTexture(wgpu::TextureUsage::Sampled, wgpu::TextureFormat::RGBA8Unorm, 1);
            mSampledTextureView = mSampledTexture.CreateView();
        }
    }

  protected:
    wgpu::Buffer mUBO;
    wgpu::Buffer mSSBO;
    wgpu::Sampler mSampler;
    wgpu::Texture mSampledTexture;
    wgpu::TextureView mSampledTextureView;
};

// Test the validation of BindGroupDescriptor::nextInChain
TEST_F(BindGroupValidationTest, NextInChainNullptr) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(device, {});

    wgpu::BindGroupDescriptor descriptor;
    descriptor.layout = layout;
    descriptor.entryCount = 0;
    descriptor.entries = nullptr;

    // Control case: check that nextInChain = nullptr is valid
    descriptor.nextInChain = nullptr;
    device.CreateBindGroup(&descriptor);

    // Check that nextInChain != nullptr is an error.
    wgpu::ChainedStruct chainedDescriptor;
    descriptor.nextInChain = &chainedDescriptor;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
}

// Check constraints on entryCount
TEST_F(BindGroupValidationTest, EntryCountMismatch) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

    // Control case: check that a descriptor with one binding is ok
    utils::MakeBindGroup(device, layout, {{0, mSampler}});

    // Check that entryCount != layout.entryCount fails.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {}));
}

// Check constraints on BindGroupEntry::binding
TEST_F(BindGroupValidationTest, WrongBindings) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

    // Control case: check that a descriptor with a binding matching the layout's is ok
    utils::MakeBindGroup(device, layout, {{0, mSampler}});

    // Check that binding must be present in the layout
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{1, mSampler}}));
}

// Check that the same binding cannot be set twice
TEST_F(BindGroupValidationTest, BindingSetTwice) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler},
                 {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

    // Control case: check that different bindings work
    utils::MakeBindGroup(device, layout, {{0, mSampler}, {1, mSampler}});

    // Check that setting the same binding twice is invalid
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, mSampler}, {0, mSampler}}));
}

// Check that a sampler binding must contain exactly one sampler
TEST_F(BindGroupValidationTest, SamplerBindingType) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

    wgpu::BindGroupEntry binding;
    binding.binding = 0;
    binding.sampler = nullptr;
    binding.textureView = nullptr;
    binding.buffer = nullptr;
    binding.offset = 0;
    binding.size = 0;

    wgpu::BindGroupDescriptor descriptor;
    descriptor.layout = layout;
    descriptor.entryCount = 1;
    descriptor.entries = &binding;

    // Not setting anything fails
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));

    // Control case: setting just the sampler works
    binding.sampler = mSampler;
    device.CreateBindGroup(&descriptor);

    // Setting the texture view as well is an error
    binding.textureView = mSampledTextureView;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
    binding.textureView = nullptr;

    // Setting the buffer as well is an error
    binding.buffer = mUBO;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
    binding.buffer = nullptr;

    // Setting the sampler to an error sampler is an error.
    {
        wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
        samplerDesc.minFilter = static_cast<wgpu::FilterMode>(0xFFFFFFFF);

        wgpu::Sampler errorSampler;
        ASSERT_DEVICE_ERROR(errorSampler = device.CreateSampler(&samplerDesc));

        binding.sampler = errorSampler;
        ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
        binding.sampler = nullptr;
    }
}

// Check that a texture binding must contain exactly a texture view
TEST_F(BindGroupValidationTest, TextureBindingType) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture}});

    wgpu::BindGroupEntry binding;
    binding.binding = 0;
    binding.sampler = nullptr;
    binding.textureView = nullptr;
    binding.buffer = nullptr;
    binding.offset = 0;
    binding.size = 0;

    wgpu::BindGroupDescriptor descriptor;
    descriptor.layout = layout;
    descriptor.entryCount = 1;
    descriptor.entries = &binding;

    // Not setting anything fails
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));

    // Control case: setting just the texture view works
    binding.textureView = mSampledTextureView;
    device.CreateBindGroup(&descriptor);

    // Setting the sampler as well is an error
    binding.sampler = mSampler;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
    binding.textureView = nullptr;

    // Setting the buffer as well is an error
    binding.buffer = mUBO;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
    binding.buffer = nullptr;

    // Setting the texture view to an error texture view is an error.
    {
        wgpu::TextureViewDescriptor viewDesc;
        viewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        viewDesc.dimension = wgpu::TextureViewDimension::e2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipLevelCount = 0;
        viewDesc.baseArrayLayer = 0;
        viewDesc.arrayLayerCount = 1000;

        wgpu::TextureView errorView;
        ASSERT_DEVICE_ERROR(errorView = mSampledTexture.CreateView(&viewDesc));

        binding.textureView = errorView;
        ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
        binding.textureView = nullptr;
    }
}

// Check that a buffer binding must contain exactly a buffer
TEST_F(BindGroupValidationTest, BufferBindingType) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

    wgpu::BindGroupEntry binding;
    binding.binding = 0;
    binding.sampler = nullptr;
    binding.textureView = nullptr;
    binding.buffer = nullptr;
    binding.offset = 0;
    binding.size = 1024;

    wgpu::BindGroupDescriptor descriptor;
    descriptor.layout = layout;
    descriptor.entryCount = 1;
    descriptor.entries = &binding;

    // Not setting anything fails
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));

    // Control case: setting just the buffer works
    binding.buffer = mUBO;
    device.CreateBindGroup(&descriptor);

    // Setting the texture view as well is an error
    binding.textureView = mSampledTextureView;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
    binding.textureView = nullptr;

    // Setting the sampler as well is an error
    binding.sampler = mSampler;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
    binding.sampler = nullptr;

    // Setting the buffer to an error buffer is an error.
    {
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = 1024;
        bufferDesc.usage = static_cast<wgpu::BufferUsage>(0xFFFFFFFF);

        wgpu::Buffer errorBuffer;
        ASSERT_DEVICE_ERROR(errorBuffer = device.CreateBuffer(&bufferDesc));

        binding.buffer = errorBuffer;
        ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
        binding.buffer = nullptr;
    }
}

// Check that a texture must have the correct usage
TEST_F(BindGroupValidationTest, TextureUsage) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture}});

    // Control case: setting a sampleable texture view works.
    utils::MakeBindGroup(device, layout, {{0, mSampledTextureView}});

    // Make an output attachment texture and try to set it for a SampledTexture binding
    wgpu::Texture outputTexture =
        CreateTexture(wgpu::TextureUsage::RenderAttachment, wgpu::TextureFormat::RGBA8Unorm, 1);
    wgpu::TextureView outputTextureView = outputTexture.CreateView();
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, outputTextureView}}));
}

// Check that a texture must have the correct component type
TEST_F(BindGroupValidationTest, TextureComponentType) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0,
                  false, wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::Float}});

    // Control case: setting a Float typed texture view works.
    utils::MakeBindGroup(device, layout, {{0, mSampledTextureView}});

    // Make a Uint component typed texture and try to set it to a Float component binding.
    wgpu::Texture uintTexture =
        CreateTexture(wgpu::TextureUsage::Sampled, wgpu::TextureFormat::RGBA8Uint, 1);
    wgpu::TextureView uintTextureView = uintTexture.CreateView();

    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, uintTextureView}}));
}

// Test which depth-stencil formats are allowed to be sampled.
// This is a regression test for a change in dawn_native mistakenly allowing the depth24plus formats
// to be sampled without proper backend support.
TEST_F(BindGroupValidationTest, SamplingDepthTexture) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture}});

    wgpu::TextureDescriptor desc;
    desc.size = {1, 1, 1};
    desc.usage = wgpu::TextureUsage::Sampled;

    // Depth32Float is allowed to be sampled.
    {
        desc.format = wgpu::TextureFormat::Depth32Float;
        wgpu::Texture texture = device.CreateTexture(&desc);

        utils::MakeBindGroup(device, layout, {{0, texture.CreateView()}});
    }

    // Depth24Plus is not allowed to be sampled.
    {
        desc.format = wgpu::TextureFormat::Depth24Plus;
        wgpu::Texture texture = device.CreateTexture(&desc);

        ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, texture.CreateView()}}));
    }

    // Depth24PlusStencil8 is not allowed to be sampled.
    {
        desc.format = wgpu::TextureFormat::Depth24PlusStencil8;
        wgpu::Texture texture = device.CreateTexture(&desc);

        ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, texture.CreateView()}}));
    }
}

// Check that a texture must have a correct format for DepthComparison
TEST_F(BindGroupValidationTest, TextureComponentTypeDepthComparison) {
    wgpu::BindGroupLayout depthLayout = utils::MakeBindGroupLayout(
        device,
        {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0, false,
          wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::DepthComparison}});

    // Control case: setting a depth texture works.
    wgpu::Texture depthTexture =
        CreateTexture(wgpu::TextureUsage::Sampled, wgpu::TextureFormat::Depth32Float, 1);
    utils::MakeBindGroup(device, depthLayout, {{0, depthTexture.CreateView()}});

    // Error case: setting a Float typed texture view fails.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, depthLayout, {{0, mSampledTextureView}}));
}

// Check that a depth texture is allowed to be used for both TextureComponentType::Float and
// ::DepthComparison
TEST_F(BindGroupValidationTest, TextureComponentTypeForDepthTexture) {
    wgpu::BindGroupLayout depthLayout = utils::MakeBindGroupLayout(
        device,
        {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0, false,
          wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::DepthComparison}});

    wgpu::BindGroupLayout floatLayout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0,
                  false, wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::Float}});

    wgpu::Texture depthTexture =
        CreateTexture(wgpu::TextureUsage::Sampled, wgpu::TextureFormat::Depth32Float, 1);

    utils::MakeBindGroup(device, depthLayout, {{0, depthTexture.CreateView()}});
    utils::MakeBindGroup(device, floatLayout, {{0, depthTexture.CreateView()}});
}

// Check that a texture must have the correct dimension
TEST_F(BindGroupValidationTest, TextureDimension) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0,
                  false, wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::Float}});

    // Control case: setting a 2D texture view works.
    utils::MakeBindGroup(device, layout, {{0, mSampledTextureView}});

    // Make a 2DArray texture and try to set it to a 2D binding.
    wgpu::Texture arrayTexture =
        CreateTexture(wgpu::TextureUsage::Sampled, wgpu::TextureFormat::RGBA8Uint, 2);
    wgpu::TextureView arrayTextureView = arrayTexture.CreateView();

    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, arrayTextureView}}));
}

// Check that a UBO must have the correct usage
TEST_F(BindGroupValidationTest, BufferUsageUBO) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer}});

    // Control case: using a buffer with the uniform usage works
    utils::MakeBindGroup(device, layout, {{0, mUBO, 0, 256}});

    // Using a buffer without the uniform usage fails
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, mSSBO, 0, 256}}));
}

// Check that a SSBO must have the correct usage
TEST_F(BindGroupValidationTest, BufferUsageSSBO) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});

    // Control case: using a buffer with the storage usage works
    utils::MakeBindGroup(device, layout, {{0, mSSBO, 0, 256}});

    // Using a buffer without the storage usage fails
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, mUBO, 0, 256}}));
}

// Check that a readonly SSBO must have the correct usage
TEST_F(BindGroupValidationTest, BufferUsageReadonlySSBO) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer}});

    // Control case: using a buffer with the storage usage works
    utils::MakeBindGroup(device, layout, {{0, mSSBO, 0, 256}});

    // Using a buffer without the storage usage fails
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, mUBO, 0, 256}}));
}

// Tests constraints on the buffer offset for bind groups.
TEST_F(BindGroupValidationTest, BufferOffsetAlignment) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                });

    // Check that offset 0 is valid
    utils::MakeBindGroup(device, layout, {{0, mUBO, 0, 512}});

    // Check that offset 256 (aligned) is valid
    utils::MakeBindGroup(device, layout, {{0, mUBO, 256, 256}});

    // Check cases where unaligned buffer offset is invalid
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, mUBO, 1, 256}}));
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, mUBO, 128, 256}}));
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, mUBO, 255, 256}}));
}

// Tests constraints on the texture for MultisampledTexture bindings
TEST_F(BindGroupValidationTest, MultisampledTexture) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::MultisampledTexture, false, 0,
                  false, wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::Float}});

    wgpu::BindGroupEntry binding;
    binding.binding = 0;
    binding.sampler = nullptr;
    binding.textureView = nullptr;
    binding.buffer = nullptr;
    binding.offset = 0;
    binding.size = 0;

    wgpu::BindGroupDescriptor descriptor;
    descriptor.layout = layout;
    descriptor.entryCount = 1;
    descriptor.entries = &binding;

    // Not setting anything fails
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));

    // Control case: setting a multisampled 2D texture works
    wgpu::TextureDescriptor textureDesc;
    textureDesc.sampleCount = 4;
    textureDesc.usage = wgpu::TextureUsage::Sampled;
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc.size = {1, 1, 1};
    wgpu::Texture msTexture = device.CreateTexture(&textureDesc);

    binding.textureView = msTexture.CreateView();
    device.CreateBindGroup(&descriptor);
    binding.textureView = nullptr;

    // Error case: setting a single sampled 2D texture is an error.
    binding.textureView = mSampledTextureView;
    ASSERT_DEVICE_ERROR(device.CreateBindGroup(&descriptor));
    binding.textureView = nullptr;
}

// Tests constraints to be sure the buffer binding fits in the buffer
TEST_F(BindGroupValidationTest, BufferBindingOOB) {
    wgpu::BindGroupLayout layout = utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                });

    wgpu::BufferDescriptor descriptor;
    descriptor.size = 1024;
    descriptor.usage = wgpu::BufferUsage::Uniform;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    // Success case, touching the start of the buffer works
    utils::MakeBindGroup(device, layout, {{0, buffer, 0, 256}});

    // Success case, touching the end of the buffer works
    utils::MakeBindGroup(device, layout, {{0, buffer, 3 * 256, 256}});

    // Error case, zero size is invalid.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, buffer, 1024, 0}}));

    // Success case, touching the full buffer works
    utils::MakeBindGroup(device, layout, {{0, buffer, 0, 1024}});
    utils::MakeBindGroup(device, layout, {{0, buffer, 0, wgpu::kWholeSize}});

    // Success case, whole size causes the rest of the buffer to be used but not beyond.
    utils::MakeBindGroup(device, layout, {{0, buffer, 256, wgpu::kWholeSize}});

    // Error case, offset is OOB
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, buffer, 256 * 5, 0}}));

    // Error case, size is OOB
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, buffer, 0, 256 * 5}}));

    // Error case, offset+size is OOB
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, layout, {{0, buffer, 1024, 256}}));

    // Error case, offset+size overflows to be 0
    ASSERT_DEVICE_ERROR(
        utils::MakeBindGroup(device, layout, {{0, buffer, 256, uint32_t(0) - uint32_t(256)}}));
}

// Tests constraints to be sure the uniform buffer binding isn't too large
TEST_F(BindGroupValidationTest, MaxUniformBufferBindingSize) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 2 * kMaxUniformBufferBindingSize;
    descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    wgpu::BindGroupLayout uniformLayout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer}});

    // Success case, this is exactly the limit
    utils::MakeBindGroup(device, uniformLayout, {{0, buffer, 0, kMaxUniformBufferBindingSize}});

    wgpu::BindGroupLayout doubleUniformLayout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                 {1, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer}});

    // Success case, individual bindings don't exceed the limit
    utils::MakeBindGroup(device, doubleUniformLayout,
                         {{0, buffer, 0, kMaxUniformBufferBindingSize},
                          {1, buffer, kMaxUniformBufferBindingSize, kMaxUniformBufferBindingSize}});

    // Error case, this is above the limit
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, uniformLayout,
                                             {{0, buffer, 0, kMaxUniformBufferBindingSize + 1}}));

    // Making sure the constraint doesn't apply to storage buffers
    wgpu::BindGroupLayout readonlyStorageLayout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer}});
    wgpu::BindGroupLayout storageLayout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});

    // Success case, storage buffer can still be created.
    utils::MakeBindGroup(device, readonlyStorageLayout,
                         {{0, buffer, 0, 2 * kMaxUniformBufferBindingSize}});
    utils::MakeBindGroup(device, storageLayout, {{0, buffer, 0, 2 * kMaxUniformBufferBindingSize}});
}

// Test what happens when the layout is an error.
TEST_F(BindGroupValidationTest, ErrorLayout) {
    wgpu::BindGroupLayout goodLayout = utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                });

    wgpu::BindGroupLayout errorLayout;
    ASSERT_DEVICE_ERROR(
        errorLayout = utils::MakeBindGroupLayout(
            device, {
                        {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                        {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                    }));

    // Control case, creating with the good layout works
    utils::MakeBindGroup(device, goodLayout, {{0, mUBO, 0, 256}});

    // Creating with an error layout fails
    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, errorLayout, {{0, mUBO, 0, 256}}));
}

class BindGroupLayoutValidationTest : public ValidationTest {
  public:
    wgpu::BindGroupLayout MakeBindGroupLayout(wgpu::BindGroupLayoutEntry* binding, uint32_t count) {
        wgpu::BindGroupLayoutDescriptor descriptor;
        descriptor.entryCount = count;
        descriptor.entries = binding;
        return device.CreateBindGroupLayout(&descriptor);
    }

    void TestCreateBindGroupLayout(wgpu::BindGroupLayoutEntry* binding,
                                   uint32_t count,
                                   bool expected) {
        wgpu::BindGroupLayoutDescriptor descriptor;

        descriptor.entryCount = count;
        descriptor.entries = binding;

        if (!expected) {
            ASSERT_DEVICE_ERROR(device.CreateBindGroupLayout(&descriptor));
        } else {
            device.CreateBindGroupLayout(&descriptor);
        }
    }

    void TestCreatePipelineLayout(wgpu::BindGroupLayout* bgl, uint32_t count, bool expected) {
        wgpu::PipelineLayoutDescriptor descriptor;

        descriptor.bindGroupLayoutCount = count;
        descriptor.bindGroupLayouts = bgl;

        if (!expected) {
            ASSERT_DEVICE_ERROR(device.CreatePipelineLayout(&descriptor));
        } else {
            device.CreatePipelineLayout(&descriptor);
        }
    }
};

// Tests setting storage buffer and readonly storage buffer bindings in vertex and fragment shader.
TEST_F(BindGroupLayoutValidationTest, BindGroupLayoutStorageBindingsInVertexShader) {
    // Checks that storage buffer binding is not supported in vertex shader.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::StorageBuffer}}));

    utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Vertex, wgpu::BindingType::ReadonlyStorageBuffer}});

    utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer}});

    utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer}});
}

// Tests setting that bind group layout bindings numbers may be very large.
TEST_F(BindGroupLayoutValidationTest, BindGroupLayoutEntryNumberLarge) {
    // Checks that uint32_t max is valid.
    utils::MakeBindGroupLayout(device,
                               {{std::numeric_limits<uint32_t>::max(), wgpu::ShaderStage::Vertex,
                                 wgpu::BindingType::UniformBuffer}});
}

// This test verifies that the BindGroupLayout bindings are correctly validated, even if the
// binding ids are out-of-order.
TEST_F(BindGroupLayoutValidationTest, BindGroupEntry) {
    utils::MakeBindGroupLayout(device,
                               {
                                   {1, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                                   {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                               });
}

// Check that dynamic = true is only allowed with buffer bindings.
TEST_F(BindGroupLayoutValidationTest, DynamicAndTypeCompatibility) {
    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer, true},
                });

    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer, true},
                });

    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer, true},
                });

    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture, true},
                }));

    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::Sampler, true},
                }));
}

// This test verifies that visibility of bindings in BindGroupLayout can be none
TEST_F(BindGroupLayoutValidationTest, BindGroupLayoutVisibilityNone) {
    utils::MakeBindGroupLayout(device,
                               {
                                   {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                               });

    wgpu::BindGroupLayoutEntry binding = {0, wgpu::ShaderStage::None,
                                          wgpu::BindingType::UniformBuffer};
    wgpu::BindGroupLayoutDescriptor descriptor;
    descriptor.entryCount = 1;
    descriptor.entries = &binding;
    device.CreateBindGroupLayout(&descriptor);
}

// This test verifies that binding with none visibility in bind group layout can be supported in
// bind group
TEST_F(BindGroupLayoutValidationTest, BindGroupLayoutVisibilityNoneExpectsBindGroupEntry) {
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Vertex, wgpu::BindingType::UniformBuffer},
                    {1, wgpu::ShaderStage::None, wgpu::BindingType::UniformBuffer},
                });
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::Uniform;
    wgpu::Buffer buffer = device.CreateBuffer(&descriptor);

    utils::MakeBindGroup(device, bgl, {{0, buffer}, {1, buffer}});

    ASSERT_DEVICE_ERROR(utils::MakeBindGroup(device, bgl, {{0, buffer}}));
}

TEST_F(BindGroupLayoutValidationTest, PerStageLimits) {
    struct TestInfo {
        uint32_t maxCount;
        wgpu::BindingType bindingType;
        wgpu::BindingType otherBindingType;
    };

    constexpr TestInfo kTestInfos[] = {
        {kMaxSampledTexturesPerShaderStage, wgpu::BindingType::SampledTexture,
         wgpu::BindingType::UniformBuffer},
        {kMaxSamplersPerShaderStage, wgpu::BindingType::Sampler, wgpu::BindingType::UniformBuffer},
        {kMaxSamplersPerShaderStage, wgpu::BindingType::ComparisonSampler,
         wgpu::BindingType::UniformBuffer},
        {kMaxStorageBuffersPerShaderStage, wgpu::BindingType::StorageBuffer,
         wgpu::BindingType::UniformBuffer},
        {kMaxStorageTexturesPerShaderStage, wgpu::BindingType::ReadonlyStorageTexture,
         wgpu::BindingType::UniformBuffer},
        {kMaxStorageTexturesPerShaderStage, wgpu::BindingType::WriteonlyStorageTexture,
         wgpu::BindingType::UniformBuffer},
        {kMaxUniformBuffersPerShaderStage, wgpu::BindingType::UniformBuffer,
         wgpu::BindingType::SampledTexture},
    };

    for (TestInfo info : kTestInfos) {
        wgpu::BindGroupLayout bgl[2];
        std::vector<wgpu::BindGroupLayoutEntry> maxBindings;

        auto PopulateEntry = [](wgpu::BindGroupLayoutEntry entry) {
            switch (entry.type) {
                case wgpu::BindingType::ReadonlyStorageTexture:
                case wgpu::BindingType::WriteonlyStorageTexture:
                    entry.storageTextureFormat = wgpu::TextureFormat::RGBA8Unorm;
                    break;
                default:
                    break;
            }
            return entry;
        };

        for (uint32_t i = 0; i < info.maxCount; ++i) {
            maxBindings.push_back(PopulateEntry({i, wgpu::ShaderStage::Compute, info.bindingType}));
        }

        // Creating with the maxes works.
        bgl[0] = MakeBindGroupLayout(maxBindings.data(), maxBindings.size());

        // Adding an extra binding of a different type works.
        {
            std::vector<wgpu::BindGroupLayoutEntry> bindings = maxBindings;
            bindings.push_back(
                PopulateEntry({info.maxCount, wgpu::ShaderStage::Compute, info.otherBindingType}));
            MakeBindGroupLayout(bindings.data(), bindings.size());
        }

        // Adding an extra binding of the maxed type in a different stage works
        {
            std::vector<wgpu::BindGroupLayoutEntry> bindings = maxBindings;
            bindings.push_back(
                PopulateEntry({info.maxCount, wgpu::ShaderStage::Fragment, info.bindingType}));
            MakeBindGroupLayout(bindings.data(), bindings.size());
        }

        // Adding an extra binding of the maxed type and stage exceeds the per stage limit.
        {
            std::vector<wgpu::BindGroupLayoutEntry> bindings = maxBindings;
            bindings.push_back(
                PopulateEntry({info.maxCount, wgpu::ShaderStage::Compute, info.bindingType}));
            ASSERT_DEVICE_ERROR(MakeBindGroupLayout(bindings.data(), bindings.size()));
        }

        // Creating a pipeline layout from the valid BGL works.
        TestCreatePipelineLayout(bgl, 1, true);

        // Adding an extra binding of a different type in a different BGL works
        bgl[1] = utils::MakeBindGroupLayout(
            device, {PopulateEntry({0, wgpu::ShaderStage::Compute, info.otherBindingType})});
        TestCreatePipelineLayout(bgl, 2, true);

        // Adding an extra binding of the maxed type in a different stage works
        bgl[1] = utils::MakeBindGroupLayout(
            device, {PopulateEntry({0, wgpu::ShaderStage::Fragment, info.bindingType})});
        TestCreatePipelineLayout(bgl, 2, true);

        // Adding an extra binding of the maxed type in a different BGL exceeds the per stage limit.
        bgl[1] = utils::MakeBindGroupLayout(
            device, {PopulateEntry({0, wgpu::ShaderStage::Compute, info.bindingType})});
        TestCreatePipelineLayout(bgl, 2, false);
    }
}

// Check that dynamic buffer numbers exceed maximum value in one bind group layout.
TEST_F(BindGroupLayoutValidationTest, DynamicBufferNumberLimit) {
    wgpu::BindGroupLayout bgl[2];
    std::vector<wgpu::BindGroupLayoutEntry> maxUniformDB;
    std::vector<wgpu::BindGroupLayoutEntry> maxStorageDB;
    std::vector<wgpu::BindGroupLayoutEntry> maxReadonlyStorageDB;

    // In this test, we use all the same shader stage. Ensure that this does not exceed the
    // per-stage limit.
    static_assert(kMaxDynamicUniformBuffersPerPipelineLayout <= kMaxUniformBuffersPerShaderStage,
                  "");
    static_assert(kMaxDynamicStorageBuffersPerPipelineLayout <= kMaxStorageBuffersPerShaderStage,
                  "");

    for (uint32_t i = 0; i < kMaxDynamicUniformBuffersPerPipelineLayout; ++i) {
        maxUniformDB.push_back(
            {i, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer, true});
    }

    for (uint32_t i = 0; i < kMaxDynamicStorageBuffersPerPipelineLayout; ++i) {
        maxStorageDB.push_back(
            {i, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer, true});
    }

    for (uint32_t i = 0; i < kMaxDynamicStorageBuffersPerPipelineLayout; ++i) {
        maxReadonlyStorageDB.push_back(
            {i, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer, true});
    }

    // Test creating with the maxes works
    {
        bgl[0] = MakeBindGroupLayout(maxUniformDB.data(), maxUniformDB.size());
        TestCreatePipelineLayout(bgl, 1, true);

        bgl[0] = MakeBindGroupLayout(maxStorageDB.data(), maxStorageDB.size());
        TestCreatePipelineLayout(bgl, 1, true);

        bgl[0] = MakeBindGroupLayout(maxReadonlyStorageDB.data(), maxReadonlyStorageDB.size());
        TestCreatePipelineLayout(bgl, 1, true);
    }

    // The following tests exceed the per-pipeline layout limits. We use the Fragment stage to
    // ensure we don't hit the per-stage limit.

    // Check dynamic uniform buffers exceed maximum in pipeline layout.
    {
        bgl[0] = MakeBindGroupLayout(maxUniformDB.data(), maxUniformDB.size());
        bgl[1] = utils::MakeBindGroupLayout(
            device, {
                        {0, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer, true},
                    });

        TestCreatePipelineLayout(bgl, 2, false);
    }

    // Check dynamic storage buffers exceed maximum in pipeline layout
    {
        bgl[0] = MakeBindGroupLayout(maxStorageDB.data(), maxStorageDB.size());
        bgl[1] = utils::MakeBindGroupLayout(
            device, {
                        {0, wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer, true},
                    });

        TestCreatePipelineLayout(bgl, 2, false);
    }

    // Check dynamic readonly storage buffers exceed maximum in pipeline layout
    {
        bgl[0] = MakeBindGroupLayout(maxReadonlyStorageDB.data(), maxReadonlyStorageDB.size());
        bgl[1] = utils::MakeBindGroupLayout(
            device,
            {
                {0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer, true},
            });

        TestCreatePipelineLayout(bgl, 2, false);
    }

    // Check dynamic storage buffers + dynamic readonly storage buffers exceed maximum storage
    // buffers in pipeline layout
    {
        bgl[0] = MakeBindGroupLayout(maxStorageDB.data(), maxStorageDB.size());
        bgl[1] = utils::MakeBindGroupLayout(
            device,
            {
                {0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ReadonlyStorageBuffer, true},
            });

        TestCreatePipelineLayout(bgl, 2, false);
    }

    // Check dynamic uniform buffers exceed maximum in bind group layout.
    {
        maxUniformDB.push_back({kMaxDynamicUniformBuffersPerPipelineLayout,
                                wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer,
                                true});
        TestCreateBindGroupLayout(maxUniformDB.data(), maxUniformDB.size(), false);
    }

    // Check dynamic storage buffers exceed maximum in bind group layout.
    {
        maxStorageDB.push_back({kMaxDynamicStorageBuffersPerPipelineLayout,
                                wgpu::ShaderStage::Fragment, wgpu::BindingType::StorageBuffer,
                                true});
        TestCreateBindGroupLayout(maxStorageDB.data(), maxStorageDB.size(), false);
    }

    // Check dynamic readonly storage buffers exceed maximum in bind group layout.
    {
        maxReadonlyStorageDB.push_back({kMaxDynamicStorageBuffersPerPipelineLayout,
                                        wgpu::ShaderStage::Fragment,
                                        wgpu::BindingType::ReadonlyStorageBuffer, true});
        TestCreateBindGroupLayout(maxReadonlyStorageDB.data(), maxReadonlyStorageDB.size(), false);
    }
}

// Test that multisampled textures must be 2D sampled textures
TEST_F(BindGroupLayoutValidationTest, MultisampledTextureViewDimension) {
    // Multisampled 2D texture works.
    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::e2D},
                });

    // Multisampled 2D (defaulted) texture works.
    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::Undefined},
                });

    // Multisampled 2D array texture is invalid.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::e2DArray},
                }));

    // Multisampled cube texture is invalid.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::Cube},
                }));

    // Multisampled cube array texture is invalid.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::CubeArray},
                }));

    // Multisampled 3D texture is invalid.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::e3D},
                }));

    // Multisampled 1D texture is invalid.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::e1D},
                }));
}

// Test that multisampled textures cannot be DepthComparison
TEST_F(BindGroupLayoutValidationTest, MultisampledTextureComponentType) {
    // Multisampled float component type works.
    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::Float},
                });

    // Multisampled float (defaulted) component type works.
    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::e2D},
                });

    // Multisampled uint component type works.
    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::Uint},
                });

    // Multisampled sint component type works.
    utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false,
                     0, false, wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::Sint},
                });

    // Multisampled depth comparison component typeworks.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device,
        {
            {0, wgpu::ShaderStage::Compute, wgpu::BindingType::MultisampledTexture, false, 0, false,
             wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::DepthComparison},
        }));
}

// Test that it is an error to pass multisampled=true for non-texture bindings.
// TODO(crbug.com/dawn/527): Remove this test when multisampled=true is removed.
TEST_F(BindGroupLayoutValidationTest, MultisampledMustBeSampledTexture) {
    // Base: Multisampled 2D texture works.
    EXPECT_DEPRECATION_WARNING(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture, false, 0,
                     true, wgpu::TextureViewDimension::e2D},
                }));

    // Multisampled uniform buffer binding is invalid
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device,
        {
            {0, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer, false, 0, true},
        }));

    // Multisampled storage buffer binding is invalid
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device,
        {
            {0, wgpu::ShaderStage::Compute, wgpu::BindingType::StorageBuffer, false, 0, true},
        }));

    // Multisampled sampler binding is invalid
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::Sampler, false, 0, true},
                }));

    // Multisampled 2D storage texture is invalid.
    ASSERT_DEVICE_ERROR(utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageTexture,
                     false, 0, true, wgpu::TextureViewDimension::e2D},
                }));
}

// Test that it is allowed to use DepthComparison for a texture used as shadow2DSampler. This is a
// regression test for crbug.com/dawn/561
TEST_F(BindGroupLayoutValidationTest, DepthComparisonAllowedWithPipelineUsingTextureForDepth) {
    wgpu::BindGroupLayout bgls[3] = {
        utils::MakeBindGroupLayout(device, {
            {0, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer},
            {5, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer},
        }),
        utils::MakeBindGroupLayout(device, {
            {0, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer},
            {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer},
        }),
        utils::MakeBindGroupLayout(device, {
            {0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ComparisonSampler},
            {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0, false,
             wgpu::TextureViewDimension::e2D, wgpu::TextureComponentType::DepthComparison},
            {2, wgpu::ShaderStage::Fragment, wgpu::BindingType::UniformBuffer},
        }),
    };

    wgpu::PipelineLayoutDescriptor plDesc;
    plDesc.bindGroupLayouts = bgls;
    plDesc.bindGroupLayoutCount = 3;

    utils::ComboRenderPipelineDescriptor desc(device);
    desc.cFragmentStage.module = 
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
#version 450
#define DIFFUSEDIRECTUV 0
#define DETAILDIRECTUV 0
#define DETAIL_NORMALBLENDMETHOD 0
#define AMBIENTDIRECTUV 0
#define OPACITYDIRECTUV 0
#define EMISSIVEDIRECTUV 0
#define SPECULARDIRECTUV 0
#define BUMPDIRECTUV 0
#define SPECULARTERM
#define NORMAL
#define NUM_BONE_INFLUENCERS 0
#define BonesPerMesh 0
#define LIGHTMAPDIRECTUV 0
#define SHADOWFLOAT
#define NUM_MORPH_INFLUENCERS 0
#define ALPHABLEND
#define PREPASS_IRRADIANCE_INDEX -1
#define PREPASS_ALBEDO_INDEX -1
#define PREPASS_DEPTHNORMAL_INDEX -1
#define PREPASS_POSITION_INDEX -1
#define PREPASS_VELOCITY_INDEX -1
#define PREPASS_REFLECTIVITY_INDEX -1
#define SCENE_MRT_COUNT 0
#define VIGNETTEBLENDMODEMULTIPLY
#define SAMPLER3DGREENDEPTH
#define SAMPLER3DBGRMAP
#define LIGHT0
#define DIRLIGHT0
#define SHADOW0
#define SHADOWPCF0
#define SHADOWS
#define SHADER_NAME fragment:default
layout(set = 2, binding = 2) uniform LeftOver {
    mat4 lightMatrix0;
    vec3 vEyePosition;
    vec3 vAmbientColor;
};

precision highp float;
layout(std140, column_major) uniform;
layout(set = 1, binding = 0) uniform Material
{
    vec4 diffuseLeftColor;
    vec4 diffuseRightColor;
    vec4 opacityParts;
    vec4 reflectionLeftColor;
    vec4 reflectionRightColor;
    vec4 refractionLeftColor;
    vec4 refractionRightColor;
    vec4 emissiveLeftColor;
    vec4 emissiveRightColor;
    vec2 vDiffuseInfos;
    vec2 vAmbientInfos;
    vec2 vOpacityInfos;
    vec2 vReflectionInfos;
    vec3 vReflectionPosition;
    vec3 vReflectionSize;
    vec2 vEmissiveInfos;
    vec2 vLightmapInfos;
    vec2 vSpecularInfos;
    vec3 vBumpInfos;
    mat4 diffuseMatrix;
    mat4 ambientMatrix;
    mat4 opacityMatrix;
    mat4 reflectionMatrix;
    mat4 emissiveMatrix;
    mat4 lightmapMatrix;
    mat4 specularMatrix;
    mat4 bumpMatrix;
    vec2 vTangentSpaceParams;
    float pointSize;
    mat4 refractionMatrix;
    vec4 vRefractionInfos;
    vec4 vSpecularColor;
    vec3 vEmissiveColor;
    vec4 vDiffuseColor;
    vec4 vDetailInfos;
    mat4 detailMatrix;
};
layout(std140, column_major) uniform;
layout(set = 0, binding = 0) uniform Scene {
    mat4 viewProjection;
    mat4 view;
    mat4 projection;
    vec4 viewPosition;
};
layout(std140, column_major) uniform;
layout(set = 1, binding = 1) uniform Mesh
{
    mat4 world;
    float visibility;
};
#define CUSTOM_FRAGMENT_BEGIN
#define RECIPROCAL_PI2 0.15915494


layout(location = 0) in vec3 vPositionW;
layout(location = 1) in vec3 vNormalW;
const float PI = 3.1415926535897932384626433832795;
const float HALF_MIN = 5.96046448e-08;
const float LinearEncodePowerApprox = 2.2;
const float GammaEncodePowerApprox = 1.0 / LinearEncodePowerApprox;
const vec3 LuminanceEncodeApprox = vec3(0.2126, 0.7152, 0.0722);
const float Epsilon = 0.0000001;
#define saturate(x) clamp(x, 0.0, 1.0)
#define absEps(x) abs(x) + Epsilon
#define maxEps(x) max(x, Epsilon)
#define saturateEps(x) clamp(x, Epsilon, 1.0)
mat3 transposeMat3(mat3 inMatrix) {
    vec3 i0 = inMatrix[0];
    vec3 i1 = inMatrix[1];
    vec3 i2 = inMatrix[2];
    mat3 outMatrix = mat3(
        vec3(i0.x, i1.x, i2.x),
        vec3(i0.y, i1.y, i2.y),
        vec3(i0.z, i1.z, i2.z)
    );
    return outMatrix;
}
mat3 inverseMat3(mat3 inMatrix) {
    float a00 = inMatrix[0][0], a01 = inMatrix[0][1], a02 = inMatrix[0][2];
    float a10 = inMatrix[1][0], a11 = inMatrix[1][1], a12 = inMatrix[1][2];
    float a20 = inMatrix[2][0], a21 = inMatrix[2][1], a22 = inMatrix[2][2];
    float b01 = a22 * a11 - a12 * a21;
    float b11 = -a22 * a10 + a12 * a20;
    float b21 = a21 * a10 - a11 * a20;
    float det = a00 * b01 + a01 * b11 + a02 * b21;
    return mat3(b01, (-a22 * a01 + a02 * a21), (a12 * a01 - a02 * a11),
        b11, (a22 * a00 - a02 * a20), (-a12 * a00 + a02 * a10),
        b21, (-a21 * a00 + a01 * a20), (a11 * a00 - a01 * a10)) / det;
}
float toLinearSpace(float color)
{
    return pow(color, LinearEncodePowerApprox);
}
vec3 toLinearSpace(vec3 color)
{
    return pow(color, vec3(LinearEncodePowerApprox));
}
vec4 toLinearSpace(vec4 color)
{
    return vec4(pow(color.rgb, vec3(LinearEncodePowerApprox)), color.a);
}
vec3 toGammaSpace(vec3 color)
{
    return pow(color, vec3(GammaEncodePowerApprox));
}
vec4 toGammaSpace(vec4 color)
{
    return vec4(pow(color.rgb, vec3(GammaEncodePowerApprox)), color.a);
}
float toGammaSpace(float color)
{
    return pow(color, GammaEncodePowerApprox);
}
float square(float value)
{
    return value * value;
}
float pow5(float value) {
    float sq = value * value;
    return sq * sq * value;
}
float getLuminance(vec3 color)
{
    return clamp(dot(color, LuminanceEncodeApprox), 0., 1.);
}
float getRand(vec2 seed) {
    return fract(sin(dot(seed.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
float dither(vec2 seed, float varianceAmount) {
    float rand = getRand(seed);
    float dither = mix(-varianceAmount / 255.0, varianceAmount / 255.0, rand);
    return dither;
}
const float rgbdMaxRange = 255.0;
vec4 toRGBD(vec3 color) {
    float maxRGB = maxEps(max(color.r, max(color.g, color.b)));
    float D = max(rgbdMaxRange / maxRGB, 1.);
    D = clamp(floor(D) / 255.0, 0., 1.);
    vec3 rgb = color.rgb * D;
    rgb = toGammaSpace(rgb);
    return vec4(rgb, D);
}
vec3 fromRGBD(vec4 rgbd) {
    rgbd.rgb = toLinearSpace(rgbd.rgb);
    return rgbd.rgb / rgbd.a;
}
layout(set = 0, binding = 5) uniform Light0
{
    vec4 vLightData;
    vec4 vLightDiffuse;
    vec4 vLightSpecular;
    vec4 shadowsInfo;
    vec2 depthValues;
} light0;
layout(location = 2) in vec4 vPositionFromLight0;
layout(location = 3) in float vDepthMetric0;
layout(set = 2, binding = 0) uniform samplerShadow shadowSampler0Sampler;
layout(set = 2, binding = 1) uniform texture2D shadowSampler0Texture;
                        #define shadowSampler0 sampler2DShadow(shadowSampler0Texture, shadowSampler0Sampler)

struct lightingInfo
{
    vec3 diffuse;
    vec3 specular;
};
lightingInfo computeLighting(vec3 viewDirectionW, vec3 vNormal, vec4 lightData, vec3 diffuseColor, vec3 specularColor, float range, float glossiness) {
    lightingInfo result;
    vec3 lightVectorW;
    float attenuation = 1.0;
    if (lightData.w == 0.) {
        vec3 direction = lightData.xyz - vPositionW;
        attenuation = max(0., 1.0 - length(direction) / range);
        lightVectorW = normalize(direction);
    }
    else {
        lightVectorW = normalize(-lightData.xyz);
    }
    float ndl = max(0., dot(vNormal, lightVectorW));
    result.diffuse = ndl * diffuseColor * attenuation;
    vec3 angleW = normalize(viewDirectionW + lightVectorW);
    float specComp = max(0., dot(vNormal, angleW));
    specComp = pow(specComp, max(1., glossiness));
    result.specular = specComp * specularColor * attenuation;
    return result;
}
lightingInfo computeSpotLighting(vec3 viewDirectionW, vec3 vNormal, vec4 lightData, vec4 lightDirection, vec3 diffuseColor, vec3 specularColor, float range, float glossiness) {
    lightingInfo result;
    vec3 direction = lightData.xyz - vPositionW;
    vec3 lightVectorW = normalize(direction);
    float attenuation = max(0., 1.0 - length(direction) / range);
    float cosAngle = max(0., dot(lightDirection.xyz, -lightVectorW));
    if (cosAngle >= lightDirection.w) {
        cosAngle = max(0., pow(cosAngle, lightData.w));
        attenuation *= cosAngle;
        float ndl = max(0., dot(vNormal, lightVectorW));
        result.diffuse = ndl * diffuseColor * attenuation;
        vec3 angleW = normalize(viewDirectionW + lightVectorW);
        float specComp = max(0., dot(vNormal, angleW));
        specComp = pow(specComp, max(1., glossiness));
        result.specular = specComp * specularColor * attenuation;
        return result;
    }
    result.diffuse = vec3(0.);
    result.specular = vec3(0.);
    return result;
}
lightingInfo computeHemisphericLighting(vec3 viewDirectionW, vec3 vNormal, vec4 lightData, vec3 diffuseColor, vec3 specularColor, vec3 groundColor, float glossiness) {
    lightingInfo result;
    float ndl = dot(vNormal, lightData.xyz) * 0.5 + 0.5;
    result.diffuse = mix(groundColor, diffuseColor, ndl);
    vec3 angleW = normalize(viewDirectionW + lightData.xyz);
    float specComp = max(0., dot(vNormal, angleW));
    specComp = pow(specComp, max(1., glossiness));
    result.specular = specComp * specularColor;
    return result;
}

float computeFallOff(float value, vec2 clipSpace, float frustumEdgeFalloff)
{
    float mask = smoothstep(1.0 - frustumEdgeFalloff, 1.00000012, clamp(dot(clipSpace, clipSpace), 0., 1.));
    return mix(value, 1.0, mask);
}


vec4 applyImageProcessing(vec4 result) {
    result.rgb = toGammaSpace(result.rgb);
    result.rgb = saturate(result.rgb);
    return result;
}
#define CUSTOM_FRAGMENT_DEFINITIONS
layout(location = 0) out vec4 glFragColor;
void main(void) {
#define CUSTOM_FRAGMENT_MAIN_BEGIN
    vec3 viewDirectionW = normalize(vEyePosition - vPositionW);
    vec4 baseColor = vec4(1., 1., 1., 1.);
    vec3 diffuseColor = vDiffuseColor.rgb;
    float alpha = vDiffuseColor.a;
    vec3 normalW = normalize(vNormalW);
    vec2 uvOffset = vec2(0.0, 0.0);
#define CUSTOM_FRAGMENT_UPDATE_DIFFUSE
    vec3 baseAmbientColor = vec3(1., 1., 1.);
#define CUSTOM_FRAGMENT_BEFORE_LIGHTS
    float glossiness = vSpecularColor.a;
    vec3 specularColor = vSpecularColor.rgb;
    vec3 diffuseBase = vec3(0., 0., 0.);
    lightingInfo info;
    vec3 specularBase = vec3(0., 0., 0.);
    float shadow = 1.;
    info = computeLighting(viewDirectionW, normalW, light0.vLightData, light0.vLightDiffuse.rgb, light0.vLightSpecular.rgb, light0.vLightDiffuse.a, glossiness);
    float computeShadowWithPCF5_0;
    {
        if (vDepthMetric0 > 1.0 || vDepthMetric0 < 0.0) {
            computeShadowWithPCF5_0 = 1.0;
        }
        else {
            vec3 clipSpace = vPositionFromLight0.xyz / vPositionFromLight0.w;
            vec3 uvDepth = vec3(0.5 * clipSpace.xyz + vec3(0.5));
            vec2 uv = uvDepth.xy * light0.shadowsInfo.yz.x;
            uv += 0.5;
            vec2 st = fract(uv);
            vec2 base_uv = floor(uv) - 0.5;
            base_uv *= light0.shadowsInfo.yz.y;
            vec2 uvw0 = 4. - 3. * st;
            vec2 uvw1 = vec2(7.);
            vec2 uvw2 = 1. + 3. * st;
            vec3 u = vec3((3. - 2. * st.x) / uvw0.x - 2., (3. + st.x) / uvw1.x, st.x / uvw2.x + 2.) * light0.shadowsInfo.yz.y;
            vec3 v = vec3((3. - 2. * st.y) / uvw0.y - 2., (3. + st.y) / uvw1.y, st.y / uvw2.y + 2.) * light0.shadowsInfo.yz.y;
            float shadow = 0.;
            shadow += uvw0.x * uvw0.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[0], v[0]), uvDepth.z));
            shadow += uvw1.x * uvw0.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[1], v[0]), uvDepth.z));
            shadow += uvw2.x * uvw0.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[2], v[0]), uvDepth.z));
            shadow += uvw0.x * uvw1.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[0], v[1]), uvDepth.z));
            shadow += uvw1.x * uvw1.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[1], v[1]), uvDepth.z));
            shadow += uvw2.x * uvw1.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[2], v[1]), uvDepth.z));
            shadow += uvw0.x * uvw2.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[0], v[2]), uvDepth.z));
            shadow += uvw1.x * uvw2.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[1], v[2]), uvDepth.z));
            shadow += uvw2.x * uvw2.y * texture(shadowSampler0, vec3(base_uv.xy + vec2(u[2], v[2]), uvDepth.z));
            shadow = shadow / 144.;
            shadow = mix(light0.shadowsInfo.x, 1., shadow);
            computeShadowWithPCF5_0 = computeFallOff(shadow, clipSpace.xy, light0.shadowsInfo.w);
        }
    }
    shadow = computeShadowWithPCF5_0;
    diffuseBase += info.diffuse * shadow;
    specularBase += info.specular * shadow;
    vec4 refractionColor = vec4(0., 0., 0., 1.);
    vec4 reflectionColor = vec4(0., 0., 0., 1.);
    vec3 emissiveColor = vEmissiveColor;
    vec3 finalDiffuse = clamp(diffuseBase * diffuseColor + emissiveColor + vAmbientColor, 0.0, 1.0) * baseColor.rgb;
    vec3 finalSpecular = specularBase * specularColor;
    vec4 color = vec4(finalDiffuse * baseAmbientColor + finalSpecular + reflectionColor.rgb + refractionColor.rgb, alpha);
#define CUSTOM_FRAGMENT_BEFORE_FOG
    color.rgb = max(color.rgb, 0.);
    color.a *= visibility;
#define CUSTOM_FRAGMENT_BEFORE_FRAGCOLOR
    glFragColor = color;
}

            )");
    desc.cFragmentStage.entryPoint = "main";
    desc.vertexStage.module =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                void main() {}
            )");
    device.CreateRenderPipeline(&desc);

    desc.cFragmentStage.module = utils::CreateShaderModuleFromASM(device, R"(
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 8
; Bound: 940
; Schema: 0
      OpCapability Shader
 %1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %vPositionW %vNormalW %vDepthMetric0 %vPositionFromLight0 %glFragColor
OpExecutionMode %main OriginUpperLeft
OpSource GLSL 450
OpName %main "main"
OpName %lightingInfo "lightingInfo"
OpMemberName %lightingInfo 0 "diffuse"
OpMemberName %lightingInfo 1 "specular"
OpName %computeLighting_vf3_vf3_vf4_vf3_vf3_f1_f1_ "computeLighting(vf3;vf3;vf4;vf3;vf3;f1;f1;"
OpName %viewDirectionW "viewDirectionW"
OpName %vNormal "vNormal"
OpName %lightData "lightData"
OpName %diffuseColor "diffuseColor"
OpName %specularColor "specularColor"
OpName %range "range"
OpName %glossiness "glossiness"
OpName %computeFallOff_f1_vf2_f1_ "computeFallOff(f1;vf2;f1;"
OpName %value "value"
OpName %clipSpace "clipSpace"
OpName %frustumEdgeFalloff "frustumEdgeFalloff"
OpName %attenuation "attenuation"
OpName %direction "direction"
OpName %vPositionW "vPositionW"
OpName %lightVectorW "lightVectorW"
OpName %ndl "ndl"
OpName %result "result"
OpName %angleW "angleW"
OpName %specComp "specComp"
OpName %mask "mask"
OpName %viewDirectionW_0 "viewDirectionW"
OpName %LeftOver "LeftOver"
OpMemberName %LeftOver 0 "lightMatrix0"
OpMemberName %LeftOver 1 "vEyePosition"
OpMemberName %LeftOver 2 "vAmbientColor"
OpName %_ ""
OpName %baseColor "baseColor"
OpName %diffuseColor_0 "diffuseColor"
OpName %Material "Material"
OpMemberName %Material 0 "diffuseLeftColor"
OpMemberName %Material 1 "diffuseRightColor"
OpMemberName %Material 2 "opacityParts"
OpMemberName %Material 3 "reflectionLeftColor"
OpMemberName %Material 4 "reflectionRightColor"
OpMemberName %Material 5 "refractionLeftColor"
OpMemberName %Material 6 "refractionRightColor"
OpMemberName %Material 7 "emissiveLeftColor"
OpMemberName %Material 8 "emissiveRightColor"
OpMemberName %Material 9 "vDiffuseInfos"
OpMemberName %Material 10 "vAmbientInfos"
OpMemberName %Material 11 "vOpacityInfos"
OpMemberName %Material 12 "vReflectionInfos"
OpMemberName %Material 13 "vReflectionPosition"
OpMemberName %Material 14 "vReflectionSize"
OpMemberName %Material 15 "vEmissiveInfos"
OpMemberName %Material 16 "vLightmapInfos"
OpMemberName %Material 17 "vSpecularInfos"
OpMemberName %Material 18 "vBumpInfos"
OpMemberName %Material 19 "diffuseMatrix"
OpMemberName %Material 20 "ambientMatrix"
OpMemberName %Material 21 "opacityMatrix"
OpMemberName %Material 22 "reflectionMatrix"
OpMemberName %Material 23 "emissiveMatrix"
OpMemberName %Material 24 "lightmapMatrix"
OpMemberName %Material 25 "specularMatrix"
OpMemberName %Material 26 "bumpMatrix"
OpMemberName %Material 27 "vTangentSpaceParams"
OpMemberName %Material 28 "pointSize"
OpMemberName %Material 29 "refractionMatrix"
OpMemberName %Material 30 "vRefractionInfos"
OpMemberName %Material 31 "vSpecularColor"
OpMemberName %Material 32 "vEmissiveColor"
OpMemberName %Material 33 "vDiffuseColor"
OpMemberName %Material 34 "vDetailInfos"
OpMemberName %Material 35 "detailMatrix"
OpName %__0 ""
OpName %alpha "alpha"
OpName %normalW "normalW"
OpName %vNormalW "vNormalW"
OpName %uvOffset "uvOffset"
OpName %baseAmbientColor "baseAmbientColor"
OpName %glossiness_0 "glossiness"
OpName %specularColor_0 "specularColor"
OpName %diffuseBase "diffuseBase"
OpName %specularBase "specularBase"
OpName %shadow "shadow"
OpName %info "info"
OpName %Light0 "Light0"
OpMemberName %Light0 0 "vLightData"
OpMemberName %Light0 1 "vLightDiffuse"
OpMemberName %Light0 2 "vLightSpecular"
OpMemberName %Light0 3 "shadowsInfo"
OpMemberName %Light0 4 "depthValues"
OpName %light0 "light0"
OpName %param "param"
OpName %param_0 "param"
OpName %param_1 "param"
OpName %param_2 "param"
OpName %param_3 "param"
OpName %param_4 "param"
OpName %param_5 "param"
OpName %vDepthMetric0 "vDepthMetric0"
OpName %computeShadowWithPCF5_0 "computeShadowWithPCF5_0"
OpName %clipSpace_0 "clipSpace"
OpName %vPositionFromLight0 "vPositionFromLight0"
OpName %uvDepth "uvDepth"
OpName %uv "uv"
OpName %st "st"
OpName %base_uv "base_uv"
OpName %uvw0 "uvw0"
OpName %uvw1 "uvw1"
OpName %uvw2 "uvw2"
OpName %u "u"
OpName %v "v"
OpName %shadow_0 "shadow"
OpName %shadowSampler0Texture "shadowSampler0Texture"
OpName %shadowSampler0Sampler "shadowSampler0Sampler"
OpName %param_6 "param"
OpName %param_7 "param"
OpName %param_8 "param"
OpName %refractionColor "refractionColor"
OpName %reflectionColor "reflectionColor"
OpName %emissiveColor "emissiveColor"
OpName %finalDiffuse "finalDiffuse"
OpName %finalSpecular "finalSpecular"
OpName %color "color"
OpName %Mesh "Mesh"
OpMemberName %Mesh 0 "world"
OpMemberName %Mesh 1 "visibility"
OpName %__1 ""
OpName %glFragColor "glFragColor"
OpName %Scene "Scene"
OpMemberName %Scene 0 "viewProjection"
OpMemberName %Scene 1 "view"
OpMemberName %Scene 2 "projection"
OpMemberName %Scene 3 "viewPosition"
OpName %__2 ""
OpDecorate %vPositionW Location 0
OpMemberDecorate %LeftOver 0 ColMajor
OpMemberDecorate %LeftOver 0 Offset 0
OpMemberDecorate %LeftOver 0 MatrixStride 16
OpMemberDecorate %LeftOver 1 Offset 64
OpMemberDecorate %LeftOver 2 Offset 80
OpDecorate %LeftOver Block
OpDecorate %_ DescriptorSet 2
OpDecorate %_ Binding 2
OpMemberDecorate %Material 0 Offset 0
OpMemberDecorate %Material 1 Offset 16
OpMemberDecorate %Material 2 Offset 32
OpMemberDecorate %Material 3 Offset 48
OpMemberDecorate %Material 4 Offset 64
OpMemberDecorate %Material 5 Offset 80
OpMemberDecorate %Material 6 Offset 96
OpMemberDecorate %Material 7 Offset 112
OpMemberDecorate %Material 8 Offset 128
OpMemberDecorate %Material 9 Offset 144
OpMemberDecorate %Material 10 Offset 152
OpMemberDecorate %Material 11 Offset 160
OpMemberDecorate %Material 12 Offset 168
OpMemberDecorate %Material 13 Offset 176
OpMemberDecorate %Material 14 Offset 192
OpMemberDecorate %Material 15 Offset 208
OpMemberDecorate %Material 16 Offset 216
OpMemberDecorate %Material 17 Offset 224
OpMemberDecorate %Material 18 Offset 240
OpMemberDecorate %Material 19 ColMajor
OpMemberDecorate %Material 19 Offset 256
OpMemberDecorate %Material 19 MatrixStride 16
OpMemberDecorate %Material 20 ColMajor
OpMemberDecorate %Material 20 Offset 320
OpMemberDecorate %Material 20 MatrixStride 16
OpMemberDecorate %Material 21 ColMajor
OpMemberDecorate %Material 21 Offset 384
OpMemberDecorate %Material 21 MatrixStride 16
OpMemberDecorate %Material 22 ColMajor
OpMemberDecorate %Material 22 Offset 448
OpMemberDecorate %Material 22 MatrixStride 16
OpMemberDecorate %Material 23 ColMajor
OpMemberDecorate %Material 23 Offset 512
OpMemberDecorate %Material 23 MatrixStride 16
OpMemberDecorate %Material 24 ColMajor
OpMemberDecorate %Material 24 Offset 576
OpMemberDecorate %Material 24 MatrixStride 16
OpMemberDecorate %Material 25 ColMajor
OpMemberDecorate %Material 25 Offset 640
OpMemberDecorate %Material 25 MatrixStride 16
OpMemberDecorate %Material 26 ColMajor
OpMemberDecorate %Material 26 Offset 704
OpMemberDecorate %Material 26 MatrixStride 16
OpMemberDecorate %Material 27 Offset 768
OpMemberDecorate %Material 28 Offset 776
OpMemberDecorate %Material 29 ColMajor
OpMemberDecorate %Material 29 Offset 784
OpMemberDecorate %Material 29 MatrixStride 16
OpMemberDecorate %Material 30 Offset 848
OpMemberDecorate %Material 31 Offset 864
OpMemberDecorate %Material 32 Offset 880
OpMemberDecorate %Material 33 Offset 896
OpMemberDecorate %Material 34 Offset 912
OpMemberDecorate %Material 35 ColMajor
OpMemberDecorate %Material 35 Offset 928
OpMemberDecorate %Material 35 MatrixStride 16
OpDecorate %Material Block
OpDecorate %__0 DescriptorSet 1
OpDecorate %__0 Binding 0
OpDecorate %vNormalW Location 1
OpMemberDecorate %Light0 0 Offset 0
OpMemberDecorate %Light0 1 Offset 16
OpMemberDecorate %Light0 2 Offset 32
OpMemberDecorate %Light0 3 Offset 48
OpMemberDecorate %Light0 4 Offset 64
OpDecorate %Light0 Block
OpDecorate %light0 DescriptorSet 0
OpDecorate %light0 Binding 5
OpDecorate %vDepthMetric0 Location 3
OpDecorate %vPositionFromLight0 Location 2
OpDecorate %shadowSampler0Texture DescriptorSet 2
OpDecorate %shadowSampler0Texture Binding 1
OpDecorate %shadowSampler0Sampler DescriptorSet 2
OpDecorate %shadowSampler0Sampler Binding 0
OpMemberDecorate %Mesh 0 ColMajor
OpMemberDecorate %Mesh 0 Offset 0
OpMemberDecorate %Mesh 0 MatrixStride 16
OpMemberDecorate %Mesh 1 Offset 64
OpDecorate %Mesh Block
OpDecorate %__1 DescriptorSet 1
OpDecorate %__1 Binding 1
OpDecorate %glFragColor Location 0
OpMemberDecorate %Scene 0 ColMajor
OpMemberDecorate %Scene 0 Offset 0
OpMemberDecorate %Scene 0 MatrixStride 16
OpMemberDecorate %Scene 1 ColMajor
OpMemberDecorate %Scene 1 Offset 64
OpMemberDecorate %Scene 1 MatrixStride 16
OpMemberDecorate %Scene 2 ColMajor
OpMemberDecorate %Scene 2 Offset 128
OpMemberDecorate %Scene 2 MatrixStride 16
OpMemberDecorate %Scene 3 Offset 192
OpDecorate %Scene Block
OpDecorate %__2 DescriptorSet 0
OpDecorate %__2 Binding 0
%void = OpTypeVoid
   %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v3float = OpTypeVector %float 3
%_ptr_Function_v3float = OpTypePointer Function %v3float
    %v4float = OpTypeVector %float 4
%_ptr_Function_v4float = OpTypePointer Function %v4float
%_ptr_Function_float = OpTypePointer Function %float
%lightingInfo = OpTypeStruct %v3float %v3float
         %13 = OpTypeFunction %lightingInfo %_ptr_Function_v3float %_ptr_Function_v3float %_ptr_Function_v4float %_ptr_Function_v3float %_ptr_Function_v3float %_ptr_Function_float %_ptr_Function_float
    %v2float = OpTypeVector %float 2
%_ptr_Function_v2float = OpTypePointer Function %v2float
         %25 = OpTypeFunction %float %_ptr_Function_float %_ptr_Function_v2float %_ptr_Function_float
    %float_1 = OpConstant %float 1
       %uint = OpTypeInt 32 0
     %uint_3 = OpConstant %uint 3
    %float_0 = OpConstant %float 0
       %bool = OpTypeBool
%_ptr_Input_v3float = OpTypePointer Input %v3float
 %vPositionW = OpVariable %_ptr_Input_v3float Input
%_ptr_Function_lightingInfo = OpTypePointer Function %lightingInfo
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
%float_1_00000012 = OpConstant %float 1.00000012
%mat4v4float = OpTypeMatrix %v4float 4
   %LeftOver = OpTypeStruct %mat4v4float %v3float %v3float
%_ptr_Uniform_LeftOver = OpTypePointer Uniform %LeftOver
          %_ = OpVariable %_ptr_Uniform_LeftOver Uniform
%_ptr_Uniform_v3float = OpTypePointer Uniform %v3float
        %128 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
   %Material = OpTypeStruct %v4float %v4float %v4float %v4float %v4float %v4float %v4float %v4float %v4float %v2float %v2float %v2float %v2float %v3float %v3float %v2float %v2float %v2float %v3float %mat4v4float %mat4v4float %mat4v4float %mat4v4float %mat4v4float %mat4v4float %mat4v4float %mat4v4float %v2float %float %mat4v4float %v4float %v4float %v3float %v4float %v4float %mat4v4float
%_ptr_Uniform_Material = OpTypePointer Uniform %Material
        %__0 = OpVariable %_ptr_Uniform_Material Uniform
     %int_33 = OpConstant %int 33
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
%_ptr_Uniform_float = OpTypePointer Uniform %float
   %vNormalW = OpVariable %_ptr_Input_v3float Input
        %147 = OpConstantComposite %v2float %float_0 %float_0
        %149 = OpConstantComposite %v3float %float_1 %float_1 %float_1
     %int_31 = OpConstant %int 31
        %159 = OpConstantComposite %v3float %float_0 %float_0 %float_0
     %Light0 = OpTypeStruct %v4float %v4float %v4float %v4float %v2float
%_ptr_Uniform_Light0 = OpTypePointer Uniform %Light0
     %light0 = OpVariable %_ptr_Uniform_Light0 Uniform
      %int_2 = OpConstant %int 2
%_ptr_Input_float = OpTypePointer Input %float
%vDepthMetric0 = OpVariable %_ptr_Input_float Input
%_ptr_Input_v4float = OpTypePointer Input %v4float
%vPositionFromLight0 = OpVariable %_ptr_Input_v4float Input
  %float_0_5 = OpConstant %float 0.5
        %215 = OpConstantComposite %v3float %float_0_5 %float_0_5 %float_0_5
      %int_3 = OpConstant %int 3
     %uint_1 = OpConstant %uint 1
     %uint_2 = OpConstant %uint 2
    %float_4 = OpConstant %float 4
    %float_3 = OpConstant %float 3
    %float_7 = OpConstant %float 7
        %254 = OpConstantComposite %v2float %float_7 %float_7
    %float_2 = OpConstant %float 2
     %uint_0 = OpConstant %uint 0
        %318 = OpTypeImage %float 2D 1 0 0 1 Unknown
%_ptr_UniformConstant_318 = OpTypePointer UniformConstant %318
%shadowSampler0Texture = OpVariable %_ptr_UniformConstant_318 UniformConstant
        %322 = OpTypeSampler
%_ptr_UniformConstant_322 = OpTypePointer UniformConstant %322
%shadowSampler0Sampler = OpVariable %_ptr_UniformConstant_322 UniformConstant
        %326 = OpTypeSampledImage %318
  %float_144 = OpConstant %float 144
        %575 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_1
     %int_32 = OpConstant %int 32
       %Mesh = OpTypeStruct %mat4v4float %float
%_ptr_Uniform_Mesh = OpTypePointer Uniform %Mesh
        %__1 = OpVariable %_ptr_Uniform_Mesh Uniform
%_ptr_Output_v4float = OpTypePointer Output %v4float
%glFragColor = OpVariable %_ptr_Output_v4float Output
      %Scene = OpTypeStruct %mat4v4float %mat4v4float %mat4v4float %v4float
%_ptr_Uniform_Scene = OpTypePointer Uniform %Scene
        %__2 = OpVariable %_ptr_Uniform_Scene Uniform
%float_3_14159274 = OpConstant %float 3.14159274
%float_5_96046448en08 = OpConstant %float 5.96046448e-08
%float_2_20000005 = OpConstant %float 2.20000005
%float_0_454545468 = OpConstant %float 0.454545468
%float_0_212599993 = OpConstant %float 0.212599993
%float_0_715200007 = OpConstant %float 0.715200007
%float_0_0722000003 = OpConstant %float 0.0722000003
        %645 = OpConstantComposite %v3float %float_0_212599993 %float_0_715200007 %float_0_0722000003
%float_1_00000001en07 = OpConstant %float 1.00000001e-07
  %float_255 = OpConstant %float 255
    %uint_64 = OpConstant %uint 64
%_arr_v3float_uint_64 = OpTypeArray %v3float %uint_64
%float_0_0640701279 = OpConstant %float 0.0640701279
%float_0_0540992692 = OpConstant %float 0.0540992692
        %652 = OpConstantComposite %v3float %float_0_0640701279 %float_0_0540992692 %float_0
%float_0_736657679 = OpConstant %float 0.736657679
%float_0_578939378 = OpConstant %float 0.578939378
        %655 = OpConstantComposite %v3float %float_0_736657679 %float_0_578939378 %float_0
%float_n0_627054214 = OpConstant %float -0.627054214
%float_n0_532027781 = OpConstant %float -0.532027781
        %658 = OpConstantComposite %v3float %float_n0_627054214 %float_n0_532027781 %float_0
%float_n0_409610689 = OpConstant %float -0.409610689
%float_0_841109514 = OpConstant %float 0.841109514
        %661 = OpConstantComposite %v3float %float_n0_409610689 %float_0_841109514 %float_0
%float_0_684956372 = OpConstant %float 0.684956372
%float_n0_49908179 = OpConstant %float -0.49908179
        %664 = OpConstantComposite %v3float %float_0_684956372 %float_n0_49908179 %float_0
%float_n0_874180973 = OpConstant %float -0.874180973
%float_n0_0457973517 = OpConstant %float -0.0457973517
        %667 = OpConstantComposite %v3float %float_n0_874180973 %float_n0_0457973517 %float_0
%float_0_998999774 = OpConstant %float 0.998999774
%float_0_000988006592 = OpConstant %float 0.000988006592
        %670 = OpConstantComposite %v3float %float_0_998999774 %float_0_000988006592 %float_0
%float_n0_0049205781 = OpConstant %float -0.0049205781
%float_n0_915164888 = OpConstant %float -0.915164888
        %673 = OpConstantComposite %v3float %float_n0_0049205781 %float_n0_915164888 %float_0
%float_0_180576295 = OpConstant %float 0.180576295
%float_0_974748313 = OpConstant %float 0.974748313
        %676 = OpConstantComposite %v3float %float_0_180576295 %float_0_974748313 %float_0
%float_n0_213845104 = OpConstant %float -0.213845104
%float_0_263581812 = OpConstant %float 0.263581812
        %679 = OpConstantComposite %v3float %float_n0_213845104 %float_0_263581812 %float_0
%float_0_109844998 = OpConstant %float 0.109844998
%float_0_388478488 = OpConstant %float 0.388478488
        %682 = OpConstantComposite %v3float %float_0_109844998 %float_0_388478488 %float_0
%float_0_0687675476 = OpConstant %float 0.0687675476
%float_n0_358107388 = OpConstant %float -0.358107388
        %685 = OpConstantComposite %v3float %float_0_0687675476 %float_n0_358107388 %float_0
%float_0_374072999 = OpConstant %float 0.374072999
%float_n0_766126573 = OpConstant %float -0.766126573
        %688 = OpConstantComposite %v3float %float_0_374072999 %float_n0_766126573 %float_0
%float_0_307913214 = OpConstant %float 0.307913214
%float_n0_121676303 = OpConstant %float -0.121676303
        %691 = OpConstantComposite %v3float %float_0_307913214 %float_n0_121676303 %float_0
%float_n0_379433513 = OpConstant %float -0.379433513
%float_n0_827158272 = OpConstant %float -0.827158272
        %694 = OpConstantComposite %v3float %float_n0_379433513 %float_n0_827158272 %float_0
%float_n0_203878 = OpConstant %float -0.203878
%float_n0_0771503374 = OpConstant %float -0.0771503374
        %697 = OpConstantComposite %v3float %float_n0_203878 %float_n0_0771503374 %float_0
%float_0_591269672 = OpConstant %float 0.591269672
%float_0_146979898 = OpConstant %float 0.146979898
        %700 = OpConstantComposite %v3float %float_0_591269672 %float_0_146979898 %float_0
%float_n0_880689979 = OpConstant %float -0.880689979
%float_0_3031784 = OpConstant %float 0.3031784
        %703 = OpConstantComposite %v3float %float_n0_880689979 %float_0_3031784 %float_0
%float_0_504010797 = OpConstant %float 0.504010797
%float_0_82837218 = OpConstant %float 0.82837218
        %706 = OpConstantComposite %v3float %float_0_504010797 %float_0_82837218 %float_0
%float_n0_584412396 = OpConstant %float -0.584412396
%float_0_54948771 = OpConstant %float 0.54948771
        %709 = OpConstantComposite %v3float %float_n0_584412396 %float_0_54948771 %float_0
%float_0_601779878 = OpConstant %float 0.601779878
%float_n0_172665402 = OpConstant %float -0.172665402
        %712 = OpConstantComposite %v3float %float_0_601779878 %float_n0_172665402 %float_0
%float_n0_555498123 = OpConstant %float -0.555498123
%float_0_155999705 = OpConstant %float 0.155999705
        %715 = OpConstantComposite %v3float %float_n0_555498123 %float_0_155999705 %float_0
%float_n0_301636904 = OpConstant %float -0.301636904
%float_n0_39009279 = OpConstant %float -0.39009279
        %718 = OpConstantComposite %v3float %float_n0_301636904 %float_n0_39009279 %float_0
%float_n0_555063188 = OpConstant %float -0.555063188
%float_n0_172376201 = OpConstant %float -0.172376201
        %721 = OpConstantComposite %v3float %float_n0_555063188 %float_n0_172376201 %float_0
%float_0_92502898 = OpConstant %float 0.92502898
%float_0_299504101 = OpConstant %float 0.299504101
        %724 = OpConstantComposite %v3float %float_0_92502898 %float_0_299504101 %float_0
%float_n0_247313693 = OpConstant %float -0.247313693
%float_0_553850472 = OpConstant %float 0.553850472
        %727 = OpConstantComposite %v3float %float_n0_247313693 %float_0_553850472 %float_0
%float_0_918303728 = OpConstant %float 0.918303728
%float_n0_286239207 = OpConstant %float -0.286239207
        %730 = OpConstantComposite %v3float %float_0_918303728 %float_n0_286239207 %float_0
%float_0_246942103 = OpConstant %float 0.246942103
%float_0_671871185 = OpConstant %float 0.671871185
        %733 = OpConstantComposite %v3float %float_0_246942103 %float_0_671871185 %float_0
%float_0_391639709 = OpConstant %float 0.391639709
%float_n0_432820886 = OpConstant %float -0.432820886
        %736 = OpConstantComposite %v3float %float_0_391639709 %float_n0_432820886 %float_0
%float_n0_0357692689 = OpConstant %float -0.0357692689
%float_n0_622003198 = OpConstant %float -0.622003198
        %739 = OpConstantComposite %v3float %float_n0_0357692689 %float_n0_622003198 %float_0
%float_n0_0466125496 = OpConstant %float -0.0466125496
%float_0_799520075 = OpConstant %float 0.799520075
        %742 = OpConstantComposite %v3float %float_n0_0466125496 %float_0_799520075 %float_0
%float_0_440292388 = OpConstant %float 0.440292388
%float_0_364031196 = OpConstant %float 0.364031196
        %745 = OpConstantComposite %v3float %float_0_440292388 %float_0_364031196 %float_0
        %746 = OpConstantComposite %_arr_v3float_uint_64 %652 %655 %658 %661 %664 %667 %670 %673 %676 %679 %682 %685 %688 %691 %694 %697 %700 %703 %706 %709 %712 %715 %718 %721 %724 %727 %730 %733 %736 %739 %742 %745 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159 %159
%float_n0_613391995 = OpConstant %float -0.613391995
%float_0_617480993 = OpConstant %float 0.617480993
        %749 = OpConstantComposite %v3float %float_n0_613391995 %float_0_617480993 %float_0
%float_0_170019001 = OpConstant %float 0.170019001
%float_n0_0402540006 = OpConstant %float -0.0402540006
        %752 = OpConstantComposite %v3float %float_0_170019001 %float_n0_0402540006 %float_0
%float_n0_299416989 = OpConstant %float -0.299416989
%float_0_791925013 = OpConstant %float 0.791925013
        %755 = OpConstantComposite %v3float %float_n0_299416989 %float_0_791925013 %float_0
%float_0_64568001 = OpConstant %float 0.64568001
%float_0_493209988 = OpConstant %float 0.493209988
        %758 = OpConstantComposite %v3float %float_0_64568001 %float_0_493209988 %float_0
%float_n0_651784003 = OpConstant %float -0.651784003
%float_0_717886984 = OpConstant %float 0.717886984
        %761 = OpConstantComposite %v3float %float_n0_651784003 %float_0_717886984 %float_0
%float_0_421003014 = OpConstant %float 0.421003014
%float_0_0270700008 = OpConstant %float 0.0270700008
        %764 = OpConstantComposite %v3float %float_0_421003014 %float_0_0270700008 %float_0
%float_n0_817193985 = OpConstant %float -0.817193985
%float_n0_271095991 = OpConstant %float -0.271095991
        %767 = OpConstantComposite %v3float %float_n0_817193985 %float_n0_271095991 %float_0
%float_n0_705374002 = OpConstant %float -0.705374002
%float_n0_668202996 = OpConstant %float -0.668202996
        %770 = OpConstantComposite %v3float %float_n0_705374002 %float_n0_668202996 %float_0
%float_0_977050006 = OpConstant %float 0.977050006
%float_n0_108615004 = OpConstant %float -0.108615004
        %773 = OpConstantComposite %v3float %float_0_977050006 %float_n0_108615004 %float_0
%float_0_0633260012 = OpConstant %float 0.0633260012
%float_0_142369002 = OpConstant %float 0.142369002
        %776 = OpConstantComposite %v3float %float_0_0633260012 %float_0_142369002 %float_0
%float_0_203528002 = OpConstant %float 0.203528002
%float_0_214331001 = OpConstant %float 0.214331001
        %779 = OpConstantComposite %v3float %float_0_203528002 %float_0_214331001 %float_0
%float_n0_667531013 = OpConstant %float -0.667531013
%float_0_326090008 = OpConstant %float 0.326090008
        %782 = OpConstantComposite %v3float %float_n0_667531013 %float_0_326090008 %float_0
%float_n0_0984219983 = OpConstant %float -0.0984219983
%float_n0_295754999 = OpConstant %float -0.295754999
        %785 = OpConstantComposite %v3float %float_n0_0984219983 %float_n0_295754999 %float_0
%float_n0_885922015 = OpConstant %float -0.885922015
%float_0_215369001 = OpConstant %float 0.215369001
        %788 = OpConstantComposite %v3float %float_n0_885922015 %float_0_215369001 %float_0
%float_0_56663698 = OpConstant %float 0.56663698
%float_0_605212986 = OpConstant %float 0.605212986
        %791 = OpConstantComposite %v3float %float_0_56663698 %float_0_605212986 %float_0
%float_0_0397659987 = OpConstant %float 0.0397659987
%float_n0_396100014 = OpConstant %float -0.396100014
        %794 = OpConstantComposite %v3float %float_0_0397659987 %float_n0_396100014 %float_0
%float_0_751945972 = OpConstant %float 0.751945972
%float_0_453352004 = OpConstant %float 0.453352004
        %797 = OpConstantComposite %v3float %float_0_751945972 %float_0_453352004 %float_0
%float_0_0787070021 = OpConstant %float 0.0787070021
%float_n0_715322971 = OpConstant %float -0.715322971
        %800 = OpConstantComposite %v3float %float_0_0787070021 %float_n0_715322971 %float_0
%float_n0_0758379996 = OpConstant %float -0.0758379996
%float_n0_529344022 = OpConstant %float -0.529344022
        %803 = OpConstantComposite %v3float %float_n0_0758379996 %float_n0_529344022 %float_0
%float_0_72447902 = OpConstant %float 0.72447902
%float_n0_58079797 = OpConstant %float -0.58079797
        %806 = OpConstantComposite %v3float %float_0_72447902 %float_n0_58079797 %float_0
%float_0_222999007 = OpConstant %float 0.222999007
%float_n0_215124995 = OpConstant %float -0.215124995
        %809 = OpConstantComposite %v3float %float_0_222999007 %float_n0_215124995 %float_0
%float_n0_467574 = OpConstant %float -0.467574
%float_n0_405438006 = OpConstant %float -0.405438006
        %812 = OpConstantComposite %v3float %float_n0_467574 %float_n0_405438006 %float_0
%float_n0_248267993 = OpConstant %float -0.248267993
%float_n0_814752996 = OpConstant %float -0.814752996
        %815 = OpConstantComposite %v3float %float_n0_248267993 %float_n0_814752996 %float_0
%float_0_354411006 = OpConstant %float 0.354411006
%float_n0_887570024 = OpConstant %float -0.887570024
        %818 = OpConstantComposite %v3float %float_0_354411006 %float_n0_887570024 %float_0
%float_0_175816998 = OpConstant %float 0.175816998
%float_0_382366002 = OpConstant %float 0.382366002
        %821 = OpConstantComposite %v3float %float_0_175816998 %float_0_382366002 %float_0
%float_0_487471998 = OpConstant %float 0.487471998
%float_n0_0630820021 = OpConstant %float -0.0630820021
        %824 = OpConstantComposite %v3float %float_0_487471998 %float_n0_0630820021 %float_0
%float_n0_084077999 = OpConstant %float -0.084077999
%float_0_898311973 = OpConstant %float 0.898311973
        %827 = OpConstantComposite %v3float %float_n0_084077999 %float_0_898311973 %float_0
%float_0_488875985 = OpConstant %float 0.488875985
%float_n0_783441007 = OpConstant %float -0.783441007
        %830 = OpConstantComposite %v3float %float_0_488875985 %float_n0_783441007 %float_0
%float_0_470016003 = OpConstant %float 0.470016003
%float_0_217932999 = OpConstant %float 0.217932999
        %833 = OpConstantComposite %v3float %float_0_470016003 %float_0_217932999 %float_0
%float_n0_696889997 = OpConstant %float -0.696889997
%float_n0_549790978 = OpConstant %float -0.549790978
        %836 = OpConstantComposite %v3float %float_n0_696889997 %float_n0_549790978 %float_0
%float_n0_149692997 = OpConstant %float -0.149692997
%float_0_605762005 = OpConstant %float 0.605762005
        %839 = OpConstantComposite %v3float %float_n0_149692997 %float_0_605762005 %float_0
%float_0_0342109986 = OpConstant %float 0.0342109986
%float_0_979979992 = OpConstant %float 0.979979992
        %842 = OpConstantComposite %v3float %float_0_0342109986 %float_0_979979992 %float_0
%float_0_503098011 = OpConstant %float 0.503098011
%float_n0_308878005 = OpConstant %float -0.308878005
        %845 = OpConstantComposite %v3float %float_0_503098011 %float_n0_308878005 %float_0
%float_n0_0162049998 = OpConstant %float -0.0162049998
%float_n0_87292099 = OpConstant %float -0.87292099
        %848 = OpConstantComposite %v3float %float_n0_0162049998 %float_n0_87292099 %float_0
%float_0_385784 = OpConstant %float 0.385784
%float_n0_393902004 = OpConstant %float -0.393902004
        %851 = OpConstantComposite %v3float %float_0_385784 %float_n0_393902004 %float_0
%float_n0_146886006 = OpConstant %float -0.146886006
%float_n0_859248996 = OpConstant %float -0.859248996
        %854 = OpConstantComposite %v3float %float_n0_146886006 %float_n0_859248996 %float_0
%float_0_643360972 = OpConstant %float 0.643360972
%float_0_164097995 = OpConstant %float 0.164097995
        %857 = OpConstantComposite %v3float %float_0_643360972 %float_0_164097995 %float_0
%float_0_63438803 = OpConstant %float 0.63438803
%float_n0_0494709983 = OpConstant %float -0.0494709983
        %860 = OpConstantComposite %v3float %float_0_63438803 %float_n0_0494709983 %float_0
%float_n0_688893974 = OpConstant %float -0.688893974
%float_0_00784299988 = OpConstant %float 0.00784299988
        %863 = OpConstantComposite %v3float %float_n0_688893974 %float_0_00784299988 %float_0
%float_0_464033991 = OpConstant %float 0.464033991
%float_n0_188817993 = OpConstant %float -0.188817993
        %866 = OpConstantComposite %v3float %float_0_464033991 %float_n0_188817993 %float_0
%float_n0_440840006 = OpConstant %float -0.440840006
%float_0_137485996 = OpConstant %float 0.137485996
        %869 = OpConstantComposite %v3float %float_n0_440840006 %float_0_137485996 %float_0
%float_0_364482999 = OpConstant %float 0.364482999
%float_0_511704028 = OpConstant %float 0.511704028
        %872 = OpConstantComposite %v3float %float_0_364482999 %float_0_511704028 %float_0
%float_0_0340280011 = OpConstant %float 0.0340280011
%float_0_325967997 = OpConstant %float 0.325967997
        %875 = OpConstantComposite %v3float %float_0_0340280011 %float_0_325967997 %float_0
%float_0_0990940034 = OpConstant %float 0.0990940034
%float_n0_308023006 = OpConstant %float -0.308023006
        %878 = OpConstantComposite %v3float %float_0_0990940034 %float_n0_308023006 %float_0
%float_0_693960011 = OpConstant %float 0.693960011
%float_n0_366252989 = OpConstant %float -0.366252989
        %881 = OpConstantComposite %v3float %float_0_693960011 %float_n0_366252989 %float_0
%float_0_678884029 = OpConstant %float 0.678884029
%float_n0_204687998 = OpConstant %float -0.204687998
        %884 = OpConstantComposite %v3float %float_0_678884029 %float_n0_204687998 %float_0
%float_0_00180099998 = OpConstant %float 0.00180099998
%float_0_780327976 = OpConstant %float 0.780327976
        %887 = OpConstantComposite %v3float %float_0_00180099998 %float_0_780327976 %float_0
%float_0_145177007 = OpConstant %float 0.145177007
%float_n0_898984015 = OpConstant %float -0.898984015
        %890 = OpConstantComposite %v3float %float_0_145177007 %float_n0_898984015 %float_0
%float_0_0626550019 = OpConstant %float 0.0626550019
%float_n0_611865997 = OpConstant %float -0.611865997
        %893 = OpConstantComposite %v3float %float_0_0626550019 %float_n0_611865997 %float_0
%float_0_315225989 = OpConstant %float 0.315225989
%float_n0_604296982 = OpConstant %float -0.604296982
        %896 = OpConstantComposite %v3float %float_0_315225989 %float_n0_604296982 %float_0
%float_n0_780144989 = OpConstant %float -0.780144989
%float_0_486250997 = OpConstant %float 0.486250997
        %899 = OpConstantComposite %v3float %float_n0_780144989 %float_0_486250997 %float_0
%float_n0_371868014 = OpConstant %float -0.371868014
%float_0_882138014 = OpConstant %float 0.882138014
        %902 = OpConstantComposite %v3float %float_n0_371868014 %float_0_882138014 %float_0
%float_0_200476006 = OpConstant %float 0.200476006
%float_0_494430006 = OpConstant %float 0.494430006
        %905 = OpConstantComposite %v3float %float_0_200476006 %float_0_494430006 %float_0
%float_n0_494551986 = OpConstant %float -0.494551986
%float_n0_711050987 = OpConstant %float -0.711050987
        %908 = OpConstantComposite %v3float %float_n0_494551986 %float_n0_711050987 %float_0
%float_0_612475991 = OpConstant %float 0.612475991
%float_0_705251992 = OpConstant %float 0.705251992
        %911 = OpConstantComposite %v3float %float_0_612475991 %float_0_705251992 %float_0
%float_n0_578845024 = OpConstant %float -0.578845024
%float_n0_768791974 = OpConstant %float -0.768791974
        %914 = OpConstantComposite %v3float %float_n0_578845024 %float_n0_768791974 %float_0
%float_n0_772454023 = OpConstant %float -0.772454023
%float_n0_0909759998 = OpConstant %float -0.0909759998
        %917 = OpConstantComposite %v3float %float_n0_772454023 %float_n0_0909759998 %float_0
%float_0_50444001 = OpConstant %float 0.50444001
%float_0_372294992 = OpConstant %float 0.372294992
        %920 = OpConstantComposite %v3float %float_0_50444001 %float_0_372294992 %float_0
%float_0_155735999 = OpConstant %float 0.155735999
%float_0_0651570037 = OpConstant %float 0.0651570037
        %923 = OpConstantComposite %v3float %float_0_155735999 %float_0_0651570037 %float_0
%float_0_39152199 = OpConstant %float 0.39152199
%float_0_849605024 = OpConstant %float 0.849605024
        %926 = OpConstantComposite %v3float %float_0_39152199 %float_0_849605024 %float_0
%float_n0_620105982 = OpConstant %float -0.620105982
%float_n0_328103989 = OpConstant %float -0.328103989
        %929 = OpConstantComposite %v3float %float_n0_620105982 %float_n0_328103989 %float_0
%float_0_789238989 = OpConstant %float 0.789238989
%float_n0_419964999 = OpConstant %float -0.419964999
        %932 = OpConstantComposite %v3float %float_0_789238989 %float_n0_419964999 %float_0
%float_n0_54539597 = OpConstant %float -0.54539597
%float_0_538133025 = OpConstant %float 0.538133025
        %935 = OpConstantComposite %v3float %float_n0_54539597 %float_0_538133025 %float_0
%float_n0_178563997 = OpConstant %float -0.178563997
%float_n0_596056998 = OpConstant %float -0.596056998
        %938 = OpConstantComposite %v3float %float_n0_178563997 %float_n0_596056998 %float_0
        %939 = OpConstantComposite %_arr_v3float_uint_64 %749 %752 %755 %758 %761 %764 %767 %770 %773 %776 %779 %782 %785 %788 %791 %794 %797 %800 %803 %806 %809 %812 %815 %818 %821 %824 %827 %830 %833 %836 %839 %842 %845 %848 %851 %854 %857 %860 %863 %866 %869 %872 %875 %878 %881 %884 %887 %890 %893 %896 %899 %902 %905 %908 %911 %914 %917 %920 %923 %926 %929 %932 %935 %938
       %main = OpFunction %void None %3
          %5 = OpLabel
%viewDirectionW_0 = OpVariable %_ptr_Function_v3float Function
  %baseColor = OpVariable %_ptr_Function_v4float Function
%diffuseColor_0 = OpVariable %_ptr_Function_v3float Function
      %alpha = OpVariable %_ptr_Function_float Function
    %normalW = OpVariable %_ptr_Function_v3float Function
   %uvOffset = OpVariable %_ptr_Function_v2float Function
%baseAmbientColor = OpVariable %_ptr_Function_v3float Function
%glossiness_0 = OpVariable %_ptr_Function_float Function
%specularColor_0 = OpVariable %_ptr_Function_v3float Function
%diffuseBase = OpVariable %_ptr_Function_v3float Function
%specularBase = OpVariable %_ptr_Function_v3float Function
     %shadow = OpVariable %_ptr_Function_float Function
       %info = OpVariable %_ptr_Function_lightingInfo Function
      %param = OpVariable %_ptr_Function_v3float Function
    %param_0 = OpVariable %_ptr_Function_v3float Function
    %param_1 = OpVariable %_ptr_Function_v4float Function
    %param_2 = OpVariable %_ptr_Function_v3float Function
    %param_3 = OpVariable %_ptr_Function_v3float Function
    %param_4 = OpVariable %_ptr_Function_float Function
    %param_5 = OpVariable %_ptr_Function_float Function
%computeShadowWithPCF5_0 = OpVariable %_ptr_Function_float Function
%clipSpace_0 = OpVariable %_ptr_Function_v3float Function
    %uvDepth = OpVariable %_ptr_Function_v3float Function
         %uv = OpVariable %_ptr_Function_v2float Function
         %st = OpVariable %_ptr_Function_v2float Function
    %base_uv = OpVariable %_ptr_Function_v2float Function
       %uvw0 = OpVariable %_ptr_Function_v2float Function
       %uvw1 = OpVariable %_ptr_Function_v2float Function
       %uvw2 = OpVariable %_ptr_Function_v2float Function
          %u = OpVariable %_ptr_Function_v3float Function
          %v = OpVariable %_ptr_Function_v3float Function
   %shadow_0 = OpVariable %_ptr_Function_float Function
    %param_6 = OpVariable %_ptr_Function_float Function
    %param_7 = OpVariable %_ptr_Function_v2float Function
    %param_8 = OpVariable %_ptr_Function_float Function
%refractionColor = OpVariable %_ptr_Function_v4float Function
%reflectionColor = OpVariable %_ptr_Function_v4float Function
%emissiveColor = OpVariable %_ptr_Function_v3float Function
%finalDiffuse = OpVariable %_ptr_Function_v3float Function
%finalSpecular = OpVariable %_ptr_Function_v3float Function
      %color = OpVariable %_ptr_Function_v4float Function
        %122 = OpAccessChain %_ptr_Uniform_v3float %_ %int_1
        %123 = OpLoad %v3float %122
        %124 = OpLoad %v3float %vPositionW
        %125 = OpFSub %v3float %123 %124
        %126 = OpExtInst %v3float %1 Normalize %125
               OpStore %viewDirectionW_0 %126
               OpStore %baseColor %128
        %135 = OpAccessChain %_ptr_Uniform_v4float %__0 %int_33
        %136 = OpLoad %v4float %135
        %137 = OpVectorShuffle %v3float %136 %136 0 1 2
               OpStore %diffuseColor_0 %137
        %140 = OpAccessChain %_ptr_Uniform_float %__0 %int_33 %uint_3
        %141 = OpLoad %float %140
               OpStore %alpha %141
        %144 = OpLoad %v3float %vNormalW
        %145 = OpExtInst %v3float %1 Normalize %144
               OpStore %normalW %145
               OpStore %uvOffset %147
               OpStore %baseAmbientColor %149
        %152 = OpAccessChain %_ptr_Uniform_float %__0 %int_31 %uint_3
        %153 = OpLoad %float %152
               OpStore %glossiness_0 %153
        %155 = OpAccessChain %_ptr_Uniform_v4float %__0 %int_31
        %156 = OpLoad %v4float %155
        %157 = OpVectorShuffle %v3float %156 %156 0 1 2
               OpStore %specularColor_0 %157
               OpStore %diffuseBase %159
               OpStore %specularBase %159
               OpStore %shadow %float_1
        %168 = OpLoad %v3float %viewDirectionW_0
               OpStore %param %168
        %170 = OpLoad %v3float %normalW
               OpStore %param_0 %170
        %172 = OpAccessChain %_ptr_Uniform_v4float %light0 %int_0
        %173 = OpLoad %v4float %172
               OpStore %param_1 %173
        %175 = OpAccessChain %_ptr_Uniform_v4float %light0 %int_1
        %176 = OpLoad %v4float %175
        %177 = OpVectorShuffle %v3float %176 %176 0 1 2
               OpStore %param_2 %177
        %179 = OpAccessChain %_ptr_Uniform_v4float %light0 %int_2
        %180 = OpLoad %v4float %179
        %181 = OpVectorShuffle %v3float %180 %180 0 1 2
               OpStore %param_3 %181
        %183 = OpAccessChain %_ptr_Uniform_float %light0 %int_1 %uint_3
        %184 = OpLoad %float %183
               OpStore %param_4 %184
        %186 = OpLoad %float %glossiness_0
               OpStore %param_5 %186
        %187 = OpFunctionCall %lightingInfo %computeLighting_vf3_vf3_vf4_vf3_vf3_f1_f1_ %param %param_0 %param_1 %param_2 %param_3 %param_4 %param_5
               OpStore %info %187
        %190 = OpLoad %float %vDepthMetric0
        %191 = OpFOrdGreaterThan %bool %190 %float_1
        %192 = OpLogicalNot %bool %191
               OpSelectionMerge %194 None
               OpBranchConditional %192 %193 %194
        %193 = OpLabel
        %195 = OpLoad %float %vDepthMetric0
        %196 = OpFOrdLessThan %bool %195 %float_0
               OpBranch %194
        %194 = OpLabel
        %197 = OpPhi %bool %191 %5 %196 %193
               OpSelectionMerge %199 None
               OpBranchConditional %197 %198 %201
        %198 = OpLabel
               OpStore %computeShadowWithPCF5_0 %float_1
               OpBranch %199
        %201 = OpLabel
        %205 = OpLoad %v4float %vPositionFromLight0
        %206 = OpVectorShuffle %v3float %205 %205 0 1 2
        %207 = OpAccessChain %_ptr_Input_float %vPositionFromLight0 %uint_3
        %208 = OpLoad %float %207
        %209 = OpCompositeConstruct %v3float %208 %208 %208
        %210 = OpFDiv %v3float %206 %209
               OpStore %clipSpace_0 %210
        %213 = OpLoad %v3float %clipSpace_0
        %214 = OpVectorTimesScalar %v3float %213 %float_0_5
        %216 = OpFAdd %v3float %214 %215
        %217 = OpCompositeExtract %float %216 0
        %218 = OpCompositeExtract %float %216 1
        %219 = OpCompositeExtract %float %216 2
        %220 = OpCompositeConstruct %v3float %217 %218 %219
               OpStore %uvDepth %220
        %222 = OpLoad %v3float %uvDepth
        %223 = OpVectorShuffle %v2float %222 %222 0 1
        %226 = OpAccessChain %_ptr_Uniform_float %light0 %int_3 %uint_1
        %227 = OpLoad %float %226
        %228 = OpVectorTimesScalar %v2float %223 %227
               OpStore %uv %228
        %229 = OpLoad %v2float %uv
        %230 = OpCompositeConstruct %v2float %float_0_5 %float_0_5
        %231 = OpFAdd %v2float %229 %230
               OpStore %uv %231
        %233 = OpLoad %v2float %uv
        %234 = OpExtInst %v2float %1 Fract %233
               OpStore %st %234
        %236 = OpLoad %v2float %uv
        %237 = OpExtInst %v2float %1 Floor %236
        %238 = OpCompositeConstruct %v2float %float_0_5 %float_0_5
        %239 = OpFSub %v2float %237 %238
               OpStore %base_uv %239
        %241 = OpAccessChain %_ptr_Uniform_float %light0 %int_3 %uint_2
        %242 = OpLoad %float %241
        %243 = OpLoad %v2float %base_uv
        %244 = OpVectorTimesScalar %v2float %243 %242
               OpStore %base_uv %244
        %248 = OpLoad %v2float %st
        %249 = OpVectorTimesScalar %v2float %248 %float_3
        %250 = OpCompositeConstruct %v2float %float_4 %float_4
        %251 = OpFSub %v2float %250 %249
               OpStore %uvw0 %251
               OpStore %uvw1 %254
        %256 = OpLoad %v2float %st
        %257 = OpVectorTimesScalar %v2float %256 %float_3
        %258 = OpCompositeConstruct %v2float %float_1 %float_1
        %259 = OpFAdd %v2float %258 %257
               OpStore %uvw2 %259
        %263 = OpAccessChain %_ptr_Function_float %st %uint_0
        %264 = OpLoad %float %263
        %265 = OpFMul %float %float_2 %264
        %266 = OpFSub %float %float_3 %265
        %267 = OpAccessChain %_ptr_Function_float %uvw0 %uint_0
        %268 = OpLoad %float %267
        %269 = OpFDiv %float %266 %268
        %270 = OpFSub %float %269 %float_2
        %271 = OpAccessChain %_ptr_Function_float %st %uint_0
        %272 = OpLoad %float %271
        %273 = OpFAdd %float %float_3 %272
        %274 = OpAccessChain %_ptr_Function_float %uvw1 %uint_0
        %275 = OpLoad %float %274
        %276 = OpFDiv %float %273 %275
        %277 = OpAccessChain %_ptr_Function_float %st %uint_0
        %278 = OpLoad %float %277
        %279 = OpAccessChain %_ptr_Function_float %uvw2 %uint_0
        %280 = OpLoad %float %279
        %281 = OpFDiv %float %278 %280
        %282 = OpFAdd %float %281 %float_2
        %283 = OpCompositeConstruct %v3float %270 %276 %282
        %284 = OpAccessChain %_ptr_Uniform_float %light0 %int_3 %uint_2
        %285 = OpLoad %float %284
        %286 = OpVectorTimesScalar %v3float %283 %285
               OpStore %u %286
        %288 = OpAccessChain %_ptr_Function_float %st %uint_1
        %289 = OpLoad %float %288
        %290 = OpFMul %float %float_2 %289
        %291 = OpFSub %float %float_3 %290
        %292 = OpAccessChain %_ptr_Function_float %uvw0 %uint_1
        %293 = OpLoad %float %292
        %294 = OpFDiv %float %291 %293
        %295 = OpFSub %float %294 %float_2
        %296 = OpAccessChain %_ptr_Function_float %st %uint_1
        %297 = OpLoad %float %296
        %298 = OpFAdd %float %float_3 %297
        %299 = OpAccessChain %_ptr_Function_float %uvw1 %uint_1
        %300 = OpLoad %float %299
        %301 = OpFDiv %float %298 %300
        %302 = OpAccessChain %_ptr_Function_float %st %uint_1
        %303 = OpLoad %float %302
        %304 = OpAccessChain %_ptr_Function_float %uvw2 %uint_1
        %305 = OpLoad %float %304
        %306 = OpFDiv %float %303 %305
        %307 = OpFAdd %float %306 %float_2
        %308 = OpCompositeConstruct %v3float %295 %301 %307
        %309 = OpAccessChain %_ptr_Uniform_float %light0 %int_3 %uint_2
        %310 = OpLoad %float %309
        %311 = OpVectorTimesScalar %v3float %308 %310
               OpStore %v %311
               OpStore %shadow_0 %float_0
        %313 = OpAccessChain %_ptr_Function_float %uvw0 %uint_0
        %314 = OpLoad %float %313
        %315 = OpAccessChain %_ptr_Function_float %uvw0 %uint_1
        %316 = OpLoad %float %315
        %317 = OpFMul %float %314 %316
        %321 = OpLoad %318 %shadowSampler0Texture
        %325 = OpLoad %322 %shadowSampler0Sampler
        %327 = OpSampledImage %326 %321 %325
        %328 = OpLoad %v2float %base_uv
        %329 = OpAccessChain %_ptr_Function_float %u %uint_0
        %330 = OpLoad %float %329
        %331 = OpAccessChain %_ptr_Function_float %v %uint_0
        %332 = OpLoad %float %331
        %333 = OpCompositeConstruct %v2float %330 %332
        %334 = OpFAdd %v2float %328 %333
        %335 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %336 = OpLoad %float %335
        %337 = OpCompositeExtract %float %334 0
        %338 = OpCompositeExtract %float %334 1
        %339 = OpCompositeConstruct %v3float %337 %338 %336
        %340 = OpCompositeExtract %float %339 2
        %341 = OpImageSampleDrefImplicitLod %float %327 %339 %340
        %342 = OpFMul %float %317 %341
        %343 = OpLoad %float %shadow_0
        %344 = OpFAdd %float %343 %342
               OpStore %shadow_0 %344
        %345 = OpAccessChain %_ptr_Function_float %uvw1 %uint_0
        %346 = OpLoad %float %345
        %347 = OpAccessChain %_ptr_Function_float %uvw0 %uint_1
        %348 = OpLoad %float %347
        %349 = OpFMul %float %346 %348
        %350 = OpLoad %318 %shadowSampler0Texture
        %351 = OpLoad %322 %shadowSampler0Sampler
        %352 = OpSampledImage %326 %350 %351
        %353 = OpLoad %v2float %base_uv
        %354 = OpAccessChain %_ptr_Function_float %u %uint_1
        %355 = OpLoad %float %354
        %356 = OpAccessChain %_ptr_Function_float %v %uint_0
        %357 = OpLoad %float %356
        %358 = OpCompositeConstruct %v2float %355 %357
        %359 = OpFAdd %v2float %353 %358
        %360 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %361 = OpLoad %float %360
        %362 = OpCompositeExtract %float %359 0
        %363 = OpCompositeExtract %float %359 1
        %364 = OpCompositeConstruct %v3float %362 %363 %361
        %365 = OpCompositeExtract %float %364 2
        %366 = OpImageSampleDrefImplicitLod %float %352 %364 %365
        %367 = OpFMul %float %349 %366
        %368 = OpLoad %float %shadow_0
        %369 = OpFAdd %float %368 %367
               OpStore %shadow_0 %369
        %370 = OpAccessChain %_ptr_Function_float %uvw2 %uint_0
        %371 = OpLoad %float %370
        %372 = OpAccessChain %_ptr_Function_float %uvw0 %uint_1
        %373 = OpLoad %float %372
        %374 = OpFMul %float %371 %373
        %375 = OpLoad %318 %shadowSampler0Texture
        %376 = OpLoad %322 %shadowSampler0Sampler
        %377 = OpSampledImage %326 %375 %376
        %378 = OpLoad %v2float %base_uv
        %379 = OpAccessChain %_ptr_Function_float %u %uint_2
        %380 = OpLoad %float %379
        %381 = OpAccessChain %_ptr_Function_float %v %uint_0
        %382 = OpLoad %float %381
        %383 = OpCompositeConstruct %v2float %380 %382
        %384 = OpFAdd %v2float %378 %383
        %385 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %386 = OpLoad %float %385
        %387 = OpCompositeExtract %float %384 0
        %388 = OpCompositeExtract %float %384 1
        %389 = OpCompositeConstruct %v3float %387 %388 %386
        %390 = OpCompositeExtract %float %389 2
        %391 = OpImageSampleDrefImplicitLod %float %377 %389 %390
        %392 = OpFMul %float %374 %391
        %393 = OpLoad %float %shadow_0
        %394 = OpFAdd %float %393 %392
               OpStore %shadow_0 %394
        %395 = OpAccessChain %_ptr_Function_float %uvw0 %uint_0
        %396 = OpLoad %float %395
        %397 = OpAccessChain %_ptr_Function_float %uvw1 %uint_1
        %398 = OpLoad %float %397
        %399 = OpFMul %float %396 %398
        %400 = OpLoad %318 %shadowSampler0Texture
        %401 = OpLoad %322 %shadowSampler0Sampler
        %402 = OpSampledImage %326 %400 %401
        %403 = OpLoad %v2float %base_uv
        %404 = OpAccessChain %_ptr_Function_float %u %uint_0
        %405 = OpLoad %float %404
        %406 = OpAccessChain %_ptr_Function_float %v %uint_1
        %407 = OpLoad %float %406
        %408 = OpCompositeConstruct %v2float %405 %407
        %409 = OpFAdd %v2float %403 %408
        %410 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %411 = OpLoad %float %410
        %412 = OpCompositeExtract %float %409 0
        %413 = OpCompositeExtract %float %409 1
        %414 = OpCompositeConstruct %v3float %412 %413 %411
        %415 = OpCompositeExtract %float %414 2
        %416 = OpImageSampleDrefImplicitLod %float %402 %414 %415
        %417 = OpFMul %float %399 %416
        %418 = OpLoad %float %shadow_0
        %419 = OpFAdd %float %418 %417
               OpStore %shadow_0 %419
        %420 = OpAccessChain %_ptr_Function_float %uvw1 %uint_0
        %421 = OpLoad %float %420
        %422 = OpAccessChain %_ptr_Function_float %uvw1 %uint_1
        %423 = OpLoad %float %422
        %424 = OpFMul %float %421 %423
        %425 = OpLoad %318 %shadowSampler0Texture
        %426 = OpLoad %322 %shadowSampler0Sampler
        %427 = OpSampledImage %326 %425 %426
        %428 = OpLoad %v2float %base_uv
        %429 = OpAccessChain %_ptr_Function_float %u %uint_1
        %430 = OpLoad %float %429
        %431 = OpAccessChain %_ptr_Function_float %v %uint_1
        %432 = OpLoad %float %431
        %433 = OpCompositeConstruct %v2float %430 %432
        %434 = OpFAdd %v2float %428 %433
        %435 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %436 = OpLoad %float %435
        %437 = OpCompositeExtract %float %434 0
        %438 = OpCompositeExtract %float %434 1
        %439 = OpCompositeConstruct %v3float %437 %438 %436
        %440 = OpCompositeExtract %float %439 2
        %441 = OpImageSampleDrefImplicitLod %float %427 %439 %440
        %442 = OpFMul %float %424 %441
        %443 = OpLoad %float %shadow_0
        %444 = OpFAdd %float %443 %442
               OpStore %shadow_0 %444
        %445 = OpAccessChain %_ptr_Function_float %uvw2 %uint_0
        %446 = OpLoad %float %445
        %447 = OpAccessChain %_ptr_Function_float %uvw1 %uint_1
        %448 = OpLoad %float %447
        %449 = OpFMul %float %446 %448
        %450 = OpLoad %318 %shadowSampler0Texture
        %451 = OpLoad %322 %shadowSampler0Sampler
        %452 = OpSampledImage %326 %450 %451
        %453 = OpLoad %v2float %base_uv
        %454 = OpAccessChain %_ptr_Function_float %u %uint_2
        %455 = OpLoad %float %454
        %456 = OpAccessChain %_ptr_Function_float %v %uint_1
        %457 = OpLoad %float %456
        %458 = OpCompositeConstruct %v2float %455 %457
        %459 = OpFAdd %v2float %453 %458
        %460 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %461 = OpLoad %float %460
        %462 = OpCompositeExtract %float %459 0
        %463 = OpCompositeExtract %float %459 1
        %464 = OpCompositeConstruct %v3float %462 %463 %461
        %465 = OpCompositeExtract %float %464 2
        %466 = OpImageSampleDrefImplicitLod %float %452 %464 %465
        %467 = OpFMul %float %449 %466
        %468 = OpLoad %float %shadow_0
        %469 = OpFAdd %float %468 %467
               OpStore %shadow_0 %469
        %470 = OpAccessChain %_ptr_Function_float %uvw0 %uint_0
        %471 = OpLoad %float %470
        %472 = OpAccessChain %_ptr_Function_float %uvw2 %uint_1
        %473 = OpLoad %float %472
        %474 = OpFMul %float %471 %473
        %475 = OpLoad %318 %shadowSampler0Texture
        %476 = OpLoad %322 %shadowSampler0Sampler
        %477 = OpSampledImage %326 %475 %476
        %478 = OpLoad %v2float %base_uv
        %479 = OpAccessChain %_ptr_Function_float %u %uint_0
        %480 = OpLoad %float %479
        %481 = OpAccessChain %_ptr_Function_float %v %uint_2
        %482 = OpLoad %float %481
        %483 = OpCompositeConstruct %v2float %480 %482
        %484 = OpFAdd %v2float %478 %483
        %485 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %486 = OpLoad %float %485
        %487 = OpCompositeExtract %float %484 0
        %488 = OpCompositeExtract %float %484 1
        %489 = OpCompositeConstruct %v3float %487 %488 %486
        %490 = OpCompositeExtract %float %489 2
        %491 = OpImageSampleDrefImplicitLod %float %477 %489 %490
        %492 = OpFMul %float %474 %491
        %493 = OpLoad %float %shadow_0
        %494 = OpFAdd %float %493 %492
               OpStore %shadow_0 %494
        %495 = OpAccessChain %_ptr_Function_float %uvw1 %uint_0
        %496 = OpLoad %float %495
        %497 = OpAccessChain %_ptr_Function_float %uvw2 %uint_1
        %498 = OpLoad %float %497
        %499 = OpFMul %float %496 %498
        %500 = OpLoad %318 %shadowSampler0Texture
        %501 = OpLoad %322 %shadowSampler0Sampler
        %502 = OpSampledImage %326 %500 %501
        %503 = OpLoad %v2float %base_uv
        %504 = OpAccessChain %_ptr_Function_float %u %uint_1
        %505 = OpLoad %float %504
        %506 = OpAccessChain %_ptr_Function_float %v %uint_2
        %507 = OpLoad %float %506
        %508 = OpCompositeConstruct %v2float %505 %507
        %509 = OpFAdd %v2float %503 %508
        %510 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %511 = OpLoad %float %510
        %512 = OpCompositeExtract %float %509 0
        %513 = OpCompositeExtract %float %509 1
        %514 = OpCompositeConstruct %v3float %512 %513 %511
        %515 = OpCompositeExtract %float %514 2
        %516 = OpImageSampleDrefImplicitLod %float %502 %514 %515
        %517 = OpFMul %float %499 %516
        %518 = OpLoad %float %shadow_0
        %519 = OpFAdd %float %518 %517
               OpStore %shadow_0 %519
        %520 = OpAccessChain %_ptr_Function_float %uvw2 %uint_0
        %521 = OpLoad %float %520
        %522 = OpAccessChain %_ptr_Function_float %uvw2 %uint_1
        %523 = OpLoad %float %522
        %524 = OpFMul %float %521 %523
        %525 = OpLoad %318 %shadowSampler0Texture
        %526 = OpLoad %322 %shadowSampler0Sampler
        %527 = OpSampledImage %326 %525 %526
        %528 = OpLoad %v2float %base_uv
        %529 = OpAccessChain %_ptr_Function_float %u %uint_2
        %530 = OpLoad %float %529
        %531 = OpAccessChain %_ptr_Function_float %v %uint_2
        %532 = OpLoad %float %531
        %533 = OpCompositeConstruct %v2float %530 %532
        %534 = OpFAdd %v2float %528 %533
        %535 = OpAccessChain %_ptr_Function_float %uvDepth %uint_2
        %536 = OpLoad %float %535
        %537 = OpCompositeExtract %float %534 0
        %538 = OpCompositeExtract %float %534 1
        %539 = OpCompositeConstruct %v3float %537 %538 %536
        %540 = OpCompositeExtract %float %539 2
        %541 = OpImageSampleDrefImplicitLod %float %527 %539 %540
        %542 = OpFMul %float %524 %541
        %543 = OpLoad %float %shadow_0
        %544 = OpFAdd %float %543 %542
               OpStore %shadow_0 %544
        %545 = OpLoad %float %shadow_0
        %547 = OpFDiv %float %545 %float_144
               OpStore %shadow_0 %547
        %548 = OpAccessChain %_ptr_Uniform_float %light0 %int_3 %uint_0
        %549 = OpLoad %float %548
        %550 = OpLoad %float %shadow_0
        %551 = OpExtInst %float %1 FMix %549 %float_1 %550
               OpStore %shadow_0 %551
        %553 = OpLoad %float %shadow_0
               OpStore %param_6 %553
        %555 = OpLoad %v3float %clipSpace_0
        %556 = OpVectorShuffle %v2float %555 %555 0 1
               OpStore %param_7 %556
        %558 = OpAccessChain %_ptr_Uniform_float %light0 %int_3 %uint_3
        %559 = OpLoad %float %558
               OpStore %param_8 %559
        %560 = OpFunctionCall %float %computeFallOff_f1_vf2_f1_ %param_6 %param_7 %param_8
               OpStore %computeShadowWithPCF5_0 %560
               OpBranch %199
        %199 = OpLabel
        %561 = OpLoad %float %computeShadowWithPCF5_0
               OpStore %shadow %561
        %562 = OpAccessChain %_ptr_Function_v3float %info %int_0
        %563 = OpLoad %v3float %562
        %564 = OpLoad %float %shadow
        %565 = OpVectorTimesScalar %v3float %563 %564
        %566 = OpLoad %v3float %diffuseBase
        %567 = OpFAdd %v3float %566 %565
               OpStore %diffuseBase %567
        %568 = OpAccessChain %_ptr_Function_v3float %info %int_1
        %569 = OpLoad %v3float %568
        %570 = OpLoad %float %shadow
        %571 = OpVectorTimesScalar %v3float %569 %570
        %572 = OpLoad %v3float %specularBase
        %573 = OpFAdd %v3float %572 %571
               OpStore %specularBase %573
               OpStore %refractionColor %575
               OpStore %reflectionColor %575
        %579 = OpAccessChain %_ptr_Uniform_v3float %__0 %int_32
        %580 = OpLoad %v3float %579
               OpStore %emissiveColor %580
        %582 = OpLoad %v3float %diffuseBase
        %583 = OpLoad %v3float %diffuseColor_0
        %584 = OpFMul %v3float %582 %583
        %585 = OpLoad %v3float %emissiveColor
        %586 = OpFAdd %v3float %584 %585
        %587 = OpAccessChain %_ptr_Uniform_v3float %_ %int_2
        %588 = OpLoad %v3float %587
        %589 = OpFAdd %v3float %586 %588
        %590 = OpCompositeConstruct %v3float %float_0 %float_0 %float_0
        %591 = OpCompositeConstruct %v3float %float_1 %float_1 %float_1
        %592 = OpExtInst %v3float %1 FClamp %589 %590 %591
        %593 = OpLoad %v4float %baseColor
        %594 = OpVectorShuffle %v3float %593 %593 0 1 2
        %595 = OpFMul %v3float %592 %594
               OpStore %finalDiffuse %595
        %597 = OpLoad %v3float %specularBase
        %598 = OpLoad %v3float %specularColor_0
        %599 = OpFMul %v3float %597 %598
               OpStore %finalSpecular %599
        %601 = OpLoad %v3float %finalDiffuse
        %602 = OpLoad %v3float %baseAmbientColor
        %603 = OpFMul %v3float %601 %602
        %604 = OpLoad %v3float %finalSpecular
        %605 = OpFAdd %v3float %603 %604
        %606 = OpLoad %v4float %reflectionColor
        %607 = OpVectorShuffle %v3float %606 %606 0 1 2
        %608 = OpFAdd %v3float %605 %607
        %609 = OpLoad %v4float %refractionColor
        %610 = OpVectorShuffle %v3float %609 %609 0 1 2
        %611 = OpFAdd %v3float %608 %610
        %612 = OpLoad %float %alpha
        %613 = OpCompositeExtract %float %611 0
        %614 = OpCompositeExtract %float %611 1
        %615 = OpCompositeExtract %float %611 2
        %616 = OpCompositeConstruct %v4float %613 %614 %615 %612
               OpStore %color %616
        %617 = OpLoad %v4float %color
        %618 = OpVectorShuffle %v3float %617 %617 0 1 2
        %619 = OpCompositeConstruct %v3float %float_0 %float_0 %float_0
        %620 = OpExtInst %v3float %1 FMax %618 %619
        %621 = OpLoad %v4float %color
        %622 = OpVectorShuffle %v4float %621 %620 4 5 6 3
               OpStore %color %622
        %626 = OpAccessChain %_ptr_Uniform_float %__1 %int_1
        %627 = OpLoad %float %626
        %628 = OpAccessChain %_ptr_Function_float %color %uint_3
        %629 = OpLoad %float %628
        %630 = OpFMul %float %629 %627
        %631 = OpAccessChain %_ptr_Function_float %color %uint_3
               OpStore %631 %630
        %634 = OpLoad %v4float %color
               OpStore %glFragColor %634
               OpReturn
               OpFunctionEnd
%computeLighting_vf3_vf3_vf4_vf3_vf3_f1_f1_ = OpFunction %lightingInfo None %13
%viewDirectionW = OpFunctionParameter %_ptr_Function_v3float
    %vNormal = OpFunctionParameter %_ptr_Function_v3float
  %lightData = OpFunctionParameter %_ptr_Function_v4float
%diffuseColor = OpFunctionParameter %_ptr_Function_v3float
%specularColor = OpFunctionParameter %_ptr_Function_v3float
      %range = OpFunctionParameter %_ptr_Function_float
 %glossiness = OpFunctionParameter %_ptr_Function_float
         %22 = OpLabel
%attenuation = OpVariable %_ptr_Function_float Function
  %direction = OpVariable %_ptr_Function_v3float Function
%lightVectorW = OpVariable %_ptr_Function_v3float Function
        %ndl = OpVariable %_ptr_Function_float Function
     %result = OpVariable %_ptr_Function_lightingInfo Function
     %angleW = OpVariable %_ptr_Function_v3float Function
   %specComp = OpVariable %_ptr_Function_float Function
               OpStore %attenuation %float_1
         %35 = OpAccessChain %_ptr_Function_float %lightData %uint_3
         %36 = OpLoad %float %35
         %39 = OpFOrdEqual %bool %36 %float_0
               OpSelectionMerge %41 None
               OpBranchConditional %39 %40 %58
         %40 = OpLabel
         %43 = OpLoad %v4float %lightData
         %44 = OpVectorShuffle %v3float %43 %43 0 1 2
         %47 = OpLoad %v3float %vPositionW
         %48 = OpFSub %v3float %44 %47
               OpStore %direction %48
         %49 = OpLoad %v3float %direction
         %50 = OpExtInst %float %1 Length %49
         %51 = OpLoad %float %range
         %52 = OpFDiv %float %50 %51
         %53 = OpFSub %float %float_1 %52
         %54 = OpExtInst %float %1 FMax %float_0 %53
               OpStore %attenuation %54
         %56 = OpLoad %v3float %direction
         %57 = OpExtInst %v3float %1 Normalize %56
               OpStore %lightVectorW %57
               OpBranch %41
         %58 = OpLabel
         %59 = OpLoad %v4float %lightData
         %60 = OpVectorShuffle %v3float %59 %59 0 1 2
         %61 = OpFNegate %v3float %60
         %62 = OpExtInst %v3float %1 Normalize %61
               OpStore %lightVectorW %62
               OpBranch %41
         %41 = OpLabel
         %64 = OpLoad %v3float %vNormal
         %65 = OpLoad %v3float %lightVectorW
         %66 = OpDot %float %64 %65
         %67 = OpExtInst %float %1 FMax %float_0 %66
               OpStore %ndl %67
         %72 = OpLoad %float %ndl
         %73 = OpLoad %v3float %diffuseColor
         %74 = OpVectorTimesScalar %v3float %73 %72
         %75 = OpLoad %float %attenuation
         %76 = OpVectorTimesScalar %v3float %74 %75
         %77 = OpAccessChain %_ptr_Function_v3float %result %int_0
               OpStore %77 %76
         %79 = OpLoad %v3float %viewDirectionW
         %80 = OpLoad %v3float %lightVectorW
         %81 = OpFAdd %v3float %79 %80
         %82 = OpExtInst %v3float %1 Normalize %81
               OpStore %angleW %82
         %84 = OpLoad %v3float %vNormal
         %85 = OpLoad %v3float %angleW
         %86 = OpDot %float %84 %85
         %87 = OpExtInst %float %1 FMax %float_0 %86
               OpStore %specComp %87
         %88 = OpLoad %float %specComp
         %89 = OpLoad %float %glossiness
         %90 = OpExtInst %float %1 FMax %float_1 %89
         %91 = OpExtInst %float %1 Pow %88 %90
               OpStore %specComp %91
         %93 = OpLoad %float %specComp
         %94 = OpLoad %v3float %specularColor
         %95 = OpVectorTimesScalar %v3float %94 %93
         %96 = OpLoad %float %attenuation
         %97 = OpVectorTimesScalar %v3float %95 %96
         %98 = OpAccessChain %_ptr_Function_v3float %result %int_1
               OpStore %98 %97
         %99 = OpLoad %lightingInfo %result
               OpReturnValue %99
               OpFunctionEnd
%computeFallOff_f1_vf2_f1_ = OpFunction %float None %25
      %value = OpFunctionParameter %_ptr_Function_float
  %clipSpace = OpFunctionParameter %_ptr_Function_v2float
%frustumEdgeFalloff = OpFunctionParameter %_ptr_Function_float
         %30 = OpLabel
       %mask = OpVariable %_ptr_Function_float Function
        %103 = OpLoad %float %frustumEdgeFalloff
        %104 = OpFSub %float %float_1 %103
        %106 = OpLoad %v2float %clipSpace
        %107 = OpLoad %v2float %clipSpace
        %108 = OpDot %float %106 %107
        %109 = OpExtInst %float %1 FClamp %108 %float_0 %float_1
        %110 = OpExtInst %float %1 SmoothStep %104 %float_1_00000012 %109
               OpStore %mask %110
        %111 = OpLoad %float %value
        %112 = OpLoad %float %mask
        %113 = OpExtInst %float %1 FMix %111 %float_1 %112
               OpReturnValue %113
               OpFunctionEnd

    )");
    device.CreateRenderPipeline(&desc);
}

constexpr uint64_t kBufferSize = 3 * kMinDynamicBufferOffsetAlignment + 8;
constexpr uint32_t kBindingSize = 9;

class SetBindGroupValidationTest : public ValidationTest {
  public:
    void SetUp() override {
        ValidationTest::SetUp();

        mBindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                      wgpu::BindingType::UniformBuffer, true},
                     {1, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                      wgpu::BindingType::UniformBuffer, false},
                     {2, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                      wgpu::BindingType::StorageBuffer, true},
                     {3, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                      wgpu::BindingType::ReadonlyStorageBuffer, true}});
    }

    wgpu::Buffer CreateBuffer(uint64_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = usage;

        return device.CreateBuffer(&bufferDescriptor);
    }

    wgpu::BindGroupLayout mBindGroupLayout;

    wgpu::RenderPipeline CreateRenderPipeline() {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                void main() {
                })");

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
                #version 450
                layout(std140, set = 0, binding = 0) uniform uBufferDynamic {
                    vec2 value0;
                };
                layout(std140, set = 0, binding = 1) uniform uBuffer {
                    vec2 value1;
                };
                layout(std140, set = 0, binding = 2) buffer SBufferDynamic {
                    vec2 value2;
                } sBuffer;
                layout(std140, set = 0, binding = 3) readonly buffer RBufferDynamic {
                    vec2 value3;
                } rBuffer;
                layout(location = 0) out vec4 fragColor;
                void main() {
                })");

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        wgpu::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &mBindGroupLayout);
        pipelineDescriptor.layout = pipelineLayout;
        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    wgpu::ComputePipeline CreateComputePipeline() {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, R"(
                #version 450
                const uint kTileSize = 4;
                const uint kInstances = 11;

                layout(local_size_x = kTileSize, local_size_y = kTileSize, local_size_z = 1) in;
                layout(std140, set = 0, binding = 0) uniform UniformBufferDynamic {
                    float value0;
                };
                layout(std140, set = 0, binding = 1) uniform UniformBuffer {
                    float value1;
                };
                layout(std140, set = 0, binding = 2) buffer SBufferDynamic {
                    float value2;
                } dst;
                layout(std140, set = 0, binding = 3) readonly buffer RBufferDynamic {
                    readonly float value3;
                } rdst;
                void main() {
                })");

        wgpu::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, &mBindGroupLayout);

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.layout = pipelineLayout;
        csDesc.computeStage.module = csModule;
        csDesc.computeStage.entryPoint = "main";

        return device.CreateComputePipeline(&csDesc);
    }

    void TestRenderPassBindGroup(wgpu::BindGroup bindGroup,
                                 uint32_t* offsets,
                                 uint32_t count,
                                 bool expectation) {
        wgpu::RenderPipeline renderPipeline = CreateRenderPipeline();
        DummyRenderPass renderPass(device);

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);
        renderPassEncoder.SetPipeline(renderPipeline);
        if (bindGroup != nullptr) {
            renderPassEncoder.SetBindGroup(0, bindGroup, count, offsets);
        }
        renderPassEncoder.Draw(3);
        renderPassEncoder.EndPass();
        if (!expectation) {
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        } else {
            commandEncoder.Finish();
        }
    }

    void TestComputePassBindGroup(wgpu::BindGroup bindGroup,
                                  uint32_t* offsets,
                                  uint32_t count,
                                  bool expectation) {
        wgpu::ComputePipeline computePipeline = CreateComputePipeline();

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipeline);
        if (bindGroup != nullptr) {
            computePassEncoder.SetBindGroup(0, bindGroup, count, offsets);
        }
        computePassEncoder.Dispatch(1);
        computePassEncoder.EndPass();
        if (!expectation) {
            ASSERT_DEVICE_ERROR(commandEncoder.Finish());
        } else {
            commandEncoder.Finish();
        }
    }
};

// This is the test case that should work.
TEST_F(SetBindGroupValidationTest, Basic) {
    // Set up the bind group.
    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mBindGroupLayout,
                                                     {{0, uniformBuffer, 0, kBindingSize},
                                                      {1, uniformBuffer, 0, kBindingSize},
                                                      {2, storageBuffer, 0, kBindingSize},
                                                      {3, readonlyStorageBuffer, 0, kBindingSize}});

    std::array<uint32_t, 3> offsets = {512, 256, 0};

    TestRenderPassBindGroup(bindGroup, offsets.data(), 3, true);

    TestComputePassBindGroup(bindGroup, offsets.data(), 3, true);
}

// Draw/dispatch with a bind group missing is invalid
TEST_F(SetBindGroupValidationTest, MissingBindGroup) {
    TestRenderPassBindGroup(nullptr, nullptr, 0, false);
    TestComputePassBindGroup(nullptr, nullptr, 0, false);
}

// Setting bind group after a draw / dispatch should re-verify the layout is compatible
TEST_F(SetBindGroupValidationTest, VerifyGroupIfChangedAfterAction) {
    // Set up the bind group
    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mBindGroupLayout,
                                                     {{0, uniformBuffer, 0, kBindingSize},
                                                      {1, uniformBuffer, 0, kBindingSize},
                                                      {2, storageBuffer, 0, kBindingSize},
                                                      {3, readonlyStorageBuffer, 0, kBindingSize}});

    std::array<uint32_t, 3> offsets = {512, 256, 0};

    // Set up bind group that is incompatible
    wgpu::BindGroupLayout invalidLayout = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::StorageBuffer}});
    wgpu::BindGroup invalidGroup =
        utils::MakeBindGroup(device, invalidLayout, {{0, storageBuffer, 0, kBindingSize}});

    {
        wgpu::ComputePipeline computePipeline = CreateComputePipeline();
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetPipeline(computePipeline);
        computePassEncoder.SetBindGroup(0, bindGroup, 3, offsets.data());
        computePassEncoder.Dispatch(1);
        computePassEncoder.SetBindGroup(0, invalidGroup, 0, nullptr);
        computePassEncoder.Dispatch(1);
        computePassEncoder.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }
    {
        wgpu::RenderPipeline renderPipeline = CreateRenderPipeline();
        DummyRenderPass renderPass(device);

        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);
        renderPassEncoder.SetPipeline(renderPipeline);
        renderPassEncoder.SetBindGroup(0, bindGroup, 3, offsets.data());
        renderPassEncoder.Draw(3);
        renderPassEncoder.SetBindGroup(0, invalidGroup, 0, nullptr);
        renderPassEncoder.Draw(3);
        renderPassEncoder.EndPass();
        ASSERT_DEVICE_ERROR(commandEncoder.Finish());
    }
}

// Test cases that test dynamic offsets count mismatch with bind group layout.
TEST_F(SetBindGroupValidationTest, DynamicOffsetsMismatch) {
    // Set up bind group.
    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mBindGroupLayout,
                                                     {{0, uniformBuffer, 0, kBindingSize},
                                                      {1, uniformBuffer, 0, kBindingSize},
                                                      {2, storageBuffer, 0, kBindingSize},
                                                      {3, readonlyStorageBuffer, 0, kBindingSize}});

    // Number of offsets mismatch.
    std::array<uint32_t, 4> mismatchOffsets = {768, 512, 256, 0};

    TestRenderPassBindGroup(bindGroup, mismatchOffsets.data(), 1, false);
    TestRenderPassBindGroup(bindGroup, mismatchOffsets.data(), 2, false);
    TestRenderPassBindGroup(bindGroup, mismatchOffsets.data(), 4, false);

    TestComputePassBindGroup(bindGroup, mismatchOffsets.data(), 1, false);
    TestComputePassBindGroup(bindGroup, mismatchOffsets.data(), 2, false);
    TestComputePassBindGroup(bindGroup, mismatchOffsets.data(), 4, false);
}

// Test cases that test dynamic offsets not aligned
TEST_F(SetBindGroupValidationTest, DynamicOffsetsNotAligned) {
    // Set up bind group.
    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mBindGroupLayout,
                                                     {{0, uniformBuffer, 0, kBindingSize},
                                                      {1, uniformBuffer, 0, kBindingSize},
                                                      {2, storageBuffer, 0, kBindingSize},
                                                      {3, readonlyStorageBuffer, 0, kBindingSize}});

    // Dynamic offsets are not aligned.
    std::array<uint32_t, 3> notAlignedOffsets = {512, 128, 0};

    TestRenderPassBindGroup(bindGroup, notAlignedOffsets.data(), 3, false);

    TestComputePassBindGroup(bindGroup, notAlignedOffsets.data(), 3, false);
}

// Test cases that test dynamic uniform buffer out of bound situation.
TEST_F(SetBindGroupValidationTest, OffsetOutOfBoundDynamicUniformBuffer) {
    // Set up bind group.
    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mBindGroupLayout,
                                                     {{0, uniformBuffer, 0, kBindingSize},
                                                      {1, uniformBuffer, 0, kBindingSize},
                                                      {2, storageBuffer, 0, kBindingSize},
                                                      {3, readonlyStorageBuffer, 0, kBindingSize}});

    // Dynamic offset + offset is larger than buffer size.
    std::array<uint32_t, 3> overFlowOffsets = {1024, 256, 0};

    TestRenderPassBindGroup(bindGroup, overFlowOffsets.data(), 3, false);

    TestComputePassBindGroup(bindGroup, overFlowOffsets.data(), 3, false);
}

// Test cases that test dynamic storage buffer out of bound situation.
TEST_F(SetBindGroupValidationTest, OffsetOutOfBoundDynamicStorageBuffer) {
    // Set up bind group.
    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mBindGroupLayout,
                                                     {{0, uniformBuffer, 0, kBindingSize},
                                                      {1, uniformBuffer, 0, kBindingSize},
                                                      {2, storageBuffer, 0, kBindingSize},
                                                      {3, readonlyStorageBuffer, 0, kBindingSize}});

    // Dynamic offset + offset is larger than buffer size.
    std::array<uint32_t, 3> overFlowOffsets = {0, 256, 1024};

    TestRenderPassBindGroup(bindGroup, overFlowOffsets.data(), 3, false);

    TestComputePassBindGroup(bindGroup, overFlowOffsets.data(), 3, false);
}

// Test cases that test dynamic uniform buffer out of bound situation because of binding size.
TEST_F(SetBindGroupValidationTest, BindingSizeOutOfBoundDynamicUniformBuffer) {
    // Set up bind group, but binding size is larger than
    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mBindGroupLayout,
                                                     {{0, uniformBuffer, 0, kBindingSize},
                                                      {1, uniformBuffer, 0, kBindingSize},
                                                      {2, storageBuffer, 0, kBindingSize},
                                                      {3, readonlyStorageBuffer, 0, kBindingSize}});

    // Dynamic offset + offset isn't larger than buffer size.
    // But with binding size, it will trigger OOB error.
    std::array<uint32_t, 3> offsets = {768, 256, 0};

    TestRenderPassBindGroup(bindGroup, offsets.data(), 3, false);

    TestComputePassBindGroup(bindGroup, offsets.data(), 3, false);
}

// Test cases that test dynamic storage buffer out of bound situation because of binding size.
TEST_F(SetBindGroupValidationTest, BindingSizeOutOfBoundDynamicStorageBuffer) {
    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::Buffer readonlyStorageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mBindGroupLayout,
                                                     {{0, uniformBuffer, 0, kBindingSize},
                                                      {1, uniformBuffer, 0, kBindingSize},
                                                      {2, storageBuffer, 0, kBindingSize},
                                                      {3, readonlyStorageBuffer, 0, kBindingSize}});
    // Dynamic offset + offset isn't larger than buffer size.
    // But with binding size, it will trigger OOB error.
    std::array<uint32_t, 3> offsets = {0, 256, 768};

    TestRenderPassBindGroup(bindGroup, offsets.data(), 3, false);

    TestComputePassBindGroup(bindGroup, offsets.data(), 3, false);
}

// Regression test for crbug.com/dawn/408 where dynamic offsets were applied in the wrong order.
// Dynamic offsets should be applied in increasing order of binding number.
TEST_F(SetBindGroupValidationTest, DynamicOffsetOrder) {
    // Note: The order of the binding numbers of the bind group and bind group layout are
    // intentionally different and not in increasing order.
    // This test uses both storage and uniform buffers to ensure buffer bindings are sorted first by
    // binding number before type.
    wgpu::BindGroupLayout bgl = utils::MakeBindGroupLayout(
        device, {
                    {3, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer, true},
                    {0, wgpu::ShaderStage::Compute, wgpu::BindingType::ReadonlyStorageBuffer, true},
                    {2, wgpu::ShaderStage::Compute, wgpu::BindingType::UniformBuffer, true},
                });

    // Create buffers which are 3x, 2x, and 1x the size of the minimum buffer offset, plus 4 bytes
    // to spare (to avoid zero-sized bindings). We will offset the bindings so they reach the very
    // end of the buffer. Any mismatch applying too-large of an offset to a smaller buffer will hit
    // the out-of-bounds condition during validation.
    wgpu::Buffer buffer3x =
        CreateBuffer(3 * kMinDynamicBufferOffsetAlignment + 4, wgpu::BufferUsage::Storage);
    wgpu::Buffer buffer2x =
        CreateBuffer(2 * kMinDynamicBufferOffsetAlignment + 4, wgpu::BufferUsage::Storage);
    wgpu::Buffer buffer1x =
        CreateBuffer(1 * kMinDynamicBufferOffsetAlignment + 4, wgpu::BufferUsage::Uniform);
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, bgl,
                                                     {
                                                         {0, buffer3x, 0, 4},
                                                         {3, buffer2x, 0, 4},
                                                         {2, buffer1x, 0, 4},
                                                     });

    std::array<uint32_t, 3> offsets;
    {
        // Base case works.
        offsets = {/* binding 0 */ 0,
                   /* binding 2 */ 0,
                   /* binding 3 */ 0};
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetBindGroup(0, bindGroup, offsets.size(), offsets.data());
        computePassEncoder.EndPass();
        commandEncoder.Finish();
    }
    {
        // Offset the first binding to touch the end of the buffer. Should succeed.
        // Will fail if the offset is applied to the first or second bindings since their buffers
        // are too small.
        offsets = {/* binding 0 */ 3 * kMinDynamicBufferOffsetAlignment,
                   /* binding 2 */ 0,
                   /* binding 3 */ 0};
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetBindGroup(0, bindGroup, offsets.size(), offsets.data());
        computePassEncoder.EndPass();
        commandEncoder.Finish();
    }
    {
        // Offset the second binding to touch the end of the buffer. Should succeed.
        offsets = {/* binding 0 */ 0,
                   /* binding 2 */ 1 * kMinDynamicBufferOffsetAlignment,
                   /* binding 3 */ 0};
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetBindGroup(0, bindGroup, offsets.size(), offsets.data());
        computePassEncoder.EndPass();
        commandEncoder.Finish();
    }
    {
        // Offset the third binding to touch the end of the buffer. Should succeed.
        // Will fail if the offset is applied to the second binding since its buffer
        // is too small.
        offsets = {/* binding 0 */ 0,
                   /* binding 2 */ 0,
                   /* binding 3 */ 2 * kMinDynamicBufferOffsetAlignment};
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetBindGroup(0, bindGroup, offsets.size(), offsets.data());
        computePassEncoder.EndPass();
        commandEncoder.Finish();
    }
    {
        // Offset each binding to touch the end of their buffer. Should succeed.
        offsets = {/* binding 0 */ 3 * kMinDynamicBufferOffsetAlignment,
                   /* binding 2 */ 1 * kMinDynamicBufferOffsetAlignment,
                   /* binding 3 */ 2 * kMinDynamicBufferOffsetAlignment};
        wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder computePassEncoder = commandEncoder.BeginComputePass();
        computePassEncoder.SetBindGroup(0, bindGroup, offsets.size(), offsets.data());
        computePassEncoder.EndPass();
        commandEncoder.Finish();
    }
}

// Test that an error is produced (and no ASSERTs fired) when using an error bindgroup in
// SetBindGroup
TEST_F(SetBindGroupValidationTest, ErrorBindGroup) {
    // Bindgroup creation fails because not all bindings are specified.
    wgpu::BindGroup bindGroup;
    ASSERT_DEVICE_ERROR(bindGroup = utils::MakeBindGroup(device, mBindGroupLayout, {}));

    TestRenderPassBindGroup(bindGroup, nullptr, 0, false);

    TestComputePassBindGroup(bindGroup, nullptr, 0, false);
}

class SetBindGroupPersistenceValidationTest : public ValidationTest {
  protected:
    void SetUp() override {
        ValidationTest::SetUp();

        mVsModule = utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
            #version 450
            void main() {
            })");
    }

    wgpu::Buffer CreateBuffer(uint64_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = usage;

        return device.CreateBuffer(&bufferDescriptor);
    }

    // Generates bind group layouts and a pipeline from a 2D list of binding types.
    std::tuple<std::vector<wgpu::BindGroupLayout>, wgpu::RenderPipeline> SetUpLayoutsAndPipeline(
        std::vector<std::vector<wgpu::BindingType>> layouts) {
        std::vector<wgpu::BindGroupLayout> bindGroupLayouts(layouts.size());

        // Iterate through the desired bind group layouts.
        for (uint32_t l = 0; l < layouts.size(); ++l) {
            const auto& layout = layouts[l];
            std::vector<wgpu::BindGroupLayoutEntry> bindings(layout.size());

            // Iterate through binding types and populate a list of BindGroupLayoutEntrys.
            for (uint32_t b = 0; b < layout.size(); ++b) {
                bindings[b] = {b, wgpu::ShaderStage::Fragment, layout[b], false};
            }

            // Create the bind group layout.
            wgpu::BindGroupLayoutDescriptor bglDescriptor;
            bglDescriptor.entryCount = static_cast<uint32_t>(bindings.size());
            bglDescriptor.entries = bindings.data();
            bindGroupLayouts[l] = device.CreateBindGroupLayout(&bglDescriptor);
        }

        // Create a pipeline layout from the list of bind group layouts.
        wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor;
        pipelineLayoutDescriptor.bindGroupLayoutCount =
            static_cast<uint32_t>(bindGroupLayouts.size());
        pipelineLayoutDescriptor.bindGroupLayouts = bindGroupLayouts.data();

        wgpu::PipelineLayout pipelineLayout =
            device.CreatePipelineLayout(&pipelineLayoutDescriptor);

        std::stringstream ss;
        ss << "#version 450\n";

        // Build a shader which has bindings that match the pipeline layout.
        for (uint32_t l = 0; l < layouts.size(); ++l) {
            const auto& layout = layouts[l];

            for (uint32_t b = 0; b < layout.size(); ++b) {
                wgpu::BindingType binding = layout[b];
                ss << "layout(std140, set = " << l << ", binding = " << b << ") ";
                switch (binding) {
                    case wgpu::BindingType::StorageBuffer:
                        ss << "buffer SBuffer";
                        break;
                    case wgpu::BindingType::UniformBuffer:
                        ss << "uniform UBuffer";
                        break;
                    default:
                        UNREACHABLE();
                }
                ss << l << "_" << b << " { vec2 set" << l << "_binding" << b << "; };\n";
            }
        }

        ss << "layout(location = 0) out vec4 fragColor;\n";
        ss << "void main() { fragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n";

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, ss.str().c_str());

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = mVsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        pipelineDescriptor.layout = pipelineLayout;
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDescriptor);

        return std::make_tuple(bindGroupLayouts, pipeline);
    }

  private:
    wgpu::ShaderModule mVsModule;
};

// Test it is valid to set bind groups before setting the pipeline.
TEST_F(SetBindGroupPersistenceValidationTest, BindGroupBeforePipeline) {
    std::vector<wgpu::BindGroupLayout> bindGroupLayouts;
    wgpu::RenderPipeline pipeline;
    std::tie(bindGroupLayouts, pipeline) = SetUpLayoutsAndPipeline({{
        {{
            wgpu::BindingType::UniformBuffer,
            wgpu::BindingType::UniformBuffer,
        }},
        {{
            wgpu::BindingType::StorageBuffer,
            wgpu::BindingType::UniformBuffer,
        }},
    }});

    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);

    wgpu::BindGroup bindGroup0 = utils::MakeBindGroup(
        device, bindGroupLayouts[0],
        {{0, uniformBuffer, 0, kBindingSize}, {1, uniformBuffer, 0, kBindingSize}});

    wgpu::BindGroup bindGroup1 = utils::MakeBindGroup(
        device, bindGroupLayouts[1],
        {{0, storageBuffer, 0, kBindingSize}, {1, uniformBuffer, 0, kBindingSize}});

    DummyRenderPass renderPass(device);
    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);

    renderPassEncoder.SetBindGroup(0, bindGroup0);
    renderPassEncoder.SetBindGroup(1, bindGroup1);
    renderPassEncoder.SetPipeline(pipeline);
    renderPassEncoder.Draw(3);

    renderPassEncoder.EndPass();
    commandEncoder.Finish();
}

// Dawn does not have a concept of bind group inheritance though the backing APIs may.
// Test that it is valid to draw with bind groups that are not "inherited". They persist
// after a pipeline change.
TEST_F(SetBindGroupPersistenceValidationTest, NotVulkanInheritance) {
    std::vector<wgpu::BindGroupLayout> bindGroupLayoutsA;
    wgpu::RenderPipeline pipelineA;
    std::tie(bindGroupLayoutsA, pipelineA) = SetUpLayoutsAndPipeline({{
        {{
            wgpu::BindingType::UniformBuffer,
            wgpu::BindingType::StorageBuffer,
        }},
        {{
            wgpu::BindingType::UniformBuffer,
            wgpu::BindingType::UniformBuffer,
        }},
    }});

    std::vector<wgpu::BindGroupLayout> bindGroupLayoutsB;
    wgpu::RenderPipeline pipelineB;
    std::tie(bindGroupLayoutsB, pipelineB) = SetUpLayoutsAndPipeline({{
        {{
            wgpu::BindingType::StorageBuffer,
            wgpu::BindingType::UniformBuffer,
        }},
        {{
            wgpu::BindingType::UniformBuffer,
            wgpu::BindingType::UniformBuffer,
        }},
    }});

    wgpu::Buffer uniformBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    wgpu::Buffer storageBuffer = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);

    wgpu::BindGroup bindGroupA0 = utils::MakeBindGroup(
        device, bindGroupLayoutsA[0],
        {{0, uniformBuffer, 0, kBindingSize}, {1, storageBuffer, 0, kBindingSize}});

    wgpu::BindGroup bindGroupA1 = utils::MakeBindGroup(
        device, bindGroupLayoutsA[1],
        {{0, uniformBuffer, 0, kBindingSize}, {1, uniformBuffer, 0, kBindingSize}});

    wgpu::BindGroup bindGroupB0 = utils::MakeBindGroup(
        device, bindGroupLayoutsB[0],
        {{0, storageBuffer, 0, kBindingSize}, {1, uniformBuffer, 0, kBindingSize}});

    DummyRenderPass renderPass(device);
    wgpu::CommandEncoder commandEncoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder renderPassEncoder = commandEncoder.BeginRenderPass(&renderPass);

    renderPassEncoder.SetPipeline(pipelineA);
    renderPassEncoder.SetBindGroup(0, bindGroupA0);
    renderPassEncoder.SetBindGroup(1, bindGroupA1);
    renderPassEncoder.Draw(3);

    renderPassEncoder.SetPipeline(pipelineB);
    renderPassEncoder.SetBindGroup(0, bindGroupB0);
    // This draw is valid.
    // Bind group 1 persists even though it is not "inherited".
    renderPassEncoder.Draw(3);

    renderPassEncoder.EndPass();
    commandEncoder.Finish();
}

class BindGroupLayoutCompatibilityTest : public ValidationTest {
  public:
    wgpu::Buffer CreateBuffer(uint64_t bufferSize, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = bufferSize;
        bufferDescriptor.usage = usage;

        return device.CreateBuffer(&bufferDescriptor);
    }

    wgpu::RenderPipeline CreateFSRenderPipeline(
        const char* fsShader,
        std::vector<wgpu::BindGroupLayout> bindGroupLayout) {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                void main() {
                })");

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, fsShader);

        wgpu::PipelineLayoutDescriptor descriptor;
        descriptor.bindGroupLayoutCount = bindGroupLayout.size();
        descriptor.bindGroupLayouts = bindGroupLayout.data();
        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&descriptor);
        pipelineDescriptor.layout = pipelineLayout;
        return device.CreateRenderPipeline(&pipelineDescriptor);
    }

    wgpu::RenderPipeline CreateRenderPipeline(std::vector<wgpu::BindGroupLayout> bindGroupLayout) {
        return CreateFSRenderPipeline(R"(
                #version 450
                layout(std140, set = 0, binding = 0) buffer SBuffer {
                    vec2 value2;
                } sBuffer;
                layout(std140, set = 1, binding = 0) readonly buffer RBuffer {
                    vec2 value3;
                } rBuffer;
                layout(location = 0) out vec4 fragColor;
                void main() {
                })",
                                      std::move(bindGroupLayout));
    }

    wgpu::ComputePipeline CreateComputePipeline(
        const char* shader,
        std::vector<wgpu::BindGroupLayout> bindGroupLayout) {
        wgpu::ShaderModule csModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Compute, shader);

        wgpu::PipelineLayoutDescriptor descriptor;
        descriptor.bindGroupLayoutCount = bindGroupLayout.size();
        descriptor.bindGroupLayouts = bindGroupLayout.data();
        wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&descriptor);

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.layout = pipelineLayout;
        csDesc.computeStage.module = csModule;
        csDesc.computeStage.entryPoint = "main";

        return device.CreateComputePipeline(&csDesc);
    }

    wgpu::ComputePipeline CreateComputePipeline(
        std::vector<wgpu::BindGroupLayout> bindGroupLayout) {
        return CreateComputePipeline(R"(
                #version 450
                const uint kTileSize = 4;
                const uint kInstances = 11;

                layout(local_size_x = kTileSize, local_size_y = kTileSize, local_size_z = 1) in;
                layout(std140, set = 0, binding = 0) buffer SBuffer {
                    float value2;
                } dst;
                layout(std140, set = 1, binding = 0) readonly buffer RBuffer {
                    readonly float value3;
                } rdst;
                void main() {
                })",
                                     std::move(bindGroupLayout));
    }
};

// Test that it is valid to pass a writable storage buffer in the pipeline layout when the shader
// uses the binding as a readonly storage buffer.
TEST_F(BindGroupLayoutCompatibilityTest, RWStorageInBGLWithROStorageInShader) {
    // Set up the bind group layout.
    wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::StorageBuffer}});
    wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::StorageBuffer}});

    CreateRenderPipeline({bgl0, bgl1});

    CreateComputePipeline({bgl0, bgl1});
}

// Test that it is invalid to pass a readonly storage buffer in the pipeline layout when the shader
// uses the binding as a writable storage buffer.
TEST_F(BindGroupLayoutCompatibilityTest, ROStorageInBGLWithRWStorageInShader) {
    // Set up the bind group layout.
    wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::ReadonlyStorageBuffer}});
    wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::ReadonlyStorageBuffer}});

    ASSERT_DEVICE_ERROR(CreateRenderPipeline({bgl0, bgl1}));

    ASSERT_DEVICE_ERROR(CreateComputePipeline({bgl0, bgl1}));
}

TEST_F(BindGroupLayoutCompatibilityTest, TextureViewDimension) {
    constexpr char kTexture2DShader[] = R"(
        #version 450
        layout(set = 0, binding = 0) uniform texture2D texture;
        void main() {
        })";

    // Render: Test that 2D texture with 2D view dimension works
    CreateFSRenderPipeline(
        kTexture2DShader,
        {utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0,
                      false, wgpu::TextureViewDimension::e2D}})});

    // Render: Test that 2D texture with 2D array view dimension is invalid
    ASSERT_DEVICE_ERROR(CreateFSRenderPipeline(
        kTexture2DShader,
        {utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0,
                      false, wgpu::TextureViewDimension::e2DArray}})}));

    // Compute: Test that 2D texture with 2D view dimension works
    CreateComputePipeline(
        kTexture2DShader,
        {utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture, false, 0,
                      false, wgpu::TextureViewDimension::e2D}})});

    // Compute: Test that 2D texture with 2D array view dimension is invalid
    ASSERT_DEVICE_ERROR(CreateComputePipeline(
        kTexture2DShader,
        {utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture, false, 0,
                      false, wgpu::TextureViewDimension::e2DArray}})}));

    constexpr char kTexture2DArrayShader[] = R"(
        #version 450
        layout(set = 0, binding = 0) uniform texture2DArray texture;
        void main() {
        })";

    // Render: Test that 2D texture array with 2D array view dimension works
    CreateFSRenderPipeline(
        kTexture2DArrayShader,
        {utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0,
                      false, wgpu::TextureViewDimension::e2DArray}})});

    // Render: Test that 2D texture array with 2D view dimension is invalid
    ASSERT_DEVICE_ERROR(CreateFSRenderPipeline(
        kTexture2DArrayShader,
        {utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture, false, 0,
                      false, wgpu::TextureViewDimension::e2D}})}));

    // Compute: Test that 2D texture array with 2D array view dimension works
    CreateComputePipeline(
        kTexture2DArrayShader,
        {utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture, false, 0,
                      false, wgpu::TextureViewDimension::e2DArray}})});

    // Compute: Test that 2D texture array with 2D view dimension is invalid
    ASSERT_DEVICE_ERROR(CreateComputePipeline(
        kTexture2DArrayShader,
        {utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute, wgpu::BindingType::SampledTexture, false, 0,
                      false, wgpu::TextureViewDimension::e2D}})}));
}

class BindingsValidationTest : public BindGroupLayoutCompatibilityTest {
  public:
    void TestRenderPassBindings(const wgpu::BindGroup* bg,
                                uint32_t count,
                                wgpu::RenderPipeline pipeline,
                                bool expectation) {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        DummyRenderPass dummyRenderPass(device);
        wgpu::RenderPassEncoder rp = encoder.BeginRenderPass(&dummyRenderPass);
        for (uint32_t i = 0; i < count; ++i) {
            rp.SetBindGroup(i, bg[i]);
        }
        rp.SetPipeline(pipeline);
        rp.Draw(3);
        rp.EndPass();
        if (!expectation) {
            ASSERT_DEVICE_ERROR(encoder.Finish());
        } else {
            encoder.Finish();
        }
    }

    void TestComputePassBindings(const wgpu::BindGroup* bg,
                                 uint32_t count,
                                 wgpu::ComputePipeline pipeline,
                                 bool expectation) {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder cp = encoder.BeginComputePass();
        for (uint32_t i = 0; i < count; ++i) {
            cp.SetBindGroup(i, bg[i]);
        }
        cp.SetPipeline(pipeline);
        cp.Dispatch(1);
        cp.EndPass();
        if (!expectation) {
            ASSERT_DEVICE_ERROR(encoder.Finish());
        } else {
            encoder.Finish();
        }
    }

    static constexpr uint32_t kBindingNum = 3;
};

// Test that it is valid to set a pipeline layout with bindings unused by the pipeline.
TEST_F(BindingsValidationTest, PipelineLayoutWithMoreBindingsThanPipeline) {
    // Set up bind group layouts.
    wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::StorageBuffer},
                 {1, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::UniformBuffer}});
    wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::ReadonlyStorageBuffer}});
    wgpu::BindGroupLayout bgl2 = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::StorageBuffer}});

    // pipelineLayout has unused binding set (bgl2) and unused entry in a binding set (bgl0).
    CreateRenderPipeline({bgl0, bgl1, bgl2});

    CreateComputePipeline({bgl0, bgl1, bgl2});
}

// Test that it is invalid to set a pipeline layout that doesn't have all necessary bindings
// required by the pipeline.
TEST_F(BindingsValidationTest, PipelineLayoutWithLessBindingsThanPipeline) {
    // Set up bind group layout.
    wgpu::BindGroupLayout bgl0 = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::StorageBuffer}});

    // missing a binding set (bgl1) in pipeline layout
    {
        ASSERT_DEVICE_ERROR(CreateRenderPipeline({bgl0}));

        ASSERT_DEVICE_ERROR(CreateComputePipeline({bgl0}));
    }

    // bgl1 is not missing, but it is empty
    {
        wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(device, {});

        ASSERT_DEVICE_ERROR(CreateRenderPipeline({bgl0, bgl1}));

        ASSERT_DEVICE_ERROR(CreateComputePipeline({bgl0, bgl1}));
    }

    // bgl1 is neither missing nor empty, but it doesn't contain the necessary binding
    {
        wgpu::BindGroupLayout bgl1 = utils::MakeBindGroupLayout(
            device, {{1, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                      wgpu::BindingType::UniformBuffer}});

        ASSERT_DEVICE_ERROR(CreateRenderPipeline({bgl0, bgl1}));

        ASSERT_DEVICE_ERROR(CreateComputePipeline({bgl0, bgl1}));
    }
}

// Test that it is valid to set bind groups whose layout is not set in the pipeline layout.
// But it's invalid to set extra entry for a given bind group's layout if that layout is set in
// the pipeline layout.
TEST_F(BindingsValidationTest, BindGroupsWithMoreBindingsThanPipelineLayout) {
    // Set up bind group layouts, buffers, bind groups, pipeline layouts and pipelines.
    std::array<wgpu::BindGroupLayout, kBindingNum + 1> bgl;
    std::array<wgpu::BindGroup, kBindingNum + 1> bg;
    std::array<wgpu::Buffer, kBindingNum + 1> buffer;
    for (uint32_t i = 0; i < kBindingNum + 1; ++i) {
        bgl[i] = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                      wgpu::BindingType::StorageBuffer}});
        buffer[i] = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
        bg[i] = utils::MakeBindGroup(device, bgl[i], {{0, buffer[i]}});
    }

    // Set 3 bindings (and 3 pipeline layouts) in pipeline.
    wgpu::RenderPipeline renderPipeline = CreateRenderPipeline({bgl[0], bgl[1], bgl[2]});
    wgpu::ComputePipeline computePipeline = CreateComputePipeline({bgl[0], bgl[1], bgl[2]});

    // Comprared to pipeline layout, there is an extra bind group (bg[3])
    TestRenderPassBindings(bg.data(), kBindingNum + 1, renderPipeline, true);

    TestComputePassBindings(bg.data(), kBindingNum + 1, computePipeline, true);

    // If a bind group has entry (like bgl1_1 below) unused by the pipeline layout, it is invalid.
    // Bind groups associated layout should exactly match bind group layout if that layout is
    // set in pipeline layout.
    bgl[1] = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::ReadonlyStorageBuffer},
                 {1, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::UniformBuffer}});
    buffer[1] = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Uniform);
    bg[1] = utils::MakeBindGroup(device, bgl[1], {{0, buffer[1]}, {1, buffer[1]}});

    TestRenderPassBindings(bg.data(), kBindingNum, renderPipeline, false);

    TestComputePassBindings(bg.data(), kBindingNum, computePipeline, false);
}

// Test that it is invalid to set bind groups that don't have all necessary bindings required
// by the pipeline layout. Note that both pipeline layout and bind group have enough bindings for
// pipeline in the following test.
TEST_F(BindingsValidationTest, BindGroupsWithLessBindingsThanPipelineLayout) {
    // Set up bind group layouts, buffers, bind groups, pipeline layouts and pipelines.
    std::array<wgpu::BindGroupLayout, kBindingNum> bgl;
    std::array<wgpu::BindGroup, kBindingNum> bg;
    std::array<wgpu::Buffer, kBindingNum> buffer;
    for (uint32_t i = 0; i < kBindingNum; ++i) {
        bgl[i] = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                      wgpu::BindingType::StorageBuffer}});
        buffer[i] = CreateBuffer(kBufferSize, wgpu::BufferUsage::Storage);
        bg[i] = utils::MakeBindGroup(device, bgl[i], {{0, buffer[i]}});
    }

    wgpu::RenderPipeline renderPipeline = CreateRenderPipeline({bgl[0], bgl[1], bgl[2]});
    wgpu::ComputePipeline computePipeline = CreateComputePipeline({bgl[0], bgl[1], bgl[2]});

    // Compared to pipeline layout, a binding set (bgl2) related bind group is missing
    TestRenderPassBindings(bg.data(), kBindingNum - 1, renderPipeline, false);

    TestComputePassBindings(bg.data(), kBindingNum - 1, computePipeline, false);

    // bgl[2] related bind group is not missing, but its bind group is empty
    bgl[2] = utils::MakeBindGroupLayout(device, {});
    bg[2] = utils::MakeBindGroup(device, bgl[2], {});

    TestRenderPassBindings(bg.data(), kBindingNum, renderPipeline, false);

    TestComputePassBindings(bg.data(), kBindingNum, computePipeline, false);

    // bgl[2] related bind group is neither missing nor empty, but it doesn't contain the necessary
    // binding
    bgl[2] = utils::MakeBindGroupLayout(
        device, {{1, wgpu::ShaderStage::Compute | wgpu::ShaderStage::Fragment,
                  wgpu::BindingType::UniformBuffer}});
    buffer[2] = CreateBuffer(kBufferSize, wgpu::BufferUsage::Uniform);
    bg[2] = utils::MakeBindGroup(device, bgl[2], {{1, buffer[2]}});

    TestRenderPassBindings(bg.data(), kBindingNum, renderPipeline, false);

    TestComputePassBindings(bg.data(), kBindingNum, computePipeline, false);
}

class ComparisonSamplerBindingTest : public ValidationTest {
  protected:
    wgpu::RenderPipeline CreateFragmentPipeline(wgpu::BindGroupLayout* bindGroupLayout,
                                                const char* fragmentSource) {
        wgpu::ShaderModule vsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
                #version 450
                void main() {
                })");

        wgpu::ShaderModule fsModule =
            utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, fragmentSource);

        utils::ComboRenderPipelineDescriptor pipelineDescriptor(device);
        pipelineDescriptor.vertexStage.module = vsModule;
        pipelineDescriptor.cFragmentStage.module = fsModule;
        wgpu::PipelineLayout pipelineLayout =
            utils::MakeBasicPipelineLayout(device, bindGroupLayout);
        pipelineDescriptor.layout = pipelineLayout;
        return device.CreateRenderPipeline(&pipelineDescriptor);
    }
};

// TODO(crbug.com/dawn/367): Disabled until we can perform shader analysis
// of which samplers are comparison samplers.
TEST_F(ComparisonSamplerBindingTest, DISABLED_ShaderAndBGLMatches) {
    // Test that sampler binding works with normal sampler in the shader.
    {
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

        CreateFragmentPipeline(&bindGroupLayout, R"(
        #version 450
        layout(set = 0, binding = 0) uniform sampler samp;

        void main() {
        })");
    }

    // Test that comparison sampler binding works with shadow sampler in the shader.
    {
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ComparisonSampler}});

        CreateFragmentPipeline(&bindGroupLayout, R"(
        #version 450
        layout(set = 0, binding = 0) uniform samplerShadow samp;

        void main() {
        })");
    }

    // Test that sampler binding does not work with comparison sampler in the shader.
    {
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

        ASSERT_DEVICE_ERROR(CreateFragmentPipeline(&bindGroupLayout, R"(
        #version 450
        layout(set = 0, binding = 0) uniform samplerShadow samp;

        void main() {
        })"));
    }

    // Test that comparison sampler binding does not work with normal sampler in the shader.
    {
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ComparisonSampler}});

        ASSERT_DEVICE_ERROR(CreateFragmentPipeline(&bindGroupLayout, R"(
        #version 450
        layout(set = 0, binding = 0) uniform sampler samp;

        void main() {
        })"));
    }
}

TEST_F(ComparisonSamplerBindingTest, SamplerAndBindGroupMatches) {
    // Test that sampler binding works with normal sampler.
    {
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

        wgpu::SamplerDescriptor desc = {};
        utils::MakeBindGroup(device, bindGroupLayout, {{0, device.CreateSampler(&desc)}});
    }

    // Test that comparison sampler binding works with sampler w/ compare function.
    {
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ComparisonSampler}});

        wgpu::SamplerDescriptor desc = {};
        desc.compare = wgpu::CompareFunction::Never;
        utils::MakeBindGroup(device, bindGroupLayout, {{0, device.CreateSampler(&desc)}});
    }

    // Test that sampler binding does not work with sampler w/ compare function.
    {
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler}});

        wgpu::SamplerDescriptor desc;
        desc.compare = wgpu::CompareFunction::Never;
        ASSERT_DEVICE_ERROR(
            utils::MakeBindGroup(device, bindGroupLayout, {{0, device.CreateSampler(&desc)}}));
    }

    // Test that comparison sampler binding does not work with normal sampler.
    {
        wgpu::BindGroupLayout bindGroupLayout = utils::MakeBindGroupLayout(
            device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::ComparisonSampler}});

        wgpu::SamplerDescriptor desc = {};
        ASSERT_DEVICE_ERROR(
            utils::MakeBindGroup(device, bindGroupLayout, {{0, device.CreateSampler(&desc)}}));
    }
}
