// Copyright 2018 The Dawn Authors
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

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr uint32_t kRTSize = 4;

class DrawIndexedIndirectTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, R"(
            [[stage(vertex)]]
            fn main([[location(0)]] pos : vec4<f32>) -> [[builtin(position)]] vec4<f32> {
                return pos;
            })");

        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
            [[stage(fragment)]] fn main() -> [[location(0)]] vec4<f32> {
                return vec4<f32>(0.0, 1.0, 0.0, 1.0);
            })");

        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = vsModule;
        descriptor.cFragment.module = fsModule;
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
        descriptor.primitive.stripIndexFormat = wgpu::IndexFormat::Uint32;
        descriptor.vertex.bufferCount = 1;
        descriptor.cBuffers[0].arrayStride = 4 * sizeof(float);
        descriptor.cBuffers[0].attributeCount = 1;
        descriptor.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
        descriptor.cTargets[0].format = renderPass.colorFormat;

        pipeline = device.CreateRenderPipeline(&descriptor);

        vertexBuffer = utils::CreateBufferFromData<float>(
            device, wgpu::BufferUsage::Vertex,
            {// First quad: the first 3 vertices represent the bottom left triangle
             -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f,
             0.0f, 1.0f,

             // Second quad: the first 3 vertices represent the top right triangle
             -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f,
             0.0f, 1.0f});
    }

    utils::BasicRenderPass renderPass;
    wgpu::RenderPipeline pipeline;
    wgpu::Buffer vertexBuffer;

    wgpu::Buffer CreateIndirectBuffer(std::initializer_list<uint32_t> indirectParamList) {
        return utils::CreateBufferFromData<uint32_t>(device, wgpu::BufferUsage::Indirect,
                                                     indirectParamList);
    }

    wgpu::Buffer CreateIndexBuffer(std::initializer_list<uint32_t> indexList) {
        return utils::CreateBufferFromData<uint32_t>(device, wgpu::BufferUsage::Index, indexList);
    }

    wgpu::CommandBuffer EncodeDrawCommands(std::initializer_list<uint32_t> bufferList,
                                           wgpu::Buffer indexBuffer,
                                           uint64_t indexOffset,
                                           uint64_t indirectOffset) {
        wgpu::Buffer indirectBuffer = CreateIndirectBuffer(bufferList);

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
            pass.SetPipeline(pipeline);
            pass.SetVertexBuffer(0, vertexBuffer);
            pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32, indexOffset);
            pass.DrawIndexedIndirect(indirectBuffer, indirectOffset);
            pass.EndPass();
        }

        return encoder.Finish();
    }

    void TestDraw(wgpu::CommandBuffer commands, RGBA8 bottomLeftExpected, RGBA8 topRightExpected) {
        queue.Submit(1, &commands);

        EXPECT_PIXEL_RGBA8_EQ(bottomLeftExpected, renderPass.color, 1, 3);
        EXPECT_PIXEL_RGBA8_EQ(topRightExpected, renderPass.color, 3, 1);
    }

    void Test(std::initializer_list<uint32_t> bufferList,
              uint64_t indexOffset,
              uint64_t indirectOffset,
              RGBA8 bottomLeftExpected,
              RGBA8 topRightExpected) {
        wgpu::Buffer indexBuffer =
            CreateIndexBuffer({0, 1, 2, 0, 3, 1,
                               // The indices below are added to test negatve baseVertex
                               0 + 4, 1 + 4, 2 + 4, 0 + 4, 3 + 4, 1 + 4});
        TestDraw(EncodeDrawCommands(bufferList, indexBuffer, indexOffset, indirectOffset),
                 bottomLeftExpected, topRightExpected);
    }
};

// The most basic DrawIndexed triangle draw.
TEST_P(DrawIndexedIndirectTest, Uint32) {
    // TODO(crbug.com/dawn/789): Test is failing after a roll on SwANGLE on Windows only.
    DAWN_SUPPRESS_TEST_IF(IsANGLE() && IsWindows());

    RGBA8 filled(0, 255, 0, 255);
    RGBA8 notFilled(0, 0, 0, 0);

    // Test a draw with no indices.
    Test({0, 0, 0, 0, 0}, 0, 0, notFilled, notFilled);

    // Test a draw with only the first 3 indices of the first quad (bottom left triangle)
    Test({3, 1, 0, 0, 0}, 0, 0, filled, notFilled);

    // Test a draw with only the last 3 indices of the first quad (top right triangle)
    Test({3, 1, 3, 0, 0}, 0, 0, notFilled, filled);

    // Test a draw with all 6 indices (both triangles).
    Test({6, 1, 0, 0, 0}, 0, 0, filled, filled);
}

