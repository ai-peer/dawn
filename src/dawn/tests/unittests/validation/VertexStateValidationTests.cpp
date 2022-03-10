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

#include "dawn/tests/unittests/validation/ValidationTest.h"

#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

class VertexStateTest : public ValidationTest {
  protected:
    void CreatePipeline(bool success,
                        const utils::ComboVertexState& state,
                        const char* vertexSource,
                        wgpu::RenderPipeline* renderPipeline = nullptr) {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, vertexSource);
        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, R"(
            @stage(fragment) fn main() -> @location(0) vec4<f32> {
                return vec4<f32>(1.0, 0.0, 0.0, 1.0);
            }
        )");

        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = vsModule;
        descriptor.vertex.bufferCount = state.vertexBufferCount;
        descriptor.vertex.buffers = &state.cVertexBuffers[0];
        descriptor.cFragment.module = fsModule;
        descriptor.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;

        if (!success) {
            ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
        } else {
            ASSERT(renderPipeline != nullptr);
            *renderPipeline = device.CreateRenderPipeline(&descriptor);
        }
    }

    const char* kDummyVertexShader = R"(
        @stage(vertex) fn main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }
    )";
};

// Check an empty vertex input is valid
TEST_F(VertexStateTest, EmptyIsOk) {
    utils::ComboVertexState state;
    CreatePipeline(true, state, kDummyVertexShader);
}

// Check null buffer is valid
TEST_F(VertexStateTest, NullBufferIsOk) {
    utils::ComboVertexState state;
    // One null buffer (buffer[0]) is OK
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].arrayStride = 0;
    state.cVertexBuffers[0].attributeCount = 0;
    state.cVertexBuffers[0].attributes = nullptr;
    CreatePipeline(true, state, kDummyVertexShader);

    // One null buffer (buffer[0]) followed by a buffer (buffer[1]) is OK
    state.vertexBufferCount = 2;
    state.cVertexBuffers[1].arrayStride = 0;
    state.cVertexBuffers[1].attributeCount = 1;
    state.cVertexBuffers[1].attributes = &state.cAttributes[0];
    state.cAttributes[0].shaderLocation = 0;
    CreatePipeline(true, state, kDummyVertexShader);

    // Null buffer (buffer[2]) sitting between buffers (buffer[1] and buffer[3]) is OK
    state.vertexBufferCount = 4;
    state.cVertexBuffers[2].attributeCount = 0;
    state.cVertexBuffers[2].attributes = nullptr;
    state.cVertexBuffers[3].attributeCount = 1;
    state.cVertexBuffers[3].attributes = &state.cAttributes[1];
    state.cAttributes[1].shaderLocation = 1;
    CreatePipeline(true, state, kDummyVertexShader);
}

// Check validation that pipeline vertex buffers are backed by attributes in the vertex input
TEST_F(VertexStateTest, PipelineCompatibility) {
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].arrayStride = 2 * sizeof(float);
    state.cVertexBuffers[0].attributeCount = 2;
    state.cAttributes[0].shaderLocation = 0;
    state.cAttributes[1].shaderLocation = 1;
    state.cAttributes[1].offset = sizeof(float);

    // Control case: pipeline with one input per attribute
    CreatePipeline(true, state, R"(
        @stage(vertex) fn main(
            @location(0) a : vec4<f32>,
            @location(1) b : vec4<f32>
        ) -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }
    )");

    // Check it is valid for the pipeline to use a subset of the VertexState
    CreatePipeline(true, state, R"(
        @stage(vertex) fn main(
            @location(0) a : vec4<f32>
        ) -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }
    )");

    // Check for an error when the pipeline uses an attribute not in the vertex input
    CreatePipeline(false, state, R"(
        @stage(vertex) fn main(
            @location(2) a : vec4<f32>
        ) -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }
    )");
}

// Test that a arrayStride of 0 is valid
TEST_F(VertexStateTest, StrideZero) {
    // Works ok without attributes
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].arrayStride = 0;
    state.cVertexBuffers[0].attributeCount = 1;
    CreatePipeline(true, state, kDummyVertexShader);

    // Works ok with attributes at a large-ish offset
    state.cAttributes[0].offset = 128;
    CreatePipeline(true, state, kDummyVertexShader);
}

