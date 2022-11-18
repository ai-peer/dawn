// Copyright 2022 The Dawn Authors
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

#include <vector>

#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace {

constexpr uint32_t kRTSize = 16;
constexpr wgpu::TextureFormat kFormat = wgpu::TextureFormat::RGBA8Unorm;

using RequireShaderF16Feature = bool;
DAWN_TEST_PARAM_STRUCT(ShaderF16TestsParams, RequireShaderF16Feature);

}  // anonymous namespace

class ShaderF16Tests : public DawnTestWithParams<ShaderF16TestsParams> {
  public:
    wgpu::Texture CreateDefault2DTexture() {
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size.width = kRTSize;
        descriptor.size.height = kRTSize;
        descriptor.size.depthOrArrayLayers = 1;
        descriptor.sampleCount = 1;
        descriptor.format = kFormat;
        descriptor.mipLevelCount = 1;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
        return device.CreateTexture(&descriptor);
    }

  protected:
    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        mIsShaderF16SupportedOnAdapter = SupportsFeatures({wgpu::FeatureName::ShaderF16});
        if (!mIsShaderF16SupportedOnAdapter) {
            return {};
        }

        if (!IsD3D12()) {
            mUseDxcEnabledOrNonD3D12 = true;
        } else {
            for (auto* enabledToggle : GetParam().forceEnabledWorkarounds) {
                if (strncmp(enabledToggle, "use_dxc", 7) == 0) {
                    mUseDxcEnabledOrNonD3D12 = true;
                    break;
                }
            }
        }

        if (GetParam().mRequireShaderF16Feature && mUseDxcEnabledOrNonD3D12) {
            return {wgpu::FeatureName::ShaderF16};
        }

        return {};
    }

    bool IsShaderF16SupportedOnAdapter() const { return mIsShaderF16SupportedOnAdapter; }
    bool UseDxcEnabledOrNonD3D12() const { return mUseDxcEnabledOrNonD3D12; }

  private:
    bool mIsShaderF16SupportedOnAdapter = false;
    bool mUseDxcEnabledOrNonD3D12 = false;
};

TEST_P(ShaderF16Tests, BasicShaderF16FeaturesTest) {
    const char* computeShader = R"(
        enable f16;

        struct Buf {
            v : f32,
        }
        @group(0) @binding(0) var<storage, read_write> buf : Buf;

        @compute @workgroup_size(1)
        fn CSMain() {
            let a : f16 = f16(buf.v) + 1.0h;
            buf.v = f32(a);
        }
    )";

    const bool shouldShaderF16FeatureSupportedByDevice =
        // Required when creating device
        GetParam().mRequireShaderF16Feature &&
        // Adapter support the feature
        IsShaderF16SupportedOnAdapter() &&
        // Proper toggle, disallow_unsafe_apis and use_dxc if d3d12
        // Note that "disallow_unsafe_apis" is always disabled in DawnTestBase::CreateDeviceImpl.
        !HasToggleEnabled("disallow_unsafe_apis") && UseDxcEnabledOrNonD3D12();
    const bool deviceSupportShaderF16Feature = device.HasFeature(wgpu::FeatureName::ShaderF16);
    EXPECT_EQ(deviceSupportShaderF16Feature, shouldShaderF16FeatureSupportedByDevice);

    if (!deviceSupportShaderF16Feature) {
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, computeShader));
        return;
    }

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = 4u;
    bufferDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer bufferOut = device.CreateBuffer(&bufferDesc);

    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = utils::CreateShaderModule(device, computeShader);
    csDesc.compute.entryPoint = "CSMain";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {
                                                         {0, bufferOut},
                                                     });

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.DispatchWorkgroups(1);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    uint32_t expected[] = {0x3f800000};  // 1.0f
    EXPECT_BUFFER_U32_RANGE_EQ(expected, bufferOut, 0, 1);
}

TEST_P(ShaderF16Tests, RenderPipelineIOF16_RenderTarget) {
    const char* shader = R"(
enable f16;

@vertex
fn VSMain(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0,  1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>(-1.0, -1.0));

    return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
}