// Test the parameter 'baseVertex' of DrawIndexed() works.
TEST_P(DrawIndexedIndirectTest, BaseVertex) {
    // TODO(crbug.com/dawn/161): add workaround for OpenGL index buffer offset (could be compute
    // shader that adds it to the draw calls)
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGL());
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    // TODO(crbug.com/dawn/966): Fails on Metal Intel, likely because [[builtin(vertex_index)]]
    // doesn't take into account BaseVertex, which breaks programmable vertex pulling.
    DAWN_SUPPRESS_TEST_IF(IsMetal() && IsIntel());

    RGBA8 filled(0, 255, 0, 255);
    RGBA8 notFilled(0, 0, 0, 0);

    // Test a draw with only the first 3 indices of the second quad (top right triangle)
    Test({3, 1, 0, 4, 0}, 0, 0, notFilled, filled);

    // Test a draw with only the last 3 indices of the second quad (bottom left triangle)
    Test({3, 1, 3, 4, 0}, 0, 0, filled, notFilled);

    const int negFour = -4;
    uint32_t unsignedNegFour;
    std::memcpy(&unsignedNegFour, &negFour, sizeof(int));

    // Test negative baseVertex
    // Test a draw with only the first 3 indices of the first quad (bottom left triangle)
    Test({3, 1, 0, unsignedNegFour, 0}, 6 * sizeof(uint32_t), 0, filled, notFilled);

    // Test a draw with only the last 3 indices of the first quad (top right triangle)
    Test({3, 1, 3, unsignedNegFour, 0}, 6 * sizeof(uint32_t), 0, notFilled, filled);
}

TEST_P(DrawIndexedIndirectTest, IndirectOffset) {
    // TODO(crbug.com/dawn/789): Test is failing after a roll on SwANGLE on Windows only.
    DAWN_SUPPRESS_TEST_IF(IsANGLE() && IsWindows());

    // TODO(crbug.com/dawn/966): Fails on Metal Intel, likely because [[builtin(vertex_index)]]
    // doesn't take into account BaseVertex, which breaks programmable vertex pulling.
    DAWN_SUPPRESS_TEST_IF(IsMetal() && IsIntel());

    RGBA8 filled(0, 255, 0, 255);
    RGBA8 notFilled(0, 0, 0, 0);

    // Test an offset draw call, with indirect buffer containing 2 calls:
    // 1) first 3 indices of the second quad (top right triangle)
    // 2) last 3 indices of the second quad

    // Test #1 (no offset)
    Test({3, 1, 0, 4, 0, 3, 1, 3, 4, 0}, 0, 0, notFilled, filled);

    // Offset to draw #2
    Test({3, 1, 0, 4, 0, 3, 1, 3, 4, 0}, 0, 5 * sizeof(uint32_t), filled, notFilled);
}

TEST_P(DrawIndexedIndirectTest, BasicValidation) {
    // TODO(crbug.com/dawn/789): Test is failing under SwANGLE on Windows only.
    DAWN_SUPPRESS_TEST_IF(IsANGLE() && IsWindows());

    // It doesn't make sense to test invalid inputs when validation is disabled.
    DAWN_SUPPRESS_TEST_IF(HasToggleEnabled("skip_validation"));

    RGBA8 filled(0, 255, 0, 255);
    RGBA8 notFilled(0, 0, 0, 0);

    wgpu::Buffer indexBuffer = CreateIndexBuffer({0, 1, 2, 0, 3, 1});

    // Test a draw with an excessive index count. Should cap at the maximum size of the index buffer
    TestDraw(EncodeDrawCommands({420000000, 1, 0, 0, 0}, indexBuffer, 0, 0), filled, filled);

    // Test a draw an excessive firstIndex. Should draw nothing.
    TestDraw(EncodeDrawCommands({3, 1, 10000, 0, 0}, indexBuffer, 0, 0), notFilled, notFilled);

    // Test a draw which partially overflows the index buffer. Should draw only what's in bounds.
    TestDraw(EncodeDrawCommands({10000, 1, 3, 0, 0}, indexBuffer, 0, 0), notFilled, filled);
}

TEST_P(DrawIndexedIndirectTest, ValidateWithOffsets) {
    // TODO(crbug.com/dawn/161): The GL/GLES backend doesn't support indirect index buffer offsets
    // yet.
    DAWN_SUPPRESS_TEST_IF(IsOpenGL() || IsOpenGLES());

    // It doesn't make sense to test invalid inputs when validation is disabled.
    DAWN_SUPPRESS_TEST_IF(HasToggleEnabled("skip_validation"));

    RGBA8 filled(0, 255, 0, 255);
    RGBA8 notFilled(0, 0, 0, 0);

    wgpu::Buffer indexBuffer = CreateIndexBuffer({0, 1, 2, 0, 3, 1, 0, 1, 2});

    // Test that validation properly accounts for index buffer offset.
    TestDraw(EncodeDrawCommands({1000, 1, 0, 0, 0}, indexBuffer, 6 * sizeof(uint32_t), 0), filled,
             notFilled);
    TestDraw(EncodeDrawCommands({1000, 1, 3, 0, 0}, indexBuffer, 3 * sizeof(uint32_t), 0), filled,
             notFilled);

    // Test that validation properly accounts for indirect buffer offset.
    TestDraw(
        EncodeDrawCommands({1, 2, 3, 4, 1000, 1, 0, 0, 0}, indexBuffer, 0, 4 * sizeof(uint32_t)),
        filled, filled);
}