// Check validation that vertex attribute offset should be within vertex buffer arrayStride,
// if vertex buffer arrayStride is not zero.
TEST_F(VertexStateTest, SetOffsetOutOfBounds) {
    // Control case, setting correct arrayStride and offset
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].arrayStride = 2 * sizeof(float);
    state.cVertexBuffers[0].attributeCount = 2;
    state.cAttributes[0].shaderLocation = 0;
    state.cAttributes[1].shaderLocation = 1;
    state.cAttributes[1].offset = sizeof(float);
    CreatePipeline(true, state, kDummyVertexShader);

    // Test vertex attribute offset exceed vertex buffer arrayStride range
    state.cVertexBuffers[0].arrayStride = sizeof(float);
    CreatePipeline(false, state, kDummyVertexShader);

    // It's OK if arrayStride is zero
    state.cVertexBuffers[0].arrayStride = 0;
    CreatePipeline(true, state, kDummyVertexShader);
}

// Check out of bounds condition on total number of vertex buffers
TEST_F(VertexStateTest, SetVertexBuffersNumLimit) {
    // Control case, setting max vertex buffer number
    utils::ComboVertexState state;
    state.vertexBufferCount = kMaxVertexBuffers;
    for (uint32_t i = 0; i < kMaxVertexBuffers; ++i) {
        state.cVertexBuffers[i].attributeCount = 1;
        state.cVertexBuffers[i].attributes = &state.cAttributes[i];
        state.cAttributes[i].shaderLocation = i;
    }
    CreatePipeline(true, state, kDummyVertexShader);

    // Test vertex buffer number exceed the limit
    state.vertexBufferCount = kMaxVertexBuffers + 1;
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check out of bounds condition on total number of vertex attributes
TEST_F(VertexStateTest, SetVertexAttributesNumLimit) {
    // Control case, setting max vertex attribute number
    utils::ComboVertexState state;
    state.vertexBufferCount = 2;
    state.cVertexBuffers[0].attributeCount = kMaxVertexAttributes;
    for (uint32_t i = 0; i < kMaxVertexAttributes; ++i) {
        state.cAttributes[i].shaderLocation = i;
    }
    CreatePipeline(true, state, kDummyVertexShader);

    // Test vertex attribute number exceed the limit
    state.cVertexBuffers[1].attributeCount = 1;
    state.cVertexBuffers[1].attributes = &state.cAttributes[kMaxVertexAttributes - 1];
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check out of bounds condition on input arrayStride
TEST_F(VertexStateTest, SetInputStrideOutOfBounds) {
    // Control case, setting max input arrayStride
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].arrayStride = kMaxVertexBufferArrayStride;
    state.cVertexBuffers[0].attributeCount = 1;
    CreatePipeline(true, state, kDummyVertexShader);

    // Test input arrayStride OOB
    state.cVertexBuffers[0].arrayStride = kMaxVertexBufferArrayStride + 1;
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check multiple of 4 bytes constraint on input arrayStride
TEST_F(VertexStateTest, SetInputStrideNotAligned) {
    // Control case, setting input arrayStride 4 bytes.
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].arrayStride = 4;
    state.cVertexBuffers[0].attributeCount = 1;
    CreatePipeline(true, state, kDummyVertexShader);

    // Test input arrayStride not multiple of 4 bytes
    state.cVertexBuffers[0].arrayStride = 2;
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check that last vertex buffer element should not be required to have the full arrayStride size
TEST_F(VertexStateTest, DrawStrideLimitsVertex) {
    DummyRenderPass renderPass(device);

    // Create a buffer of size 28, containing 4 float32 elements, array stride size = 8
    // The last element doesn't have the full stride size
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 28;
    descriptor.usage = wgpu::BufferUsage::Vertex;
    wgpu::Buffer vertexBuffer = device.CreateBuffer(&descriptor);

    // Vertex attribute offset is 0
    wgpu::RenderPipeline pipeline1;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 0;

        CreatePipeline(true, state, kDummyVertexShader, &pipeline1);
    }

    // Vertex attribute offset is 4
    wgpu::RenderPipeline pipeline2;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 4;

        CreatePipeline(true, state, kDummyVertexShader, &pipeline2);
    }

    // Control case: draw 3 elements, 3 * 8 = 24 <= 28, is valid anyway
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 elements with firstVertex == 1, (2 + 1) * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3, 0, 1, 0);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 elements with offset == 4, 4 + 3 * 8 = 24 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 4 elements, 4 * 8 = 32 > 28
    // But the last element does not require to have the full stride size
    // So 3 * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(4);
        pass.End();
    }
    encoder.Finish();

    // Invalid: draw 4 elements with firstVertex == 1,
    // It requires a buffer with size of (3 + 1) * 8 + 4 = 36 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(4, 0, 1, 0);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Invalid: draw 4 elements with offset == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// TODO: Draw Instanced attribute