@fragment
fn FSMain() -> @location(0) vec4<f16> {
    // Paint it blue
    return vec4<f16>(0.0, 0.0, 1.0, 1.0);
})";

    const bool deviceSupportShaderF16Feature = device.HasFeature(wgpu::FeatureName::ShaderF16);

    if (!deviceSupportShaderF16Feature) {
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, shader));
        return;
    }

    wgpu::ShaderModule shaderModule = utils::CreateShaderModule(device, shader);

    // Create render pipeline.
    wgpu::RenderPipeline pipeline;
    {
        utils::ComboRenderPipelineDescriptor descriptor;

        descriptor.vertex.module = shaderModule;
        descriptor.vertex.entryPoint = "VSMain";

        descriptor.cFragment.module = shaderModule;
        descriptor.cFragment.entryPoint = "FSMain";
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        descriptor.cTargets[0].format = kFormat;

        pipeline = device.CreateRenderPipeline(&descriptor);
    }

    wgpu::Texture renderTarget1 = CreateDefault2DTexture();
    wgpu::Texture renderTarget2 = CreateDefault2DTexture();

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    {
        // In the first render pass we clear renderTarget1 to red and draw a blue triangle in the
        // bottom left of renderTarget1.
        utils::ComboRenderPassDescriptor renderPass({renderTarget1.CreateView()});
        renderPass.cColorAttachments[0].clearValue = {1.0f, 0.0f, 0.0f, 1.0f};

        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.Draw(3);
        pass.End();
    }

    {
        // In the second render pass we clear renderTarget2 to green and draw a blue triangle in the
        // bottom left of renderTarget2.
        utils::ComboRenderPassDescriptor renderPass({renderTarget2.CreateView()});
        renderPass.cColorAttachments[0].clearValue = {0.0f, 1.0f, 0.0f, 1.0f};

        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.Draw(3);
        pass.End();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    // Validate that bottom left of render target is drawed to blue while upper right is still red
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kBlue, renderTarget1, 1, kRTSize - 1);
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kRed, renderTarget1, kRTSize - 1, 1);

    // Validate that bottom left of render target is drawed to blue while upper right is still green
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kBlue, renderTarget2, 1, kRTSize - 1);
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kGreen, renderTarget2, kRTSize - 1, 1);
}

TEST_P(ShaderF16Tests, RenderPipelineIOF16_InterstageVariable) {
    const char* shader = R"(
enable f16;

struct VSOutput{
    @builtin(position)
    pos: vec4<f32>,
    @location(3)
    color_vsout: vec4<f16>,
}

@vertex
fn VSMain(@builtin(vertex_index) VertexIndex : u32) -> VSOutput {
    var pos = array<vec2<f32>, 3>(
        vec2<f32>(-1.0,  1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>(-1.0, -1.0));

    // Blue
    var color = vec4<f16>(0.0h, 0.0h, 1.0h, 1.0h);

    var result: VSOutput;
    result.pos = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
    result.color_vsout = color;

    return result;
}

struct FSInput{
    @location(3)
    color_fsin: vec4<f16>,
}

@fragment
fn FSMain(fsInput: FSInput) -> @location(0) vec4<f16> {
    // Paint it with given color
    return fsInput.color_fsin;
})";

    const bool deviceSupportShaderF16Feature = device.HasFeature(wgpu::FeatureName::ShaderF16);

    if (!deviceSupportShaderF16Feature) {
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, shader));
        return;
    }

    wgpu::ShaderModule shaderModule = utils::CreateShaderModule(device, shader);

    // Create render pipeline.
    wgpu::RenderPipeline pipeline;
    {
        utils::ComboRenderPipelineDescriptor descriptor;

        descriptor.vertex.module = shaderModule;
        descriptor.vertex.entryPoint = "VSMain";

        descriptor.cFragment.module = shaderModule;
        descriptor.cFragment.entryPoint = "FSMain";
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        descriptor.cTargets[0].format = kFormat;

        pipeline = device.CreateRenderPipeline(&descriptor);
    }

    wgpu::Texture renderTarget1 = CreateDefault2DTexture();
    wgpu::Texture renderTarget2 = CreateDefault2DTexture();

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    {
        // In the first render pass we clear renderTarget1 to red and draw a blue triangle in the
        // bottom left of renderTarget1.
        utils::ComboRenderPassDescriptor renderPass({renderTarget1.CreateView()});
        renderPass.cColorAttachments[0].clearValue = {1.0f, 0.0f, 0.0f, 1.0f};

        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.Draw(3);
        pass.End();
    }

    {
        // In the second render pass we clear renderTarget2 to green and draw a blue triangle in the
        // bottom left of renderTarget2.
        utils::ComboRenderPassDescriptor renderPass({renderTarget2.CreateView()});
        renderPass.cColorAttachments[0].clearValue = {0.0f, 1.0f, 0.0f, 1.0f};

        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.Draw(3);
        pass.End();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    // Validate that bottom left of render target is drawed to blue while upper right is still red
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kBlue, renderTarget1, 1, kRTSize - 1);
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kRed, renderTarget1, kRTSize - 1, 1);

    // Validate that bottom left of render target is drawed to blue while upper right is still green
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kBlue, renderTarget2, 1, kRTSize - 1);
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kGreen, renderTarget2, kRTSize - 1, 1);
}

