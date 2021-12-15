// Copyright 2021 The Dawn Authors
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

#include "tests/DawnTest.h"

#include "common/Math.h"
#include "dawn_native/DawnNative.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/TestUtils.h"
#include "utils/WGPUHelpers.h"

#include <algorithm>

class BufferAllocatedSizeTests : public DawnTest {
  protected:
    wgpu::Buffer CreateBuffer(wgpu::BufferUsage usage, uint64_t size) {
        wgpu::BufferDescriptor desc = {};
        desc.usage = usage;
        desc.size = size;
        return device.CreateBuffer(&desc);
    }

    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    }
};

// Test expected allocated size for buffers with uniform usage
TEST_P(BufferAllocatedSizeTests, UniformUsage) {
    // Some backends have a minimum buffer size, so make surewe allocate above that.
    constexpr uint32_t kMinBufferSize = 4u;

    uint32_t requiredBufferAlignment = 1u;
    if (IsD3D12()) {
        requiredBufferAlignment = 256u;
    } else if (IsMetal()) {
        requiredBufferAlignment = 16u;
    } else if (IsVulkan()) {
        requiredBufferAlignment = 4u;
    }

    // Test uniform usage
    {
        const uint32_t bufferSize = kMinBufferSize;
        wgpu::Buffer buffer = CreateBuffer(wgpu::BufferUsage::Uniform, bufferSize);
        EXPECT_EQ(dawn_native::GetAllocatedSizeForTesting(buffer.Get()),
                  Align(bufferSize, requiredBufferAlignment));
    }

    // Test uniform usage and with size just above requiredBufferAlignment allocates to the next
    // multiple of |requiredBufferAlignment|
    {
        const uint32_t bufferSize = std::max(1u + requiredBufferAlignment, kMinBufferSize);
        wgpu::Buffer buffer =
            CreateBuffer(wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage, bufferSize);
        EXPECT_EQ(dawn_native::GetAllocatedSizeForTesting(buffer.Get()),
                  Align(bufferSize, requiredBufferAlignment));
    }

    // Test uniform usage and another usage
    {
        const uint32_t bufferSize = kMinBufferSize;
        wgpu::Buffer buffer =
            CreateBuffer(wgpu::BufferUsage::Uniform | wgpu::BufferUsage::Storage, bufferSize);
        EXPECT_EQ(dawn_native::GetAllocatedSizeForTesting(buffer.Get()),
                  Align(bufferSize, requiredBufferAlignment));
    }
}