TEST_P(DrawIndexedIndirectTest, ValidateMultiplePasses) {
    // TODO(crbug.com/dawn/789): Test is failing under SwANGLE on Windows only.
    DAWN_SUPPRESS_TEST_IF(IsANGLE() && IsWindows());

    // It doesn't make sense to test invalid inputs when validation is disabled.
    DAWN_SUPPRESS_TEST_IF(HasToggleEnabled("skip_validation"));

    RGBA8 filled(0, 255, 0, 255);
    RGBA8 notFilled(0, 0, 0, 0);

    wgpu::Buffer indexBuffer = CreateIndexBuffer({0, 1, 2, 0, 3, 1, 0, 1, 2});

    // Test that validation with multiple passes in a row. Namely this is exercising robustness of
    // the validation scratch buffer's treatment, for example to ensure that data for use with a
    // previous pass's validation commands is not overwritten before it can be used.
    TestDraw(EncodeDrawCommands({1000, 1, 0, 0, 0}, indexBuffer, 0, 0), filled, filled);
    TestDraw(EncodeDrawCommands({1000, 1, 6, 0, 0}, indexBuffer, 0, 0), filled, notFilled);
    TestDraw(EncodeDrawCommands({1000, 1, 9, 0, 0}, indexBuffer, 0, 0), notFilled, notFilled);
}

TEST_P(DrawIndexedIndirectTest, ValidateMultipleDraws) {
    // TODO(crbug.com/dawn/789): Test is failing under SwANGLE on Windows only.
    DAWN_SUPPRESS_TEST_IF(IsANGLE() && IsWindows());

    // It doesn't make sense to test invalid inputs when validation is disabled.
    DAWN_SUPPRESS_TEST_IF(HasToggleEnabled("skip_validation"));

    RGBA8 filled(0, 255, 0, 255);
    RGBA8 notFilled(0, 0, 0, 0);

    // Validate multiple draw calls using the same index and indirect buffers as input, but with
    // different indirect offsets.
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::Buffer indirectBuffer = CreateIndirectBuffer({1000, 1, 3, 0, 0, 1000, 1, 6, 0, 0});
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(CreateIndexBuffer({0, 1, 2, 0, 3, 1}), wgpu::IndexFormat::Uint32, 0);
        pass.DrawIndexedIndirect(indirectBuffer, 0);
        pass.DrawIndexedIndirect(indirectBuffer, 20);
        pass.EndPass();
    }
    wgpu::CommandBuffer commands = encoder.Finish();

    queue.Submit(1, &commands);
    EXPECT_PIXEL_RGBA8_EQ(notFilled, renderPass.color, 1, 3);
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color, 3, 1);

    // Validate multiple draw calls using the same index buffer but different indirect buffers as
    // input.
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(CreateIndexBuffer({0, 1, 2, 0, 3, 1}), wgpu::IndexFormat::Uint32, 0);
        pass.DrawIndexedIndirect(CreateIndirectBuffer({10000, 1, 3, 0, 0}), 0);
        pass.DrawIndexedIndirect(CreateIndirectBuffer({10000, 1, 6, 0, 0}), 0);
        pass.EndPass();
    }
    commands = encoder.Finish();

    queue.Submit(1, &commands);
    EXPECT_PIXEL_RGBA8_EQ(notFilled, renderPass.color, 1, 3);
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color, 3, 1);

    // Validate multiple draw calls across different index and indirect buffers.
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
        pass.SetPipeline(pipeline);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(CreateIndexBuffer({0, 1, 2, 0, 3, 1}), wgpu::IndexFormat::Uint32, 0);
        pass.DrawIndexedIndirect(CreateIndirectBuffer({10000, 1, 3, 0, 0}), 0);
        pass.SetIndexBuffer(CreateIndexBuffer({0, 3, 1}), wgpu::IndexFormat::Uint32, 0);
        pass.DrawIndexedIndirect(CreateIndirectBuffer({10000, 1, 0, 0, 0}), 0);
        pass.EndPass();
    }
    commands = encoder.Finish();

    queue.Submit(1, &commands);
    EXPECT_PIXEL_RGBA8_EQ(notFilled, renderPass.color, 1, 3);
    EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color, 3, 1);
}

DAWN_INSTANTIATE_TEST(DrawIndexedIndirectTest,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
