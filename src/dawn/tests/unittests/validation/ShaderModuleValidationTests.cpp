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

#include <sstream>
#include <string>

#include "dawn/common/Constants.h"
#include "dawn/native/ShaderModule.h"
#include "dawn/tests/unittests/validation/ValidationTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

class ShaderModuleValidationTest : public ValidationTest {};

// Test case with a simpler shader that should successfully be created
TEST_F(ShaderModuleValidationTest, CreationSuccess) {
    const char* shader = R"(
                   OpCapability Shader
              %1 = OpExtInstImport "GLSL.std.450"
                   OpMemoryModel Logical GLSL450
                   OpEntryPoint Fragment %main "main" %fragColor
                   OpExecutionMode %main OriginUpperLeft
                   OpSource GLSL 450
                   OpSourceExtension "GL_GOOGLE_cpp_style_line_directive"
                   OpSourceExtension "GL_GOOGLE_include_directive"
                   OpName %main "main"
                   OpName %fragColor "fragColor"
                   OpDecorate %fragColor Location 0
           %void = OpTypeVoid
              %3 = OpTypeFunction %void
          %float = OpTypeFloat 32
        %v4float = OpTypeVector %float 4
    %_ptr_Output_v4float = OpTypePointer Output %v4float
      %fragColor = OpVariable %_ptr_Output_v4float Output
        %float_1 = OpConstant %float 1
        %float_0 = OpConstant %float 0
             %12 = OpConstantComposite %v4float %float_1 %float_0 %float_0 %float_1
           %main = OpFunction %void None %3
              %5 = OpLabel
                   OpStore %fragColor %12
                   OpReturn
                   OpFunctionEnd)";

    utils::CreateShaderModuleFromASM(device, shader);
}

// Tests that if the output location exceeds kMaxColorAttachments the fragment shader will fail to
// be compiled.
TEST_F(ShaderModuleValidationTest, FragmentOutputLocationExceedsMaxColorAttachments) {
    std::ostringstream stream;
    stream << "@stage(fragment) fn main() -> @location(" << kMaxColorAttachments << R"() vec4<f32> {
            return vec4<f32>(0.0, 1.0, 0.0, 1.0);
        })";
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, stream.str().c_str()));
}

// Test that it is invalid to create a shader module with no chained descriptor. (It must be
// WGSL or SPIRV, not empty)
TEST_F(ShaderModuleValidationTest, NoChainedDescriptor) {
    wgpu::ShaderModuleDescriptor desc = {};
    ASSERT_DEVICE_ERROR(device.CreateShaderModule(&desc));
}

// Test that it is not allowed to use combined texture and sampler.
TEST_F(ShaderModuleValidationTest, CombinedTextureAndSampler) {
    // SPIR-V ASM produced by glslang for the following fragment shader:
    //
    //   #version 450
    //   layout(set = 0, binding = 0) uniform sampler2D tex;
    //   void main () {}
    //
    // Note that the following defines an interface combined texture/sampler which is not allowed
    // in Dawn / WebGPU.
    //
    //   %8 = OpTypeSampledImage %7
    //   %_ptr_UniformConstant_8 = OpTypePointer UniformConstant %8
    //   %tex = OpVariable %_ptr_UniformConstant_8 UniformConstant
    const char* shader = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpName %main "main"
               OpName %tex "tex"
               OpDecorate %tex DescriptorSet 0
               OpDecorate %tex Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
          %7 = OpTypeImage %float 2D 0 0 0 1 Unknown
          %8 = OpTypeSampledImage %7
%_ptr_UniformConstant_8 = OpTypePointer UniformConstant %8
        %tex = OpVariable %_ptr_UniformConstant_8 UniformConstant
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
        )";

    ASSERT_DEVICE_ERROR(utils::CreateShaderModuleFromASM(device, shader));
}