TEST_F(VertexStateTest, DrawStrideLimitsInstance) {
    DummyRenderPass renderPass(device);

    // Create a buffer of size 28, containing 4 float32 elements, array stride size = 8
    // The last element doesn't have the full stride size
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 28;
    descriptor.usage = wgpu::BufferUsage::Vertex;
    wgpu::Buffer vertexBuffer = device.CreateBuffer(&descriptor);

    // Vertex attribute offset is 0
    wgpu::RenderPipeline pipeline1;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 0;

        CreatePipeline(true, state, kDummyVertexShader, &pipeline1);
    }

    // Vertex attribute offset is 4
    wgpu::RenderPipeline pipeline2;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 4;

        CreatePipeline(true, state, kDummyVertexShader, &pipeline2);
    }

    // Control case: draw 3 instances, 3 * 8 = 24 <= 28, is valid anyway
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 instances with firstInstance == 1, (2 + 1) * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 3, 0, 1);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 instances with offset == 4, 4 + 3 * 8 = 24 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 4 instances, 4 * 8 = 32 > 28
    // But the last element does not require to have the full stride size
    // So 3 * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 4);
        pass.End();
    }
    encoder.Finish();

    // Invalid: draw 4 instances with firstInstance == 1,
    // It requires a buffer with size of (3 + 1) * 8 + 4 = 36 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 4, 0, 1);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Invalid: draw 4 instances with offset == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.Draw(1, 4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// TODO: DrawIndexed ArrayStride
TEST_F(VertexStateTest, DrawIndexedStrideLimitsInstance) {
    DummyRenderPass renderPass(device);

    // Create a buffer of size 28, containing 4 float32 elements, array stride size = 8
    // The last element doesn't have the full stride size
    wgpu::BufferDescriptor descriptor;
    descriptor.size = 28;
    descriptor.usage = wgpu::BufferUsage::Vertex;
    wgpu::Buffer vertexBuffer = device.CreateBuffer(&descriptor);

    wgpu::Buffer indexBuffer =
        utils::CreateBufferFromData<uint32_t>(device, wgpu::BufferUsage::Index, {0, 1, 2});

    // Vertex attribute offset is 0
    wgpu::RenderPipeline pipeline1;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 0;

        CreatePipeline(true, state, kDummyVertexShader, &pipeline1);
    }

    // Vertex attribute offset is 4
    wgpu::RenderPipeline pipeline2;
    {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 8;
        state.cVertexBuffers[0].stepMode = wgpu::VertexStepMode::Instance;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].offset = 4;

        CreatePipeline(true, state, kDummyVertexShader, &pipeline2);
    }

    // Control case: draw 3 instances, 3 * 8 = 24 <= 28, is valid anyway
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.DrawIndexed(3, 3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 instances with firstInstance == 1, (2 + 1) * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 3, 0, 1);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 3 instances with offset == 4, 4 + 3 * 8 = 24 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 3);
        pass.End();
    }
    encoder.Finish();

    // Valid: draw 4 instances, 4 * 8 = 32 > 28
    // But the last element does not require to have the full stride size
    // So 3 * 8 + 4 = 28 <= 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 4);
        pass.End();
    }
    encoder.Finish();

    // Invalid: draw 4 instances with firstInstance == 1,
    // It requires a buffer with size of (3 + 1) * 8 + 4 = 36 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline1);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 4, 0, 1);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());

    // Invalid: draw 4 instances with offset == 4
    // It requires a buffer with size of 4 + 3 * 8 + 4 = 32 > 28
    encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline2);
        pass.SetVertexBuffer(0, vertexBuffer);
        pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32);
        pass.Draw(3, 4);
        pass.End();
    }
    ASSERT_DEVICE_ERROR(encoder.Finish());
}

