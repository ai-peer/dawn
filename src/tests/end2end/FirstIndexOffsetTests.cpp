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

constexpr uint32_t kRTSize = 4;

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

        [[location(0)]] var<out> frag_color : vec4<f32>;
        [[builtin(frag_coord)]] var<in> frag_coord : vec4<f32>;
        [[stage(fragment)]] fn main() -> void  {
            # Bottom-left pixel
            if (frag_coord.x == 0.5 && frag_coord.y == ()"
                   << kRTSize << R"(.0 - 0.5)) {
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
            }
            frag_color = vec4<f32>(1, 0, 0, 1);
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
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
    std::vector<float> vertexData(vertexIndex * kComponentsPerVertex);
    vertexData.insert(vertexData.end(),
                      {// The bottom left triangle
                       -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f});

    wgpu::Buffer vertices = utils::CreateBufferFromData(
        device, vertexData.data(), vertexData.size() * sizeof(float), wgpu::BufferUsage::Vertex);
    wgpu::Buffer buffer = utils::CreateBufferFromData(
        device, wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::Storage, {0u, 0u});
    wgpu::Buffer indices =
        utils::CreateBufferFromData<uint32_t>(device, wgpu::BufferUsage::Index, {0, 1, 2, 3, 4, 5});
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

constexpr uint32_t kVertexIndexOffset = 7;
constexpr uint32_t kInstanceIndexOffset = 11;

TEST_P(FirstIndexOffsetTests, NonIndexedVertexOffset) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestVertexIndex(DrawMode::NonIndexed, kVertexIndexOffset);
}

TEST_P(FirstIndexOffsetTests, NonIndexedInstanceOffset) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestInstanceIndex(DrawMode::NonIndexed, kInstanceIndexOffset);
}

TEST_P(FirstIndexOffsetTests, NonIndexedBothOffset) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestBothIndices(DrawMode::NonIndexed, kVertexIndexOffset, kInstanceIndexOffset);
}

TEST_P(FirstIndexOffsetTests, IndexedVertex) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestVertexIndex(DrawMode::Indexed, kVertexIndexOffset);
}

TEST_P(FirstIndexOffsetTests, IndexedInstance) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestInstanceIndex(DrawMode::Indexed, kInstanceIndexOffset);
}

TEST_P(FirstIndexOffsetTests, IndexedBothOffset) {
    // WGSL doesn't have the ability to tag attributes as "flat". "flat" is required on u32
    // attributes for correct runtime behavior under Vulkan and codegen under OpenGL(ES).
    DAWN_SKIP_TEST_IF(IsVulkan() || IsOpenGL() || IsOpenGLES());
    TestBothIndices(DrawMode::Indexed, kVertexIndexOffset, kInstanceIndexOffset);
}

DAWN_INSTANTIATE_TEST(FirstIndexOffsetTests,
                      D3D12Backend({"use_tint_generator"}),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