// Test that it is not allowed to declare a multisampled-array interface texture.
// TODO(enga): Also test multisampled cube, cube array, and 3D. These have no GLSL keywords.
TEST_F(ShaderModuleValidationTest, MultisampledArrayTexture) {
    // SPIR-V ASM produced by glslang for the following fragment shader:
    //
    //  #version 450
    //  layout(set=0, binding=0) uniform texture2DMSArray tex;
    //  void main () {}}
    //
    // Note that the following defines an interface array multisampled texture which is not allowed
    // in Dawn / WebGPU.
    //
    //  %7 = OpTypeImage %float 2D 0 1 1 1 Unknown
    //  %_ptr_UniformConstant_7 = OpTypePointer UniformConstant %7
    //  %tex = OpVariable %_ptr_UniformConstant_7 UniformConstant
    const char* shader = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpName %main "main"
               OpName %tex "tex"
               OpDecorate %tex DescriptorSet 0
               OpDecorate %tex Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
          %7 = OpTypeImage %float 2D 0 1 1 1 Unknown
%_ptr_UniformConstant_7 = OpTypePointer UniformConstant %7
        %tex = OpVariable %_ptr_UniformConstant_7 UniformConstant
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
        )";

    ASSERT_DEVICE_ERROR(utils::CreateShaderModuleFromASM(device, shader));
}

// Tests that shader module compilation messages can be queried.
TEST_F(ShaderModuleValidationTest, GetCompilationMessages) {
    // This test works assuming ShaderModule is backed by a dawn::native::ShaderModuleBase, which
    // is not the case on the wire.
    DAWN_SKIP_TEST_IF(UsesWire());

    wgpu::ShaderModule shaderModule = utils::CreateShaderModule(device, R"(
        @stage(fragment) fn main() -> @location(0) vec4<f32> {
            return vec4<f32>(0.0, 1.0, 0.0, 1.0);
        })");

    dawn::native::ShaderModuleBase* shaderModuleBase = dawn::native::FromAPI(shaderModule.Get());
    dawn::native::OwnedCompilationMessages* messages = shaderModuleBase->GetCompilationMessages();
    messages->ClearMessages();
    messages->AddMessageForTesting("Info Message");
    messages->AddMessageForTesting("Warning Message", wgpu::CompilationMessageType::Warning);
    messages->AddMessageForTesting("Error Message", wgpu::CompilationMessageType::Error, 3, 4);
    messages->AddMessageForTesting("Complete Message", wgpu::CompilationMessageType::Info, 3, 4, 5,
                                   6);

    auto callback = [](WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo* info,
                       void* userdata) {
        ASSERT_EQ(WGPUCompilationInfoRequestStatus_Success, status);
        ASSERT_NE(nullptr, info);
        ASSERT_EQ(4u, info->messageCount);

        const WGPUCompilationMessage* message = &info->messages[0];
        ASSERT_STREQ("Info Message", message->message);
        ASSERT_EQ(WGPUCompilationMessageType_Info, message->type);
        ASSERT_EQ(0u, message->lineNum);
        ASSERT_EQ(0u, message->linePos);

        message = &info->messages[1];
        ASSERT_STREQ("Warning Message", message->message);
        ASSERT_EQ(WGPUCompilationMessageType_Warning, message->type);
        ASSERT_EQ(0u, message->lineNum);
        ASSERT_EQ(0u, message->linePos);

        message = &info->messages[2];
        ASSERT_STREQ("Error Message", message->message);
        ASSERT_EQ(WGPUCompilationMessageType_Error, message->type);
        ASSERT_EQ(3u, message->lineNum);
        ASSERT_EQ(4u, message->linePos);

        message = &info->messages[3];
        ASSERT_STREQ("Complete Message", message->message);
        ASSERT_EQ(WGPUCompilationMessageType_Info, message->type);
        ASSERT_EQ(3u, message->lineNum);
        ASSERT_EQ(4u, message->linePos);
        ASSERT_EQ(5u, message->offset);
        ASSERT_EQ(6u, message->length);
    };

    shaderModule.GetCompilationInfo(callback, nullptr);
}