// TODO: multiple attributes arraystride

// Test that we cannot set an already set attribute
TEST_F(VertexStateTest, AlreadySetAttribute) {
    // Control case, setting attribute 0
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].attributeCount = 1;
    state.cAttributes[0].shaderLocation = 0;
    CreatePipeline(true, state, kDummyVertexShader);

    // Oh no, attribute 0 is set twice
    state.cVertexBuffers[0].attributeCount = 2;
    state.cAttributes[0].shaderLocation = 0;
    state.cAttributes[1].shaderLocation = 0;
    CreatePipeline(false, state, kDummyVertexShader);
}

// Test that a arrayStride of 0 is valid
TEST_F(VertexStateTest, SetSameShaderLocation) {
    // Control case, setting different shader locations in two attributes
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].attributeCount = 2;
    state.cAttributes[0].shaderLocation = 0;
    state.cAttributes[1].shaderLocation = 1;
    state.cAttributes[1].offset = sizeof(float);
    CreatePipeline(true, state, kDummyVertexShader);

    // Test same shader location in two attributes in the same buffer
    state.cAttributes[1].shaderLocation = 0;
    CreatePipeline(false, state, kDummyVertexShader);

    // Test same shader location in two attributes in different buffers
    state.vertexBufferCount = 2;
    state.cVertexBuffers[0].attributeCount = 1;
    state.cAttributes[0].shaderLocation = 0;
    state.cVertexBuffers[1].attributeCount = 1;
    state.cVertexBuffers[1].attributes = &state.cAttributes[1];
    state.cAttributes[1].shaderLocation = 0;
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check out of bounds condition on attribute shader location
TEST_F(VertexStateTest, SetAttributeLocationOutOfBounds) {
    // Control case, setting last attribute shader location
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].attributeCount = 1;
    state.cAttributes[0].shaderLocation = kMaxVertexAttributes - 1;
    CreatePipeline(true, state, kDummyVertexShader);

    // Test attribute location OOB
    state.cAttributes[0].shaderLocation = kMaxVertexAttributes;
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check attribute offset out of bounds
TEST_F(VertexStateTest, SetAttributeOffsetOutOfBounds) {
    // Control case, setting max attribute offset for FloatR32 vertex format
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].attributeCount = 1;
    state.cAttributes[0].offset = kMaxVertexBufferArrayStride - sizeof(wgpu::VertexFormat::Float32);
    CreatePipeline(true, state, kDummyVertexShader);

    // Test attribute offset out of bounds
    state.cAttributes[0].offset = kMaxVertexBufferArrayStride - 1;
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check the min(4, formatSize) alignment constraint for the offset.
TEST_F(VertexStateTest, SetOffsetNotAligned) {
    // Control case, setting the offset at the correct alignments.
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].attributeCount = 1;

    // Test that for small formats, the offset must be aligned to the format size.
    state.cAttributes[0].format = wgpu::VertexFormat::Float32;
    state.cAttributes[0].offset = 4;
    CreatePipeline(true, state, kDummyVertexShader);
    state.cAttributes[0].offset = 2;
    CreatePipeline(false, state, kDummyVertexShader);

    state.cAttributes[0].format = wgpu::VertexFormat::Snorm16x2;
    state.cAttributes[0].offset = 4;
    CreatePipeline(true, state, kDummyVertexShader);
    state.cAttributes[0].offset = 2;
    CreatePipeline(false, state, kDummyVertexShader);

    state.cAttributes[0].format = wgpu::VertexFormat::Unorm8x2;
    state.cAttributes[0].offset = 2;
    CreatePipeline(true, state, kDummyVertexShader);
    state.cAttributes[0].offset = 1;
    CreatePipeline(false, state, kDummyVertexShader);

    // Test that for large formts the offset only needs to be aligned to 4.
    state.cAttributes[0].format = wgpu::VertexFormat::Snorm16x4;
    state.cAttributes[0].offset = 4;
    CreatePipeline(true, state, kDummyVertexShader);

    state.cAttributes[0].format = wgpu::VertexFormat::Uint32x3;
    state.cAttributes[0].offset = 4;
    CreatePipeline(true, state, kDummyVertexShader);

    state.cAttributes[0].format = wgpu::VertexFormat::Sint32x4;
    state.cAttributes[0].offset = 4;
    CreatePipeline(true, state, kDummyVertexShader);
}

