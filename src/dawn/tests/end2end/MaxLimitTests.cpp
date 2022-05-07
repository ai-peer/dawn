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

#include <algorithm>
#include <limits>
#include <string>

#include "dawn/common/Math.h"
#include "dawn/common/Platform.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

class MaxLimitTests : public DawnTest {
  public:
    wgpu::RequiredLimits GetRequiredLimits(const wgpu::SupportedLimits& supported) override {
        wgpu::RequiredLimits required = {};
        required.limits = supported.limits;
        return required;
    }
};

// Test using the maximum amount of workgroup memory works
TEST_P(MaxLimitTests, MaxComputeWorkgroupStorageSize) {
    uint32_t maxComputeWorkgroupStorageSize =
        GetSupportedLimits().limits.maxComputeWorkgroupStorageSize;

    std::string shader = R"(
        struct Dst {
            value0 : u32,
            value1 : u32,
        }

        @group(0) @binding(0) var<storage, write> dst : Dst;

        struct WGData {
          value0 : u32,
          // padding such that value0 and value1 are the first and last bytes of the memory.
          @size()" + std::to_string(maxComputeWorkgroupStorageSize / 4 - 2) +
                         R"() padding : u32,
          value1 : u32,
        }
        var<workgroup> wg_data : WGData;

        @stage(compute) @workgroup_size(2,1,1)
        fn main(@builtin(local_invocation_index) LocalInvocationIndex : u32) {
            if (LocalInvocationIndex == 0u) {
                // Put data into the first and last byte of workgroup memory.
                wg_data.value0 = 79u;
                wg_data.value1 = 42u;
            }

            workgroupBarrier();

            if (LocalInvocationIndex == 1u) {
                // Read data out of workgroup memory into a storage buffer.
                dst.value0 = wg_data.value0;
                dst.value1 = wg_data.value1;
            }
        }
    )";
    wgpu::ComputePipelineDescriptor csDesc;
    csDesc.compute.module = utils::CreateShaderModule(device, shader.c_str());
    csDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

    // Set up dst storage buffer
    wgpu::BufferDescriptor dstDesc;
    dstDesc.size = 8;
    dstDesc.usage =
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer dst = device.CreateBuffer(&dstDesc);

    // Set up bind group and issue dispatch
    wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                     {
                                                         {0, dst},
                                                     });

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.DispatchWorkgroups(1);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    EXPECT_BUFFER_U32_EQ(79, dst, 0);
    EXPECT_BUFFER_U32_EQ(42, dst, 4);
}