TEST_P(ShaderF16Tests, RenderPipelineIOF16_VeretxAttribute) {
    const char* shader = R"(
enable f16;

@vertex
fn VSMain(@location(0) pos_half : vec2<f16>) -> @builtin(position) vec4<f32> {
    return vec4<f32>(vec2<f32>(pos_half * 2.0h), 0.0, 1.0);
}

@fragment
fn FSMain() -> @location(0) vec4<f16> {
    // Paint it blue
    return vec4<f16>(0.0, 0.0, 1.0, 1.0);
})";

    const bool deviceSupportShaderF16Feature = device.HasFeature(wgpu::FeatureName::ShaderF16);

    if (!deviceSupportShaderF16Feature) {
        ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, shader));
        return;
    }

    wgpu::ShaderModule shaderModule = utils::CreateShaderModule(device, shader);

    // Store the data as float32x2 in vertex buffer, which should be convert to corresponding WGSL
    // type vec2<f16> by driver.
    wgpu::Buffer vertexBuffer = utils::CreateBufferFromData(
        device, wgpu::BufferUsage::Vertex, {-0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f});

    // Create render pipeline.
    wgpu::RenderPipeline pipeline;
    {
        utils::ComboRenderPipelineDescriptor descriptor;

        descriptor.vertex.module = shaderModule;
        descriptor.vertex.entryPoint = "VSMain";
        descriptor.vertex.bufferCount = 1;
        // Interprete the vertex buffer data as Float32x2, and the result should be converted to
        // vec2<f16>
        descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x2;
        descriptor.cAttributes[0].offset = 0;
        descriptor.cAttributes[0].shaderLocation = 0;
        descriptor.cBuffers[0].stepMode = wgpu::VertexStepMode::Vertex;
        descriptor.cBuffers[0].arrayStride = 8;
        descriptor.cBuffers[0].attributeCount = 1;
        descriptor.cBuffers[0].attributes = &descriptor.cAttributes[0];

        descriptor.cFragment.module = shaderModule;
        descriptor.cFragment.entryPoint = "FSMain";
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        descriptor.cTargets[0].format = kFormat;

        pipeline = device.CreateRenderPipeline(&descriptor);
    }

    wgpu::Texture renderTarget = CreateDefault2DTexture();

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    {
        // Clear renderTarget to red and draw a blue triangle in the bottom left of renderTarget1.
        utils::ComboRenderPassDescriptor renderPass({renderTarget.CreateView()});
        renderPass.cColorAttachments[0].clearValue = {1.0f, 0.0f, 0.0f, 1.0f};

        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3);
        pass.End();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    // Validate that bottom left of render target is drawed to blue while upper right is still red
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kBlue, renderTarget, 1, kRTSize - 1);
    EXPECT_PIXEL_RGBA8_EQ(utils::RGBA8::kRed, renderTarget, kRTSize - 1, 1);
}

// DawnTestBase::CreateDeviceImpl always disable disallow_unsafe_apis toggle.
DAWN_INSTANTIATE_TEST_P(ShaderF16Tests,
                        {
                            D3D12Backend(),
                            D3D12Backend({"use_dxc"}),
                            VulkanBackend(),
                            MetalBackend(),
                            OpenGLBackend(),
                            OpenGLESBackend(),
                        },
                        {true, false});