// Check attribute offset overflow
TEST_F(VertexStateTest, SetAttributeOffsetOverflow) {
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].attributeCount = 1;
    state.cAttributes[0].offset = std::numeric_limits<uint32_t>::max();
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check for some potential underflow in the vertex input validation
TEST_F(VertexStateTest, VertexFormatLargerThanNonZeroStride) {
    utils::ComboVertexState state;
    state.vertexBufferCount = 1;
    state.cVertexBuffers[0].arrayStride = 4;
    state.cVertexBuffers[0].attributeCount = 1;
    state.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
    CreatePipeline(false, state, kDummyVertexShader);
}

// Check that the vertex format base type must match the shader's variable base type.
TEST_F(VertexStateTest, BaseTypeMatching) {
    auto DoTest = [&](wgpu::VertexFormat format, std::string shaderType, bool success) {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 16;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].format = format;

        std::string shader = "@stage(vertex) fn main(@location(0) attrib : " + shaderType +
                             R"() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        })";

        CreatePipeline(success, state, shader.c_str());
    };

    // Test that a float format is compatible only with f32 base type.
    DoTest(wgpu::VertexFormat::Float32, "f32", true);
    DoTest(wgpu::VertexFormat::Float32, "i32", false);
    DoTest(wgpu::VertexFormat::Float32, "u32", false);

    // Test that an unorm format is compatible only with f32.
    DoTest(wgpu::VertexFormat::Unorm16x2, "f32", true);
    DoTest(wgpu::VertexFormat::Unorm16x2, "i32", false);
    DoTest(wgpu::VertexFormat::Unorm16x2, "u32", false);

    // Test that an snorm format is compatible only with f32.
    DoTest(wgpu::VertexFormat::Snorm16x4, "f32", true);
    DoTest(wgpu::VertexFormat::Snorm16x4, "i32", false);
    DoTest(wgpu::VertexFormat::Snorm16x4, "u32", false);

    // Test that an uint format is compatible only with u32.
    DoTest(wgpu::VertexFormat::Uint32x3, "f32", false);
    DoTest(wgpu::VertexFormat::Uint32x3, "i32", false);
    DoTest(wgpu::VertexFormat::Uint32x3, "u32", true);

    // Test that an sint format is compatible only with u32.
    DoTest(wgpu::VertexFormat::Sint8x4, "f32", false);
    DoTest(wgpu::VertexFormat::Sint8x4, "i32", true);
    DoTest(wgpu::VertexFormat::Sint8x4, "u32", false);

    // Test that formats are compatible with any width of vectors.
    DoTest(wgpu::VertexFormat::Float32, "f32", true);
    DoTest(wgpu::VertexFormat::Float32, "vec2<f32>", true);
    DoTest(wgpu::VertexFormat::Float32, "vec3<f32>", true);
    DoTest(wgpu::VertexFormat::Float32, "vec4<f32>", true);

    DoTest(wgpu::VertexFormat::Float32x4, "f32", true);
    DoTest(wgpu::VertexFormat::Float32x4, "vec2<f32>", true);
    DoTest(wgpu::VertexFormat::Float32x4, "vec3<f32>", true);
    DoTest(wgpu::VertexFormat::Float32x4, "vec4<f32>", true);
}

// Check that we only check base type compatibility for vertex inputs the shader uses.
TEST_F(VertexStateTest, BaseTypeMatchingForInexistentInput) {
    auto DoTest = [&](wgpu::VertexFormat format) {
        utils::ComboVertexState state;
        state.vertexBufferCount = 1;
        state.cVertexBuffers[0].arrayStride = 16;
        state.cVertexBuffers[0].attributeCount = 1;
        state.cAttributes[0].format = format;

        std::string shader = R"(@stage(vertex) fn main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        })";

        CreatePipeline(true, state, shader.c_str());
    };

    DoTest(wgpu::VertexFormat::Float32);
    DoTest(wgpu::VertexFormat::Unorm16x2);
    DoTest(wgpu::VertexFormat::Snorm16x4);
    DoTest(wgpu::VertexFormat::Uint8x4);
    DoTest(wgpu::VertexFormat::Sint32x2);
}