// Test using the maximum uniform/storage buffer binding size works
TEST_P(MaxLimitTests, MaxBufferBindingSize) {
    // The uniform buffer layout used in this test is not supported on ES.
    DAWN_TEST_UNSUPPORTED_IF(IsOpenGLES());

    // TODO(crbug.com/dawn/1172)
    DAWN_SUPPRESS_TEST_IF(IsWindows() && IsVulkan() && IsIntel());

    // TODO(crbug.com/dawn/1217): Remove this suppression.
    DAWN_SUPPRESS_TEST_IF(IsWindows() && IsVulkan() && IsNvidia());

    for (wgpu::BufferUsage usage : {wgpu::BufferUsage::Storage, wgpu::BufferUsage::Uniform}) {
        uint64_t maxBufferBindingSize;
        std::string shader;
        switch (usage) {
            case wgpu::BufferUsage::Storage:
                maxBufferBindingSize = GetSupportedLimits().limits.maxStorageBufferBindingSize;
                // TODO(crbug.com/dawn/1160): Usually can't actually allocate a buffer this large
                // because allocating the buffer for zero-initialization fails.
                maxBufferBindingSize =
                    std::min(maxBufferBindingSize, uint64_t(2) * 1024 * 1024 * 1024);
                // With WARP or on 32-bit platforms, such large buffer allocations often fail.
#ifdef DAWN_PLATFORM_32_BIT
                if (IsWindows()) {
                    continue;
                }
#endif
                if (IsWARP()) {
                    maxBufferBindingSize =
                        std::min(maxBufferBindingSize, uint64_t(512) * 1024 * 1024);
                }
                shader = R"(
                  struct Buf {
                      values : array<u32>
                  }

                  struct Result {
                      value0 : u32,
                      value1 : u32,
                  }

                  @group(0) @binding(0) var<storage, read> buf : Buf;
                  @group(0) @binding(1) var<storage, write> result : Result;

                  @stage(compute) @workgroup_size(1,1,1)
                  fn main() {
                      result.value0 = buf.values[0];
                      result.value1 = buf.values[arrayLength(&buf.values) - 1u];
                  }
              )";
                break;
            case wgpu::BufferUsage::Uniform:
                maxBufferBindingSize = GetSupportedLimits().limits.maxUniformBufferBindingSize;

                // Clamp to not exceed the maximum i32 value for the WGSL @size(x) annotation.
                maxBufferBindingSize = std::min(maxBufferBindingSize,
                                                uint64_t(std::numeric_limits<int32_t>::max()) + 8);

                shader = R"(
                  struct Buf {
                      value0 : u32,
                      // padding such that value0 and value1 are the first and last bytes of the memory.
                      @size()" +
                         std::to_string(maxBufferBindingSize - 8) + R"() padding : u32,
                      value1 : u32,
                  }

                  struct Result {
                      value0 : u32,
                      value1 : u32,
                  }

                  @group(0) @binding(0) var<uniform> buf : Buf;
                  @group(0) @binding(1) var<storage, write> result : Result;

                  @stage(compute) @workgroup_size(1,1,1)
                  fn main() {
                      result.value0 = buf.value0;
                      result.value1 = buf.value1;
                  }
              )";
                break;
            default:
                UNREACHABLE();
        }

        device.PushErrorScope(wgpu::ErrorFilter::OutOfMemory);

        wgpu::BufferDescriptor bufDesc;
        bufDesc.size = Align(maxBufferBindingSize, 4);
        bufDesc.usage = usage | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer buffer = device.CreateBuffer(&bufDesc);

        WGPUErrorType oomResult;
        device.PopErrorScope([](WGPUErrorType type, const char*,
                                void* userdata) { *static_cast<WGPUErrorType*>(userdata) = type; },
                             &oomResult);
        FlushWire();
        // Max buffer size is smaller than the max buffer binding size.
        DAWN_TEST_UNSUPPORTED_IF(oomResult == WGPUErrorType_OutOfMemory);

        wgpu::BufferDescriptor resultBufDesc;
        resultBufDesc.size = 8;
        resultBufDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
        wgpu::Buffer resultBuffer = device.CreateBuffer(&resultBufDesc);

        uint32_t value0 = 89234;
        queue.WriteBuffer(buffer, 0, &value0, sizeof(value0));

        uint32_t value1 = 234;
        uint64_t value1Offset = Align(maxBufferBindingSize - sizeof(value1), 4);
        queue.WriteBuffer(buffer, value1Offset, &value1, sizeof(value1));

        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = utils::CreateShaderModule(device, shader.c_str());
        csDesc.compute.entryPoint = "main";
        wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&csDesc);

        wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, pipeline.GetBindGroupLayout(0),
                                                         {{0, buffer}, {1, resultBuffer}});

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);
        pass.DispatchWorkgroups(1);
        pass.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        EXPECT_BUFFER_U32_EQ(value0, resultBuffer, 0)
            << "maxBufferBindingSize=" << maxBufferBindingSize << "; offset=" << 0
            << "; usage=" << usage;
        EXPECT_BUFFER_U32_EQ(value1, resultBuffer, 4)
            << "maxBufferBindingSize=" << maxBufferBindingSize << "; offset=" << value1Offset
            << "; usage=" << usage;
    }
}

DAWN_INSTANTIATE_TEST(MaxLimitTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());

// Tested on Nvidia 2080Ti
// maxInterStageShaderComponents == 128 on D3D12 and Vulkan

class MaxInterStageShaderComponentsTests : public MaxLimitTests {
  public:
    void SetUp() {
        MaxLimitTests::SetUp();
        device.SetUncapturedErrorCallback(MaxInterStageShaderComponentsTests::OnDeviceError,
                                          nullptr);
    }

    static void OnDeviceError(WGPUErrorType type, const char* message, void* userdata) {
        ASSERT(type != WGPUErrorType_NoError);
        FAIL() << "Unexpected error: " << message;
    }