// Tests that we validate workgroup size limits.
TEST_F(ShaderModuleValidationTest, ComputeWorkgroupSizeLimits) {
    auto CheckShaderWithWorkgroupSize = [this](bool success, uint32_t x, uint32_t y, uint32_t z) {
        std::ostringstream ss;
        ss << "@stage(compute) @workgroup_size(" << x << "," << y << "," << z << ") fn main() {}";

        wgpu::ComputePipelineDescriptor desc;
        desc.compute.entryPoint = "main";
        desc.compute.module = utils::CreateShaderModule(device, ss.str().c_str());

        if (success) {
            device.CreateComputePipeline(&desc);
        } else {
            ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&desc));
        }
    };

    wgpu::Limits supportedLimits = GetSupportedLimits().limits;

    CheckShaderWithWorkgroupSize(true, 1, 1, 1);
    CheckShaderWithWorkgroupSize(true, supportedLimits.maxComputeWorkgroupSizeX, 1, 1);
    CheckShaderWithWorkgroupSize(true, 1, supportedLimits.maxComputeWorkgroupSizeY, 1);
    CheckShaderWithWorkgroupSize(true, 1, 1, supportedLimits.maxComputeWorkgroupSizeZ);

    CheckShaderWithWorkgroupSize(false, supportedLimits.maxComputeWorkgroupSizeX + 1, 1, 1);
    CheckShaderWithWorkgroupSize(false, 1, supportedLimits.maxComputeWorkgroupSizeY + 1, 1);
    CheckShaderWithWorkgroupSize(false, 1, 1, supportedLimits.maxComputeWorkgroupSizeZ + 1);

    // No individual dimension exceeds its limit, but the combined size should definitely exceed the
    // total invocation limit.
    CheckShaderWithWorkgroupSize(false, supportedLimits.maxComputeWorkgroupSizeX,
                                 supportedLimits.maxComputeWorkgroupSizeY,
                                 supportedLimits.maxComputeWorkgroupSizeZ);
}

// Tests that we validate workgroup storage size limits.
TEST_F(ShaderModuleValidationTest, ComputeWorkgroupStorageSizeLimits) {
    wgpu::Limits supportedLimits = GetSupportedLimits().limits;

    constexpr uint32_t kVec4Size = 16;
    const uint32_t maxVec4Count = supportedLimits.maxComputeWorkgroupStorageSize / kVec4Size;
    constexpr uint32_t kMat4Size = 64;
    const uint32_t maxMat4Count = supportedLimits.maxComputeWorkgroupStorageSize / kMat4Size;

    auto CheckPipelineWithWorkgroupStorage = [this](bool success, uint32_t vec4_count,
                                                    uint32_t mat4_count) {
        std::ostringstream ss;
        std::ostringstream body;
        if (vec4_count > 0) {
            ss << "var<workgroup> vec4_data: array<vec4<f32>, " << vec4_count << ">;";
            body << "_ = vec4_data;";
        }
        if (mat4_count > 0) {
            ss << "var<workgroup> mat4_data: array<mat4x4<f32>, " << mat4_count << ">;";
            body << "_ = mat4_data;";
        }
        ss << "@stage(compute) @workgroup_size(1) fn main() { " << body.str() << " }";

        wgpu::ComputePipelineDescriptor desc;
        desc.compute.entryPoint = "main";
        desc.compute.module = utils::CreateShaderModule(device, ss.str().c_str());

        if (success) {
            device.CreateComputePipeline(&desc);
        } else {
            ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&desc));
        }
    };

    CheckPipelineWithWorkgroupStorage(true, 1, 1);
    CheckPipelineWithWorkgroupStorage(true, maxVec4Count, 0);
    CheckPipelineWithWorkgroupStorage(true, 0, maxMat4Count);
    CheckPipelineWithWorkgroupStorage(true, maxVec4Count - 4, 1);
    CheckPipelineWithWorkgroupStorage(true, 4, maxMat4Count - 1);

    CheckPipelineWithWorkgroupStorage(false, maxVec4Count + 1, 0);
    CheckPipelineWithWorkgroupStorage(false, maxVec4Count - 3, 1);
    CheckPipelineWithWorkgroupStorage(false, 0, maxMat4Count + 1);
    CheckPipelineWithWorkgroupStorage(false, 4, maxMat4Count);
}

