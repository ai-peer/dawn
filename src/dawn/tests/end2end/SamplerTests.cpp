// Copyright 2018 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <array>
#include <cmath>
#include <vector>

#include "dawn/tests/DawnTest.h"

#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

constexpr static unsigned int kRTSize = 4096;

const char* kBasicFS = R"(
            @group(0) @binding(0) var sampler0 : sampler;
            @group(0) @binding(1) var texture0 : texture_2d<f32>;

            @fragment
            fn main(@builtin(position) FragCoord : vec4f) -> @location(0) vec4f {
                return textureSample(texture0, sampler0, FragCoord.xy / vec2(2.0, 2.0));
            })";

const char* kPassThroughUserFunctionsFS = R"(
            @group(0) @binding(0) var sampler0 : sampler;
            @group(0) @binding(1) var texture0 : texture_2d<f32>;

            fn foo(t : texture_2d<f32>, s : sampler, FragCoord : vec4f) -> vec4f {
                return textureSample(t, s, FragCoord.xy / vec2(2.0, 2.0));
            }

            @fragment
            fn main(@builtin(position) FragCoord : vec4f) -> @location(0) vec4f {
                return foo(texture0, sampler0, FragCoord);
            })";

struct AddressModeTestCase {
    wgpu::AddressMode mMode;
    uint8_t mExpected2;
    uint8_t mExpected3;
};
AddressModeTestCase addressModes[] = {
    {
        wgpu::AddressMode::Repeat,
        0,
        255,
    },
    {
        wgpu::AddressMode::MirrorRepeat,
        255,
        0,
    },
    {
        wgpu::AddressMode::ClampToEdge,
        255,
        255,
    },
};