    wgpu::RenderPipeline CreateRenderPipeline(const std::string& vertexShader,
                                              const std::string& fragmentShader) {
        wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, vertexShader.c_str());
        wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, fragmentShader.c_str());

        utils::ComboRenderPipelineDescriptor descriptor;
        descriptor.vertex.module = vsModule;
        descriptor.cFragment.module = fsModule;
        descriptor.vertex.bufferCount = 0;
        descriptor.cBuffers[0].attributeCount = 0;
        descriptor.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;
        descriptor.primitive.topology = wgpu::PrimitiveTopology::LineList;
        return device.CreateRenderPipeline(&descriptor);
    }
};

// #1: 30x vec4<f32> + vec2<f32> + f32 (+ position + "PointSize" in vertex shader)
// D3D12: Pass
// Vulkan: Pass
TEST_P(MaxInterStageShaderComponentsTests, vec4x30_vec2_f32) {
    DAWN_TEST_UNSUPPORTED_IF(IsSwiftshader() || IsWARP() || IsANGLE());

    uint32_t maxInterStageComponentCount =
        GetSupportedLimits().limits.maxInterStageShaderComponents;
    DAWN_TEST_UNSUPPORTED_IF(maxInterStageComponentCount != 128);

    constexpr uint32_t kVec4Count = 30;

    std::stringstream stream;
    for (uint32_t i = 0; i < kVec4Count; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec4<f32>, " << std::endl;
    }
    stream << "    @location(30) color30 : vec2<f32>, " << std::endl
           << "    @location(31) color31 : f32, " << std::endl;

    std::string vertexShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
            @builtin(position) position : vec4<f32>,
        }
        @stage(vertex)
        fn main(@builtin(vertex_index) vertexIndex : u32) -> VertexOut {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 2.0,  0.0),
                vec2<f32>( 0.0,  2.0));
            var output : VertexOut;
            output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
            return output;
        })";

    std::string fragmentShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
        }
        @stage(fragment)
        fn main(input: VertexOut) -> @location(0) vec4<f32> {
            return input.color0;
        })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    EXPECT_NE(nullptr, pipeline.Get());
}

// #2: 28x vec4<f32> + vec2<f32> + 4x f32 (+ position + "PointSize" in vertex shader)
// D3D12: Pass
// Vulkan: Error [ VUID-RuntimeSpirv-Location-06272 ]
// Vertex shader output variable uses location that exceeds component limit
// VkPhysicalDeviceLimits::maxVertexOutputComponents (128)
// Fragment shader input variable uses location that exceeds component limit
// VkPhysicalDeviceLimits::maxFragmentInputComponents (128)
TEST_P(MaxInterStageShaderComponentsTests, vec4x28_vec2_f32x4) {
    DAWN_TEST_UNSUPPORTED_IF(IsSwiftshader() || IsWARP() || IsANGLE());

    uint32_t maxInterStageComponentCount =
        GetSupportedLimits().limits.maxInterStageShaderComponents;
    DAWN_TEST_UNSUPPORTED_IF(maxInterStageComponentCount != 128);

    constexpr uint32_t kVec4Count = 28;

    std::stringstream stream;
    for (uint32_t i = 0; i < kVec4Count; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec4<f32>, " << std::endl;
    }
    stream << "    @location(28) color28 : vec2<f32>, " << std::endl
           << "    @location(29) color29 : f32, " << std::endl
           << "    @location(30) color30 : f32, " << std::endl
           << "    @location(31) color31 : f32, " << std::endl
           << "    @location(32) color32 : f32, " << std::endl;

    std::string vertexShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
            @builtin(position) position : vec4<f32>,
        }
        @stage(vertex)
        fn main(@builtin(vertex_index) vertexIndex : u32) -> VertexOut {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 2.0,  0.0),
                vec2<f32>( 0.0,  2.0));
            var output : VertexOut;
            output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
            return output;
        })";

    std::string fragmentShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
        }
        @stage(fragment)
        fn main(input: VertexOut) -> @location(0) vec4<f32> {
            return input.color0;
        })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    EXPECT_NE(nullptr, pipeline.Get());
}