// Test that numeric ID must be unique
TEST_F(ShaderModuleValidationTest, OverridableConstantsNumericIDConflicts) {
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
@id(1234) override c0: u32;
@id(1234) override c1: u32;

struct Buf {
    data : array<u32, 2>
}

@group(0) @binding(0) var<storage, read_write> buf : Buf;

@stage(compute) @workgroup_size(1) fn main() {
    // make sure the overridable constants are not optimized out
    buf.data[0] = c0;
    buf.data[1] = c1;
})"));
}

// Test that @binding must be less then kMaxBindingNumber
TEST_F(ShaderModuleValidationTest, MaxBindingNumber) {
    static_assert(kMaxBindingNumber == 65535);

    wgpu::ComputePipelineDescriptor desc;
    desc.compute.entryPoint = "main";

    // kMaxBindingNumber is valid.
    desc.compute.module = utils::CreateShaderModule(device, R"(
        @group(0) @binding(65535) var s : sampler;
        @stage(compute) @workgroup_size(1) fn main() {
            _ = s;
        }
    )");
    device.CreateComputePipeline(&desc);

    // kMaxBindingNumber + 1 is an error
    desc.compute.module = utils::CreateShaderModule(device, R"(
        @group(0) @binding(65536) var s : sampler;
        @stage(compute) @workgroup_size(1) fn main() {
            _ = s;
        }
    )");
    ASSERT_DEVICE_ERROR(device.CreateComputePipeline(&desc));
}

// Test that missing decorations on shader IO or bindings causes a validation error.
TEST_F(ShaderModuleValidationTest, MissingDecorations) {
    // Vertex input.
    utils::CreateShaderModule(device, R"(
        @stage(vertex) fn main(@location(0) a : vec4<f32>) -> @builtin(position) vec4<f32> {
            return vec4(1.0);
        }
    )");
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        @stage(vertex) fn main(a : vec4<f32>) -> @builtin(position) vec4<f32> {
            return vec4(1.0);
        }
    )"));

    // Vertex output
    utils::CreateShaderModule(device, R"(
        struct Output {
            @builtin(position) pos : vec4<f32>,
            @location(0) a : f32,
        }
        @stage(vertex) fn main() -> Output {
            var output : Output;
            return output;
        }
    )");
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        struct Output {
            @builtin(position) pos : vec4<f32>,
            a : f32,
        }
        @stage(vertex) fn main() -> Output {
            var output : Output;
            return output;
        }
    )"));

    // Fragment input
    utils::CreateShaderModule(device, R"(
        @stage(fragment) fn main(@location(0) a : vec4<f32>) -> @location(0) f32 {
            return 1.0;
        }
    )");
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        @stage(fragment) fn main(a : vec4<f32>) -> @location(0) f32 {
            return 1.0;
        }
    )"));

    // Fragment input
    utils::CreateShaderModule(device, R"(
        @stage(fragment) fn main() -> @location(0) f32 {
            return 1.0;
        }
    )");
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        @stage(fragment) fn main() -> f32 {
            return 1.0;
        }
    )"));

    // Binding decorations
    utils::CreateShaderModule(device, R"(
        @group(0) @binding(0) var s : sampler;
        @stage(fragment) fn main() -> @location(0) f32 {
            _ = s;
            return 1.0;
        }
    )");
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        @binding(0) var s : sampler;
        @stage(fragment) fn main() -> @location(0) f32 {
            _ = s;
            return 1.0;
        }
    )"));
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
        @group(0) var s : sampler;
        @stage(fragment) fn main() -> @location(0) f32 {
            _ = s;
            return 1.0;
        }
    )"));
}

// Test that WGSL extension used by enable directives must be allowed by WebGPU.
TEST_F(ShaderModuleValidationTest, ExtensionMustBeAllowed) {
    ASSERT_DEVICE_ERROR(utils::CreateShaderModule(device, R"(
enable InternalExtensionForTesting;

@stage(compute) @workgroup_size(1) fn main() {})"));
}