class SamplerTest : public DawnTest {
  protected:
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> requiredFeatures = {};
        if (SupportsFeatures({wgpu::FeatureName::TimestampQuery})) {
            requiredFeatures.push_back(wgpu::FeatureName::TimestampQuery);
        }
        return requiredFeatures;
    }

    void SetUp() override {
        DawnTest::SetUp();
        mRenderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size.width = 2;
        descriptor.size.height = 2;
        descriptor.size.depthOrArrayLayers = 1;
        descriptor.sampleCount = 1;
        descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
        descriptor.mipLevelCount = 1;
        descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::TextureBinding;
        wgpu::Texture texture = device.CreateTexture(&descriptor);

        // Create a 2x2 checkerboard texture, with black in the top left and bottom right corners.
        const uint32_t rowPixels = kTextureBytesPerRowAlignment / sizeof(utils::RGBA8);
        std::array<utils::RGBA8, rowPixels * 2> pixels;
        pixels[0] = pixels[rowPixels + 1] = utils::RGBA8::kBlack;
        pixels[1] = pixels[rowPixels] = utils::RGBA8::kWhite;

        wgpu::Buffer stagingBuffer =
            utils::CreateBufferFromData(device, pixels.data(), pixels.size() * sizeof(utils::RGBA8),
                                        wgpu::BufferUsage::CopySrc);
        wgpu::ImageCopyBuffer imageCopyBuffer = utils::CreateImageCopyBuffer(stagingBuffer, 0, 256);
        wgpu::ImageCopyTexture imageCopyTexture =
            utils::CreateImageCopyTexture(texture, 0, {0, 0, 0});
        wgpu::Extent3D copySize = {2, 2, 1};

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&imageCopyBuffer, &imageCopyTexture, &copySize);

        wgpu::CommandBuffer copy = encoder.Finish();
        queue.Submit(1, &copy);

        mTextureView = texture.CreateView();

        descriptor.size.width = 2048;
        descriptor.size.height = 2048;
        wgpu::Texture largeTexture = device.CreateTexture(&descriptor);
        mLargeTextureView = largeTexture.CreateView();

        DAWN_TEST_UNSUPPORTED_IF(!SupportsFeatures({wgpu::FeatureName::TimestampQuery}));

        wgpu::QuerySetDescriptor qdescriptor;
        qdescriptor.count = 2u;
        qdescriptor.type = wgpu::QueryType::Timestamp;
        mQuerySet = device.CreateQuerySet(&qdescriptor);

        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = 2 * sizeof(uint64_t);
        bufferDescriptor.usage = wgpu::BufferUsage::QueryResolve | wgpu::BufferUsage::CopySrc;
        mQueryBuffer = device.CreateBuffer(&bufferDescriptor);
    }

    // Initializes the pipeline used by tests. Uses `bgl` to set the pipeline
    // layout if it is non-null; otherwise the pipeline is constructed with the
    // default layout.
    void InitShaders(const char* frag_shader, wgpu::BindGroupLayout bgl = nullptr) {
        auto vsModule = utils::CreateShaderModule(device, R"(
            @vertex
            fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
                var pos = array(
                    vec2f(-2.0, -2.0),
                    vec2f(-2.0,  2.0),
                    vec2f( 2.0, -2.0),
                    vec2f(-2.0,  2.0),
                    vec2f( 2.0, -2.0),
                    vec2f( 2.0,  2.0));
                return vec4f(pos[VertexIndex], 0.0, 1.0);
            }
        )");
        auto fsModule = utils::CreateShaderModule(device, frag_shader);

        utils::ComboRenderPipelineDescriptor pipelineDescriptor;

        if (bgl) {
            wgpu::PipelineLayout pl = utils::MakePipelineLayout(device, {bgl});
            pipelineDescriptor.layout = pl;
        }

        pipelineDescriptor.vertex.module = vsModule;
        pipelineDescriptor.cFragment.module = fsModule;
        pipelineDescriptor.cTargets[0].format = mRenderPass.colorFormat;

        mPipeline = device.CreateRenderPipeline(&pipelineDescriptor);
    }

    // Creates a sampler with the given address modes.
    wgpu::Sampler CreateSampler(AddressModeTestCase u,
                                AddressModeTestCase v,
                                AddressModeTestCase w) {
        wgpu::Sampler sampler;
        wgpu::SamplerDescriptor descriptor = {};
        descriptor.minFilter = wgpu::FilterMode::Nearest;
        descriptor.magFilter = wgpu::FilterMode::Nearest;
        descriptor.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
        descriptor.addressModeU = u.mMode;
        descriptor.addressModeV = v.mMode;
        descriptor.addressModeW = w.mMode;
        return device.CreateSampler(&descriptor);
    }

    wgpu::Sampler CreateLinearSampler(AddressModeTestCase u,
                                      AddressModeTestCase v,
                                      AddressModeTestCase w) {
        wgpu::SamplerDescriptor descriptor = {};
        descriptor.minFilter = wgpu::FilterMode::Linear;
        descriptor.magFilter = wgpu::FilterMode::Linear;
        descriptor.mipmapFilter = wgpu::MipmapFilterMode::Linear;
        descriptor.addressModeU = wgpu::AddressMode::Repeat;
        descriptor.addressModeV = wgpu::AddressMode::Repeat;
        descriptor.addressModeW = wgpu::AddressMode::Repeat;
        return device.CreateSampler(&descriptor);
    }

    // Creates a bind group that has a sampler with the given address modes and
    // `mTextureView` as the texture to be sampled.
    wgpu::BindGroup CreateBindGroup(AddressModeTestCase u,
                                    AddressModeTestCase v,
                                    AddressModeTestCase w) {
        wgpu::Sampler sampler = CreateSampler(u, v, w);
        return utils::MakeBindGroup(device, mPipeline.GetBindGroupLayout(0),
                                    {{0, sampler}, {1, mTextureView}});
    }

    wgpu::BindGroup CreateBindGroupPerf(AddressModeTestCase u,
                                        AddressModeTestCase v,
                                        AddressModeTestCase w) {
        wgpu::Sampler sampler = CreateLinearSampler(u, v, w);
        return utils::MakeBindGroup(device, mPipeline.GetBindGroupLayout(0),
                                    {{0, sampler}, {1, mLargeTextureView}});
    }

    // Tests drawing with the given address modes and bind group (if non-null).
    // The pipeline must already have been configured. If the bind group is
    // null, creates a bind group that has a sampler with the given address
    // modes. If non-null, the bind group must be compatible with the configured
    // pipeline.
    void TestAddressModes(AddressModeTestCase u,
                          AddressModeTestCase v,
                          AddressModeTestCase w,
                          wgpu::BindGroup bindGroup = nullptr) {
        if (!bindGroup) {
            bindGroup = CreateBindGroup(u, v, w);
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&mRenderPass.renderPassInfo);
            pass.SetPipeline(mPipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.Draw(6);
            pass.End();
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        utils::RGBA8 expectedU2(u.mExpected2, u.mExpected2, u.mExpected2, 255);
        utils::RGBA8 expectedU3(u.mExpected3, u.mExpected3, u.mExpected3, 255);
        utils::RGBA8 expectedV2(v.mExpected2, v.mExpected2, v.mExpected2, 255);
        utils::RGBA8 expectedV3(v.mExpected3, v.mExpected3, v.mExpected3, 255);
        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kBlack, mRenderPass.color, 0, 0);
        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kWhite, mRenderPass.color, 0, 1);
        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kWhite, mRenderPass.color, 1, 0);
        EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kBlack, mRenderPass.color, 1, 1);
        EXPECT_PIXEL_RGBA8_EQ(expectedU2, mRenderPass.color, 2, 0);
        EXPECT_PIXEL_RGBA8_EQ(expectedU3, mRenderPass.color, 3, 0);
        EXPECT_PIXEL_RGBA8_EQ(expectedV2, mRenderPass.color, 0, 2);
        EXPECT_PIXEL_RGBA8_EQ(expectedV3, mRenderPass.color, 0, 3);
    }

    void TestSamplerPerformance(wgpu::BindGroup bindGroup) {
        wgpu::BufferDescriptor bufferDescriptor;
        bufferDescriptor.size = 2 * sizeof(uint64_t);
        bufferDescriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;

        wgpu::Buffer readbackBuffer = device.CreateBuffer(&bufferDescriptor);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            encoder.WriteTimestamp(mQuerySet, 0);

            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&mRenderPass.renderPassInfo);
            pass.SetBindGroup(0, bindGroup);
            pass.SetPipeline(mPipeline);

            pass.Draw(6);

            pass.End();

            encoder.WriteTimestamp(mQuerySet, 1);

            encoder.CopyBufferToBuffer(mQueryBuffer, 0, readbackBuffer, 0, 2 * sizeof(uint64_t));
            encoder.ResolveQuerySet(mQuerySet, 0, 2, mQueryBuffer, 0);
        }

        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        MapAsyncAndWait(readbackBuffer, wgpu::MapMode::Read, 0, 2 * sizeof(uint64_t));

        DAWN_ASSERT(readbackBuffer.GetMapState() == wgpu::BufferMapState::Mapped);

        const uint64_t* data = reinterpret_cast<const uint64_t*>(
            readbackBuffer.GetConstMappedRange(0, 2 * sizeof(uint64_t)));
        const uint64_t start = data[0];
        const uint64_t end = data[1];

        dawn::DebugLog() << "Time taken: " << (end - start) / 1000000.0 << " ms";
    }

    void TestBindingPerformance() {
        std::vector<wgpu::BindGroupLayout> bgls;
        std::vector<wgpu::BindGroup> bgs;

        wgpu::MipmapFilterMode mipmapFilters[] = {
            wgpu::MipmapFilterMode::Nearest,
            wgpu::MipmapFilterMode::Linear,
        };

        wgpu::FilterMode minFilters[] = {
            wgpu::FilterMode::Nearest,
            wgpu::FilterMode::Linear,
        };

        wgpu::FilterMode magFilters[] = {
            wgpu::FilterMode::Nearest,
            wgpu::FilterMode::Linear,
        };

        wgpu::AddressMode addrModes[] = {
            wgpu::AddressMode::ClampToEdge,
            wgpu::AddressMode::Repeat,
            wgpu::AddressMode::MirrorRepeat,
        };

        for (auto mipmapFilter : mipmapFilters) {
            for (auto minFilter : minFilters) {
                for (auto magFilter : magFilters) {
                    for (auto addressModeU : addrModes) {
                        for (auto addressModeV : addrModes) {
                            for (auto addressModeW : addrModes) {
                                wgpu::SamplerDescriptor descriptor = {};
                                descriptor.minFilter = minFilter;
                                descriptor.magFilter = magFilter;
                                descriptor.mipmapFilter = mipmapFilter;
                                descriptor.addressModeU = addressModeU;
                                descriptor.addressModeV = addressModeV;
                                descriptor.addressModeW = addressModeW;
                                wgpu::Sampler sampler = device.CreateSampler(&descriptor);

                                wgpu::BindGroupLayoutEntry entries[2] = {};

                                entries[0].binding = 0;
                                entries[0].visibility = wgpu::ShaderStage::Fragment;
                                entries[0].sampler.type = wgpu::SamplerBindingType::Filtering;

                                entries[1].binding = 1;
                                entries[1].visibility = wgpu::ShaderStage::Fragment;
                                entries[1].texture.sampleType = wgpu::TextureSampleType::Float;
                                entries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;
                                entries[1].texture.multisampled = false;

                                wgpu::BindGroupLayoutDescriptor desc = {};
                                desc.entryCount = 2;
                                desc.entries = entries;

                                wgpu::BindGroupLayout bgl = device.CreateBindGroupLayout(&desc);

                                wgpu::BindGroup bindGroup = utils::MakeBindGroup(
                                    device, bgl, {{0, sampler}, {1, mTextureView}});

                                bgls.push_back(bgl);
                                bgs.push_back(bindGroup);
                            }
                        }
                    }
                }
            }
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        std::chrono::high_resolution_clock::time_point start, end;

        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&mRenderPass.renderPassInfo);

            start = std::chrono::high_resolution_clock::now();
            for (auto bg : bgs) {
                pass.SetBindGroup(0, bg);
            }
            end = std::chrono::high_resolution_clock::now();

            pass.End();
        }

        wgpu::CommandBuffer commands = encoder.Finish();

        queue.Submit(1, &commands);

        dawn::DebugLog()
            << "Binding time taken: "
            << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
            << " ns for " << bgs.size() << " bind groups";
    }

    utils::BasicRenderPass mRenderPass;
    wgpu::RenderPipeline mPipeline;
    wgpu::TextureView mTextureView;
    wgpu::TextureView mLargeTextureView;
    wgpu::QuerySet mQuerySet;
    wgpu::Buffer mQueryBuffer;
};