// #3: 27x vec4<f32> + 5x vec3<f32> (+ position + "PointSize" in vertex shader)
// D3D12: error X4571: vs_5_1 output limit (32) exceeded, shader uses 33 outputs.
// Vulkan: Pass
TEST_P(MaxInterStageShaderComponentsTests, vec4x27_vec3x5) {
    DAWN_TEST_UNSUPPORTED_IF(IsSwiftshader() || IsWARP() || IsANGLE());

    uint32_t maxInterStageComponentCount =
        GetSupportedLimits().limits.maxInterStageShaderComponents;
    DAWN_TEST_UNSUPPORTED_IF(maxInterStageComponentCount != 128);

    constexpr uint32_t kVec4Count = 27;

    std::stringstream stream;
    for (uint32_t i = 0; i < kVec4Count; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec4<f32>, " << std::endl;
    }
    for (uint32_t i = kVec4Count; i < maxInterStageComponentCount / 4; ++i) {
        stream << "    @location(" << i << ") color" << i << " : vec3<f32>, " << std::endl;
    }

    std::string vertexShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
            @builtin(position) position : vec4<f32>,
        }
        @stage(vertex)
        fn main(@builtin(vertex_index) vertexIndex : u32) -> VertexOut {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 2.0,  0.0),
                vec2<f32>( 0.0,  2.0));
            var output : VertexOut;
            output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
            return output;
        })";

    std::string fragmentShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
        }
        @stage(fragment)
        fn main(input: VertexOut) -> @location(0) vec4<f32> {
            return input.color0;
        })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    EXPECT_NE(nullptr, pipeline.Get());
}

// #5: 31x vec4<f32> + sample_mask (+ position + "PointSize" in vertex shader)
// D3D12: Pass
// Vulkan: [ VUID-RuntimeSpirv-Location-06272 ]
// Vertex shader exceeds VkPhysicalDeviceLimits::maxVertexOutputComponents of 128 components by 1
// components
TEST_P(MaxInterStageShaderComponentsTests, vec4x31_sample_mask) {
    DAWN_TEST_UNSUPPORTED_IF(IsSwiftshader() || IsWARP() || IsANGLE());

    uint32_t maxInterStageComponentCount =
        GetSupportedLimits().limits.maxInterStageShaderComponents;
    DAWN_TEST_UNSUPPORTED_IF(maxInterStageComponentCount != 128);

    constexpr uint32_t kVec4Count = 31;

    std::stringstream stream;
    for (uint32_t i = 0; i < kVec4Count; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec4<f32>, " << std::endl;
    }

    std::string vertexShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
            @builtin(position) position : vec4<f32>,
        }
        @stage(vertex)
        fn main(@builtin(vertex_index) vertexIndex : u32) -> VertexOut {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 2.0,  0.0),
                vec2<f32>( 0.0,  2.0));
            var output : VertexOut;
            output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
            return output;
        })";

    std::string fragmentShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
            @builtin(sample_mask) sample_mask : u32,
        }
        @stage(fragment)
        fn main(input: VertexOut) -> @location(0) vec4<f32> {
            return input.color0;
        })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    EXPECT_NE(nullptr, pipeline.Get());
}

// #6: 27x vec4<f32> + 4x vec3<f32> + position + front_facing (+ "PointSize" in vertex shader)
// D3D12: error X4506: ps_5_1 input limit (32) exceeded, shader uses 33 inputs.
// Vulkan: Pass
TEST_P(MaxInterStageShaderComponentsTests, vec4x27_vec3x4_position_front_facing) {
    DAWN_TEST_UNSUPPORTED_IF(IsSwiftshader() || IsWARP() || IsANGLE());

    uint32_t maxInterStageComponentCount =
        GetSupportedLimits().limits.maxInterStageShaderComponents;
    DAWN_TEST_UNSUPPORTED_IF(maxInterStageComponentCount != 128);

    constexpr uint32_t kVec4Count = 27;

    std::stringstream stream;
    for (uint32_t i = 0; i < kVec4Count; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec4<f32>, " << std::endl;
    }
    for (uint32_t i = kVec4Count; i < 31; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec3<f32>, " << std::endl;
    }
    stream << "   @builtin(position) position : vec4<f32>," << std::endl;

    std::string vertexShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
        }
        @stage(vertex)
        fn main(@builtin(vertex_index) vertexIndex : u32) -> VertexOut {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 2.0,  0.0),
                vec2<f32>( 0.0,  2.0));
            var output : VertexOut;
            output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
            return output;
        })";

    std::string fragmentShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
            @builtin(front_facing) frontFacing : bool,
        }
        @stage(fragment)
        fn main(input: VertexOut) -> @location(0) vec4<f32> {
            return input.color0;
        })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    EXPECT_NE(nullptr, pipeline.Get());
}