DAWN_INSTANTIATE_TEST(BufferAllocatedSizeTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

namespace {
    using VertexFormat = wgpu::VertexFormat;
    using ExtraBytes = uint32_t;
    DAWN_TEST_PARAM_STRUCT(BufferAllocationPaddingTestParams, VertexFormat, ExtraBytes);
}  // namespace

// Tests for crbug.com/dawn/837 and crbug.com/dawn/1214.
class BufferAllocationPaddingTest : public DawnTestWithParams<BufferAllocationPaddingTestParams> {
  protected:
    void SetUp() override {
        // The setup for these tests involve writing out to a series of buffers, then forcing the
        // test to resolve the writes and "teardown" once before re-setting up for the actual
        // tests. This is currently required because the issue in crbug.com/dawn/1214 has not been
        // reproduceable otherwise.
        DawnTestBase::SetUp();
        {
            // Creates, fills, and deallocates buffers so that the memory isn't "clean" when reused.
            std::vector<wgpu::Buffer> buffers(kBufferCount);
            std::vector<uint8_t> expected(kBufferFillSize);
            for (uint32_t i = 0; i < kBufferFillSize; i++) {
                expected[i] = i % 256;
            }
            for (uint32_t i = 0; i < kBufferCount; i++) {
                buffers[i] = utils::CreateBufferFromData(
                    device, expected.data(), kBufferFillSize,
                    wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Uniform |
                        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

                // We need to do a map/read on the buffers, otherwise it seems like something may
                // optimize writing out the memory.
                EXPECT_BUFFER_U8_RANGE_EQ(expected.data(), buffers[i], 0, kBufferFillSize);
            }
        }
        TearDown();
        DawnTestBase::SetUp();

        // Creates the render pipeline that's used to produce output based on the vertices read.
        wgpu::VertexFormat vertexFormat = GetParam().mVertexFormat;
        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = utils::CreateShaderModule(device, R"(
            struct VertexOut {
                [[location(0)]] color : vec4<f32>;
                [[builtin(position)]] position : vec4<f32>;
            };

            [[stage(vertex)]] fn main([[location(0)]] pos : vec2<f32>) -> VertexOut {
                var output : VertexOut;
                if (all(pos == vec2<f32>(0.0, 0.0))) {
                    output.color = vec4<f32>(0.0, 1.0, 0.0, 1.0);
                } else {
                    output.color = vec4<f32>(1.0, 0.0, 0.0, 1.0);
                }
                output.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
                return output;
            })");
        descriptor.cFragment.module = utils::CreateShaderModule(device, R"(
            [[stage(fragment)]]
            fn main([[location(0)]] i_color : vec4<f32>) -> [[location(0)]] vec4<f32> {
                return i_color;
            })");
        descriptor.primitive.topology = wgpu::PrimitiveTopology::PointList;
        descriptor.vertex.bufferCount = 1u;
        descriptor.cBuffers[0].arrayStride = Align(utils::VertexFormatSize(vertexFormat), 4);
        descriptor.cBuffers[0].attributeCount = 1;
        descriptor.cAttributes[0].format = vertexFormat;
        descriptor.cTargets[0].format = kColorAttachmentFormat;
        renderPipeline = device.CreateRenderPipeline(&descriptor);
    }

    void TearDown() override {
        DawnTestBase::TearDown();
        queue = wgpu::Queue();
        backendDevice = nullptr;
        device = wgpu::Device();
    }

    // Size of the buffer that we fill and deallocate at the start of the test so that future
    // allocations may not be zero-ed entirely.
    static constexpr uint32_t kBufferCount = 32;
    static constexpr uint32_t kBufferFillSize = 4096;
    static constexpr wgpu::TextureFormat kColorAttachmentFormat = wgpu::TextureFormat::RGBA8Unorm;

    RGBA8 kExpectedPixelValue = {0, 255, 0, 255};

    wgpu::RenderPipeline renderPipeline;
};

// Test that the padding after a buffer allocation is initialized to 0. This test makes unaligned
// vertex buffers which should be padded in the backend allocation. It then tries to index off the
// end of the vertex buffer in an indexed draw call. A backend which implements robust buffer
// access via clamping should still see zeros at the end of the buffer.
TEST_P(BufferAllocationPaddingTest, PaddingInitializedAndRobustAccess) {
    DAWN_SUPPRESS_TEST_IF(IsANGLE());  // TODO(crbug.com/dawn/1084).

    const uint32_t vertexFormatSize = utils::VertexFormatSize(GetParam().mVertexFormat);
    const uint32_t vertexBufferSize = vertexFormatSize + GetParam().mExtraBytes;
    std::vector<uint8_t> vertexBufferData(vertexBufferSize, 0);
    wgpu::Buffer vertexBuffer =
        utils::CreateBufferFromData(device, vertexBufferData.data(), vertexBufferSize,
                                    wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Uniform |
                                        wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst);

    // Using the vertex buffer offset and vertex buffer index, iterate across the entire buffer,
    // into the padded region, and past the allocated region to verify that the values are zeros.
    const uint32_t maxVertexIndex =
        (dawn_native::GetAllocatedSizeForTesting(vertexBuffer.Get()) / vertexFormatSize);
    for (uint32_t vertexBufferOffset = 0; vertexBufferOffset <= vertexBufferSize;
         vertexBufferOffset += 4u) {
        for (uint32_t vertexIndex = 0; vertexIndex <= maxVertexIndex; vertexIndex++) {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

            wgpu::Buffer indexBuffer = utils::CreateBufferFromData<uint32_t>(
                device, wgpu::BufferUsage::Index, {vertexIndex});

            wgpu::Texture colorAttachment =
                utils::CreateTextureFromColor(device, {1, 1, 1}, kColorAttachmentFormat);
            utils::ComboRenderPassDescriptor renderPassDescriptor({colorAttachment.CreateView()});

            wgpu::RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDescriptor);
            renderPass.SetVertexBuffer(0, vertexBuffer, vertexBufferOffset);
            renderPass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
            renderPass.SetPipeline(renderPipeline);
            renderPass.DrawIndexed(1);
            renderPass.EndPass();

            wgpu::CommandBuffer commandBuffer = encoder.Finish();
            queue.Submit(1, &commandBuffer);
            EXPECT_PIXEL_RGBA8_EQ(kExpectedPixelValue, colorAttachment, 0, 0);
        }
    }
}

DAWN_INSTANTIATE_TEST_P(
    BufferAllocationPaddingTest,
    {D3D12Backend({"nonzero_clear_resources_on_creation_for_testing"}),
     MetalBackend({"nonzero_clear_resources_on_creation_for_testing"}),
     OpenGLBackend({"nonzero_clear_resources_on_creation_for_testing"}),
     OpenGLESBackend({"nonzero_clear_resources_on_creation_for_testing"}),
     VulkanBackend({"nonzero_clear_resources_on_creation_for_testing"})},
    // A small sub-4-byte format means a single vertex can fit entirely within the padded buffer,
    // touching some of the padding. Test a small format, as well as larger formats.
    {wgpu::VertexFormat::Unorm8x2, wgpu::VertexFormat::Float16x2, wgpu::VertexFormat::Float32x2},
    // Additional bytes added to the buffer size to test partially indexing OOB into padding.
    {0u, 1u, 2u, 3u, 4u});