// Test drawing a rect with a checkerboard texture with different address modes.
TEST_P(SamplerTest, AddressMode) {
    InitShaders(kBasicFS);
    for (auto u : addressModes) {
        for (auto v : addressModes) {
            for (auto w : addressModes) {
                TestAddressModes(u, v, w);
            }
        }
    }
}

// Test performance of drawing a rect with a large texture with different address modes.
TEST_P(SamplerTest, Performance) {
    InitShaders(kBasicFS);
    for (auto u : addressModes) {
        for (auto v : addressModes) {
            for (auto w : addressModes) {
                TestSamplerPerformance(CreateBindGroupPerf(u, v, w));
            }
        }
    }
}

// Test performance of binding different bind groups with different samplers.
TEST_P(SamplerTest, BindingPerformance) {
    InitShaders(kBasicFS);
    TestBindingPerformance();
}

// Test that passing texture and sampler objects through user-defined functions works correctly.
TEST_P(SamplerTest, PassThroughUserFunctionParameters) {
    InitShaders(kPassThroughUserFunctionsFS);
    for (auto u : addressModes) {
        for (auto v : addressModes) {
            for (auto w : addressModes) {
                TestAddressModes(u, v, w);
            }
        }
    }
}

DAWN_INSTANTIATE_TEST(SamplerTest,
                      D3D11Backend(),
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

class StaticSamplerTest : public SamplerTest {
  protected:
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> requiredFeatures = {};
        if (SupportsFeatures({wgpu::FeatureName::StaticSamplers})) {
            requiredFeatures.push_back(wgpu::FeatureName::StaticSamplers);
        }
        if (SupportsFeatures({wgpu::FeatureName::TimestampQuery})) {
            requiredFeatures.push_back(wgpu::FeatureName::TimestampQuery);
        }
        return requiredFeatures;
    }

    void SetUp() override {
        SamplerTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(!SupportsFeatures({wgpu::FeatureName::StaticSamplers}));
        DAWN_TEST_UNSUPPORTED_IF(!SupportsFeatures({wgpu::FeatureName::TimestampQuery}));
    }

    // Creates a bind group layout with a static sampler with the given address
    // modes as well as the texture to be sampled.
    wgpu::BindGroupLayout CreateBindGroupLayoutWithStaticSampler(AddressModeTestCase u,
                                                                 AddressModeTestCase v,
                                                                 AddressModeTestCase w) {
        wgpu::Sampler sampler = CreateSampler(u, v, w);
        std::vector<wgpu::BindGroupLayoutEntry> entries;

        wgpu::BindGroupLayoutEntry binding = {};
        binding.binding = 0;
        binding.visibility = wgpu::ShaderStage::Fragment;
        wgpu::StaticSamplerBindingLayout staticSamplerBinding = {};
        staticSamplerBinding.sampler = sampler;
        binding.nextInChain = &staticSamplerBinding;
        entries.push_back(binding);

        wgpu::BindGroupLayoutEntry binding1 = {};
        binding1.binding = 1;
        binding1.visibility = wgpu::ShaderStage::Fragment;
        binding1.texture.sampleType = wgpu::TextureSampleType::Float;
        entries.push_back(binding1);

        wgpu::BindGroupLayoutDescriptor desc = {};
        desc.entryCount = 2;
        desc.entries = entries.data();

        return device.CreateBindGroupLayout(&desc);
    }

    wgpu::BindGroupLayout CreateBindGroupLayoutWithStaticSamplerPerf(AddressModeTestCase u,
                                                                     AddressModeTestCase v,
                                                                     AddressModeTestCase w) {
        wgpu::Sampler sampler = CreateLinearSampler(u, v, w);
        std::vector<wgpu::BindGroupLayoutEntry> entries;

        wgpu::BindGroupLayoutEntry binding = {};
        binding.binding = 0;
        binding.visibility = wgpu::ShaderStage::Fragment;
        wgpu::StaticSamplerBindingLayout staticSamplerBinding = {};
        staticSamplerBinding.sampler = sampler;
        binding.nextInChain = &staticSamplerBinding;
        entries.push_back(binding);

        wgpu::BindGroupLayoutEntry binding1 = {};
        binding1.binding = 1;
        binding1.visibility = wgpu::ShaderStage::Fragment;
        binding1.texture.sampleType = wgpu::TextureSampleType::Float;
        entries.push_back(binding1);

        wgpu::BindGroupLayoutDescriptor desc = {};
        desc.entryCount = 2;
        desc.entries = entries.data();

        return device.CreateBindGroupLayout(&desc);
    }

    // Creates a bind group from the given layout (which must have a static
    // sampler at binding 0) that contains the texture to be sampled.
    wgpu::BindGroup CreateBindGroupWithStaticSampler(wgpu::BindGroupLayout bgl) {
        return utils::MakeBindGroup(device, bgl, {{1, mTextureView}});
    }

    wgpu::BindGroup CreateBindGroupWithStaticSamplerPerf(wgpu::BindGroupLayout bgl) {
        return utils::MakeBindGroup(device, bgl, {{1, mLargeTextureView}});
    }

    void TestBindingPerformance() {
        std::vector<wgpu::BindGroupLayout> bgls;
        std::vector<wgpu::BindGroup> bgs;

        wgpu::MipmapFilterMode mipmapFilters[] = {
            wgpu::MipmapFilterMode::Nearest,
            wgpu::MipmapFilterMode::Linear,
        };

        wgpu::FilterMode minFilters[] = {
            wgpu::FilterMode::Nearest,
            wgpu::FilterMode::Linear,
        };

        wgpu::FilterMode magFilters[] = {
            wgpu::FilterMode::Nearest,
            wgpu::FilterMode::Linear,
        };

        wgpu::AddressMode addrModes[] = {
            wgpu::AddressMode::ClampToEdge,
            wgpu::AddressMode::Repeat,
            wgpu::AddressMode::MirrorRepeat,
        };

        for (auto mipmapFilter : mipmapFilters) {
            for (auto minFilter : minFilters) {
                for (auto magFilter : magFilters) {
                    for (auto addressModeU : addrModes) {
                        for (auto addressModeV : addrModes) {
                            for (auto addressModeW : addrModes) {
                                wgpu::SamplerDescriptor descriptor = {};
                                descriptor.minFilter = minFilter;
                                descriptor.magFilter = magFilter;
                                descriptor.mipmapFilter = mipmapFilter;
                                descriptor.addressModeU = addressModeU;
                                descriptor.addressModeV = addressModeV;
                                descriptor.addressModeW = addressModeW;
                                wgpu::Sampler sampler = device.CreateSampler(&descriptor);

                                wgpu::BindGroupLayoutEntry entries[2] = {};

                                wgpu::StaticSamplerBindingLayout staticSamplerBinding = {};
                                staticSamplerBinding.sampler = sampler;

                                entries[0].binding = 0;
                                entries[0].visibility = wgpu::ShaderStage::Fragment;
                                entries[0].nextInChain = &staticSamplerBinding;

                                entries[1].binding = 1;
                                entries[1].visibility = wgpu::ShaderStage::Fragment;
                                entries[1].texture.sampleType = wgpu::TextureSampleType::Float;
                                entries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;
                                entries[1].texture.multisampled = false;

                                wgpu::BindGroupLayoutDescriptor desc = {};
                                desc.entryCount = 2;
                                desc.entries = entries;

                                wgpu::BindGroupLayout bgl = device.CreateBindGroupLayout(&desc);

                                wgpu::BindGroup bindGroup =
                                    utils::MakeBindGroup(device, bgl, {{1, mTextureView}});

                                bgls.push_back(bgl);
                                bgs.push_back(bindGroup);
                            }
                        }
                    }
                }
            }
        }

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

        std::chrono::high_resolution_clock::time_point start, end;

        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&mRenderPass.renderPassInfo);

            start = std::chrono::high_resolution_clock::now();
            for (auto bg : bgs) {
                pass.SetBindGroup(0, bg);
            }
            end = std::chrono::high_resolution_clock::now();

            pass.End();
        }

        wgpu::CommandBuffer commands = encoder.Finish();

        queue.Submit(1, &commands);

        dawn::DebugLog()
            << "Binding time taken: "
            << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
            << " ns for " << bgs.size() << " bind groups";
    }
};