// #7: 27x vec4<f32> + 4x vec3<f32> + position + sample_index (+ "PointSize" in vertex shader)
// D3D12: error X4506: ps_5_1 input limit (32) exceeded, shader uses 33 inputs.
// Vulkan: Pass
TEST_P(MaxInterStageShaderComponentsTests, vec4x27_vec3x4_position_sample_index) {
    DAWN_TEST_UNSUPPORTED_IF(IsSwiftshader() || IsWARP() || IsANGLE());

    uint32_t maxInterStageComponentCount =
        GetSupportedLimits().limits.maxInterStageShaderComponents;
    DAWN_TEST_UNSUPPORTED_IF(maxInterStageComponentCount != 128);

    constexpr uint32_t kVec4Count = 27;

    std::stringstream stream;
    for (uint32_t i = 0; i < kVec4Count; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec4<f32>, " << std::endl;
    }
    for (uint32_t i = kVec4Count; i < 31; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec3<f32>, " << std::endl;
    }
    stream << "   @builtin(position) position : vec4<f32>," << std::endl;

    std::string vertexShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
        }
        @stage(vertex)
        fn main(@builtin(vertex_index) vertexIndex : u32) -> VertexOut {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 2.0,  0.0),
                vec2<f32>( 0.0,  2.0));
            var output : VertexOut;
            output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
            return output;
        })";

    std::string fragmentShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
            @builtin(sample_index) sampleIndex : u32,
        }
        @stage(fragment)
        fn main(input: VertexOut) -> @location(0) vec4<f32> {
            return input.color0;
        })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    EXPECT_NE(nullptr, pipeline.Get());
}

// #8: 30x vec4<f32> + position + front_facing + sample_index + sample_mask (+ "PointSize" in vertex
// shader)
// D3D12: Pass
// Vulkan: Pass
TEST_P(MaxInterStageShaderComponentsTests, vec4x30_position_front_facing_sample_index_sample_mask) {
    DAWN_TEST_UNSUPPORTED_IF(IsSwiftshader() || IsWARP() || IsANGLE());

    uint32_t maxInterStageComponentCount =
        GetSupportedLimits().limits.maxInterStageShaderComponents;
    DAWN_TEST_UNSUPPORTED_IF(maxInterStageComponentCount != 128);

    constexpr uint32_t kVec4Count = 30;

    std::stringstream stream;
    for (uint32_t i = 0; i < kVec4Count; ++i) {
        stream << "    @location(" << i << ") color" << i << ": vec4<f32>, " << std::endl;
    }
    stream << "   @builtin(position) position : vec4<f32>," << std::endl;

    std::string vertexShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
        }
        @stage(vertex)
        fn main(@builtin(vertex_index) vertexIndex : u32) -> VertexOut {
            var pos = array<vec2<f32>, 3>(
                vec2<f32>(-1.0, -1.0),
                vec2<f32>( 2.0,  0.0),
                vec2<f32>( 0.0,  2.0));
            var output : VertexOut;
            output.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
            return output;
        })";

    std::string fragmentShader = R"(
        struct VertexOut {
)" + stream.str() + R"(
            @builtin(front_facing) frontFacing: bool,
            @builtin(sample_index) sampleIndex : u32,
            @builtin(sample_mask) sampleMask: u32,
        }
        @stage(fragment)
        fn main(input: VertexOut) -> @location(0) vec4<f32> {
            return input.color0;
        })";

    wgpu::RenderPipeline pipeline = CreateRenderPipeline(vertexShader, fragmentShader);
    EXPECT_NE(nullptr, pipeline.Get());
}

DAWN_INSTANTIATE_TEST(MaxInterStageShaderComponentsTests,
                      D3D12Backend(),
                      MetalBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend(),
                      VulkanBackend());
