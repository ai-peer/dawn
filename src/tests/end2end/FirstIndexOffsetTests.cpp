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

#include <sstream>
#include <vector>

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/WGPUHelpers.h"

constexpr uint32_t kRTSize = 1;

enum class DrawMode {
    NonIndexed,
    Indexed,
};

enum class CheckIndex : uint32_t {
    Vertex = 0x0000001,
    Instance = 0x0000002,
};

namespace wgpu {
    template <>
    struct IsDawnBitmask<CheckIndex> {
        static constexpr bool enable = true;
    };
}  // namespace wgpu

class FirstIndexOffsetTests : public DawnTest {
  public:
    void TestVertexIndex(DrawMode mode, uint32_t vertexIndex);
    void TestInstanceIndex(DrawMode mode, uint32_t instanceIndex);
    void TestBothIndices(DrawMode mode, uint32_t vertexIndex, uint32_t instanceIndex);

  private:
    void TestImpl(DrawMode mode,
                  CheckIndex checkIndex,
                  uint32_t vertexIndex,
                  uint32_t instanceIndex);
};

void FirstIndexOffsetTests::TestVertexIndex(DrawMode mode, uint32_t vertexIndex) {
    TestImpl(mode, CheckIndex::Vertex, vertexIndex, 0);
}

void FirstIndexOffsetTests::TestInstanceIndex(DrawMode mode, uint32_t instanceIndex) {
    TestImpl(mode, CheckIndex::Instance, 0, instanceIndex);
}

void FirstIndexOffsetTests::TestBothIndices(DrawMode mode,
                                            uint32_t vertexIndex,
                                            uint32_t instanceIndex) {
    using wgpu::operator|;
    TestImpl(mode, CheckIndex::Vertex | CheckIndex::Instance, vertexIndex, instanceIndex);
}