// Test drawing a rect with a checkerboard texture using a static sampler with different address
// modes.
TEST_P(StaticSamplerTest, AddressMode) {
    for (auto u : addressModes) {
        for (auto v : addressModes) {
            for (auto w : addressModes) {
                // Create the bind group layout with a static sampler for the
                // given address modes, configure the pipeline with that layout,
                // and test drawing with a bind group created from that layout.
                auto bgl = CreateBindGroupLayoutWithStaticSampler(u, v, w);
                InitShaders(kBasicFS, bgl);
                TestAddressModes(u, v, w, CreateBindGroupWithStaticSampler(bgl));
            }
        }
    }
}

// Test performance of binding different bind groups with different samplers.
TEST_P(StaticSamplerTest, BindingPerformance) {
    InitShaders(kBasicFS);
    TestBindingPerformance();
}

// Test that passing texture and static sampler objects through user-defined functions works
// correctly.
TEST_P(StaticSamplerTest, PassThroughUserFunctionParameters) {
    for (auto u : addressModes) {
        for (auto v : addressModes) {
            for (auto w : addressModes) {
                // Create the bind group layout with a static sampler for the
                // given address modes, configure the pipeline with that layout,
                // and test drawing with a bind group created from that layout.
                auto bgl = CreateBindGroupLayoutWithStaticSampler(u, v, w);
                InitShaders(kPassThroughUserFunctionsFS, bgl);
                TestAddressModes(u, v, w, CreateBindGroupWithStaticSamplerPerf(bgl));
            }
        }
    }
}

// Test performance of drawing a rect with a large texture using a static sampler with different
TEST_P(StaticSamplerTest, Performance) {
    for (auto u : addressModes) {
        for (auto v : addressModes) {
            for (auto w : addressModes) {
                // Create the bind group layout with a static sampler for the
                // given address modes, configure the pipeline with that layout,
                // and test drawing with a bind group created from that layout.
                auto bgl = CreateBindGroupLayoutWithStaticSamplerPerf(u, v, w);
                InitShaders(kBasicFS, bgl);
                TestSamplerPerformance(CreateBindGroupWithStaticSamplerPerf(bgl));
            }
        }
    }
}

DAWN_INSTANTIATE_TEST(StaticSamplerTest,
                      D3D11Backend(),
                      D3D12Backend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      MetalBackend(),
                      VulkanBackend());

}  // anonymous namespace
}  // namespace dawn