// Conditionally tests if first/baseVertex and/or firstInstance have been correctly passed to the
// vertex shader. Since vertex shaders can't write to storage buffers, we pass vertex/instance
// indices to a fragment shader via u32 attributes. The fragment shader runs once and writes the
// values. In order to support DrawIndexed, TriangleStrip is used. A single oversized triangle on a
// 1x1 framebuffer.
// (-1, 2)
//         |\
//         |  \
//         |___ \ (1, 1)
//     1px |   |  \
//         |___|____\
// (-1, -1) 1px       (2, -1)
//
// If vertex index is used, the vertex buffer is padded with 0 so that the same triangle is always
// drawn.
void FirstIndexOffsetTests::TestImpl(DrawMode mode,
                                     CheckIndex checkIndex,
                                     uint32_t vertexIndex,
                                     uint32_t instanceIndex) {
    using wgpu::operator&;
    std::stringstream vertexShader;
    std::stringstream fragmentShader;
    uint32_t location = 1;
    if ((checkIndex & CheckIndex::Vertex) != 0) {
        vertexShader << R"(
        [[builtin(vertex_idx)]] var<in> vertex_idx : u32;
        [[location()" << location
                     << R"()]] var<out> out_vertex_idx : u32;
        )";
        fragmentShader << R"(
        [[location()" << location
                       << R"()]] var<in> in_vertex_idx : u32;
    )";
        ++location;
    }
    if ((checkIndex & CheckIndex::Instance) != 0) {
        vertexShader << R"(
            [[builtin(instance_idx)]] var<in> instance_idx : u32;
            [[location()"
                     << location << R"()]] var<out> out_instance_idx : u32;
            )";
        fragmentShader << R"(
            [[location()"
                       << location << R"()]] var<in> in_instance_idx : u32;
        )";
        ++location;
    }

    vertexShader << R"(
        [[builtin(position)]] var<out> position : vec4<f32>;
        [[location(0)]] var<in> pos : vec4<f32>;

        [[stage(vertex)]] fn main() -> void {)";
    fragmentShader << R"(
         [[block]] struct IndexVals {
             [[offset(0)]] vertex_idx : u32;
             [[offset(4)]] instance_idx : u32;
         };

        [[set(0), binding(0)]] var<storage_buffer> idx_vals : [[access(read_write)]] IndexVals;

        [[stage(fragment)]] fn main() -> void  {
        )";

    if ((checkIndex & CheckIndex::Vertex) != 0) {
        vertexShader << R"(
            out_vertex_idx = vertex_idx;
            )";
        fragmentShader << R"(
            idx_vals.vertex_idx = in_vertex_idx;
            )";
    }
    if ((checkIndex & CheckIndex::Instance) != 0) {
        vertexShader << R"(
            out_instance_idx = instance_idx;
            )";
        fragmentShader << R"(
            idx_vals.instance_idx = in_instance_idx;
            )";
    }

    vertexShader << R"(
            position = pos;
            return;
        })";

    fragmentShader << R"(
            return;
        })";

    utils::BasicRenderPass renderPass = utils::CreateBasicRenderPass(device, kRTSize, kRTSize);

    constexpr uint32_t kComponentsPerVertex = 4;

    utils::ComboRenderPipelineDescriptor pipelineDesc(device);
    pipelineDesc.vertexStage.module =
        utils::CreateShaderModuleFromWGSL(device, vertexShader.str().c_str());
    pipelineDesc.cFragmentStage.module =
        utils::CreateShaderModuleFromWGSL(device, fragmentShader.str().c_str());
    pipelineDesc.primitiveTopology = wgpu::PrimitiveTopology::TriangleStrip;
    pipelineDesc.cVertexState.indexFormat = wgpu::IndexFormat::Uint32;
    pipelineDesc.cVertexState.vertexBufferCount = 1;
    pipelineDesc.cVertexState.cVertexBuffers[0].arrayStride = kComponentsPerVertex * sizeof(float);
    pipelineDesc.cVertexState.cVertexBuffers[0].attributeCount = 1;
    pipelineDesc.cVertexState.cAttributes[0].format = wgpu::VertexFormat::Float4;
    pipelineDesc.cColorStates[0].format = renderPass.colorFormat;

    wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDesc);

    std::vector<float> vertexData(vertexIndex * kComponentsPerVertex);
    vertexData.insert(vertexData.end(),
                      {// Large triangle to cover viewport
                       -1.0f, 2.0f, 0.0f, 1.0f, 2.0f, -1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f});
    wgpu::Buffer vertices = utils::CreateBufferFromData(
        device, vertexData.data(), vertexData.size() * sizeof(float), wgpu::BufferUsage::Vertex);
    wgpu::Buffer indices =
        utils::CreateBufferFromData<uint32_t>(device, wgpu::BufferUsage::Index, {0, 1, 2, 3, 4, 5});
    wgpu::Buffer buffer = utils::CreateBufferFromData(
        device, wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::Storage, {0u, 0u});

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    pass.SetPipeline(pipeline);
    pass.SetVertexBuffer(0, vertices);
    pass.SetBindGroup(0, utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                              {{0, buffer, 0, sizeof(uint32_t) * 2}}));
    if (mode == DrawMode::Indexed) {
        pass.SetIndexBuffer(indices, pipelineDesc.cVertexState.indexFormat);
        pass.DrawIndexed(3, 1, 0, vertexIndex, instanceIndex);

    } else {
        pass.Draw(3, 1, vertexIndex, instanceIndex);
    }
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    std::array<uint32_t, 2> expected = {vertexIndex, instanceIndex};
    EXPECT_BUFFER_U32_RANGE_EQ(expected.data(), buffer, 0, expected.size());
}

// Test that vertex_index starts at kVertexIndexOffset when drawn using Draw()
TEST_P(FirstIndexOffsetTests, NonIndexedVertexOffset) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestVertexIndex(DrawMode::NonIndexed, 7);
}

// Test that instance_index starts at kInstanceIndexOffset when drawn using Draw()
TEST_P(FirstIndexOffsetTests, NonIndexedInstanceOffset) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestInstanceIndex(DrawMode::NonIndexed, 11);
}

// Test that vertex_index and instance_index start at kVertexIndexOffset and kInstanceIndexOffset
// respectively when drawn using Draw()
TEST_P(FirstIndexOffsetTests, NonIndexedBothOffset) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestBothIndices(DrawMode::NonIndexed, 7, 11);
}

// Test that vertex_index starts at kVertexIndexOffset when drawn using DrawIndexed()
TEST_P(FirstIndexOffsetTests, IndexedVertex) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestVertexIndex(DrawMode::Indexed, 7);
}

// Test that instance_index starts at kInstanceIndexOffset when drawn using DrawIndexed()
TEST_P(FirstIndexOffsetTests, IndexedInstance) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestInstanceIndex(DrawMode::Indexed, 11);
}

// Test that vertex_index and instance_index start at kVertexIndexOffset and kInstanceIndexOffset
// respectively when drawn using DrawIndexed()
TEST_P(FirstIndexOffsetTests, IndexedBothOffset) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestBothIndices(DrawMode::Indexed, 7, 11);
}

DAWN_INSTANTIATE_TEST(FirstIndexOffsetTests,
                      D3D12Backend({"use_tint_generator"}),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
